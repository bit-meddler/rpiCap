
import socket
import time

SERVER_IP      = "192.168.0.34"
UDP_PORT_TX    = 1234
UDP_PORT_RX    = 1235

sock = socket.socket( socket.AF_INET, socket.SOCK_DGRAM )
sock.setsockopt( socket.SOL_SOCKET, socket.SO_REUSEADDR, 1 )
sock.bind( ("192.168.0.20", UDP_PORT_RX) )

for i in range( 8 ):
    msg = "Message '{}'".format( i )
    enc = msg.encode( "ascii" )
    sock.sendto( enc, (SERVER_IP, UDP_PORT_TX) )
    time.sleep( 1 )

sock.close()
