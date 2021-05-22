#include <stdio.h>
#include <iostream>
#include <cmath>
#include <stdlib.h>
#include <algorithm>
#include <vector>
#include <map>
#include <set>


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

// debug
void printLine( const scanline X ) {
    printf( "x%d, y%d, m%d, n%d", X.x, X.y, X.m, X.n ) ;
}

RoiVec_t ConnectedComponents(
    uint8_t*      &img,              // image to analyse
    const int     img_w,             // img width
    const int     img_h,             // img height
    const int     img_in,            // Start idx of the 'slice' to analyse (inclusive)
    const int     img_out,           // End idx of the 'slice' to analyse (exclusive)
    const uint8_t threshold,         // Blob Brightness
    const int     vec_size,          // Vctor reserve size
    // returns
    RoiIdSet_t    &gutterballs       // Region ids that touch the first or last row, might merge
)// returns a Vector of ROIs
{
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
        0,   0,   0,   0,   0,   0,  60,  60,  60,   0,   0,   0, // 9
        0,  60,  60,   0,   0,   0,   0,   0,   0,   0,   0,   0, // 10
        0,  60,  60,   0,   0,   0,   0,   0,   0,   0,   0,   0  // 11
    } ; // 12x12

    for( int i = 0; i < 12; i++ ) {
        for( int j = 0; j < 12; j++ )
            printf( "%3d,", test_img[ (i * 12) + j ] ) ;
        printf( "\n" ) ;
    }
    uint8_t*    image_data = &test_img[0] ; 
    RoiIdSet_t  gutters ;
    uint8_t     threshold = 50 ;
    RoiVec_t    regions ;

    regions = ConnectedComponents( image_data, 12, 12, 0, 144, threshold, 5, gutters ) ;

    for( size_t i = 0; i < regions.size(); i++ ) {
        simpleROI R = regions[i] ;
        printf( "{%d} %d: x%d, y%d, m%d, n%d\n", i, R.id, R.bb_x, R.bb_y, R.bb_m, R.bb_n ) ;
    }
    printf( "These RoI ids are gutterballs: " ) ;
    std::set<line_t>::iterator it ;
    for( it = gutters.begin(); it != gutters.end(); ++it ) {
        printf( "%d ", *it ) ;
    }
}

void test_structs() {
    scanline X ;
    X.x = 1 ;
    X.y = 2 ;
    X.m = 3 ;
    X.n = 4 ;

    printf( "x%d, y%d, m%d, n%d\n", X.x, X.y, X.m, X.n ) ;

    simpleROI Y ;
    Y.reset() ;
    Y.takeScanLine( X ) ;
    Y.update() ;

    printf( "BB: x%d, y%d, m%d, n%d SL:x%d, m%d, n%d\n",
            Y.bb_x, Y.bb_y, Y.bb_m, Y.bb_n,
            Y.sl.x, Y.sl.m, Y.sl.n ) ;
}

int main() {
    test_cc() ;
}

