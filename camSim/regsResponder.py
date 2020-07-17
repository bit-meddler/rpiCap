import numpy as np # It's started....
import socket
import struct

SERVER_IP      = "127.0.0.1" #"192.168.0.32"
UDP_PORT_TX    = 1234  # Server's RX
UDP_PORT_RX    = 1235  # Server's TX
SOCKET_TIMEOUT = 10
RECV_BUFF_SZ   = 10240 # bigger than a Jumbo Frame

sock = socket.socket( socket.AF_INET, socket.SOCK_DGRAM )
sock.setsockopt( socket.SOL_SOCKET, socket.SO_REUSEADDR, 1 )
sock.bind( (SERVER_IP, UDP_PORT_TX) )

print( "Dawson up to the plate, a beautiful day here at Wrigley Field..." )

# Listen for incoming message and just repeat them back
running = True

myRegs = np.zeros( (227,), dtype=np.uint8 )

while( running ):

    data, address = sock.recvfrom( RECV_BUFF_SZ )

    print( "Message from {}:'{}'".format( address, data ) )
   
    # understand the data
    sz = len( data )
    if( sz == 1 ):
        if( data==b"r" ):
            # low Regs
            sock.sendto( myRegs[:56].tobytes(), address )
        elif( data==b"R" ):
            # high Regs
            sock.sendto( myRegs.tobytes(), address )
        elif( data==b"h" ):
            # Hello
            sock.sendto( b"h" , address )
        else:
            sock.sendto( b"**** Received a Command ****" , address )
        
    else:
        if( sz == 3 ):
            # low reg command
            _, reg, val = struct.unpack( ">BBB", data )
            print( reg, val )
            myRegs[ reg-200 ] = val
            
        if( sz == 5 ):
            # High Reg Command
            _, reg, val1, val2 = struct.unpack( ">BHBB", data )
            myRegs[ reg-200 ] = val1
            myRegs[ reg-199 ] = val2 # or reg + 1 :D
            
    if( data == b"bye" ):
        running = False
        
print( "What do you think, Sirs?" )