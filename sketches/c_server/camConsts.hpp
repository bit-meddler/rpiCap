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

#include <string>

const std::string SERVER_IP      = "127.0.0.1" ;
const int         UDP_PORT_TX    =  1234 ; // Server's RX
const int         UDP_PORT_RX    =  1235 ; // Server's TX
const int         SOCKET_TIMEOUT =    10 ;
const int         RECV_BUFF_SZ   = 10240 ; // bigger than a Jumbo Frame

