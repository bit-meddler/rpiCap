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

#ifndef __VISION__
#define __VISION__

// Testing different skipping methods __TEST_OPTIMIZED__ / __TEST_NORMAL__ / __TEST_BAD__
#define __TEST_OPTIMIZED__

#include <cmath>
#include <vector>
#include <set>
#include <map>
#include <cstring>
#include <algorithm>

#ifdef __TEST_OPTIMIZED__
#include <arm_neon.h>
#endif

namespace vision {

struct simpleDet {
    float_t x, y, r, score ;
    uint8_t w, h ;
} ;
typedef std::vector< simpleDet > DetVec_t ;

struct packedCentroids8 {
    uint16_t  xd ; // Decimal part of X
    uint8_t   xf ; // Fractional part of X
    
    uint16_t  yd ; // Decimal part of Y
    uint8_t   yf ; // Fractional part of Y

    uint8_t   rd ; // Decimal part of Radius
    uint8_t   rf ; // Fractional part of Radius
} ;
typedef std::vector< packedCentroids8 > Roid8Vec_t ;

struct packedCentroids10 {
    uint16_t  xd ; // Decimal part of X
    uint8_t   xf ; // Fractional part of X
    
    uint16_t  yd ; // Decimal part of Y
    uint8_t   yf ; // Fractional part of Y

    uint8_t   rd ; // Decimal part of Radius
    uint8_t   rf ; // Fractional part of Radius

    uint8_t    w ;  // RoI Width
    uint8_t    h ;  // RoI Height
} ;
typedef std::vector< packedCentroids10 > Roid10Vec_t ;


/*
 * x,y -> m,n
 * x and m are cols,
 * y and n are rows
 */
 
// Optimize type of line data ? Will this make a difference for computation?
typedef uint16_t line_t; // test: int, uint16_t, uint32_t,

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
        m_00 = m_01 = m_10 = m_11 = m_02 = m_20 = 0.0f ;
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

extern const line_t ROI_BIG_NUM = 65535 ;
extern const line_t ROI_INVALID = 60666 ;
extern const line_t ROI_NO_ID   = 60000 ;

// Simple region is only a bounding box and scanline data
struct simpleROI {
    // Bounding Box 4
    line_t bb_x, bb_y, bb_m, bb_n ;
    // RoI ID 1
    line_t id ;
    // Last hot line 3
    scanline sl ;

    inline void reset() {
        bb_x = bb_y = ROI_BIG_NUM ; 
        bb_m = bb_n = 0 ;
        sl.x = sl.m = sl.n = 0 ;
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

    inline void takeScanLine( simpleROI &other ) {
        sl = other.sl ;
    }

    inline void merge( simpleROI &other ) {
        bb_x = std::min( bb_x, other.bb_x ) ;
        bb_y = std::min( bb_y, other.bb_y ) ;
        bb_m = std::max( bb_m, other.bb_m ) ;
        bb_n = std::max( bb_n, other.bb_n ) ;
        // take oldest ID
        id = std::min( id, other.id ) ;
    }

    inline line_t area() {
        return (bb_m - bb_x) * (bb_n - bb_y) ;
    }
    
    // Comparitor for sorting
    bool operator < ( const simpleROI &other ) const {
        return id < other.id ;
    }
} ;

// type Helpers for simpleROIs
typedef std::vector< simpleROI >               sRoiVec_t ;
typedef std::vector< simpleROI >::iterator     sRoiVecIt_t ;


// a "full" RoI has the Image Moments as well
struct ROI {
    // Bounding Box 4
    line_t bb_x, bb_y, bb_m, bb_n ;
    // RoI ID 1
    line_t id ;
    // Last hot line 3
    scanline sl ;
    // Image Moments
    moment hu ;

    inline void reset() {
        bb_x = bb_y = ROI_BIG_NUM ; 
        bb_m = bb_n = 0 ;
        sl.x = sl.m = sl.n = 0 ;
        id = ROI_NO_ID ;
        hu.reset() ;
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

// type Helpers for ROIs
typedef std::vector< ROI >               RoiVec_t ;
typedef std::vector< ROI >::iterator     RoiVecIt_t ;

// type helpers for Maps / Sets of Region IDs (Region type agnostic)
typedef std::set< line_t >                     RoiIdSet_t ;
typedef std::map< line_t, line_t >             RoiIdMap_t ;
typedef std::map< line_t, line_t >::iterator   RoiIdIt_t ;


// Do Connected Components on a slice of a flatened Image
// The Idea is to allow parallel processing of the slices, and then any regions in "gutters" (top or
// bottom of the slice) need to be joined to gutters from the subsequent slice.
// Turns out the pi3b can do this >1ms on one core.
// ~~~~~~~~~~~~~~~~~~~~~~~~~ D E P R I C A T E D ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
sRoiVec_t ConnectedComponentsSlice(
    uint8_t*      &img,              // image to analyse
    const int     img_w,             // img width
    const int     img_h,             // img height
    const int     img_in,            // Start idx of the 'slice' to analyse (inclusive)
    const int     img_out,           // End idx of the 'slice' to analyse (exclusive)
    const uint8_t threshold,         // Blob Brightness
    const size_t  vec_size,          // Vctor reserve size
    // returns
    RoiIdSet_t   &gutterballs       // Region ids that touch the first or last row, might merge
    // returns a Vector of ROIs
){
    // Image stuff
    int         img_idx   = 0 ;      // index into img (remember it's flat)
    int         row_start = 0 ;      // index of current row start
    int         row_end   = 0 ;      // index of current row end
    line_t      tmp_x     = 0 ;      // temp x,y of a bright pixel
    line_t      tmp_y     = 0 ;      // Shuttup compiler, I know it will get initialized
    line_t      last_y    = 0 ;      // last y we encountered

    // Regions & Region list stuff
    // TODO: should be 2 vectors, val and ref, and I should manipulate the refs
    sRoiVec_t region_list ;           // the list of ROIs we have found
    size_t    reg_idx = 0 ;           // index of 1st reg we might examine
    size_t    reg_len = 0 ;           // length of region list
    line_t    reg_id  = 0 ;           // Rolling auto ID of Regions
    
    // Region housekeeping to find join candidates
    size_t   reg_start  = 0 ;        // first region that might touch current scanline
    size_t   reg_scan   = 0 ;        // used to split region_list
    line_t   previous_y = 0 ;        // used to test of touchable regions
    region_list.reserve( vec_size ) ;

    // Line detection
    scanline    current_line ;       // temp scanline

    // Region joining
    scanline    master_line ;        // master region scanline - for joining
    line_t      master_line_id ;     // Region ID of the masterline
    simpleROI   tmp_reg ;            // temp region for comparison and merging
    simpleROI*  merge_target ;       // temp space for merging regions
    RoiIdMap_t  merge_map ;          // Map of RoI.ids that need to merge

    // Ejection control
    bool        ended = 0 ;          // have we finished looking at this slice

    // Gutters
    const line_t gut_y = img_in  / img_w ; // top of image slice
    const line_t gut_n = (img_out-1) / img_w ; // bottom of image slice

    // Guards
    const uint8_t* img_in_ptr  = &img[ img_in ]  ; // pointer to the 1st pxiel in the slice
    const uint8_t* img_out_ptr = &img[ img_out ] ; // pointer to the terminus pxiel in the slice

    // Setup for scanning
    img_idx = img_in;
    row_end = ((img_in % img_w) * img_w) - 1 ; // first run, force scanline housekeeping
    uint8_t* img_ptr = &img[img_in] ; // img pointer that scans the image

    // initalize structs
    current_line.reset() ;
    master_line.reset() ;
    tmp_reg.reset() ;

    // Initally, there can't be a masterline
    master_line_id = ROI_INVALID ;

    while( !ended ) {
        // Skip dark pixels
        
#ifdef __TEST_OPTIMIZED__
        // NEON
        uint8x16_t threshold_vector = vdupq_n_u8( threshold ) ;
        uint8x16_t mask_vector ;
        uint8x16_t img_slice   ;
        uint8x8_t  merge_bits  ;
        uint8_t    any_above = 0 ;
        
        while( !any_above ) {
            // load Img slice
            img_slice = vld1q_u8( img_ptr ) ;
            img_ptr += 16 ;
            
            //memcpy( &img_slice, img_ptr, 16 ) ; // this can't be fast :(
            
            // test for greatness - this should optimize into NEON, right?
            mask_vector = img_slice >= threshold_vector ;

            // https://stackoverflow.com/questions/15389539/fastest-way-to-test-a-128-bit-neon-register-for-a-value-of-0-using-intrinsics
            merge_bits = vorr_u8( vget_low_u8( mask_vector ), vget_high_u8( mask_vector ) ) ;
            any_above  = vget_lane_u8( vpmax_u8( merge_bits, merge_bits ), 0 ) ;

            // need a guard against going off the end
            if( img_ptr >= img_out_ptr ) {
                break ; // exit the while
            } 
        }
        // Something isn't right here, need to skip back further than the 16 pixels we stride over
        img_ptr -= 16 ; // be nice to get idx of fist non-zero somehow
#endif
#ifndef __TEST_BAD__
        while( *img_ptr < threshold && img_ptr < img_out_ptr ) {
            ++img_ptr ;
        }
#endif
#ifdef __TEST_BAD__
        while( img[ img_idx ] < threshold && img_idx < img_out ) {
            ++img_ptr ;
            ++img_idx ;
        }
#endif
        if( img_ptr >= img_out_ptr ) {
            ended = 1 ;
            break ;
        }
        // Bright px
        img_idx = img_ptr - img_in_ptr ;

        //printf("hot px found at idx:%d (x%d, y%d).\n", img_idx, img_idx % img_w, img_idx / img_w);

        // scan this line
        current_line.reset() ;

        // scanline housekeeping
        if( img_idx > row_end ) {
            // we're on a new row
            tmp_y = img_idx / img_w ;
            tmp_x = img_idx % img_w ;
            row_start = tmp_y * img_w ;
            row_end = ((tmp_y + 1) * img_w) - 1 ;
            previous_y = tmp_y - 1 ; // y above current
            
            // validate the masterline
            if( master_line.y != previous_y ) {
                // The new scanline can't touch the masterline
                master_line_id = ROI_INVALID ;
            }
        } else {
            // same row as last scanline
            tmp_x = img_idx - row_start ;
        }

        current_line.x = tmp_x ;
        current_line.y = tmp_y ;

        // region list housekeeping
        // There must be a bug in here somewhere
        if( tmp_y > last_y ) {            
            // "left slice" have sl_n < last_y so can't join into the new scanline
            // "Right slice" could join this or future scanlines on this y
            reg_scan = reg_start ;
            reg_idx = reg_start ;
            reg_len = region_list.size() ;
            while( reg_scan < reg_len ) {
                if( region_list[reg_scan].sl.y < previous_y ) {
                    // move reg_scan to the left
                    std::swap( region_list[ reg_idx ], region_list[ reg_scan ] ) ;
                    ++reg_idx ;
                }
                ++reg_scan ;
            }
            reg_start = reg_idx ; // unreachable regions are moved to the left of start
            last_y = tmp_y ;
        } // if y changed

        // scan bright pixels, break if we go off the end of the row
        while( row_end >= img_idx && *img_ptr >= threshold ) {
            ++img_idx ;
            ++img_ptr ;
        }
        if( img_idx >= img_out ) {
            ended = 1 ; // off the end of the image, mark ended
        }


        // Do with pointers as well?
        // needs a row end ptr calculating in scanline housekeeping
        //while (*img_ptr >= threshold && img_ptr < row_end_ptr) {
        //  ++img_ptr;
        //}
        //img_idx = img_ptr - img_in_ptr;


        // Set the end of the scanline
        current_line.m = (img_idx - row_start) ;
        //printf("ri %d, rl %d\n", reg_idx, reg_len);

        // Insert new Region, or add Scanline to existing region, or use current_line or master_line
        // to join regions
        if( (reg_idx == reg_len) ||                         // 1st region, or end of list 
            (current_line.m < region_list[ reg_idx ].sl.x)  // ends before a potential join
          ) { 
            // Insert a new RoI
            //printf("New Region, can't touch\n");
            simpleROI new_reg ;
            new_reg.reset() ;
            new_reg.id = reg_id++ ;
            new_reg.takeScanLine( current_line ) ;
            new_reg.update() ;
            region_list.insert( region_list.begin() + reg_idx, new_reg ) ;
            reg_len = region_list.size() ;
            ++reg_idx ;

            // Could this new region join a masterline?
            if( (master_line_id != ROI_INVALID) && current_line.touches( master_line ) ) {
                    // This region underhangs a masterline, map the masterline's id to the region
                    //printf("Masterline 1 \n");
                    merge_map.insert(
                        std::pair< line_t, line_t >(
                            std::max( master_line_id, new_reg.id ),
                            std::min( master_line_id, new_reg.id )
                        )
                    ) ;
            }
            
        } else {
            // check for potential joins

            // skip regions that can't touch
            while( reg_idx < reg_len && region_list[ reg_idx ].sl.m < current_line.x ) {
                ++reg_idx;
            }
            // x-m   x----m   xm
            // 132456789123456789
            //  x-----m x--m x--m

            if( current_line.touches( region_list[ reg_idx ].sl ) ) {
                //printf("Touching region %d\n", reg_idx);
                // touching regions
                tmp_reg.reset() ;
                tmp_reg.takeScanLine( current_line ) ;
                tmp_reg.update() ;
                
                master_line    = region_list[ reg_idx ].sl ;
                master_line_id = region_list[ reg_idx ].id ;

                region_list[ reg_idx ].merge( tmp_reg ) ;
                region_list[ reg_idx ].takeScanLine( current_line ) ;
                
                merge_target = &region_list[ reg_idx ] ;
                // are there subsequent regions to join?
                ++reg_idx ;
                while( (reg_idx < reg_len) && current_line.touches( region_list[ reg_idx ].sl )  ) {
                    // 'merge_target' consumes the subsequent regions
                    merge_target->merge( region_list[ reg_idx ] ) ;

                    // remove the consumed region
                    region_list.erase( region_list.begin() + reg_idx ) ;
                    reg_len = region_list.size() ;
                }
                
                // Take back one khadam to honor the Hebrew God, whose Ark this is
                reg_idx-- ;
                
            } else {
                // insert between regions
                //printf("Between, might merge\n");
                simpleROI new_reg ;
                new_reg.reset() ;
                new_reg.id = reg_id++ ;
                new_reg.takeScanLine( current_line ) ;
                new_reg.update() ;
                region_list.insert( region_list.begin() + reg_idx, new_reg ) ;
                reg_len = region_list.size() ;
                ++reg_idx ;

                // Could this new region join a masterline?
                if( (master_line_id != ROI_INVALID) && current_line.touches( master_line ) ) {
                    // This region underhangs a masterline, map the masterline's id to the region
                    //printf("Masterline 2\n");
                    merge_map.insert(
                        std::pair< line_t, line_t >(
                            std::max( master_line_id, new_reg.id ),
                            std::min( master_line_id, new_reg.id )
                        )
                    ) ;
                }
            }

        } // if simple merge
    } // while not ended

    // Tidy regions, flag gutters ------------------------------------------------------------------
    
    // Sort the RoI List on ID, low to high
    std::sort( region_list.begin(), region_list.end() ) ;
    
    // make a map of id -> idx in the sorted Vector, collect gutters
    std::map< line_t, size_t > RoI_LUT ;
    for( size_t i = 0; i < region_list.size(); i++ ) {
        tmp_reg = region_list[i] ;
        RoI_LUT.insert( std::pair< line_t, size_t >( tmp_reg.id, i ) ) ;
        
        // Gutters not required for "Whole Image" processing, only relevent when splitting image
        if( tmp_reg.bb_y == gut_y || tmp_reg.bb_n == gut_n ) {
            gutterballs.insert( tmp_reg.id ) ;
        }
    }
    
    size_t from, into ; // region indexes for merging
    //std::map< line_t, size_t >::reverse_iterator it ; <- Pukes if not using auto ?!?
    for( auto rit = merge_map.rbegin(); rit != merge_map.rend(); ++rit ) {
        // the map is ordered by key, iterate through it backwards
        from = RoI_LUT[ rit->first ] ;
        into = RoI_LUT[ rit->second ] ;
        //printf( "id '%d' (%d) to merge with '%d' (%d)\n", rit->first, from ,rit->second, into ) ;
        // merge in the region
        region_list[ into ].merge( region_list[ from ] ) ;
        // delete the 'from' region, as we are working backwards 'forward' indexs are not corrupted
        region_list.erase( region_list.begin() + from ) ;
    }
    
    return region_list ;
} // ConnectedComponents


// Do Connected Components on the whole Image (flat)
// As above, but optimize for processing a whole Image - still flat
DetVec_t ConnectedComponentsImage(
    uint8_t*      &img,              // image to analyse
    const int     img_w,             // img width
    const int     img_h,             // img height
    const uint8_t threshold,         // Blob Brightness
    const size_t  vec_size           // Vctor reserve size
    // returns a Vector of Detections
){
    // Image stuff
    const int   img_in    = 0 ;      // Start idx of the 'slice' to analyse (inclusive)
    const int   img_out   = img_w * img_h ;  // End idx of the 'slice' to analyse (exclusive)
    int         img_idx   = 0 ;      // index into img (remember it's flat)
    int         row_start = 0 ;      // index of current row start
    int         row_end   = 0 ;      // index of current row end
    line_t      tmp_x     = 0 ;      // temp x,y of a bright pixel
    line_t      tmp_y     = 0 ;      // Shuttup compiler, I know it will get initialized
    line_t      last_y    = 0 ;      // last y we encountered

    // Regions & Region list stuff
    // TODO: should be 2 vectors, val and ref, and I should manipulate the refs
    RoiVec_t region_list ;           // the list of ROIs we have found
    size_t   reg_idx = 0 ;           // index of 1st reg we might examine
    size_t   reg_len = 0 ;           // length of region list
    line_t   reg_id  = 0 ;           // Rolling auto ID of Regions
    
    // Region housekeeping to find join candidates
    size_t   reg_start  = 0 ;        // first region that might touch current scanline
    size_t   reg_scan   = 0 ;        // used to split region_list
    line_t   previous_y = 0 ;        // used to test of touchable regions
    region_list.reserve( vec_size ) ;

    // Line detection
    scanline    current_line ;       // temp scanline

    // Image Moments
    moment      current_moment ;     // temp image moment
    float_t     val, pxi, pxj ;      // temps
    
    // Region joining
    scanline    master_line ;        // master region scanline - for joining
    line_t      master_line_id ;     // Region ID of the masterline
    ROI         tmp_reg ;            // temp region for comparison and merging
    ROI*        merge_target ;       // temp space for merging regions
    RoiIdMap_t  merge_map ;          // Map of RoI.ids that need to merge

    // Ejection control
    bool        ended = 0 ;          // have we finished looking at this slice

    // Guards
    const uint8_t* img_in_ptr  = &img[ img_in ]  ; // pointer to the 1st pxiel in the slice
    const uint8_t* img_out_ptr = &img[ img_out ] ; // pointer to the terminus pxiel in the slice

    // Setup for scanning
    img_idx = img_in;
    row_end = ((img_in % img_w) * img_w) - 1 ; // first run, force scanline housekeeping
    uint8_t* img_ptr = &img[img_in] ; // img pointer that scans the image

    // initalize structs
    current_line.reset() ;
    master_line.reset() ;
    tmp_reg.reset() ;

    // Initally, there can't be a masterline
    master_line_id = ROI_INVALID ;

    while( !ended ) {
        // Skip dark pixels
        
#ifdef __TEST_OPTIMIZED__
        // NEON
        uint8x16_t threshold_vector = vdupq_n_u8( threshold ) ;
        uint8x16_t mask_vector ;
        uint8x16_t img_slice   ;
        uint8x8_t  merge_bits  ;
        uint8_t    any_above = 0 ;
        
        while( !any_above ) {
            // load Img slice
            img_slice = vld1q_u8( img_ptr ) ;
            img_ptr += 16 ;
            
            //memcpy( &img_slice, img_ptr, 16 ) ; // this can't be fast :(
            
            // test for greatness - this should optimize into NEON, right?
            mask_vector = img_slice >= threshold_vector ;

            // https://stackoverflow.com/questions/15389539/fastest-way-to-test-a-128-bit-neon-register-for-a-value-of-0-using-intrinsics
            merge_bits = vorr_u8( vget_low_u8( mask_vector ), vget_high_u8( mask_vector ) ) ;
            any_above  = vget_lane_u8( vpmax_u8( merge_bits, merge_bits ), 0 ) ;

            // need a guard against going off the end
            if( img_ptr >= img_out_ptr ) {
                break ; // exit the while
            } 
        }
        // Something isn't right here, need to skip back further than the 16 pixels we stride over
        img_ptr -= 16 ; // be nice to get idx of fist non-zero somehow
#endif
#ifndef __TEST_BAD__
        while( *img_ptr < threshold && img_ptr < img_out_ptr ) {
            ++img_ptr ;
        }
#endif
#ifdef __TEST_BAD__
        while( img[ img_idx ] < threshold && img_idx < img_out ) {
            ++img_ptr ;
            ++img_idx ;
        }
#endif
        if( img_ptr >= img_out_ptr ) {
            ended = 1 ;
            break ;
        }
        // Bright px
        img_idx = img_ptr - img_in_ptr ;

        //printf("hot px found at idx:%d (x%d, y%d).\n", img_idx, img_idx % img_w, img_idx / img_w);

        // scan this line
        current_line.reset() ;

        // scanline housekeeping
        if( img_idx > row_end ) {
            // we're on a new row
            tmp_y = img_idx / img_w ;
            tmp_x = img_idx % img_w ;
            row_start = tmp_y * img_w ;
            row_end = ((tmp_y + 1) * img_w) - 1 ;
            previous_y = tmp_y - 1 ; // y above current
            
            // validate the masterline
            if( master_line.y != previous_y ) {
                // The new scanline can't touch the masterline
                master_line_id = ROI_INVALID ;
            }
        } else {
            // same row as last scanline
            tmp_x = img_idx - row_start ;
        }

        current_line.x = tmp_x ;
        current_line.y = tmp_y ;

        // region list housekeeping
        // There must be a bug in here somewhere
        if( tmp_y > last_y ) {            
            // "left slice" have sl_n < last_y so can't join into the new scanline
            // "Right slice" could join this or future scanlines on this y
            reg_scan = reg_start ;
            reg_idx = reg_start ;
            reg_len = region_list.size() ;
            while( reg_scan < reg_len ) {
                if( region_list[reg_scan].sl.y < previous_y ) {
                    // move reg_scan to the left
                    std::swap( region_list[ reg_idx ], region_list[ reg_scan ] ) ;
                    ++reg_idx ;
                }
                ++reg_scan ;
            }
            reg_start = reg_idx ; // unreachable regions are moved to the left of start
            last_y = tmp_y ;
        } // if y changed

        // scan bright pixels, break if we go off the end of the row
        // acumulate image moments
        current_moment.reset() ;
        while( row_end >= img_idx && *img_ptr >= threshold ) {
            // get intensity & x & y factors
            val = (float) (*img_ptr - threshold) ;
            pxi = val * tmp_x ;
            pxj = val * tmp_y ;

            // 1st Order
            current_moment.m_00 += val ;
            current_moment.m_10 += pxi ;
            current_moment.m_01 += pxj ;
            current_moment.m_11 += pxi * tmp_y ;

            // 2nd Order
            current_moment.m_20 += pxi * tmp_x ;
            current_moment.m_02 += pxj * tmp_y ;
            // inc
            ++tmp_x ;
            ++img_idx ;
            ++img_ptr ;
        }
        
        if( img_idx >= img_out ) {
            ended = 1 ; // off the end of the image, mark ended
        }

        // Set the end of the scanline
        current_line.m = tmp_x ;
        //printf("ri %d, rl %d\n", reg_idx, reg_len);

        // Insert new Region, or add Scanline to existing region, or use current_line or master_line
        // to join regions
        if( (reg_idx == reg_len) ||                         // 1st region, or end of list 
            (current_line.m < region_list[ reg_idx ].sl.x)  // ends before a potential join
          ) { 
            // Insert a new RoI
            //printf("New Region, can't touch\n");
            ROI new_reg ;
            new_reg.reset() ;
            new_reg.id = reg_id++ ;
            new_reg.takeScanLine( current_line ) ;
            new_reg.addMoment( current_moment ) ;
            new_reg.update() ;
            region_list.insert( region_list.begin() + reg_idx, new_reg ) ;
            reg_len = region_list.size() ;
            ++reg_idx ;

            // Could this new region join a masterline?
            if( (master_line_id != ROI_INVALID) && current_line.touches( master_line ) ) {
                    // This region underhangs a masterline, map the masterline's id to the region
                    //printf("Masterline 1 \n");
                    merge_map.insert(
                        std::pair< line_t, line_t >(
                            std::max( master_line_id, new_reg.id ),
                            std::min( master_line_id, new_reg.id )
                        )
                    ) ;
            }
            
        } else {
            // check for potential joins

            // skip regions that can't touch
            while( reg_idx < reg_len && region_list[ reg_idx ].sl.m < current_line.x ) {
                ++reg_idx;
            }
            // x-m   x----m   xm
            // 132456789123456789
            //  x-----m x--m x--m

            if( current_line.touches( region_list[ reg_idx ].sl ) ) {
                //printf("Touching region %d\n", reg_idx);
                // touching regions
                tmp_reg.reset() ;
                tmp_reg.takeScanLine( current_line ) ;
                tmp_reg.addMoment( current_moment ) ;
                tmp_reg.update() ;
                
                master_line    = region_list[ reg_idx ].sl ;
                master_line_id = region_list[ reg_idx ].id ;

                region_list[ reg_idx ].merge( tmp_reg ) ;
                region_list[ reg_idx ].takeScanLine( current_line ) ;
                
                merge_target = &region_list[ reg_idx ] ;
                // are there subsequent regions to join?
                ++reg_idx ;
                while( (reg_idx < reg_len) && current_line.touches( region_list[ reg_idx ].sl )  ) {
                    // 'merge_target' consumes the subsequent regions
                    merge_target->merge( region_list[ reg_idx ] ) ;

                    // remove the consumed region
                    region_list.erase( region_list.begin() + reg_idx ) ;
                    reg_len = region_list.size() ;
                }
                
                // Take back one khadam to honor the Hebrew God, whose Ark this is
                reg_idx-- ;
                
            } else {
                // insert between regions
                //printf("Between, might merge\n");
                ROI new_reg ;
                new_reg.reset() ;
                new_reg.id = reg_id++ ;
                new_reg.takeScanLine( current_line ) ;
                new_reg.addMoment( current_moment ) ;
                new_reg.update() ;
                region_list.insert( region_list.begin() + reg_idx, new_reg ) ;
                reg_len = region_list.size() ;
                ++reg_idx ;

                // Could this new region join a masterline?
                if( (master_line_id != ROI_INVALID) && current_line.touches( master_line ) ) {
                    // This region underhangs a masterline, map the masterline's id to the region
                    //printf("Masterline 2\n");
                    merge_map.insert(
                        std::pair< line_t, line_t >(
                            std::max( master_line_id, new_reg.id ),
                            std::min( master_line_id, new_reg.id )
                        )
                    ) ;
                }
            }

        } // if simple merge
    } // while not ended

    // Tidy regions, flag gutters ------------------------------------------------------------------

    // make a map of id -> idx in the region_list
    std::map< line_t, size_t > RoI_LUT ;
    for( size_t i = 0; i < region_list.size(); i++ ) {
        tmp_reg = region_list[i] ;
        RoI_LUT.insert( std::pair< line_t, size_t >( tmp_reg.id, i ) ) ;
    }
    
    size_t from, into ; // region indexes for merging
    for( auto it = merge_map.begin(); it != merge_map.end(); ++it ) {
        from = RoI_LUT[ it->first ] ;
        into = RoI_LUT[ it->second ] ;
        //debug
        //printf( "id '%d' (%d) to merge with '%d' (%d)\n", rit->first, from ,rit->second, into ) ;
        // merge in the region
        region_list[ into ].merge( region_list[ from ] ) ;
        // flag from as invalid
        region_list[ from ].id = ROI_INVALID ;
    }
    
    // Use the region's moments to compute Detections ----------------------------------------------
    float_t        u_02, u_20, u_11 ; // Central moments
    float_t        m_00R ;            // Area Reciprical
    float_t        x, y, r, score ;   // Centroid of the RoI
    double_t       L1, L2, a, b, c, d;// Used to compute Radius
    DetVec_t       ret ;              // The return
    uint8_t        w, h ;             // BB dimentions
    
    ret.reserve( region_list.size() ) ;
    
    for( size_t i = 0; i < region_list.size(); i++ ) {
        // region
        tmp_reg = region_list[i] ;
        
        // Don't do invalid regions
        if( tmp_reg.id == ROI_INVALID ){
            continue ;
        }
        
        // bb Dims
        w = tmp_reg.bb_m - tmp_reg.bb_x ;
        h = tmp_reg.bb_n - tmp_reg.bb_y ;
        
        // Reset Computation Vars
        m_00R = u_02 = u_20 = u_11 = 0.0f ;
        L1 = L2 = a = b = c = d = 0.0f ;
        x = y = r = score = 0.0f ;
        
        // Compute Centroid --------------------------------------------
        // compute 1st order moments
        m_00R = (1.0f / tmp_reg.hu.m_00) ;
        x = tmp_reg.hu.m_10 * m_00R ;
        y = tmp_reg.hu.m_01 * m_00R ;

        // Compute radius ----------------------------------------------
        // Implementing equations from ImageMagik Script 'moments'
        // Cos all the scientific methods give bogus results!

        // Compute Central moments
        u_11 = tmp_reg.hu.m_11 - (x * tmp_reg.hu.m_01) ;
        u_20 = tmp_reg.hu.m_20 - (x * tmp_reg.hu.m_10) ;
        u_02 = tmp_reg.hu.m_02 - (y * tmp_reg.hu.m_01) ;

        // Solve the Eigen values
        a = 2.0f * m_00R ;
        b = u_20 + u_02  ;
        c = 4.0 * (u_11 * u_11) ;
        d = (u_20 - u_02) * (u_20 - u_02) ;

        // Magik method
        L1 = sqrt( a * (b + sqrt( c + d )) ) ;
        L2 = sqrt( a * (b - sqrt( c + d )) ) ;
        
        r = ((float) w + h) / 4.0f ; // Backup guess

        if( L1 > 0.0f ) {
            r = L1 ;
        }

        // Circularity Score -------------------------------------------
        // being clever requies both Eigen Values to be correct
        score = (float) std::min( w, h ) / (float) std::max( w, h ) ;


        // fix center (center of a pixel, not in the up-left corner)
        x += 0.5f ;
        y += 0.5f ;


        // Add to return
        simpleDet newDet ;
        newDet.x = x ;
        newDet.y = y ;
        newDet.r = r ;
        newDet.score = score ;
        newDet.w = w ;
        newDet.h = h ;

        ret.push_back( newDet ) ;
    }
    return ret ;
} // ConnectedComponents


DetVec_t CircleFit(
    uint8_t*         &img,             // image to analyse  
    const sRoiVec_t  &regions,         // Vector of RoIs containing Blobs
    const int        img_w,            // image width
    const uint8_t    threshold         // Blob Brightness
    // Returns Vector of Centroids
) {
    // Use Hu-Moments to compute the Centroid of the bright RoI's we have found

    DetVec_t       ret;              // Return vector
    ret.reserve(regions.size());

    // image navigation
    const uint8_t* img_in = &img[0] ; // ref to 1st Pixel
    uint8_t*       img_ptr ;          // Pixel being processed

    // Image Moments
    float_t        m_00, m_01, m_10 ; // Raw Moments
    float_t        m_11, m_02, m_20 ; // 2nd Order Moments
    float_t        u_02, u_20, u_11 ; // Central moments
    float_t        m_00R ;            // Area Reciprical
    float_t        val, pxi, pxj ;    // temps
    float_t        x, y, r, score ;   // Centroid of the RoI
    double_t       L1, L2, a, b, c, d;// Used to compute Radius
    
    // Region we are processing
    simpleROI      region ;           // Temp Region     
    uint8_t        w, h ;             // BB dimentions
    
    // for each region compute moments
    for( size_t r_idx = 0; r_idx < regions.size(); r_idx++ ) {
        region = regions[ r_idx ] ;
        
        // bb Dims
        w = region.bb_m - region.bb_x ;
        h = region.bb_n - region.bb_y ;
        
        // reset acumulation vars
        m_00 = m_01 = m_10 = pxi = pxj = val = 0.0f ;
        m_11 = m_02 = m_20 = 0.0f ;

        // examine region BB some dims could be 1 px
        for( size_t j = region.bb_y; j <= region.bb_n; j++ ) {
            img_ptr = (uint8_t*) img_in + (j * img_w) ;
            img_ptr += region.bb_x ;
            for( size_t i = region.bb_x; i <= region.bb_m; i++ ) {
                if( *img_ptr > threshold ) {
                   // acumulate statistics
                   val = (float) (*img_ptr - threshold) ;
                   pxi = val * i ;
                   pxj = val * j ;

                   // 1st Order
                   m_00 += val ;
                   m_10 += pxi ;
                   m_01 += pxj ;
                   m_11 += pxi * j ;
                   
                   // 2nd Order
                   m_20 += pxi * i ;
                   m_02 += pxj * j ;
                }
                ++img_ptr ;
            }
        }

        // Reset Computation Vars
        m_00R = u_02 = u_20 = u_11 = 0.0f ;
        L1 = L2 = a = b = c = d = 0.0f ;
        x = y = r = score = 0.0f ;
        
        // Compute Centroid --------------------------------------------
        // compute 1st order moments
        m_00R = (1.0f / m_00) ;
        x = m_10 * m_00R ;
        y = m_01 * m_00R ;

        // Compute radius ----------------------------------------------
        // Implementing equations from ImageMagik Script 'moments'
        // Cos all the scientific methods give bogus results!

        // Compute Central moments
        u_11 = m_11 - (x * m_01) ;
        u_20 = m_20 - (x * m_10) ;
        u_02 = m_02 - (y * m_01) ;

        // Solve the Eigen values
        a = 2.0f * m_00R ;
        b = u_20 + u_02  ;
        c = 4.0 * (u_11 * u_11) ;
        d = (u_20 - u_02) * (u_20 - u_02) ;

        // Magik method
        L1 = sqrt( a * (b + sqrt( c + d )) ) ;
        L2 = sqrt( a * (b - sqrt( c + d )) ) ;
        
        r = ((float) w + h) / 4.0f ;

        if( L1 > 0.0f ) {
            r = L1 ;
        }

        // Circularity Score -------------------------------------------
        // being clever requies both Eigen Values to be correct
        score = (float) std::min( w, h ) / (float) std::max( w, h ) ;


        // fix center (center of a pixel, not in the up-left corner)
        x += 0.5f ;
        y += 0.5f ;


        // Add to return
        simpleDet newDet ;
        newDet.x = x ;
        newDet.y = y ;
        newDet.r = r ;
        newDet.score = score ;
        newDet.w = w ;
        newDet.h = h ;

        ret.push_back( newDet ) ;
    }

    // Done
    return ret ;
} // CircleFit


Roid8Vec_t PackCentroids8( const DetVec_t &centroids ) { // returns Vector of packed centroids
    // TODO: Move to a more sensible library
    Roid8Vec_t ret ;
    ret.reserve( centroids.size() ) ;

    simpleDet  c ;        // temp centroid
    float_t    dec, frc ; // temp decimal * modulus

    for( size_t i = 0; i < centroids.size(); i++ ) {
        packedCentroids8 r ;
        c = centroids[ i ] ;

        // TODO: Could NEON help here?
        
        // X
        frc = std::modf( c.x, &dec ) ;
        r.xd = (uint16_t) dec ;
        r.xf = (uint8_t) (frc * 255) ;

        // Y
        frc = std::modf( c.y, &dec ) ;
        r.yd = (uint16_t) dec ;
        r.yf = (uint8_t) (frc * 255) ;

        // R
        frc = std::modf( c.r, &dec ) ;
        r.rd = (uint8_t)   dec ;
        r.rf = ((uint8_t) (frc * 128)) << 4 ;
        
        ret.push_back( r ) ;
    }

    return ret ;
} // PackCentroids9


} // namespace vision
#endif
