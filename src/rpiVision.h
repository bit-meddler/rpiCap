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

#include <cmath>
#include <map>
#include <vector>

#include <arm_neon.h>


namespace vision {

/* 
 * NOTE: TopLeft BottomRight convention used in this project
 * x,y -> m,n
 * x and m are cols,
 * y and n are rows
 */

/******************************************************************************* Computer Vision Types ****************/

// Optimize type of line data ? Will this make a difference for computation?
typedef uint16_t line_t ; // test: int, uint16_t, uint32_t, uint_64_t ???

// Essential Centroid data, (x,y) position, radius, circularity score, and bounding box w/h
struct SimpleDet {
    float_t x, y, r, score ;
    uint8_t w, h ;
} ;

struct PackedCentroids8 {
    uint16_t  xd ; // Decimal part of X
    uint8_t   xf ; // Fractional part of X
    
    uint16_t  yd ; // Decimal part of Y
    uint8_t   yf ; // Fractional part of Y

    uint8_t   rd ; // Decimal part of Radius
    uint8_t   rf ; // Fractional part of Radius
} ;

struct PackedCentroids10 {
    uint16_t  xd ; // Decimal part of X
    uint8_t   xf ; // Fractional part of X
    
    uint16_t  yd ; // Decimal part of Y
    uint8_t   yf ; // Fractional part of Y

    uint8_t   rd ; // Decimal part of Radius
    uint8_t   rf ; // Fractional part of Radius

    uint8_t    w ;  // RoI Width
    uint8_t    h ;  // RoI Height
} ;

struct scanline {
    line_t x ; // first hot pixel
    line_t m ; // last hot pixel
    union {    // the line's y
        line_t y ;
        line_t n ;
    } ;

    inline void reset() {
        x = m = n = 0 ;
    }

    inline bool touches( scanline &other ) {
        // TODO: Make this a Macro?
        /*return (x >= other.x && x <= other.m) || \
               (m >= other.x && m <= other.m) || \
               (other.x >= x && other.x <= m) || \
               (other.m >= x && other.m <= m) ;
        */
        return (other.x >= x && other.x < m) || (x >= other.x && x < other.m) ;
    }
} ;

struct moment {
    float_t m_00, m_01, m_10, m_11 ; // 1st Order Moments
    float_t       m_02, m_20 ;       // 2nd Order Moments
    
    inline void reset() {
        m_00 = 0.0f ;
        m_01 = 0.0f ;
        m_10 = 0.0f ;
        m_11 = 0.0f ;
        m_02 = 0.0f ;
        m_20 = 0.0f ;
    }

    inline void merge( moment &other ) {
        m_00 += other.m_00 ;
        m_01 += other.m_01 ;
        m_10 += other.m_10 ;
        m_11 += other.m_11 ;
        m_20 += other.m_20 ;
        m_02 += other.m_02 ;
    }
} ;

// ROI Flags (should only be used in 'vision' for connected component detection
extern const line_t ROI_BIG_NUM = 65535 ;
extern const line_t ROI_INVALID = 60666 ;
extern const line_t ROI_NO_ID   = 60000 ;

// An RoI has a scanline, bounding box and Image Moments 
struct ROI {
    // Bounding Box 4
    line_t bb_x, bb_y, bb_m, bb_n ;
    // RoI ID 1
    line_t id ;
    // Last hot line 3
    scanline sl ;
    // Image Moments 6
    moment hu ;

    inline void reset() {
        bb_x = bb_y = ROI_BIG_NUM ; 
        bb_m = bb_n = 0 ;
        //sl.x = sl.m = sl.n = 0 ;
        id = ROI_NO_ID ;
    }

    inline void update() {
        bb_x = std::min( bb_x, sl.x ) ;
        bb_y = std::min( bb_y, sl.y ) ;
        bb_m = std::max( bb_m, sl.m ) ;
        bb_n = std::max( bb_n, sl.n ) ;
    }

    inline void takeScanLine( scanline &other ) {
        sl = other ;
    }

    inline void takeScanLine( ROI &other ) {
        sl = other.sl ;
    }

    inline void addMoment( moment &other ) {
        hu.merge( other ) ;
    }

    inline void merge( ROI &other ) {
        bb_x = std::min( bb_x, other.bb_x ) ;
        bb_y = std::min( bb_y, other.bb_y ) ;
        bb_m = std::max( bb_m, other.bb_m ) ;
        bb_n = std::max( bb_n, other.bb_n ) ;
        // take oldest ID
        id = std::min( id, other.id ) ;
        // merge moments
        hu.merge( other.hu ) ;
    }

    inline line_t area() {
        return (bb_m - bb_x) * (bb_n - bb_y) ;
    }
    
    // Comparitor for sorting
    bool operator < ( const ROI &other ) const {
        return id < other.id ;
    }
} ;

// Vector helpers
typedef std::vector< SimpleDet >         VecDet_t ;
typedef std::vector< PackedCentroids8 >  VecRoids8_t ;
typedef std::vector< PackedCentroids10 > VecRoids10_t ;

typedef std::vector< ROI >               VecRoi_t ;
typedef std::vector< ROI >::iterator     VecRoiIt_t ;

// Map helpers
typedef std::map< line_t, line_t >             MapRoiId_t ;
typedef std::map< line_t, line_t >::iterator   MapRoiIdIt_t ;
typedef std::map< line_t, size_t >             MapRoIdx_t ;

/******************************************************************************* Computer Vision Functions ************/

VecDet_t ConnectedComponentsImage(
    uint8_t*      &img,              // image to analyse
    const int     img_w,             // img width
    const int     img_h,             // img height
    const uint8_t threshold,         // Blob Brightness
    const size_t  vec_size           // Vctor reserve size (hint with last)
    // returns a Vector of Detections (VecDet_t)
) ;

VecRoids8_t  PackCentroids8(  const VecDet_t &centroids ) ;
VecRoids10_t PackCentroids10( const VecDet_t &centroids ) ;

} // ns vision

