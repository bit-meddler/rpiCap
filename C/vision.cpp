#ifndef __VISION__
#define __VISION__

// Testing NEON Vs nieve dark px skipping __TEST_NEON__ / __TEST_NORMAL__
#define __TEST_NORMAL__

#include <cmath>
#include <vector>
#include <set>
#include <map>
#include <cstring>

#ifdef __TEST_NEON__
#include <arm_neon.h>
#endif

namespace vision {

struct simpleDet {
    float_t x, y, r ;
    uint8_t w, h ;
} ;
// type Helpers for ROIs
typedef std::vector< simpleDet > Detvec_t ;


/*
 * x,y -> m,n
 * x and m are cols,
 * y and n are rows
 */
 
// Optimize type of line data
typedef int line_t; // test uint16_t uint32_t

extern const line_t ROI_BIG_NUM = 65535;
extern const line_t ROI_NO_ID   = 60666;

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
        return (x >= other.x && x <= other.m) || \
               (m >= other.x && m <= other.m) || \
               (other.x >= x && other.x <= m) || \
               (other.m >= x && other.m <= m) ;
    }
} ;

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

    inline void takeScanLine( simpleROI &other) {
        sl = other.sl ;
    }

    inline void merge( simpleROI &other ) {
        bb_x = std::min( bb_x, other.bb_x ) ;
        bb_y = std::min( bb_y, other.bb_y ) ;
        bb_m = std::max( bb_m, other.bb_m ) ;
        bb_n = std::max( bb_n, other.bb_n ) ;
        sl = other.sl ;
        // take oldest ID
        id = std::min( id, other.id ) ;
    }

    inline line_t area() {
        return (bb_m - bb_x) * (bb_n - bb_y) ;
    }
} ;

// type Helpers for ROIs
typedef std::vector< simpleROI >               RoiVec_t ;
typedef std::set< line_t >                     RoiIdSet_t ;
typedef std::map< line_t, line_t >             RoiIdMap_t ;
typedef std::map< line_t, line_t >::iterator   RoiIdIt_t ;

void neon() {
    #ifdef __TEST_ignore__
    // Test 1, just skip pix
    while( img[ idx ] <= threshold ) {
        ++idx ;
    }
    #endif

    #ifdef __TEST_NEON__
    // test 2, NEON
    uint8x16_t threshold_vector = vdupq_n_u8( threshold ) ;
    uint8x16_t mask_vector ;
    uint8x16_t img_slice   ;
    uint8x8_t  merge_bits  ;
    uint8_t    any_above = 0 ;
    
    while( !any_above ) {
        // load Img slice
        // What's wrong with that? img_slice = (uint8x16_t) img[ idx ] ;
        
        memcpy( &img_slice, &img[ idx ], 16 ) ; // this can't be fast :(
        // test for greatness - this should optimize into NEON, right?
        mask_vector = img_slice > threshold_vector ;

        // https://stackoverflow.com/questions/15389539/fastest-way-to-test-a-128-bit-neon-register-for-a-value-of-0-using-intrinsics
        merge_bits = vorr_u8( vget_low_u8( mask_vector ), vget_high_u8( mask_vector ) ) ;
        any_above  = vget_lane_u8( vpmax_u8( merge_bits, merge_bits ), 0 ) ;
        idx += 16 ;
    }
    idx -= 16 ; // be nice to get idx of fist non-zero somehow
    while( img[ idx ] <= threshold ) {
        ++idx ;
    }
    #endif
}


RoiVec_t ConnectedComponents(
    uint8_t*      &img,              // image to analyse
    const int     img_w,             // img width
    const int     img_h,             // img height
    const int     img_in,            // Start idx of the 'slice' to analyse (inclusive)
    const int     img_out,           // End idx of the 'slice' to analyse (exclusive)
    const uint8_t threshold,         // Blob Brightness
    const size_t  vec_size,          // Vctor reserve size
    // returns
    RoiIdSet_t    &gutterballs       // Region ids that touch the first or last row, might merge
    // returns a Vector of ROIs
){
    // Image stuff
    int         img_idx   = 0 ;      // index into img (remember it's flat)
    int         row_start = 0 ;      // index of current row start
    int         row_end   = 0 ;      // index of current row end
    line_t      tmp_x, tmp_y  ;      // temp x,y of a bright pixel
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

    while( !ended ) {
        // Skip dark pixels
        while( *img_ptr < threshold && img_ptr < img_out_ptr ) {
            ++img_ptr ;
        }
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
            previous_y = tmp_y - 1 ; // y above current
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
        current_line.m = (img_idx - row_start) - 1;
        //printf("ri %d, rl %d\n", reg_idx, reg_len);
        if( (reg_idx == reg_len) || // 1st region 
            (current_line.m < region_list[ reg_idx ].sl.x) // before a potential join
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

                merge_target = &region_list[ reg_idx ] ;
                // are there subsequent regions to join?
                ++reg_idx ;
                while( reg_idx < reg_len && current_line.touches( region_list[ reg_idx ].sl )  ) {
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
                if( current_line.touches( master_line ) && current_line.y == (master_line.y + 1) ) {
                    // This region underhangs a masterline, map the masterline's id to the region
                    //printf("Masterline\n");
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

    // Tidy regions, flag gutters
    // TODO: How to merge and remove RoIs?
    for( RoiIdIt_t it = merge_map.begin(); it != merge_map.end(); ++it ) {
        printf( "region id '%d' needs to merge into id '%d'\n", it->first, it->second ) ;
    }
    for( size_t i = 0; i < region_list.size(); i++ ) {
        tmp_reg = region_list[i] ;
        if( tmp_reg.bb_y == gut_y || tmp_reg.bb_n == gut_n ) {
            gutterballs.insert( tmp_reg.id ) ;
        }
    }
    return region_list ;
} // ConnectedComponents

} // namespace vision
#endif
