# add Midget into the path
import os, sys
_git_root_ = os.path.dirname( os.path.dirname( os.path.dirname( os.path.realpath(__file__) ) ) )
sys.path.append( os.path.join( _git_root_, "midget", "Python" ) )
DATA_PATH = os.path.join( _git_root_, "rpiCap", "exampleData" )


import numpy as np # It's started....
from numpy.lib import recfunctions as rfn

import argparse
import pickle
from queue import PriorityQueue
import select
import socket
import struct
import threading
import time

from Comms import piCam, piComunicate
from Utils.timecode import SimpleTimecode


# Socket Defaults
SOCKET_TIMEOUT = 10
RECV_BUFF_SZ   = 10240 # bigger than a Jumbo Frame

# Replay
AVAILABLE_REPLAYS = ("ROM", "calibration")

# System Defaults
CENTROID_DATA_SIZE = 8 # HBHBBB


# Util Functions ---------------------------------------------------------------
def hex_( data ):
    return ':'.join( format(x, '02x') for x in data )

def encodeCentroids( dets ):
    """ Expects a det list [ [x,y], [x,y]  ], emits packed encoded centroid data"""
    if( len( dets ) < 1 ):
        return np.asarray( [], dtype=np.float32 )

    det_array = np.asarray( dets, dtype=np.float32 )

    Xs = det_array[:,0]
    Ys = det_array[:,1]

    Rs = np.random.random_sample( det_array.shape[0] )
    Rs *= 6.5

    Xs, Xf = np.divmod( Xs, 1. )
    Ys, Yf = np.divmod( Ys, 1. )
    Rs, Rf = np.divmod( Rs, 1. )

    Xs = Xs.astype( np.uint16 )
    Ys = Ys.astype( np.uint16 )
    Rs = Rs.astype( np.uint8  )

    Xf *= 256  # 8-Bit Fraction
    Yf *= 256
    Rf *= 128  # 4-Bit Fraction

    Xf = Xf.astype( np.uint8 )
    Yf = Yf.astype( np.uint8 )
    Rf = Rf.astype( np.uint8 )

    Rf = np.left_shift( Rf, 4 )

    # TODO: Add width, height bytes for full 10-Byte message, HB, HB, B, B, BB = X, Y, W, H, R
    return rfn.merge_arrays( (Xs, Xf, Ys, Yf, Rs, Rf) )


class PacketManager( object ):
    # Priorities
    PRIORITY_URGENT =  1
    PRIORITY_NORMAL = 10
    PRIORITY_IMAGES = 50

    # Reg locations of params (reg, sz, hi/lo)
    IDX_DET_CAP = piCam.TRAIT_LOCATIONS[ "numdets" ][0] - 200
    IDX_DGM_CAP = piCam.TRAIT_LOCATIONS[ "mtu" ][0]     - 200

    def __init__( self, cam_regs ):
        self.roll_count = 0
        self.send_count = 0
        self.q_out = PriorityQueue()
        self.cam_regs = cam_regs

    def packetize( self, dtype, data ):
        """
        Fragment 'data' to fit into packet specification (such as max dets / packet,
        Jumbo Frames).  'data' is Bytes-Like, so could be a retort string, or a flat
        ndarray of det data or RAW image (8-Bit Bitmap)

        :param dtype: (int) Message Type
        :param data:  (bytes) Message
        """
        if( dtype > piCam.PACKET_TYPES[ "imagedata" ] ):
            self.q_out.put( (self.PRIORITY_NORMAL, (0, dtype, 1, 1, data)) )

        elif (dtype == piCam.PACKET_TYPES[ "centroids" ]):
            # Get params from cam regs
            max_roids = self.cam_regs[ self.IDX_DET_CAP ]

            # Simulated centroids, data is a ndarray
            num_roids = len( data ) // CENTROID_DATA_SIZE

            num_packs, leftover_roids = divmod( num_roids, max_roids )

            if( num_packs == 0 and leftover_roids == 0 ):
                # Empty Packet
                self.q_out.put( (self.PRIORITY_URGENT, (0, dtype, 1, 1, b"")) )
                return

            if (leftover_roids > 0):
                num_packs += 1

            packet_no = 1
            slice_sz = max_roids * CENTROID_DATA_SIZE
            total_sz = num_roids * CENTROID_DATA_SIZE
            for i in range( 0, total_sz, slice_sz ):
                self.q_out.put(
                    (self.PRIORITY_URGENT + packet_no, (num_roids, dtype, packet_no, num_packs, data[ i:i + slice_sz ]))
                )
                packet_no += 1

        elif (dtype == piCam.PACKET_TYPES[ "imagedata" ]):
            # Same sort of logic as above, but queued with "PRIORITY_IMAGES", so centroids and messages go first while
            # the image queue is cleared
            max_dgm = (self.cam_regs[ IDX_DGM_CAP ] * 1024 ) + 1500
            dat_len = len( data )
            num_packs, leftovers = divmod( dat_len, max_dgm )

            # if it doesn't perfectly fit into the MTU
            if( leftovers > 0 ):
                num_packs += 1

            packet_no = 1
            for i in range( 0, dat_len, max_dgm ):
                self.q_out.put(
                    (self.PRIORITY_IMAGES + packet_no, (0, dtype, packet_no, num_packs, data[ i:i + max_dgm ]))
                )
                packet_no += 1

    def incCount( self ):
        self.send_count += 1
        self.roll_count += 1
        self.roll_count &= 0x00ff


class CamSim( object ):

    def __init__( self, local_ip, server_ip, cam_num=None, replay_data=None, id_overide=None, stride=None ):
        # basic settings
        self.local_ip = local_ip
        self.server_ip = server_ip
        self.cam_id = id_overide or self.local_ip.rsplit(".",1)[1]
        self.cam_num = cam_num or int( self.cam_id ) % 10

        ########
        # Replay
        self.replay = replay_data if replay_data in AVAILABLE_REPLAYS else "calibration"
        self.stride = stride or 0
        replay_pkl_fq = os.path.join( DATA_PATH, self.replay + ".a2d_cam{:0>2}.pik".format( self.cam_num ) )
        self.frames = []
        with open( replay_pkl_fq, "rb" ) as fh:
            self.frames = pickle.load( fh )
        self.num_frames = len( self.frames )
        self.cur_frame  = -1

        ######
        # regs
        self.regs = np.zeros( (823,), dtype=np.uint8 )
        for param, (reg, _, _) in piCam.TRAIT_LOCATIONS.items():
            default = piCam.CAMERA_CAPABILITIES[ param ].default
            self.regs[ reg - 200 ] = default

        ###################
        # Configure Sockets
        self.target_addr = (self.server_ip, piCam.UDP_PORT_TX)

        # Coms Socket
        self.coms = socket.socket( socket.AF_INET, socket.SOCK_DGRAM )
        self.coms.setsockopt( socket.SOL_SOCKET, socket.SO_REUSEADDR, 1 )
        self.coms.bind( (LOCAL_IP, piCam.UDP_PORT_RX) )

        # Sync Socket
        self.sync = socket.socket( socket.AF_INET, socket.SOCK_DGRAM, socket.IPPROTO_UDP )
        try:
            self.sync.setsockopt( socket.SOL_SOCKET, socket.SO_REUSEADDR, 1 )
        except AttributeError:
            pass
        self.sync.setsockopt( socket.IPPROTO_IP, socket.IP_MULTICAST_TTL, piCam.MCAST_TTL )
        self.sync.setsockopt( socket.IPPROTO_IP, socket.IP_MULTICAST_LOOP, 1 )
        try: # Windows workaround
            self.sync.bind( (piCam.MCAST_GRP, piCam.MCAST_PORT) )
        except socket.error:
            self.sync.bind( ("", piCam.MCAST_PORT) )
        self.sync.setsockopt( socket.SOL_IP, socket.IP_MULTICAST_IF, socket.inet_aton( self.local_ip ) )
        self.sync.setsockopt( socket.SOL_IP, socket.IP_ADD_MEMBERSHIP, socket.inet_aton( piCam.MCAST_GRP ) + socket.inet_aton( self.local_ip ) )

        # Input socks for Select
        self._ins = [ self.sync, self.coms ]

        #################
        # Other Internals
        self.pacman = PacketManager( self.regs )
        self.timecode = SimpleTimecode( 25, multi=2 )
        self.capturing = False


    def packetSend( self, num_roids, dtype, pack_no, total_packs, data ):
        msg = piCam.encodePacket( self.pacman.send_count, num_roids, 0x04, dtype,
                                  self.pacman.roll_count, self.timecode.toBCDlist(),
                                  pack_no, total_packs, data )

        self.coms.sendto( msg, self.target_addr )
        # Header Debug print
        # print( hex_( msg[ 0:48 ] ) )
        self.pacman.incCount()

    def makeLog( self, msg ):
        return "*** CAMERA {} {} ***".format( self.cam_id, msg ).encode( "ascii" )

    def run( self ):
        # Run the core loop
        running = True
        print( "Dawson up to the plate, a beautiful day here at Wrigley Field..." )
        print( "Camera {} playing '{}' data as id {}, skipping {} frames.".format( self.cam_id, self.replay, self.cam_num, self.stride ) )

        while( running ):
            readable, _, _ = select.select( self._ins, [], [], 0 )
            for sock in readable:
                # Sync Msg
                if( sock == self.sync ):
                    # Drop everything and send a packet
                    data, _ = sock.recvfrom( RECV_BUFF_SZ )
                    cmd, val = struct.unpack( "4sI", data )
                    if( cmd == b"TICK" ):
                        self.cur_frame = (val *(1+self.stride)) % self.num_frames
                        self.timecode.setQSM( self.cur_frame )
                        # Debug Ticks
                        if( val % 50 == 0 ): # about 2 secs with current test harness
                            print( "ticked: {}".format( self.timecode.toString() ) )
                        if( self.capturing ):
                            centroids = encodeCentroids( self.frames[ self.cur_frame ] )
                            self.pacman.packetize( piCam.PACKET_TYPES[ "centroids" ], centroids.tobytes() )
                            break # Jump to emit packets

                    elif( cmd == b"ALL1" ):
                        self.capturing = True
                        self.pacman.packetize( piCam.PACKET_TYPES[ "textslug" ], self.makeLog( "Start Capturing" ) )

                    elif (cmd == b"ALL0"):
                        self.capturing = False
                        self.pacman.packetize( piCam.PACKET_TYPES[ "textslug" ], self.makeLog( "Stop Capturing" ) )

                    else:
                        print("Some other sync command UNIMP")

                # C&C Msg
                if( sock == self.coms ):
                    data, _ = sock.recvfrom( RECV_BUFF_SZ )

                    if( data == b"bye" ): # Quick exit
                        running = False
                        self.capturing = False
                        continue

                    # understand the data
                    if( len( data ) == 1 ):
                        # One Byte Commands are of the GET or EXE nature

                        if( data in piCam.CAMERA_COMMANDS_REV ):
                            cmd = piCam.CAMERA_COMMANDS_REV[ data ]
                            if( cmd == "start" ):
                                self.capturing = True

                            elif( cmd == "stop" ):
                                self.capturing = False

                            elif( cmd == "single" ):
                                # Send a single image
                                pass

                            elif( cmd == "stream" ):
                                # Stream Images
                                pass

                            # report
                            self.pacman.packetize( piCam.PACKET_TYPES[ "textslug" ], self.makeLog( cmd ) )

                        elif( data in piCam.CAMERA_REQUESTS_REV ):
                            req = piCam.CAMERA_REQUESTS_REV[ data ]
                            if( req == "regslo" ):
                                self.pacman.packetize( piCam.PACKET_TYPES[ "regslo" ], self.regs[:56].tobytes() )

                            elif( req == "regshi" ):
                                self.pacman.packetize( piCam.PACKET_TYPES[ "regshi" ], self.regs.tobytes() )

                            elif( req == "version" ):
                                self.pacman.packetize( piCam.PACKET_TYPES[ "version" ], b"DEADBEEEF" )

                            elif( req == "hello" ):
                                # literally just send an h with no header!
                                self.coms.sendto( b"h", self.target_addr )

                    else:
                        if( data[0] == piCam.CHAR_COMMAND_VAL ):
                            # low reg command
                            _, reg, val = struct.unpack( ">BBB", data )
                            self.regs[ reg-200 ] = val

                            # report
                            msg = self.makeLog( "Register {} Set to {}".format( reg, val ) )
                            self.pacman.packetize( piCam.PACKET_TYPES[ "textslug" ], msg )

                        elif( data[0] == piCam.SHORT_COMMAND_VAL ):
                            # High Reg Command
                            _, reg, msb, lsb = struct.unpack( ">BHBB", data )
                            _, _, val = struct.unpack( ">BHh", data )

                            self.regs[ reg-200 ] = msb
                            self.regs[ reg-199 ] = lsb # or reg + 1 :D

                            # report
                            msg = self.makeLog( "Register {} Set to {}".format( reg, val ) )
                            self.pacman.packetize( piCam.PACKET_TYPES[ "textslug" ], msg )
            # for sock in readables

            # emit packets in queue
            if( not self.pacman.q_out.empty() ):
                _, packet = self.pacman.q_out.get()
                self.packetSend( *packet )

        print( "Sent {} Packets during the experiment".format( self.pacman.send_count ) )
        print( "What do you think, Sirs?" )



if( __name__ == "__main__"):
    # Defau;ts
    SERVER_IP = "192.168.0.20"
    REPLAY = "calibration"
    ips =  piComunicate.listIPs()
    LOCAL_IP = ips[0] if len(ips)>0 else "192.168.0.32"

    parser = argparse.ArgumentParser()
    parser.add_argument( "-s", "--server", action="store", dest="server_ip",  default=SERVER_IP, help="Server's ip" )
    parser.add_argument( "-l", "--local",  action="store", dest="local_ip",   default=LOCAL_IP,  help="Host IP (on same subnet as camera" )
    parser.add_argument( "-r", "--replay", action="store", dest="replay",     default=REPLAY,    help="Example Data to replay" )
    parser.add_argument( "-i", "--camid",  action="store", dest="id_overide", default=None,      help="Camera Id to simulate (Default: modulus of ip/24)" )
    parser.add_argument( "-k", "--skip",   action="store", dest="stride",     default=0,         help="Number of frames to skip when replaying. For 100fps file, skip 3 to get realtime at 25fps Sync.", type=int )

    args = parser.parse_args()


    cam = CamSim( args.local_ip, args.server_ip, replay_data=args.replay, id_overide=args.id_overide, stride=args.stride )
    cam.run()
