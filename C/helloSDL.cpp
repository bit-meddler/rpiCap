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
#include <string>
#include <sys/stat.h>
#include "SDL2/SDL.h"
#include "vision.cpp"


timespec TimeDelta( timespec start, timespec end ) {
    timespec delta ;

    delta.tv_sec  = end.tv_sec  - start.tv_sec  ;
    delta.tv_nsec = end.tv_nsec - start.tv_nsec ;

    // rolled into secs ?
    if( delta.tv_nsec  < 0 ) {
        delta.tv_sec  -= 1 ;
        delta.tv_nsec += 1000000000 ;
    }
    //printf( "start: %lds %ldns\n", start.tv_sec, start.tv_nsec ) ;
    //printf( "  end: %lds %ldns\n",   end.tv_sec,   end.tv_nsec ) ;
    return delta ;
}


void SDL_DrawFilledCircle( SDL_Renderer *renderer, int x, int y, int r ) {
    // You're kidding, SDL can't draw a circle! WTF!
    const int rad2 = r * 2 ;
    const int rsq  = r * r ;
    int dx, dy ;
    
    for( int w = 0; w < rad2; w++ ) {
        for( int h = 0; h < rad2; h++ ) {
            dx = r - w ;
            dy = r - h ;
            if( ((dx*dx)+(dy*dy)) <= rsq ){
                SDL_RenderDrawPoint( renderer, x - dx, y - dy ) ;
            }
        }
    }
} // Draw Circle


int main( int argc, char* args[] ) {
    // Just your average typical first compiled program
    printf("Testing Computer Vision !\n" ) ;
    
    std::string  DEFAULT_IMG = "/code/rpiCap/benchmarkData/testing_000_0000.bmp" ;
    bool         show_img  = 0 ;
    bool         show_dets = 1 ;
    bool         show_regs = 1 ;
    bool         show_info = 0 ;
    int          show_time = 5000 ;
    uint8_t      threshold = 166 ;
    std::string  img_path_fq = DEFAULT_IMG ;
    
    for( int i=1; i<argc; ) {
        
        if( (std::strcmp( args[i], "--help" ) == 0) ||
            (std::strcmp( args[i], "-h" ) == 0) ) {
            printf( "'-h' or '--help' Display this message\n" ) ;
            printf( "'-i' Display Info\n" ) ;
            printf( "'-s' Display Image (needs X session)\n" ) ;
            printf( "'-sr' only show the Regions\n" ) ;
            printf( "'-sc' only show the Centroids\n" ) ;
            printf( "'-d MILLISECONDS' delay image for this long\n" ) ;
            printf( "'-t THRESHOLD' set the blob threshold [0..255]\n" ) ;
            printf( "'-f FILENAME' process the supplied file (fully Qualified)\n" ) ;
            printf( "No switches just runs a short test\n" ) ;
        }
        
        if( std::strcmp( args[i], "-s" ) == 0 ) {
            show_img = 1 ;
        }
        
        
        if( std::strcmp( args[i], "-sr" ) == 0 ) {
            show_img  = 1 ;
            show_dets = 0 ;
        }
        
        
        if( std::strcmp( args[i], "-sc" ) == 0 ) {
            show_img  = 1 ;
            show_regs = 0 ;
        }
        
        if( std::strcmp( args[i], "-i" ) == 0 ) {
            show_info = 1 ;
        }
        
        if( std::strcmp( args[i], "-d" ) == 0 ) {
            show_img = 1 ;
            i++ ;
            show_time = atoi( args[i] ) ;
        }
        
        if( std::strcmp( args[i], "-t" ) == 0 ) {
            i++ ;
            threshold = atoi( args[i] ) ;
        }
        
        if( std::strcmp( args[i], "-f" ) == 0 ) {
            i++ ;
            img_path_fq = args[i] ;
        }
        
        i++ ;
    } // args

    // Check the passed filepath exists
    struct stat stat_tmp ;
    if( stat( img_path_fq.c_str(), &stat_tmp ) != 0 ) {
        // no file
        fprintf( stderr, "ERROR: can't find '%s'", img_path_fq.c_str() ) ;
        img_path_fq = DEFAULT_IMG ;
    } else {
        // cast to string ?
    }
    
    // init SDL
    if( SDL_Init( SDL_INIT_VIDEO ) < 0 ) {
        fprintf( stderr, "ERROR, SDL_Init() - '%s'.\n", SDL_GetError() ) ;
        return -1 ;
    }
    
    SDL_Surface *bmp = SDL_LoadBMP( img_path_fq.c_str() ) ;
    if( bmp == nullptr ) {
        fprintf( stderr, "ERROR, SDL_LoadBMP() - '%s'.\n", SDL_GetError() ) ;
        SDL_Quit() ;
        return -1 ;
    }
    int w = bmp->w ;
    int h = bmp->h ;
    Uint16 row = bmp->pitch ;
    SDL_PixelFormat *pixfmt = bmp->format ;
    int bpp = pixfmt->BitsPerPixel ;

    int num_px = h * row ;
    
    if( show_info ) {
        printf( "Image: '%s'\ndims: (%d x %d), row len:%d, bit-depth: %d, size: %dpx.\n",
                img_path_fq.c_str(), w, h, row, bpp, num_px
        ) ;
    }
    
    // types used to do mocap
    vision::sRoiVec_t  regions ;
    vision::RoiIdSet_t gutters ;
    vision::DetVec_t   detections ;
    vision::Roid8Vec_t roids ;
    
    // Profile Computer Vision Algos 
    timespec tm_start, tm_end, tm_delta ;   
    
    // Test 1, just the whole image
    uint8_t* image_data = static_cast<uint8_t*>( bmp->pixels ) ;
    size_t   vec_size   = 50 ;

	// Profile Connected Components ----------------------------------------------------------------


    // O L D   M E T H O D /////////////////////////////////////////////////////////////////////////
    printf( "Old Method\n" ) ;
    clock_gettime( CLOCK_PROCESS_CPUTIME_ID, &tm_start ) ;

    regions = vision::ConnectedComponentsSlice( image_data, w, h, 0, num_px, threshold, vec_size, gutters ) ;

    clock_gettime( CLOCK_PROCESS_CPUTIME_ID, &tm_end ) ;

    tm_delta = TimeDelta( tm_start, tm_end ) ;
    printf( "Detected % 2d Regions in  ", regions.size() ) ;
    printf( "%lds %ld.%03ldms \n", tm_delta.tv_sec, tm_delta.tv_nsec / 1000000, tm_delta.tv_nsec / 1000 ) ; // 1ms = ns / 1e6

	// Profile Circle-Fitting ----------------------------------------------------------------------
    clock_gettime( CLOCK_PROCESS_CPUTIME_ID, &tm_start ) ;

    detections = vision::CircleFit( image_data, regions, w, threshold ) ;

    clock_gettime( CLOCK_PROCESS_CPUTIME_ID, &tm_end ) ;

    tm_delta = TimeDelta( tm_start, tm_end ) ;
    printf( "RoI Centers Computed in  " ) ;
    printf( "%lds %ld.%03ldms \n", tm_delta.tv_sec, tm_delta.tv_nsec / 1000000, tm_delta.tv_nsec / 1000 ) ; // 1ms = ns / 1e6

	// Profile Roid Packing ------------------------------------------------------------------------
    clock_gettime( CLOCK_PROCESS_CPUTIME_ID, &tm_start ) ;

    roids = vision::PackCentroids8( detections) ;

    clock_gettime( CLOCK_PROCESS_CPUTIME_ID, &tm_end ) ;

    tm_delta = TimeDelta( tm_start, tm_end ) ;
    printf( "Packed roids to int type " ) ;
    printf( "%lds %ld.%03ldms \n", tm_delta.tv_sec, tm_delta.tv_nsec / 1000000, tm_delta.tv_nsec / 1000 ) ; // 1ms = ns / 1e6


    // N E W   M E T H O D /////////////////////////////////////////////////////////////////////////
    printf( "\n\nNew Method\n" ) ;
    // Profile Combined Moment collection ----------------------------------------------------------
    clock_gettime( CLOCK_PROCESS_CPUTIME_ID, &tm_start ) ;

    detections = vision::ConnectedComponentsImage( image_data, w, h, threshold, vec_size ) ;

    clock_gettime( CLOCK_PROCESS_CPUTIME_ID, &tm_end ) ;

    tm_delta = TimeDelta( tm_start, tm_end ) ;
    printf( "Processed % 2d 'Roids in  ", regions.size() ) ;
    printf( "%lds %ld.%03ldms \n", tm_delta.tv_sec, tm_delta.tv_nsec / 1000000, tm_delta.tv_nsec / 1000 ) ; // 1ms = ns / 1e6

	// Profile Roid Packing ------------------------------------------------------------------------
    clock_gettime( CLOCK_PROCESS_CPUTIME_ID, &tm_start ) ;

    roids = vision::PackCentroids8( detections) ;

    clock_gettime( CLOCK_PROCESS_CPUTIME_ID, &tm_end ) ;

    tm_delta = TimeDelta( tm_start, tm_end ) ;
    printf( "Packed roids to int type " ) ;
    printf( "%lds %ld.%03ldms \n", tm_delta.tv_sec, tm_delta.tv_nsec / 1000000, tm_delta.tv_nsec / 1000 ) ; // 1ms = ns / 1e6


    if( show_img ) {
        // make an SDL Window to show the results
        SDL_Window* win = SDL_CreateWindow(
            "Testing Connected Components",
            SDL_WINDOWPOS_UNDEFINED,
            SDL_WINDOWPOS_UNDEFINED,
            w, h,
            SDL_WINDOW_SHOWN
        ) ;
        SDL_Renderer* renderer = SDL_CreateRenderer( win, -1, SDL_RENDERER_SOFTWARE ) ;
        
        // Clear
        SDL_SetRenderDrawColor( renderer, 0, 0, 0, 255 ) ;
        SDL_RenderClear( renderer ) ;
        
        // Blit the BMP to it
        SDL_Texture* tex = SDL_CreateTextureFromSurface( renderer, bmp ) ;
        SDL_RenderCopy( renderer, tex, NULL, NULL ) ;
        
        // Report regions
        if( show_regs ) {
            SDL_SetRenderDrawColor( renderer, 255, 0, 0, 255 ) ;
            SDL_Rect r;
            vision::simpleROI R ;
            for( size_t i = 0; i < regions.size(); i++ ) {
                R = regions[i] ;
                r.x = R.bb_x ;
                r.y = R.bb_y ;
                r.w = R.bb_m - R.bb_x;
                r.h = R.bb_n - R.bb_y;
                SDL_RenderDrawRect( renderer, &r ) ;
            }
        }
        
        // Report detections
        if( show_dets ) {
            SDL_SetRenderDrawColor( renderer, 0, 255, 0, 48 ) ;
            vision::simpleDet D ;
            for( size_t i = 0; i < regions.size(); i++ ) {
                D = detections[i] ;
                SDL_DrawFilledCircle( renderer, (int)D.x, (int)D.y, (int)D.r ) ;
            }
        }
        
        // Swap to screen and show
        SDL_RenderPresent( renderer ) ;

        SDL_Delay( show_time ) ;

        // SDL Cleanup
        SDL_DestroyWindow( win ) ;
        SDL_DestroyTexture( tex ) ;
        
    }
    
    if( show_info ) {
        vision::simpleROI R ;
        vision::simpleDet D ;
        for( size_t i = 0; i < regions.size(); i++ ) {
            R = regions[i] ;
            D = detections[i] ;
            printf( "{%d} %d: x%d, y%d, m%d, n%d\n", i, R.id, R.bb_x, R.bb_y, R.bb_m, R.bb_n ) ;
            printf( "{%d} centroid %4f, %4f, %4f\n", i, D.x, D.y, D.r ) ;
        }
    }
    
    SDL_FreeSurface( bmp ) ;
    SDL_Quit() ;

    return 1 ;
}
