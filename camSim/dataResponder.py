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
    Rs = Rs.astype( np.uint8 )

    Xf *= 256  # 8-Bit Fraction
    Yf *= 256
    Rf *= 128  # 4-Bit Fraction

    Xf = Xf.astype( np.uint8 )
    Yf = Yf.astype( np.uint8 )
    Rf = Rf.astype( np.uint8 )

    Rf = np.left_shift( Rf, 4 )

    return rfn.merge_arrays( (Xs, Xf, Ys, Yf, Rs, Rf) )


class PacketManager( object ):
    # Priorities
    PRIORITY_IMMEDIATE = 1
    PRIORITY_NORMAL = 10
    PRIORITY_IMAGES = 50

    def __init__( self, max_roids=None ):
        self.roll_count = 0
        self.send_count = 0
        self.q_out = PriorityQueue()
        self.max_roids = max_roids or 10

    def packetize( self, dtype, data ):
        if( dtype > piCam.PACKET_TYPES[ "imagedata" ] ):
            self.q_out.put( (PRIORITY_NORMAL, (0, dtype, 1, 1, data)) )

        elif (dtype == piCam.PACKET_TYPES[ "centroids" ]):
            # Simulated centroids, data is a ndarray
            num_roids = len( data ) // CENTROID_DATA_SIZE

            num_packs, leftover_roids = divmod( num_roids, self.max_roids )
            if (leftover_roids > 0):
                num_packs += 1

            packet_no = 1
            slice_sz = self.max_roids * CENTROID_DATA_SIZE
            total_sz = num_roids * CENTROID_DATA_SIZE
            for i in range( 0, total_sz, slice_sz ):
                self.q_out.put(
                    (PRIORITY_IMMEDIATE + i, (num_roids, dtype, packet_no, num_packs, data[ i:i + slice_sz ]))
                )
                packet_no += 1

        elif (dtype == piCam.PACKET_TYPES[ "imagedata" ]):
            # Same sort of logic as above, but starting from "PRIORITY_IMAGES"
            pass

    def incCount( self ):
        self.send_count += 1
        self.roll_count += 1
        self.roll_count &= 0x00ff


class CamSim( object ):

    def __init__( self, local_ip, server_ip, cam_num=None, replay_data=None ):
        # basic settings
        self.local_ip = local_ip
        self.server_ip = server_ip

        self.cam_id = self.local_ip.splitr(".",1)[1]

        self.cam_num = cam_num or int( self.cam_id ) % 10

        ########
        # Replay
        self.replay = replay_data if replay_data in AVAILABLE_REPLAYS else "calibration"
        replay_pkl_fq = os.path.join( DATA_PATH, self.replay + "_cam{:0>2}.pik".format( self.cam_num ) )
        self.frames = []
        with open( replay_pkl_fq, "wb" ) as fh:
            self.frames = pickle.load( fh )
        self.num_frames = len( self.frames )
        self.cur_frame = -1

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
            self.sync.bind( (MCAST_GRP, MCAST_PORT) )
        except socket.error:
            self.sync.bind( ("", MCAST_PORT) )
        self.sync.setsockopt( socket.SOL_IP, socket.IP_MULTICAST_IF, socket.inet_aton( self.local_ip ) )
        self.sync.setsockopt( socket.SOL_IP, socket.IP_ADD_MEMBERSHIP, socket.inet_aton( MCAST_GRP ) + socket.inet_aton( .local_ip ) )

        self._ins = [ self.sync, self.coms ]

        #################
        # Other Internals
        # Packetization
        self.REG_DET_CAP, _, _ = piCam.TRAIT_LOCATIONS[ "numdets" ]
        self.IDX_DET_CAP = self.REG_DET_CAP - 200
        self.pacman = PacketManager()

        # Timecode
        self.timecode = SimpleTimecode()

        # camera state
        self.capturing = False


    def packetSend( self, num_roids, dtype, pack_no, total_packs, data ):
        msg = piCam.encodePacket( self.pacman.send_count, num_roids, 0x04, dtype,
                                  self.pacman.roll_count, self.timecode.toBCDlist(),
                                  pack_no, total_packs, data )

        self.coms.sendto( msg, self.target_addr )

        print( hex_( msg[ 0:48 ] ) )
        self.pacman.incCount()

    def makeLog( self, msg ):
        return "*** CAMERA {} {} ***".format( self.cam_id, msg ).encode( "ascii" )

    def run( self ):
        # Run the core loop
        running = True
        print( "Dawson up to the plate, a beautiful day here at Wrigley Field..." )
        print( "Camera {} playing {} as '{}'".format( self.cam_id, self.replay, self.cam_num ) )

        while( running ):
            readable, _, _ = select.select( self._ins, [], [], 0 )
            for sock in readable:
                # Sync Msg
                if( sock == self.sync ):
                    # Drop everything and send a packet
                    data, address = sock.recvfrom( RECV_BUFF_SZ )
                    cmd, val = struct.unpack( "s4I", data )

                    if( cmd == "TICK" ):
                        self.cur_frame = val % self.num_frames
                        if( self.capturing ):
                            centroids = encodeCentroids( self.frames[ self.cur_frame ] )
                            self.pacman.packetize( piCam.PACKET_TYPES[ "centroids" ], centroids.tobytes() )
                            break # Jump to emit packets

                    elif( cmd == "ALL1" ):
                        self.capturing = True
                        self.pacman.packetize( piCam.PACKET_TYPES[ "textslug" ], self.makeLog( "Start Capturing" ) )

                    elif (cmd == "ALL0"):
                        self.capturing = False
                        self.pacman.packetize( piCam.PACKET_TYPES[ "textslug" ], self.makeLog( "Stop Capturing" ) )

                    else:
                        print("Some other sync command UNIMP")

                # C&C Msg
                if( sock == self.coms ):
                    data, address = sock.recvfrom( RECV_BUFF_SZ )

                    if( data == b"bye" ): # Quick exit
                        running = False
                        self.capturing = False
                        continue

                    # understand the data
                    sz = len( data )
                    if( sz == 1 ):
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
                            self.pacman.packetize( piCam.PACKET_TYPES[ "textslug" ], makeLog( cmd ) )

                        elif( data in piCam.CAMERA_REQUESTS_REV ):
                            req = piCam.CAMERA_REQUESTS_REV[ data ]
                            if( req == "regslo" ):
                                self.pacman.packetize( piCam.PACKET_TYPES[ "regslo" ], self.regs[:56].tobytes() )

                            elif( req == "regshi" ):
                                self.pacman.packetize( piCam.PACKET_TYPES[ "regshi" ], self.regs.tobytes() )

                            elif( req == "version" ):
                                self.pacman.packetize( piCam.PACKET_TYPES[ "version" ], b"DEADBEEEF" )

                            elif( req == "hello" ):
                                # litterally just send an h with no header!
                                self.coms.sendto( b"h", self.target_addr )

                    else:
                        if( data[0] == piCam.CHAR_COMMAND_VAL ):
                            # low reg command
                            _, reg, val = struct.unpack( ">BBB", data )
                            if( reg == self.REG_DET_CAP ):
                                # det cap changed
                                self.pacman.max_roids = val
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
                _, packet = q_out.get()
                self.packetSend( *packet )

        print( "Sent {} Packets this session".format( send_count ) )
        print( "What do you think, Sirs?" )



if( __name__ == "__main__"):
    # Defau;ts
    SERVER_IP = "192.168.0.20"
    REPLAY = "calibration"
    ips =  piComunicate.listIPs()
    LOCAL_IP = ips[0] if len(ips)>0 else "192.168.0.32"

    parser = argparse.ArgumentParser()
    parser.add_argument( "-s", action="store", dest="server_ip", default=SERVER_IP, help="Server's ip" )
    parser.add_argument( "-l", action="store", dest="local_ip", default=LOCAL_IP, help="Host IP (on same subnet as camera" )
    parser.add_argument( "-r", action="store", dest="replay", default=REPLAY, help="Example Data to replay" )

    args = parser.parse_args()


    cam = CamSim( args.local_ip, args.server_ip, replay_data=args.replay )
    cam.run()