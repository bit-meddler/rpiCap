import argparse
import socket
import struct
import time

MCAST_GRP  = "224.10.6.10"
MCAST_PORT = 5678
MCAST_TTL  = 6

HOST_IP = "192.168.0.20"
BUF_SZ = 1024

def main( fps=None ):
    # Timing
    target_fps = fps or 25
    period = 1. / float( target_fps )
    count = 0
    
    # Socket
    sock = socket.socket( socket.AF_INET, socket.SOCK_DGRAM, socket.IPPROTO_UDP )
    sock.setsockopt( socket.SOL_SOCKET, socket.SO_REUSEADDR, 1 )
    sock.setsockopt( socket.IPPROTO_IP, socket.IP_MULTICAST_TTL, MCAST_TTL ) 

    # Prep header
    tick = bytes( "TICK", "utf-8" )

    # Start Cameras
    sock.sendto( struct.pack( "4sI", bytes( "ALL1", "utf-8" ), 0 ), (MCAST_GRP, MCAST_PORT) )

    # Main loop
    while True:
        msg = struct.pack( "4sI", tick, count )
        sock.sendto( msg, (MCAST_GRP, MCAST_PORT) )
        if( (count % target_fps) == 0 ):
            print( "sent TICK {}".format( count ) )
        count += 1
        time.sleep( period ) # Ok up to nearly 100fps in pure Python!

if( __name__ == "__main__" ):
    parser = argparse.ArgumentParser()
    parser.add_argument( "-r", "--rate", action="store", dest="fps", default=25, help="Frame rate to tick at. (Default: 25)", type=int )

    args = parser.parse_args()
    main( fps=args.fps )
