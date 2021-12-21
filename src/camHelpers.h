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

#include <iostream>
#include <string>

#include "camTypes.h"

// A big enough error to abort the program
void bail( const std::string msg ) ;

// An errror, but just one to report / log
void fail( const std::string msg ) ;

// Just a report or log, not error
void wail( const std::string msg ) ;

// helps debug network packets
void hexdump( const uint8_t* data, const size_t size ) ;

// dump the "QPacket" strict that lives in the transmittion Queue
void packetDump( CamTypes::QPacket const& p ) ;

