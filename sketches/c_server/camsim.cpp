/*
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
*/

// Networking Imports
#include <arpa/inet.h>
#include <sys/socket.h>
#include <unistd.h>

// Language
#include <iostream>
#include <string>
#include <cstring> // memset

// libs
#include "camConsts.hpp"

void fail( const std::string msg ){
    std::cerr << msg << std::endl ;
    exit( 1 ) ;
}

int main( int argc, char* args[] ){
    std::cout << "Hello World!" << std::endl ;
    
    // Setup Socket Address data
    struct sockaddr_in sock_addr ;
	socklen_t sa_size = sizeof( sock_addr ) ;
    memset( (char *) &sock_addr, 0, sa_size ) ;

    // Create and Bind the Socket
	int sock_fd ;
	if( (sock_fd=socket( AF_INET, SOCK_DGRAM, IPPROTO_UDP )) == -1 ) {
    		fail( "Socket Creation" ) ;
	}

	sock_addr.sin_family = AF_INET ;
	sock_addr.sin_port = htons( UDP_PORT_TX ) ;
	sock_addr.sin_addr.s_addr = htonl( INADDR_ANY ) ;

	if( bind( sock_fd, (struct sockaddr*) &sock_addr, sa_size ) == -1) {
		fail("bind");
	}

    // Get ready to recv
    char in_buf[ RECV_BUFF_SZ ] ;
    struct sockaddr_in sender_data ;
    bool running = 1 ;
    int recv_len = 0 ;
    int recv_cnt = 0 ;

    while( running ) {
        std::cout << "Waiting for Packet" << std::endl ;

        // receve
		if( (recv_len = recvfrom( sock_fd, in_buf, RECV_BUFF_SZ, 0, (struct sockaddr *) &sender_data, &sa_size )) == -1 ) {
			fail( "recvfrom()" ) ;
		}

        printf( "[%d] Received %d Bytes from %s:%d\n", recv_cnt, recv_len, inet_ntoa(sender_data.sin_addr), ntohs(sender_data.sin_port) ) ;
		printf( ">%s<\n", in_buf ) ;

        // reflect
        if( sendto( sock_fd, in_buf, recv_len, 0, (struct sockaddr*) &sender_data, sa_size) == -1 ) {
			fail( "sendto()" ) ;
		}

        // exit
        recv_cnt++ ;
        if( recv_cnt > 10 ){
            running = 0 ;
        }
    }

    close( sock_fd ) ;
    return 0 ;
}