# 
# Copyright (C) 2016~2021 The Gimli Project
# This file is part of rpiCap <https://github.com/bit-meddler/rpiCap>.
#
# rpiCap is free software: you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation, either version 3 of the License, or
# (at your option) any later version.
#
# rpiCap is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.
#
# You should have received a copy of the GNU General Public License
# along with rpiCap.  If not, see <http://www.gnu.org/licenses/>.
#

import socket

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

while( running ):

    data, address = sock.recvfrom( RECV_BUFF_SZ )

    print( "Message from {}:'{}'".format( address, data ) )
   
    # Back at ya, Pal!
    msg = "Just got '{}'".format( data )
    enc = msg.encode( "ascii" )
    sock.sendto( enc , address )
    
    if( data == b"bye" ):
        running = False
        
print( "What do you think, Sirs?" )