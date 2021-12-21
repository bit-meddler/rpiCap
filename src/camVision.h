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


#include <algorithm>
#include <cmath>

#include <arm_neon.h>

#include "camTypes.h"

namespace vision {

VecDet_t ConnectedComponentsImage(
    uint8_t*      &img,              // image to analyse
    const int     img_w,             // img width
    const int     img_h,             // img height
    const uint8_t threshold,         // Blob Brightness
    const size_t  vec_size           // Vctor reserve size
    // returns a Vector of Detections
) ;

VecRoids8_t  PackCentroids8(  const VecDet_t &centroids ) ;
VecRoids10_t PackCentroids10( const VecDet_t &centroids ) ;

} // ns vision

