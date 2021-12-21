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

#include <map>
#include <queue>
#include <vector>

/* NOTE: TopLeft BottomRight convention used in this project
 * x,y -> m,n
 * x and m are cols,
 * y and n are rows
 */
 
namespace CamTypes {
/*
******************************************************************************** Computer Vision Stuff
*/
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

/*
******************************************************************************** Cam Server Stuff
*/

// Prioritized data packet (remember to delete the data!)
struct QPacket {
    int      priority ; // priority
    size_t   size ;     // size of the buffer
    uint8_t* data ;     // pointer to a dynamic buffer (The datagram)
 
    QPacket( const int priority, const size_t size, uint8_t* datagram ) :
             priority(priority), size(size), data(datagram) {
    }

    friend bool operator< (QPacket const& lhs, QPacket const& rhs) {
        // note: reversing for "lowest is best" ordering
        return lhs.priority > rhs.priority ;
    }
 
    friend std::ostream& operator<< (std::ostream& os, QPacket const& p) {
        return os << "{" << p.priority << " [" << p.size << "]} " ;
    }
} ;

// Camera Registers - Generated by "$REPO/piCap/sketches/genRegsStruct.py"
struct CamRegs {
    union {
        uint8_t       reg[1024] ;          // index access
        struct {                           // variable access
            /******** P R I V A T E  R E G S ****************************/
            uint8_t   roid_stream ;        // PRIVATE should we be streaming 'roids?
            uint8_t   send_one_img ;       // send one image
            uint8_t   img_stream ;         // stream images
            uint8_t   X_unknown_0[197] ;   // Here be Dragons
            /******** K N O W N   R E G S *******************************/
            uint8_t   fps ;                // reg 200
            uint8_t   X_unknown_1[3] ;     // Unknown
            uint8_t   strobe ;             // reg 204
            uint8_t   X_unknown_2 ;        // Unknown
            uint8_t   shutter ;            // reg 206
            uint8_t   X_unknown_3[7] ;     // Unknown
             int16_t  impactrefx ;         // reg 214 signed
             int16_t  impactrefy ;         // reg 216 signed
             int16_t  impactrefz ;         // reg 218 signed
            uint8_t   X_unknown_4[6] ;     // Unknown
            uint8_t   arpdelay ;           // reg 226
            uint8_t   X_unknown_5[3] ;     // Unknown
             int16_t  impactvalx ;         // reg 230 signed
             int16_t  impactvaly ;         // reg 232 signed
             int16_t  impactvalz ;         // reg 234 signed
            uint8_t   impactflags ;        // reg 236
            uint8_t   X_unknown_6[12] ;    // Unknown
            uint8_t   impactlimit ;        // reg 249
            uint8_t   X_unknown_7 ;        // Unknown
            uint8_t   mtu ;                // reg 251
            uint8_t   iscale ;             // reg 252
            uint8_t   idelay ;             // reg 253
            uint8_t   numdets ;            // reg 254
            uint8_t   threshold ;          // reg 255
            uint8_t   X_unknown_8[44] ;    // Here be Dragons
            uint16_t  maskzone01x ;        // reg 300
            uint16_t  maskzone01y ;        // reg 302
            uint16_t  maskzone01m ;        // reg 304
            uint16_t  maskzone01n ;        // reg 306
            uint16_t  maskzone02x ;        // reg 308
            uint16_t  maskzone02y ;        // reg 310
            uint16_t  maskzone02m ;        // reg 312
            uint16_t  maskzone02n ;        // reg 314
            uint16_t  maskzone03x ;        // reg 316
            uint16_t  maskzone03y ;        // reg 318
            uint16_t  maskzone03m ;        // reg 320
            uint16_t  maskzone03n ;        // reg 322
            uint16_t  maskzone04x ;        // reg 324
            uint16_t  maskzone04y ;        // reg 326
            uint16_t  maskzone04m ;        // reg 328
            uint16_t  maskzone04n ;        // reg 330
            uint16_t  maskzone05x ;        // reg 332
            uint16_t  maskzone05y ;        // reg 334
            uint16_t  maskzone05m ;        // reg 336
            uint16_t  maskzone05n ;        // reg 338
            uint16_t  maskzone06x ;        // reg 340
            uint16_t  maskzone06y ;        // reg 342
            uint16_t  maskzone06m ;        // reg 344
            uint16_t  maskzone06n ;        // reg 346
            uint16_t  maskzone07x ;        // reg 348
            uint16_t  maskzone07y ;        // reg 350
            uint16_t  maskzone07m ;        // reg 352
            uint16_t  maskzone07n ;        // reg 354
            uint16_t  maskzone08x ;        // reg 356
            uint16_t  maskzone08y ;        // reg 358
            uint16_t  maskzone08m ;        // reg 360
            uint16_t  maskzone08n ;        // reg 362
            uint16_t  maskzone09x ;        // reg 364
            uint16_t  maskzone09y ;        // reg 366
            uint16_t  maskzone09m ;        // reg 368
            uint16_t  maskzone09n ;        // reg 370
            uint16_t  maskzone10x ;        // reg 372
            uint16_t  maskzone10y ;        // reg 374
            uint16_t  maskzone10m ;        // reg 376
            uint16_t  maskzone10n ;        // reg 378
            uint16_t  maskzone11x ;        // reg 380
            uint16_t  maskzone11y ;        // reg 382
            uint16_t  maskzone11m ;        // reg 384
            uint16_t  maskzone11n ;        // reg 386
            uint16_t  maskzone12x ;        // reg 388
            uint16_t  maskzone12y ;        // reg 390
            uint16_t  maskzone12m ;        // reg 392
            uint16_t  maskzone12n ;        // reg 394
            uint16_t  maskzone13x ;        // reg 396
            uint16_t  maskzone13y ;        // reg 398
            uint16_t  maskzone13m ;        // reg 400
            uint16_t  maskzone13n ;        // reg 402
            uint16_t  maskzone14x ;        // reg 404
            uint16_t  maskzone14y ;        // reg 406
            uint16_t  maskzone14m ;        // reg 408
            uint16_t  maskzone14n ;        // reg 410
            uint16_t  maskzone15x ;        // reg 412
            uint16_t  maskzone15y ;        // reg 414
            uint16_t  maskzone15m ;        // reg 416
            uint16_t  maskzone15n ;        // reg 418
            uint16_t  maskzone16x ;        // reg 420
            uint16_t  maskzone16y ;        // reg 422
            uint16_t  maskzone16m ;        // reg 424
            uint16_t  maskzone16n ;        // reg 426
            uint8_t   X_unknown_9[595] ;   // Here be Dragons
        } ; // var struct
    } ; // union
} ; // camRegs

// Packetization helper
struct PacketData {
    uint32_t roll_cnt {0} ; // rolling counter
    uint32_t packet_cnt {0} ; // packet counter

    void inc( void ) {
        packet_cnt++ ;
        roll_cnt++   ;
        packet_cnt = packet_cnt & 0x1FFF ; // 0~8191
        roll_cnt   = roll_cnt   & 0x00FF ; // 0~255
    }
} ;

// Simple Timecode
struct Timecode {
        uint8_t h, m, s, f {0} ; 
} ;

// Packet Header
struct Header {
     union {
        uint8_t       X_data[24]{0} ;
        struct {
            uint16_t   packet_count, centroid_count ;
            uint8_t    flag, dtype, roll_count, size, tc_h, tc_m, tc_s, tc_f ;
            uint16_t   frag_num, frag_count ;  
            uint32_t   img_os, img_sz ;
        } ;
     } ;
} ;

typedef std::priority_queue< CamConsts::QPacket > QueuePackets_t ;

} // ns CamTypes

