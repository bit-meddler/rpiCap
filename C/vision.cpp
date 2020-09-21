#ifndef __VISION__
#define __VISION__

#include <cmath>
#include <vector>
#include <set>
#include <map>

namespace vision {

struct simpleDet {
	float_t x, y, r ;
	uint8_t w, h ;
} ;

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

int sum( int a, int b ) {
	return a + b ;
}

std::vector<simpleROI> connectedComponents( unsigned char &img,
											const uint16_t w,
											const uint16_t h, 
											const uint16_t img_in,
											const uint16_t img_out,
											const unsigned char threshold )
{
	size_t 						idx = 0 ; 			// index into img
	size_t 						reg_idx = 0 ; 		// index of 1st reg we might examine
	size_t 						reg_start = 0 ; 	// first region that might touch current scanline
	std::vector<simpleROI> 		reg_list ; 			// list of ROIs
	std::map<uint16_t,uint16_t> reg_lut ;			// Map of ROI.id that need to merge
	size_t						last_y = 0 ;		// last y we encountered
	size_t						row_end = 0 ;		// idx of the end of the row we're scanning
	size_t						tmp_x, tmp_y ;		// temp x,y of a bright pixel
	simpleROI					tmp_reg ;			// temp region for comparison and merging
	simpleROI					master_line ;		// master region scanline
	simpleROI					scanline ; 			// current scanline
	simpleROI					merge_target ; 		// temp space for merging regions
	bool						ended = 0 ;			// have we finished looking at this slice
	bool						skipping = 0 ; 		// are we trying to skip the predominantly dark regions
	bool 						scanning = 0 ;		// are we scanning bright pix
	bool 						inserting = 0 ;		// are we placing a ROI into the list
	bool						merging = 0 ;		// are we merging to an existing ROI
	
	// TODO: solve connected components
	
	return reg_list ;
}
											


} // namespace vision
#endif
