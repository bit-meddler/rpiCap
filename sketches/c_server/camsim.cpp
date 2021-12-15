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

// Language
#include <iostream>
#include <string>
#include <cstring> // memset

// libs
#include "camConsts.hpp"

// Globals
CamConsts::camRegs registers{ 0 } ; // camera control registers
uint32_t roll_cnt = 0 ; // rolling counter
uint32_t packet_cnt = 0 ; // packet counter
const int cam_id = 1 ;

void bail( const std::string msg ) {
    std::cerr << msg << std::endl ;
    exit( 1 ) ;
}

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

int setupSocket( void ) {
    
    struct sockaddr_in sock_addr ;
    memset( (char *) &sock_addr, 0, sizeof( sock_addr ) ) ;

    int sock ;
	if( (sock=socket( AF_INET, SOCK_DGRAM, IPPROTO_UDP )) == -1 ) {
    		bail( "Socket Creation" ) ;
	}

	sock_addr.sin_family = AF_INET ;
	sock_addr.sin_port = htons( CamConsts::UDP_PORT_TX ) ;
	sock_addr.sin_addr.s_addr = htonl( INADDR_ANY ) ;

	if( bind( sock, (struct sockaddr*) &sock_addr, sizeof( sock_addr ) ) == -1) {
		bail("bind");
	}

    return sock ;
}

// packetize

// send packets

void retort( const std::string msg ) {
    // will be packetized and emitted later...
    char buff[100] ;
    snprintf( buff, sizeof(buff), "*** CAMERA %d %s ***", cam_id, msg.c_str() ) ;
    std::string payload = buff ;
    printf( payload.c_str() ) ;
}

void retortReg( const int reg, const int val ) {
    // will be packetized and emitted later...
    char buff[100] ;
    snprintf( buff, sizeof(buff), "*** CAMERA %d setting register %d to %d ***", cam_id, reg, val ) ;
    std::string payload = buff ;
    printf( payload.c_str() ) ;
}

void recvLoop( const int sock_fd ) {
    // Get ready to recv

    char in_buf[ CamConsts::RECV_BUFF_SZ ] ;
    memset( (char *) &in_buf, 0, CamConsts::RECV_BUFF_SZ ) ;

    struct sockaddr_in sender_data ;
    socklen_t sa_size = sizeof( sender_data ) ;

    bool running = 1 ;
    int recv_len = 0 ;
    int recv_cnt = 0 ;

    while( running ) {
        std::cout << "Waiting for Packet" << std::endl ;

        // receve
		if( (recv_len = recvfrom( sock_fd, in_buf, CamConsts::RECV_BUFF_SZ, 0, (struct sockaddr *) &sender_data, &sa_size )) == -1 ) {
			bail( "recvfrom()" ) ;
		}

        for( int i=0; i<recv_len; i++ ) {
		    printf( "%02x ", in_buf[i] ) ;
        }
        printf( "\n" ) ;

        // interpret
        switch ( in_buf[ 0 ] ) {
            /*************** C O M M A N D S *************************************/
            case CamConsts::CMD_START : {
                // Start Streaming Centroids
                retort( "Start Centroids" ) ;
            }
            break ;
            case CamConsts::CMD_STOP  : {
                // Stop Streaming Centroids
                retort( "Stop Centroids" ) ;
            }
            break ;
            case CamConsts::CMD_SINGLE : {
                // Send a lineup image
                retort( "Send Image" ) ;
            }
            break ;
            case CamConsts::CMD_STREAM : {
                // Start Streaming Images (line speed limited)
                retort( "Start Image Stream") ;
            }
            break ;

            /*************** R E Q U E S T S *************************************/
            case CamConsts::REQ_REGSLO : {
                // Request lower registers
                retort( "Sending Low Registers" ) ;
            }
            break ;
            case CamConsts::REQ_REGSHI  : {
                // Request higher registers
                retort( "Sending High Registers" ) ;
            }
            break ;
            case CamConsts::REQ_VERSION : {
                // Request version data
                retort( "Version info" ) ;
            }
            break ;
            case CamConsts::REQ_HELLO : {
                retort( "h" ) ;
            }
            break ;
            
            /*************** R E G I S T E R S *************************************/
            case CamConsts::SET_BYTE : {
                // set 1 byte data in 1 byte register
                retortReg( (int) in_buf[1], in_buf[2] ) ;
                registers.reg[ (size_t) in_buf[1] ] = in_buf[2] ;
            }
            break ;
            case CamConsts::SET_SHORT  : {
                // set 2 Byte data in 2 Byte register
                // Bit of a problem with ended-ness here
                int reg_address = __bswap_16( *reinterpret_cast< uint16_t *>(  &in_buf[1] ) ) ;

                if( reg_address >= 300 ) {
                    // masks are uint16_t
                    uint16_t val = __bswap_16( *reinterpret_cast< uint16_t* >( &in_buf[3] ) ) ;
                    registers.reg[ reg_address   ] = in_buf[4] ;
                    registers.reg[ reg_address+1 ] = in_buf[3] ;
                    retortReg( reg_address, (int) val ) ;

                } else {
                    // IMUs are int16_t
                    int16_t val = __bswap_16( *reinterpret_cast< uint16_t* >( &in_buf[3] ) ) ;
                    registers.reg[ reg_address ] = (int) val ;
                    registers.reg[ reg_address   ] = in_buf[4] ;
                    registers.reg[ reg_address+1 ] = in_buf[3] ;
                    retortReg( reg_address, (int) val ) ;
                }
            }
            break ;
            case CamConsts::SET_WORD : {
                // set 4 byte data in 1 byte register
                perror( "No 4 byte commands for a cammera!" ) ;
            }
            break ;

            default : {
                perror( "Unexpected item in the bagging area!" ) ;
            }
            break;
        }

        // DEBUG: Monitor regs
        printf( "\nfps: %d strobe: %d threashold: %d maskzone01x: %d impactrefx %d\n",
            registers.fps, registers.strobe, registers.threshold, registers.maskzone01x, registers.impactrefx
        ) ;

        // exit
        recv_cnt++ ;
        if( recv_cnt > 10 ){
            running = 0 ;
        }
    }
}

int main( int argc, char* args[] ) {

    // Initalization
    initalizeRegs() ;
    printf( "impactHi %d ImpactLo %d Impact %d\n", registers.reg[214], registers.reg[215], registers.impactrefx ) ;

    // Create and Bind the Socket
	int sock_fd = setupSocket() ;

    // receve
    recvLoop( sock_fd ) ;

    // cleanup
    close( sock_fd ) ;
    

    return 0 ;
}