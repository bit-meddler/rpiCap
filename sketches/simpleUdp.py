
import socket
import struct
import time

SERVER_IP      = "192.168.0.34"
UDP_PORT_TX    = 1234
UDP_PORT_RX    = 1235

sock = socket.socket( socket.AF_INET, socket.SOCK_DGRAM )
sock.setsockopt( socket.SOL_SOCKET, socket.SO_REUSEADDR, 1 )
sock.bind( ("192.168.0.20", UDP_PORT_RX) )


# send simple commands
for msg in [ "h", "r", "R", "n", " ", "h", "h", "h" ]:
    enc = msg.encode( "ascii" )
    sock.sendto( enc, (SERVER_IP, UDP_PORT_TX) )
    time.sleep( .1 )
    a = sock.recv( 1024 )
    print( a )

# send some low reg settings
for reg, val in [ [200, 50], [255, 187] ] :
    payload = struct.pack( ">BB", reg, val )
    cmd_str = b"*"
    enc = cmd_str + payload
    sock.sendto( enc, (SERVER_IP, UDP_PORT_TX) )
    time.sleep( .1 )
    a = sock.recv( 1024 )
    print( a )
# send some hi reg settings
for reg, val in [ [300, 350], [214, -666] ] :
    if( val < 0 ):
        payload = struct.pack( ">Hh", reg, val )
    else:
        payload = struct.pack( ">HH", reg, val )
        
    cmd_str = b"~"
    enc = cmd_str + payload
    sock.sendto( enc, (SERVER_IP, UDP_PORT_TX) )
    time.sleep( 1 )
    a = sock.recv( 1024 )
    print( a )
    
sock.close()
