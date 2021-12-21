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

#include "rpiVision.h"

namespace vision {

/* 
 * NOTE: TopLeft BottomRight convention used in this project
 * x,y -> m,n
 * x and m are cols,
 * y and n are rows
 */

// Do Connected Components on the whole Image (flat)
// As above, but optimize for processing a whole Image - still flat
VecDet_t ConnectedComponentsImage(
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
    //int         last_idx  = 0 ;      // last hot index (Debug)
    line_t      tmp_x     = 0 ;      // temp x,y of a bright pixel
    line_t      tmp_y     = 0 ;      // Shuttup compiler, I know it will get initialized
    line_t      last_y    = 0 ;      // last y we encountered

    // Regions & Region list stuff
    // TODO: should be 2 vectors, val and ref, and I should manipulate the refs
    VecRoi_t region_list ;           // the list of ROIs we have found
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
    line_t      tmp_ysq = 0 ;        // y Squared, used in 2nd order moments
    moment      current_moment ;     // temp image moment
    float_t     val, pix, piy ;      // temps
    
    // Region joining
    scanline    master_line ;        // master region scanline - for joining
    line_t      master_line_id ;     // Region ID of the masterline
    ROI         tmp_reg ;            // temp region for comparison and merging
    ROI*        merge_target ;       // temp space for merging regions
    MapRoiId_t  merge_map ;          // Map of RoI.ids that need to merge

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

        // NEON
        uint8x16_t threshold_vector = vdupq_n_u8( threshold ) ;
        uint8x16_t mask_vector ;
        uint8x16_t img_slice   ;
        uint8x8_t  merge_bits  ;
        uint8_t    any_above = 0 ;

        // Align to 16-Byte strides
        uint8_t* img_align_ptr = img_ptr + ((img_out_ptr - img_ptr) % 16) ;
        while( img_ptr < img_align_ptr ) {
            if( *img_ptr >= threshold ) { // we got one!
                any_above = 1 ; // don't do SIMD Skipping
                img_ptr += 16 ; // this gets taken back
                break ;
            }
            ++img_ptr ;
        }
        
        // skip 16-Byte strides
        while( !any_above ) {
            // load Img slice
            img_slice = vld1q_u8( img_ptr ) ;
            img_ptr += 16 ;
            
            // test for greatness - this should optimize into NEON, right?
            mask_vector = img_slice >= threshold_vector ;

            // https://stackoverflow.com/questions/15389539/fastest-way-to-test-a-128-bit-neon-register-for-a-value-of-0-using-intrinsics
            merge_bits = vorr_u8( vget_low_u8( mask_vector ), vget_high_u8( mask_vector ) ) ;
            any_above  = vget_lane_u8( vpmax_u8( merge_bits, merge_bits ), 0 ) ;

            // have we go to the end of the image?
            if( img_ptr >= img_out_ptr ) {
                break ; // exit the while
            } 
        }
        
        if( any_above ) {
            // Wind back to scan for hot px
            img_ptr -= 20 ;
        }
        
        // rescan in a slower mode
        while( *img_ptr < threshold && img_ptr < img_out_ptr ) {
            ++img_ptr ;
        }


        if( img_ptr >= img_out_ptr ) {
            ended = 1 ;
            break ;
        }
        // Bright px
        img_idx = img_ptr - img_in_ptr ;

        // scanline housekeeping
        if( img_idx > row_end ) {
            // we're on a new row
            tmp_y = img_idx / img_w ;
            tmp_x = img_idx % img_w ;
            row_start = tmp_y * img_w ;
            row_end = ((tmp_y + 1) * img_w) - 1 ;
            previous_y = tmp_y - 1 ; // y above current
            tmp_ysq = tmp_y * tmp_y ;
            
            // validate the masterline
            if( master_line.y != previous_y ) {
                // The new scanline can't touch the masterline
                master_line_id = ROI_INVALID ;
            }
        } else {
            // same row as last scanline
            tmp_x = img_idx - row_start ;
        }

        //if( img_idx < last_idx ) {
            //printf( "Bad IDX! - px found at idx:%d (x%d, y%d).\n", img_idx, tmp_x, tmp_y ) ;
        //}
        //last_idx = img_idx ;
        
        // scan this line
        current_line.reset() ;
        current_line.x = tmp_x ;
        current_line.y = tmp_y ;

        // region list housekeeping
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
            pix = val * tmp_x ; // Pixel Intensity X
            piy = val * tmp_y ; // Pixel Intensity Y

            // 1st Order
            current_moment.m_00 += val ;
            current_moment.m_10 += pix ;
            current_moment.m_01 += piy ;
            current_moment.m_11 += pix *  tmp_y ;

            // 2nd Order
            current_moment.m_20 += pix * tmp_x ;
            current_moment.m_02 += val * tmp_ysq ;
            
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
        // TODO: something wrong here, but I can't figure it out
        if( (reg_idx == reg_len) ||                         // 1st region, or end of list 
            (current_line.m < region_list[ reg_idx ].sl.x)  // ends before a potential join
          ) { 
            // Insert a new RoI
            //printf("New Region, can't touch\n");
            ROI new_reg ;
            new_reg.reset() ;
            new_reg.id = reg_id++ ;
            new_reg.takeScanLine( current_line ) ;
            new_reg.hu = current_moment ;
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

            if( current_line.touches( region_list[ reg_idx ].sl ) ) {
                //printf("Touching region %d\n", reg_idx);
                // touching regions
                tmp_reg.reset() ;
                //tmp_reg.hu.reset() ;
                tmp_reg.takeScanLine( current_line ) ;
                //tmp_reg.addMoment( current_moment ) ;
                tmp_reg.hu = current_moment ;
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
                new_reg.hu = current_moment ;
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
    MapRoIdx_t RoI_LUT ;
    for( size_t i = 0; i < region_list.size(); i++ ) {
        RoI_LUT.insert( std::pair< line_t, size_t >( region_list[ i ].id, i ) ) ;
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
    VecDet_t       ret ;              // The return
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
        // 'Cos all the scientific methods give bogus results!

        // Compute Central moments
        u_11 = tmp_reg.hu.m_11 - (x * tmp_reg.hu.m_01) ;
        u_20 = tmp_reg.hu.m_20 - (x * tmp_reg.hu.m_10) ;
        u_02 = tmp_reg.hu.m_02 - (y * tmp_reg.hu.m_01) ;

        // Solve the Eigen values
        a = 2.0f * m_00R ;
        b = u_20 + u_02  ;
        c = 4.0 * (u_11 * u_11) ;
        d = (u_20 - u_02) * (u_20 - u_02) ;

        L1 = sqrt( a * (b + sqrt( c + d )) ) ;
        //L2 = sqrt( a * (b - sqrt( c + d )) ) ; // minor axis is always off by a massive amount

        if( L1 > 0.0f ) {
            r = L1 ;
        } else {
            r = ((float) w + h) / 4.0f ; // Backup guess
        }

        // Circularity Score -------------------------------------------
        // being clever requies both Eigen Values to be correct
        score = (float) std::min( w, h ) / (float) std::max( w, h ) ;


        // fix center (center of a pixel, not in the up-left corner)
        x += 0.5f ;
        y += 0.5f ;


        // Add to return
        SimpleDet newDet ;
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

VecRoids8_t PackCentroids8( const VecDet_t &centroids ) { // returns Vector of packed centroids
    // TODO: Move to a more sensible library
    VecRoids8_t ret ;
    ret.reserve( centroids.size() ) ;

    SimpleDet  c ;        // temp centroid
    float_t    dec, frc ; // temp decimal * modulus

    for( size_t i = 0; i < centroids.size(); i++ ) {
        PackedCentroids8 r ;
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
} // PackCentroids8

VecRoids10_t PackCentroids10( const VecDet_t &centroids ) {
    // TODO: PackedCentroids10 Implementation
    VecRoids10_t ret ;
    return ret ;
}// PackCentroids10

} // ns vision

