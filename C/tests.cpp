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

#include <stdio.h>
#include "vision.cpp"


void test_cc() {
    uint8_t test_img[] = \
    { //0    1    2    3    4    5    6    7    8    9   10   11      
        0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0, // 0
        0,   0,   0,   0,  60,   0,   0,   0,   0,   0,   0,   0, // 1 
        0,  60,  60,   0,   0,   0,   0,   0,  40,  60,  40,   0, // 2
        0,  60,  60,   0,   0,  60,  60,   0,  60,  70,  60,   0, // 3
        0,   0,   0,   0,   0,  60,  60,   0,  40,  60,  40,   0, // 4
        0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0, // 5
        0,   0,   0,   0,   0,   0,   0,   0,   0,   0,  60,   0, // 6
        0,   0,   0,   0,   0,   0,  60,  60,  60,  60,  60,  60, // 7
        0,   0,   0,   0,   0,   0,  60,  60,  60,   0,  60,   0, // 8
        0,   0,   0,   0,   0,   0,  60,  60,  60,   0,  60,   0, // 9
        0,  60,  60,   0,   0,   0,   0,   0,   0,   0,  60,   0, // 10
        0,  60,  60,   0,   0,   0,   0,   0,   0,   0,   0,   0  // 11
    } ; // 12x12

    for( int i = 0; i < 12; i++ ) {
        for( int j = 0; j < 12; j++ )
            printf( "%3d,", test_img[ (i * 12) + j ] ) ;
        printf( "\n" ) ;
    }
    
    uint8_t*            image_data = &test_img[0] ;
    uint8_t             threshold = 50 ;
    vision::simpleROI   R ;
    vision::RoiVec_t    regions ;
    vision::RoiIdSet_t  gutters ;

    regions = vision::ConnectedComponents( image_data, 12, 12, 0, 144, threshold, 5, gutters ) ;
    
    for( size_t i = 0; i < regions.size(); i++ ) {
        R = regions[i] ;
        printf( "{%d} %d: x%d, y%d, m%d, n%d\n", i, R.id, R.bb_x, R.bb_y, R.bb_m, R.bb_n ) ;
    }
}


int main() {
    test_cc() ;
}

