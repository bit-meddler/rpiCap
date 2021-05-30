# add Gimli into the path
import os, sys
_git_root_ = os.path.dirname( os.path.dirname( os.path.dirname( os.path.realpath(__file__) ) ) )
sys.path.append( os.path.join( _git_root_, "Gimli", "Python" ) )
DATA_PATH = os.path.join( _git_root_, "rpiCap", "exampleData" )


import numpy as np # It's started....
from numpy.lib import recfunctions as rfn

from queue import PriorityQueue
import select
import socket
import struct
import threading
import time

from Comms import piCam, piComunicate

# Network setup ----------------------------------------------------------------
LOCAL_IP       = piComunicate.listIPs()[0] # Default realworld IP
SERVER_IP      = "192.168.0.20"
SOCKET_TIMEOUT = 10
RECV_BUFF_SZ   = 10240 # bigger than a Jumbo Frame

TARGET_ADDR = ( SERVER_IP, piCam.UDP_PORT_TX )

coms_socket = socket.socket( socket.AF_INET, socket.SOCK_DGRAM )
coms_socket.setsockopt( socket.SOL_SOCKET, socket.SO_REUSEADDR, 1 )
coms_socket.bind( (LOCAL_IP, piCam.UDP_PORT_RX) )

CAMERA_ID = SERVER_IP.split(".")[-1]

SELECT_INPUTS = [coms_socket,]

# utils ------------------------------------------------------------------------

def hex_( data ):
    return ':'.join( format(x, '02x') for x in data )

def updateTC():
    global time_stamp
    now = time.time()
    ms = now - int( now )
    frames = int( ms / FRAME_PERIOD )
    t = time.localtime( now )
    time_stamp = [ t.tm_hour, t.tm_min, t.tm_sec, frames ]
    
# Timing system to periodically emit Centroids ---------------------------------
class Metronome( threading.Thread ):
    
    def __init__( self, the_flag, delay ):
        super( Metronome, self ).__init__()
        self._flag = the_flag
        self._delay = delay
        self.running = threading.Event()
        self.running.set()
        
    def run( self ):
        while self.running.isSet():
            time.sleep( self._delay )
            self._flag.set()

    def stop( self ):
        self.running.clear()
        
CAMERA_RESOLUTION = (1280,720)
CENTROID_DATA_SIZE = 8 # HBHBBB

def generateCentroids( num ):
    Xs = np.random.random_sample( num )
    Xs *= CAMERA_RESOLUTION[0]
    
    Ys = np.random.random_sample( num )
    Ys *= CAMERA_RESOLUTION[1]

    Rs = np.random.random_sample( num )
    Rs *= 6.5

    Xs, Xf = np.divmod( Xs, 1. )
    Ys, Yf = np.divmod( Ys, 1. )
    Rs, Rf = np.divmod( Rs, 1. )

    Xs = Xs.astype( np.uint16 )
    Ys = Ys.astype( np.uint16 )
    Rs = Rs.astype( np.uint8  )
    
    Xf *= 256 # 8-Bit Fraction
    Yf *= 256
    Rf *= 128 # 4-Bit Fraction

    Xf = Xf.astype( np.uint8 )
    Yf = Yf.astype( np.uint8 )
    Rf = Rf.astype( np.uint8 )

    Rf = np.left_shift( Rf, 4 )
    
    centroids = rfn.merge_arrays( (Xs,Xf,Ys,Yf,Rs,Rf) )

    return centroids
    

have_roids = threading.Event()
have_roids.clear()

TEST_PERIOD = 3 # 3 secs

timer = None

# camera state -----------------------------------------------------------------
myRegs = np.zeros( (823,), dtype=np.uint8 )
REG_DET_CAP, _, _ = piCam.TRAIT_LOCATIONS[ "numdets" ]
REG_DET_CAP -= 200 # Only time I've ever changed a "Const", whaduya know
time_stamp = [ 0, 0, 0, 0 ]
FPS = 60
FRAME_PERIOD = 1. / FPS

# Packet Managment -------------------------------------------------------------
roll_count = 0
send_count = 0
q_out = PriorityQueue()

# Priorities
PRIORITY_IMMEDIATE =  1
PRIORITY_NORMAL    = 10
PRIORITY_IMAGES    = 50

def packetize( dtype, data ):
    
    global q_out, myRegs

    if( dtype > piCam.PACKET_TYPES[ "imagedata" ] ):
        q_out.put(
            ( PRIORITY_NORMAL, (0, dtype, 1, 1, data) )
        )
    
    elif( dtype == piCam.PACKET_TYPES[ "centroids" ] ):
        # Simulated centroids, data is a ndarray
        
        max_dets = myRegs[ REG_DET_CAP ]
        num_roids = len( data ) // CENTROID_DATA_SIZE
        
        num_packs, leftover_roids = divmod( num_roids, max_dets )
        if( leftover_roids > 0 ):
            num_packs += 1

        packet_no = 1
        slice_sz = max_dets * CENTROID_DATA_SIZE
        total_sz = num_roids * CENTROID_DATA_SIZE
        for i in range( 0, total_sz, slice_sz ):
            q_out.put( ( PRIORITY_IMMEDIATE+i, (num_roids, dtype, packet_no, num_packs, data[ i:i+slice_sz ]) ) )
            packet_no += 1
            
    elif( dtype == piCam.PACKET_TYPES[ "imagedata" ] ):
        # Same sort of logic as above, but starting from "PRIORITY_IMAGES"
        pass
    
def packetSend( num_roids, dtype, pack_no, total_packs, data ):

    global roll_count, send_count, time_stamp

    msg = piCam.encodePacket( send_count, num_roids, 0x04, dtype, roll_count, time_stamp, pack_no, total_packs, data )

    sock.sendto( msg, TARGET_ADDR )
    
    print( hex_( msg [0:48] ) )

    send_count += 1
    roll_count += 1
    roll_count &= 0x00ff
    
def makeLog( msg ):
    return "*** CAMERA {} {} ***".format( CAMERA_ID, msg ).encode( "ascii" )



# Initalize the camera
for param, (reg,_,_) in piCam.TRAIT_LOCATIONS.items():
    default = piCam.CAMERA_CAPABILITIES[ param ].default
    myRegs[ reg-200 ] = default
updateTC()
    
# Run the core loop
running = True
print( "Dawson up to the plate, a beautiful day here at Wrigley Field..." )
while( running ):
    readable, _, _ = select.select( SELECT_INPUTS, [], [], 0 )
    for sock in readable:
        data, address = sock.recvfrom( RECV_BUFF_SZ )

        #print( "Message from {}:'{}'".format( address, data ) )

        if( data == b"bye" ): # Quick exit
            running = False
            continue
        
        # understand the data
        sz = len( data )
        if( sz == 1 ):
            if( data in piCam.CAMERA_COMMANDS_REV ):
                cmd = piCam.CAMERA_COMMANDS_REV[ data ]
                if( cmd == "start" ):
                    timer = Metronome( have_roids, TEST_PERIOD )
                    timer.start()
                    
                elif( cmd == "stop" ):
                    if( timer is not None ):
                        timer.stop()
                        timer = None
                        
                elif( cmd == "single" ):
                    # Send a single image
                    pass
                
                elif( cmd == "stream" ):
                    # Stream Images
                    pass
                
                # report
                packetize( piCam.PACKET_TYPES[ "textslug" ], makeLog( cmd ) )
                
            elif( data in piCam.CAMERA_REQUESTS_REV ):
                req = piCam.CAMERA_REQUESTS_REV[ data ]
                if( req == "regslo" ):
                    packetize( piCam.PACKET_TYPES[ "regslo" ], myRegs[:56].tobytes() )
                    
                elif( req == "regshi" ):
                    packetize( piCam.PACKET_TYPES[ "regshi" ], myRegs.tobytes() )
                    
                elif( req == "version" ):
                    packetize( piCam.PACKET_TYPES[ "version" ], b"DEADBEEEF" )
                    
                elif( req == "hello" ):
                    # litterally just send an h with no header!
                    sock.sendto( b"h", TARGET_ADDR )

        else:
            if( data[0] == piCam.CHAR_COMMAND_VAL ):
                # low reg command
                _, reg, val = struct.unpack( ">BBB", data )
                myRegs[ reg-200 ] = val
                
                # report
                msg = makeLog( "Register {} Set to {}".format( reg, val ) )
                packetize( piCam.PACKET_TYPES[ "textslug" ], msg )
                
            elif( data[0] == piCam.SHORT_COMMAND_VAL ):
                # High Reg Command
                _, reg, msb, lsb = struct.unpack( ">BHBB", data )
                _, _, val = struct.unpack( ">BHh", data )
                
                myRegs[ reg-200 ] = msb
                myRegs[ reg-199 ] = lsb # or reg + 1 :D
                
                # report
                msg = makeLog( "Register {} Set to {}".format( reg, val ) )
                packetize( piCam.PACKET_TYPES[ "textslug" ], msg )
            
    if( have_roids.isSet() ):
        # Connected components, Hu moments, Quantize, and a Partridge in a Pear Tree!
        # Generate some random Dummy Centroid Data
        updateTC()
        num = np.random.randint( 5, 36, size=1 )
        centroids = generateCentroids( num )
        packetize( piCam.PACKET_TYPES[ "centroids" ], centroids.tobytes() )
        have_roids.clear()
        
    # emit packets in queue
    if( not q_out.empty() ):
        _, packet = q_out.get()
        packetSend( *packet )
        
if( timer is not None ):
    timer.stop()

print( "Sent {} Packets this session".format( send_count ) )
print( "What do you think, Sirs?" )
