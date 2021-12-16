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
#pragma once

#include <typeinfo>
#include <iostream>

// Slow but handy for debuging
void hexdump( const uint8_t* data, const size_t size ) {
    size_t idx = 0 ;
    size_t tmp = idx ;
    size_t i ;
    while( idx < size ) {
        tmp = idx ;
        for( i=0; i<16 && idx<size; i++ ) {
            printf( "%02x ", data[ idx ] ) ;
            idx++ ;
            if( i==7 || i==15 ){
                printf( " " ) ;
            }
        }
        for( ; i<16; i++ ) {
            printf( "   " ) ;
            if( i==7 || i==15 ){
                printf( " " ) ;
            }
        }
        idx = tmp ;
        for( i=0; i<16 && idx<size; i++ ) {
            printf( "%c", data[ idx ] ) ;
            idx++ ;
            if( i==7 ){
                printf( " " ) ;
            }
        }
        printf( "\n" ) ;
    }
}

