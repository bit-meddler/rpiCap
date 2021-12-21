/*
 
 Copyright (C) 2016~Present The Gimli Project
 This file is part of rpiCap <https://github.com/bit-meddler/rpiCap>.

 rpiCap is free software: you can redistribute it and/or modify
 it under the terms of the GNU General Public License as published by
 the Free Software Foundation, either version 3 of the License, or
 (at your option) any later version.

 rpiCap is distributed in the hope that it will be useful,
 but WITHOUT ANY WARRANTY; without even the implied warranty of
 MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 GNU General Public License for more details.

 You should have received a copy of the GNU General Public License
 along with rpiCap.  If not, see <http://www.gnu.org/licenses/>.

*/

#pragma once

#include <string>

namespace CamConsts {

// Socket and comms
const std::string SERVER_IP      = "192.168.0.20" ;
const int         UDP_PORT_TX    = 1234 ; // Server's RX
const int         UDP_PORT_RX    = 1235 ; // Server's TX
const int         SOCKET_TIMEOUT =   10 ; // Select Timeout (secs)
const int         RECV_BUFF_SZ   =  256 ; // should only receve 1, 3, 5, or 7 Byte messages

// can these be defines ?
// Command & Control
const char CMD_START   = 0x6E ; // 'n'
const char CMD_STOP    = 0x20 ; // ' '
const char CMD_SINGLE  = 0x70 ; // 'p'
const char CMD_STREAM  = 0x73 ; // 's'
const char REQ_REGSLO  = 0x72 ; // 'r'
const char REQ_REGSHI  = 0x52 ; // 'R'
const char REQ_VERSION = 0x3F ; // '?'
const char REQ_HELLO   = 0x68 ; // 'h'
const char SET_BYTE    = 0x2A ; // '*'
const char SET_SHORT   = 0x7E ; // '~'
const char SET_WORD    = 0x21 ; // '!'

// Packetization
const char PACKET_ROIDS = 0x00 ;
const char PACKET_IMAGE = 0x0A ;
const char PACKET_REGLO = 0x0C ;
const char PACKET_REGHI = 0x0E ;
const char PACKET_TEXT  = 0x10 ;
const char PACKET_VERS  = 0x11 ;

// Centroid Pack format
const char ROIDS_8BYTE  = 0x00 ;
const char ROIDS_10BYTE = 0x04 ;

// Queue Priority
const int  PRI_IMMEDIATE =  1 ;
const int  PRI_NORMAL    = 10 ;
const int  PRI_IMAGES    = 50 ;

} // ns CamConsts

