import socket
import struct
import time

MCAST_GRP  = "224.10.6.10"
MCAST_PORT = 5678
MCAST_TTL  = 6

HOST_IP = "192.168.0.20"
BUF_SZ = 1024

def main():
    sock = socket.socket( socket.AF_INET, socket.SOCK_DGRAM, socket.IPPROTO_UDP )

    sock.setsockopt( socket.SOL_SOCKET, socket.SO_REUSEADDR, 1 )
    #sock.setsockopt( socket.SOL_SOCKET, socket.SO_REUSEPORT, 1 )
    
    sock.setsockopt( socket.IPPROTO_IP, socket.IP_MULTICAST_TTL, MCAST_TTL ) 
    #sock.setsockopt( socket.IPPROTO_IP, socket.IP_MULTICAST_LOOP, 1 )

    #sock.bind( (MCAST_GRP, MCAST_PORT) )

    tick = bytes( "TICK", "utf-8" )
    
    count = 0
    while True:
        msg = struct.pack( "4sI", tick, count )
        sock.sendto( msg, (MCAST_GRP, MCAST_PORT) )
        print( "sent '{}".format( msg ) )
        count += 1
        time.sleep( 2 )
        
if( __name__ == "__main__" ):
    main()
