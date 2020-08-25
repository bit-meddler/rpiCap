import socket
import struct
import time

MCAST_GRP  = "224.10.6.10"
MCAST_PORT = 5678
MCAST_TTL  = 6

HOST_IP = "192.168.0.20"
BUF_SZ = 1024

def main():
    sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM, socket.IPPROTO_UDP)
    try:
        sock.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
    except AttributeError:
        pass
    sock.setsockopt(socket.IPPROTO_IP, socket.IP_MULTICAST_TTL, 32) 
    sock.setsockopt(socket.IPPROTO_IP, socket.IP_MULTICAST_LOOP, 1)

    
    # Windows workaround
    try:
        sock.bind( (MCAST_GRP, MCAST_PORT) )
    except socket.error:
        sock.bind( ( "", MCAST_PORT) )
        
    host = socket.gethostbyname(socket.gethostname())
    print( host )
    sock.setsockopt(socket.SOL_IP, socket.IP_MULTICAST_IF, socket.inet_aton(host))
    sock.setsockopt(socket.SOL_IP, socket.IP_ADD_MEMBERSHIP, 
                   socket.inet_aton(MCAST_GRP) + socket.inet_aton(host))

    while True:
        try:
            data, source = sock.recvfrom( BUF_SZ )
            print( "{} sends '{}'".format( source, data ) )
        except e:
            print( "Expection" )



if( __name__ == "__main__" ):
    main()
