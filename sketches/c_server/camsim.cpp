/*
# 
# Copyright (C) 2016~2021 The Gimli Project
# This file is part of rpiCap <https://github.com/bit-meddler/rpiCap>.
#
# rpiCap is free software: you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation, either version 3 of the License, or
# (at your option) any later version.
#
# rpiCap is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.
#
# You should have received a copy of the GNU General Public License
# along with rpiCap.  If not, see <http://www.gnu.org/licenses/>.
#
*/

// Networking Imports
#include <arpa/inet.h>
#include <sys/socket.h>
#include <unistd.h>
#include <sys/select.h>
#include <sys/fcntl.h> // set non-blocking on the CnC Port?

// Linux / Posix stuff
#include <unistd.h>
#include <sys/eventfd.h>

// Language
#include <sys/types.h>
#include <iostream>
#include <string>
#include <cstring> // memset

// Threading
#include <thread>
#include <mutex>

// Containers
#include <queue>

// libs
#include "camConsts.hpp"
#include "camUtils.hpp"


// Globals
// General Camera Control
CamConsts::CamRegs registers{ 0 } ; // camera control registers
CamConsts::PacketData packetData ; // Packetization aids
std::mutex packet_dat_mtx ; // OK this needs a mutex as well
CamConsts::Timecode timecode{0} ; // Global clock (This will probably change)

// 'Fixed' Settings
const int  CAM_ID = 1 ; // should be derived from IP TBH
const char CENTROID_COMPRESSION = CamConsts::ROIDS_8BYTE ;
const char CENTROID_SIZE = 10 ;

// Socket Comunications
int sock_rx_fd = -1 ; // CnC socket (receve)
struct sockaddr_in sock_addr {0} ; // The "Server" to which all cameras send 'roids and other goodies
socklen_t SA_SIZE = sizeof( sock_addr ) ; // Frequently used
// eventfd
int queue_rx_efd = -1 ; // when emplacing on the queue also write to the efd
const size_t EFD_SIZE = sizeof( uint64_t ) ;
const uint64_t EFD_ONE = 1 ; // we frequently write one to the efd
// for select
int num_fds = -1; // max fd num

// 'Threadsafe' Packet Queue
std::priority_queue< CamConsts::QPacket > transmit_queue ; // Packet queue
std::mutex queue_mtx ;

// Implementation

// get the expected header size, based on packet data type
inline size_t getHeadSize( const char dtype ){
    return (dtype==CamConsts::PACKET_IMAGE) ? 24 : 16 ;
}

// A big enough error to abort the program
void bail( const std::string msg ) {
    std::cerr << msg << std::endl ;
    exit( EXIT_FAILURE ) ;
}

// An errror, but just one to report / log
void fail( const std::string msg ) {
    std::cerr << msg << std::endl ;
}

// Just a report or log, not error
void wail( const std::string msg ) {
    std::cout << msg << std::endl ;
}

// initalize the registers to 0 then apply defaults
void initalizeRegs( void ) {
    // sys defaults
    registers.fps       =  19 ;
    registers.strobe    =  20 ;
    registers.shutter   = 250 ;
    registers.mtu       =   0 ;
    registers.iscale    =   1 ;
    registers.idelay    =  15 ;
    registers.threshold = 128 ;
    registers.numdets   =  13 ;
    registers.arpdelay  =  15 ;

    // dummy figures for the IMU
    registers.impactflags =  48 ;
    registers.impactlimit = 150 ;
    registers.impactrefx  = 666 ;
    registers.impactrefy  = 666 ;
    registers.impactrefz  = 666 ;
    registers.impactvalx  = 666 ;
    registers.impactvaly  = 666 ;
    registers.impactvalz  = 666 ;
}

// setup the UDP Socket for CnC
void setupComunications( void ) {

    //memset( (char *) &sock_addr, 0, sizeof( sock_addr ) ) ;

    if( sock_rx_fd > 0 ) {
        // already bound?
        fail("The RX Socket seems to be already bound, closing and rebinding" ) ;
        close( sock_rx_fd ) ;
    }

    // Receving Socket
    sock_rx_fd = socket( AF_INET, SOCK_DGRAM, IPPROTO_UDP ) ;
    if( sock_rx_fd == -1 ) {
        bail( "Failed to Create RX Socket" ) ;
    }

    sock_addr.sin_family = AF_INET ;
    sock_addr.sin_port = htons( CamConsts::UDP_PORT_TX ) ;
    sock_addr.sin_addr.s_addr = htonl( INADDR_ANY ) ;

    if( bind( sock_rx_fd, (struct sockaddr*) &sock_addr, SA_SIZE ) == -1) {
        bail( "Error Binding RX SOcket" ) ;
    }

    // Setup Target Address (Allways send to fixed ip:port)
    sock_addr.sin_family = AF_INET ;
    sock_addr.sin_port = htons( CamConsts::UDP_PORT_RX ) ;
    sock_addr.sin_addr.s_addr = inet_addr( CamConsts::SERVER_IP.c_str() ) ;

    // Event File Descriptors
    queue_rx_efd = eventfd( 0, 0 ) ;
    if( queue_rx_efd == -1 ) {
        bail( "ERROR creating eventfd" ) ;
    }

    // count fds for select
    num_fds = queue_rx_efd + 1 ;
}

// cleanly close the sockets and anything else
void teardownComunications( void ) {
    close( sock_rx_fd ) ;
    close( queue_rx_efd ) ;
}

// form a packet header
void composeHeader(       uint8_t* data,          // buffer to write headder into
                    const size_t   num_dts,       // Number of Centroids
                    const uint8_t  compression,   // Centroids Format
                    const char     packet_type,   // Packet Type
                    const size_t   frag_num,      // Fragment number
                    const size_t   frag_count,    // Fragment Count
                    const CamConsts::Timecode tc, // current time
                    const size_t   img_os=0,      // image offset
                    const size_t   img_sz=0       // image total size
) {
    std::lock_guard<std::mutex> lock_packet_dat( packet_dat_mtx ) ;
    CamConsts::Header header ; // should be initalized as 0

    // Encode Rolling Packet count
    uint16_t short_tmp = (((packetData.packet_cnt & 0x1F80) << 1) | 0x8000) | (packetData.packet_cnt & 0x007F) ;
    header.packet_count = __bswap_16( short_tmp ) ;
    short_tmp = 0 ;

    // Encode centroid count and format
    short_tmp = (((num_dts & 0x0380) << 1) | 0x4000) | (num_dts & 0x007F) | ((compression & 0x0007) << 11) ;
    header.centroid_count = __bswap_16( short_tmp ) ;

    // Chose correct header size
    size_t size = getHeadSize( packet_type ) ;

    // Compose the header
    header.flag = 0 ;
    header.dtype = packet_type ;
    header.roll_count = packetData.roll_cnt ;
    header.size = size ;
    header.tc_h = tc.h ;    
    header.tc_m = tc.m ;
    header.tc_s = tc.s ;
    header.tc_f = tc.f ;
    header.frag_num   = frag_num ;
    header.frag_count = frag_count ;

    // Just zeros if unset
    header.img_os = img_os ; 
    header.img_sz = img_sz ; 

    // copy header into the buffer
    memcpy( data, &header, size ) ;
}

void packetizeNoneImg( const size_t   roid_count,   // total number of centroids
                       const char     packet_type,  // Packet Type
                       const size_t   frag_num,     // Fragment number
                       const size_t   frag_count,   // Fragment Count
                       const uint8_t* data,         // Payload
                       const size_t   data_sz,      // Size of Payload
                       const CamConsts::Timecode tc // Current time
) {
    size_t buffer_required = 16 + data_sz ;
    uint8_t* dat = new uint8_t[ buffer_required ]{0} ;
    composeHeader( dat, roid_count, CENTROID_COMPRESSION, packet_type, frag_num, frag_count, tc ) ;
    {
        std::lock_guard<std::mutex> lock_packet_dat( packet_dat_mtx ) ;
        packetData.inc() ;
    }
    memcpy( dat+16, data, data_sz ) ;
    int priority = (packet_type==CamConsts::PACKET_ROIDS) ? CamConsts::PRI_IMMEDIATE : CamConsts::PRI_NORMAL ;
    priority += frag_num ;
    
    // Enqueue
    CamConsts::QPacket* dgm = new CamConsts::QPacket( priority, buffer_required, dat ) ;
    {
        std::lock_guard<std::mutex> lock_queue( queue_mtx ) ;
        transmit_queue.push( *dgm ) ;

        // announce the packet on the queue
        size_t write_sz = write( queue_rx_efd, &EFD_ONE, EFD_SIZE ) ;
        if( write_sz != EFD_SIZE ) {
            bail( "ERROR writing to the queue efd" ) ;
        }
    }
}

// break apart a vector of packed Centroids into fragments, respecting the "max dets" register setting.
void fragmentCentroids(       vision::Roid8Vec_t  data, // Vector of centroids to fragment & enque (can't be const for cast)
                        const CamConsts::Timecode tc    // Current time
) {
    const int max_dets = registers.numdets * 10 ;
    const int num_dets = data.size() ;
    int num_packets     = num_dets / max_dets ;
    int remaining_roids = num_dets % max_dets ;
    num_packets += (remaining_roids>0) ? 1 : 0 ;

    const int slice_size = max_dets * CENTROID_SIZE ;
    const int total_size = num_dets * CENTROID_SIZE ;
    int packet_no = 1 ;

    for( size_t i=0; i<total_size; i+=slice_size ) {
        packetizeNoneImg( num_dets, CamConsts::PACKET_ROIDS, packet_no, num_packets,
                          reinterpret_cast<uint8_t*>(data.data()+i), slice_size, tc ) ;
        packet_no++ ;
    }
}

// send packets
void sendDatagram( uint8_t* data, size_t size ) {
    int sent = sendto( sock_rx_fd, data, size, 0, (struct sockaddr*) &sock_addr, SA_SIZE ) ;
        if( sent != (int) size ) {
            fail( "failed to send packet" ) ;
        }
}

// Simple Message
void retort( const std::string msg ) {
    // will be packetized and emitted later...
    char buff[100] ;
    snprintf( buff, sizeof(buff), "*** CAMERA %d %s ***", CAM_ID, msg.c_str() ) ;
    packetizeNoneImg( 0, CamConsts::PACKET_TEXT, 1, 1, (uint8_t*)buff, strlen( buff ), timecode ) ;
}

// Regiser Setting Message
void retortReg( const int reg, const int val ) {
    // will be packetized and emitted later...
    char buff[100] ;
    snprintf( buff, sizeof(buff), "*** CAMERA %d setting register %d to %d ***", CAM_ID, reg, val ) ;
    packetizeNoneImg( 0, CamConsts::PACKET_TEXT, 1, 1, (uint8_t*)buff, strlen( buff ), timecode ) ;
}

// Handle CnC Messages
void handleCnC( uint8_t* msg, const size_t len ) {
    switch ( msg[ 0 ] ) {
            /*************** C O M M A N D S *************************************/
            case CamConsts::CMD_START : {
                // Start Streaming Centroids
                retort( "Start Centroids" ) ;
                registers._roid_stream = 1 ;
            }
            break ;
            case CamConsts::CMD_STOP  : {
                // Stop Streaming everything
                retort( "Stop Streaming" ) ;
                registers._roid_stream = 0 ;
                registers._img_stream  = 0 ;
            }
            break ;
            case CamConsts::CMD_SINGLE : {
                // Send a lineup image
                retort( "Send Image" ) ;
                registers._send_one_img = 1 ;
            }
            break ;
            case CamConsts::CMD_STREAM : {
                // Start Streaming Images (line speed limited)
                retort( "Start Image Stream") ;
                registers._img_stream = 1 ;
            }
            break ;

            /*************** R E Q U E S T S *************************************/
            case CamConsts::REQ_REGSLO : {
                // Request lower registers
                wail( "Sending Low Registers" ) ;
                packetizeNoneImg( 0, CamConsts::PACKET_REGLO, 1, 1, &registers.fps,  56, timecode ) ;
            }
            break ;
            case CamConsts::REQ_REGSHI  : {
                // Request higher registers
                wail( "Sending High Registers" ) ;
                packetizeNoneImg( 0, CamConsts::PACKET_REGHI, 1, 1, &registers.fps, 824, timecode ) ;
            }
            break ;
            case CamConsts::REQ_VERSION : {
                // Request version data
                wail( "Version info" ) ;
                packetizeNoneImg( 0, CamConsts::PACKET_VERS, 1, 1, (uint8_t*)"DEADBEEF", 8, timecode ) ;
            }
            break ;
            case CamConsts::REQ_HELLO : {
                packetizeNoneImg( 0, CamConsts::PACKET_TEXT, 1, 1, (uint8_t*)"h", 1, timecode ) ;
            }
            break ;
            
            /*************** R E G I S T E R S *************************************/
            case CamConsts::SET_BYTE : {
                // set 1 byte data in 1 byte register
                if( len < 3 ){
                    fail( "incomplete 'Set' Message (Byte)" ) ;
                    break ;
                }
                retortReg( (int) msg[1], msg[2] ) ;
                registers.reg[ (size_t) msg[1] ] = msg[2] ;
            }
            break ;
            case CamConsts::SET_SHORT  : {
                // set 2 Byte data in 2 Byte register
                if( len < 5 ){
                    fail( "incomplete 'Set' Message (Short)" ) ;
                    break ;
                }
                // Bit of a problem with ended-ness here
                int reg_address = __bswap_16( *reinterpret_cast< uint16_t *>(  &msg[1] ) ) ;

                if( reg_address >= 300 ) {
                    // masks are uint16_t
                    uint16_t val = __bswap_16( *reinterpret_cast< uint16_t* >( &msg[3] ) ) ;
                    registers.reg[ reg_address   ] = msg[4] ;
                    registers.reg[ reg_address+1 ] = msg[3] ;
                    retortReg( reg_address, (int) val ) ;

                } else {
                    // IMUs are int16_t
                    int16_t val = __bswap_16( *reinterpret_cast< uint16_t* >( &msg[3] ) ) ;
                    registers.reg[ reg_address ] = (int) val ;
                    registers.reg[ reg_address   ] = msg[4] ;
                    registers.reg[ reg_address+1 ] = msg[3] ;
                    retortReg( reg_address, (int) val ) ;
                }
            }
            break ;
            case CamConsts::SET_WORD : {
                // set 4 byte data in 1 byte register
                fail( "No 4 byte commands for a cammera!" ) ;
            }
            break ;

            default : {
                fail( "Unexpected item in the bagging area!" ) ;
            }
            break;
        }
} // handleCnC( buffer, len )

// Main loop for CnC.  Will be semaphored when data (Image/Centroids) are available.
void recvLoop( void ) {

    // receve Buffer
    using CamConsts::RECV_BUFF_SZ ;
    uint8_t in_buf[ RECV_BUFF_SZ ] ; // receving buffer
    memset( (char *) &in_buf, 0, RECV_BUFF_SZ ) ;
    int recv_len = 0 ; // how many bytes we have read
    uint64_t queue_rx_cnt = 0 ; // value from the queue event

    // Networking
    struct sockaddr_in sender_data ; // not used
    fd_set readable_fds ; // set of readable fds
    struct timeval select_timeout ; // timeout for select
    int ready_fd = 0 ; // result of select -1: error, 0: timeout, 1: OK

    // System Control
    bool   running  = 1 ;
    size_t recv_cnt = 0 ;
    size_t send_cnt = 0 ;

    // For Packet sending
    CamConsts::QPacket packet( 0, 0, nullptr ) ; // Temp data packet

    while( running ) {
        std::cout << "Waiting for Packet" << std::endl ;

        // Select
        select_timeout.tv_sec = CamConsts::SOCKET_TIMEOUT ;
        select_timeout.tv_usec = 0 ;
        FD_ZERO( &readable_fds ) ;
        FD_SET( sock_rx_fd,   &readable_fds) ;
        FD_SET( queue_rx_efd, &readable_fds) ;

        queue_rx_cnt = 0 ; // size of queue

        ready_fd = select( num_fds, &readable_fds, NULL, NULL, &select_timeout) ;

        if( ready_fd < 0) {
            bail( "Error in 'select'" ) ;
        } else if( ready_fd == 0 )  {
            wail( "select Timed out" ) ;
        } else {
            // DTRT bassed on the available socket
            if( FD_ISSET( sock_rx_fd, &readable_fds ) ) {
                // receve from CnC Socket
                recv_len = recvfrom( sock_rx_fd, in_buf, RECV_BUFF_SZ, 0, (struct sockaddr *) &sender_data, &SA_SIZE ) ;
                if( recv_len == -1 ) {
                    bail( "Error Reading from CnC Socket" ) ;
                }
                // message receved
                recv_cnt++ ;

                // interpret the message
                handleCnC( in_buf, recv_len ) ;

                // clean up the buffer
                memset( (char *) &in_buf, 0, recv_len ) ;

                // Tidy up the socket set
                FD_CLR( sock_rx_fd, &readable_fds ) ;
            }

            if( FD_ISSET( queue_rx_efd, &readable_fds ) ) {
                //
                recv_len = read( queue_rx_efd, &queue_rx_cnt, EFD_SIZE ) ;
                if( recv_len != EFD_SIZE ) {
                    bail( "ERROR reading" ) ;
                }
                printf( "efd says '%lld'\n", queue_rx_cnt ) ;
                FD_CLR( queue_rx_efd, &readable_fds ) ;
            }
        } // handling 'selected' file descriptors

        // emit any available packets
        if( queue_rx_cnt > 0 ) {
            std::lock_guard<std::mutex> lock_queue( queue_mtx ) ;

            // If available, send an enqueued packet. Deleting the datagram buffer once sent.
            if( !transmit_queue.empty() ) {
                packet = transmit_queue.top() ;
                sendDatagram( packet.data, packet.size ) ;
                send_cnt += 1 ;
                hexdump( packet.data, packet.size ) ;
                delete [] packet.data ;
                transmit_queue.pop() ; // does this delete the packet?
            }

            // If more datagrams are in the queue, write to the efd
            if( !transmit_queue.empty() ) {
                recv_len = write( queue_rx_efd, &EFD_ONE, EFD_SIZE ) ;
                if( recv_len != EFD_SIZE ) {
                    bail( "ERROR writing to the queue efd" ) ;
                }
            }
        } // end queue lockguard scope

        // exit
        if( recv_cnt > 11 ) {
            running = 0 ;
        }

    } // while running
    printf( "receved %d, sent %d\n", recv_cnt, send_cnt ) ;

} // recvLoop

// this is spawned as a thread, 
void simulateCamera( void ) {
    while( 1 ) {
        std::this_thread::sleep_for( std::chrono::milliseconds( 1000 ) ) ;
        // enqueue some bogus centroid data
        if( registers._roid_stream ) {
            //
            vision::Roid8Vec_t packed_centroids ;
            packed_centroids.reserve( 12 ) ;

        }
    }
}

// entry point
int main( int argc, char* args[] ) {

    // Initalization
    initalizeRegs() ;

    // Just a bogus timecode
    timecode.h = 10 ;
    timecode.m = 11 ;
    timecode.s = 12 ;
    timecode.f = 13 ;

    // Create and Bind the Socket
    setupComunications() ;

    // receve
    recvLoop() ;

    // cleanup
    teardownComunications() ;

    return EXIT_SUCCESS ;
}