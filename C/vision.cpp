#ifndef __VISION__
#define __VISION__

// Testing NEON Vs nieve dark px skipping __TEST_NEON__ / __TEST_NORMAL__
#define __TEST_NORMAL__

#include <cmath>
#include <vector>
#include <set>
#include <map>

#include <arm_neon.h>

namespace vision {

struct simpleDet {
    float_t x, y, r ;
    uint8_t w, h ;
} ;
// type Helpers for ROIs
typedef std::vector< simpleDet > tROIvec ;


/*
 * x,y -> m,n
 * x and m are cols,
 * y and n are rows
 */
extern const uint16_t ROI_BIG_NUM = 65535 ;
extern const uint16_t ROI_NO_ID   = 60666 ;

struct simpleROI {
    // Bounding Box
    uint16_t bb_x, bb_y, bb_m, bb_n ;
    // *Previous* Scanline
    uint16_t sl_x, sl_m, sl_n ;
    // Region's id
    uint16_t id ;
    
    // functions
    void reset() {
        bb_x = bb_y = ROI_BIG_NUM ;
        bb_m = bb_n = 0 ;
        sl_x = sl_m = sl_n = 0 ;
        id = ROI_NO_ID ;
    }
    
    void update() {
        bb_x = std::min( bb_x, sl_x ) ;
        bb_y = std::min( bb_y, sl_n ) ;
        bb_m = std::max( bb_m, sl_m ) ;
        bb_n = std::max( bb_n, sl_n ) ;
    }
    
    void takeSL( simpleROI &other ) {
        sl_x = other.sl_x ;
        sl_m = other.sl_m ;
        sl_n = other.sl_n ;
    }
    
    void merge( simpleROI &other ) {
        // BB box
        bb_x = std::min( bb_x, other.bb_x ) ;
        bb_y = std::min( bb_y, other.bb_y ) ;
        bb_m = std::max( bb_m, other.bb_m ) ;
        bb_n = std::max( bb_n, other.bb_n ) ;
        // scanline
        sl_x = other.sl_x ;
        sl_m = other.sl_m ;
        sl_n = other.sl_n ;
        // take oldest id
        id = std::min( id, other.id ) ;
    }
} ; // struct simpleROI

// type Helpers for ROIs
typedef std::vector< simpleROI >        tROIvec ;
typedef std::set< simpleROI >           tROIset ;
typedef std::map< uint16_t, uint16_t >  tROImap ;

int sum( int a, int b ) {
    return a + b ;
}

tROIvec ConnectedComponents( unsigned char  &img,           // image to analyse
                             const uint16_t w,              // img width
                             const uint16_t h,              // img height
                             const uint16_t img_in,         // Start idx of the 'slice' to analyse (inclusive)
                             const uint16_t img_out,        // End idx of the 'slice' to analyse (exclusive)
                             const unsigned char threshold, // Blob Brightness
                             // returns
                             tROIset &gutterballs,          // Regions that touch the first or last row, might merge
)
{
    size_t      idx       = 0 ;     // index into img
    size_t      reg_idx   = 0 ;     // index of 1st reg we might examine
    size_t      reg_start = 0 ;     // first region that might touch current scanline
    size_t      last_y    = 0 ;     // last y we encountered
    size_t      row_end   = 0 ;     // idx of the end of the row we're scanning
    size_t      tmp_x, tmp_y ;      // temp x,y of a bright pixel
    size_t      gut_y, gut_n ;      // rows that are gutters

    simpleROI   tmp_reg ;           // temp region for comparison and merging
    simpleROI   master_line ;       // master region scanline
    simpleROI   scanline ;          // current scanline
    simpleROI   merge_target ;      // temp space for merging regions

    bool        ended     = 0 ;     // have we finished looking at this slice
    bool        skipping  = 0 ;     // are we trying to skip the predominantly dark regions
    bool        scanning  = 0 ;     // are we scanning bright pix
    bool        inserting = 0 ;     // are we placing a ROI into the list
    bool        merging   = 0 ;     // are we merging to an existing ROI
    
    tROIvec     reg_vec ;           // vector of ROIs
    tROImap     reg_lut ;           // Map of ROI.id that need to merge
    tROImap     mergers ;           // Regions that have merged (Debug)

    idx   = img_in ;
    gut_y = img_in  / w ;
    gut_n = img_out / w ;

    // TODO: solve connected components

    #ifdef __TEST_NORMAL__
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
        img_slice = (uint8x16_t *) img[ idx ] ;
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


    printf( "hot px found at %d.\n", idx ) ;
    return reg_list ;
}
                                            


} // namespace vision
#endif
