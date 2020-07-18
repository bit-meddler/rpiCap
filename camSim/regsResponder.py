# add Midget into the path
import os, sys
_git_root_ = os.path.dirname( os.path.dirname( os.path.dirname( os.path.realpath(__file__) ) ) )
sys.path.append( os.path.join( _git_root_, "midget", "Python" ) )


import numpy as np # It's started....
import socket
import struct

from Comms import piCam

SERVER_IP      = "127.0.0.1" #"192.168.0.32"
UDP_PORT_TX    = 1234  # Server's RX
UDP_PORT_RX    = 1235  # Server's TX
SOCKET_TIMEOUT = 10
RECV_BUFF_SZ   = 10240 # bigger than a Jumbo Frame

sock = socket.socket( socket.AF_INET, socket.SOCK_DGRAM )
sock.setsockopt( socket.SOL_SOCKET, socket.SO_REUSEADDR, 1 )
sock.bind( (SERVER_IP, UDP_PORT_TX) )

CAMERA_ID = SERVER_IP.split(".")[-1]

print( "Dawson up to the plate, a beautiful day here at Wrigley Field..." )

# Listen for incoming message and just repeat them back
running = True

myRegs = np.zeros( (823,), dtype=np.uint8 )

# some internals for counting packets
roll_count = 0
packet_num = 0

def hex_( data ):
    return ':'.join( format(x, '02x') for x in data )

def packetize( dtype, data, address ):
    # TODO Simulate breaking big data into several packets
    # packet_num, num_roids, compression, dtype, roll_count, payload_os
    
    global roll_count, packet_num
    
    num_roids = 0
    
    if( dtype > piCam.PACKET_TYPES[ "imagedata" ] ):
        # a dump, data is just flat bytes.
        pass
    
    elif( dtype == piCam.PACKET_TYPES[ "centroids" ] ):
        # Simulated centroids, data is a ndarray
        num_roids = len( data )
        data = data.flatten().tobytes()
        
    elif( dtype == piCam.PACKET_TYPES[ "imagedata" ] ):
        # Don't know how to do this... will be idx then length of image fragmnt
        pass
    
    header = struct.pack( ">HHBBBB", packet_num, num_roids, 0x04, dtype, roll_count, 8 )
    
    msg = header + data
    sock.sendto( msg, address )
    
    #print( hex_( msg )[:90] )

    packet_num += 1
    roll_count += 1
    roll_count &= 0x00ff


def makeLog( msg ):
    return "*** CAMERA {} {} ***".format( CAMERA_ID, msg ).encode( "ascii" )

sim_roids = False # are we simulating sending Centroids?
while( running ):

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
                sim_roids = True
                
            elif( cmd == "stop" ):
                sim_roids = False
                
            elif( cmd == "single" ):
                # Send a single image
                pass
            
            elif( cmd == "stream" ):
                # Stream Images
                pass
            # report
            packetize( piCam.PACKET_TYPES[ "textslug" ], makeLog( cmd ), address )
            
        elif( data in piCam.CAMERA_REQUESTS_REV ):
            req = piCam.CAMERA_REQUESTS_REV[ data ]
            if( req == "regslo" ):
                packetize( piCam.PACKET_TYPES[ "regslo" ], myRegs[:56].tobytes(), address )
                
            elif( req == "regshi" ):
                packetize( piCam.PACKET_TYPES[ "regshi" ], myRegs.tobytes(), address )
                
            elif( req == "version" ):
                packetize( piCam.PACKET_TYPES[ "version" ], b"DEADBEEEF", address )
                
            elif( req == "hello" ):
                packetize( piCam.PACKET_TYPES[ "textslug" ], b"h", address )

    else:
        if( data[0] == piCam.CHAR_COMMAND_VAL ):
            # low reg command
            _, reg, val = struct.unpack( ">BBB", data )
            myRegs[ reg-200 ] = val
            
            # report
            msg = makeLog( "Register {} Set to {}".format( reg, val ) )
            packetize( piCam.PACKET_TYPES[ "textslug" ], msg, address )
            
        if( data[0] == piCam.SHORT_COMMAND_VAL ):
            # High Reg Command
            _, reg, msb, lsb = struct.unpack( ">BHBB", data )
            _, _, val = struct.unpack( ">BHh", data )
            
            myRegs[ reg-200 ] = msb
            myRegs[ reg-199 ] = lsb # or reg + 1 :D
            
            # report
            msg = makeLog( "Register {} Set to {}".format( reg, val ) )
            packetize( piCam.PACKET_TYPES[ "textslug" ], msg, address )
            
    if( sim_roids ):
        # Connected components, Hu moments, Quantize, and a Partridge in a Pear Tree!
        print( "Sim Centroids" )

    
print( "What do you think, Sirs?" )
