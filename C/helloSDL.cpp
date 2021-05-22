#include <stdio.h>
#include <string>
#include "time.h"
#include "SDL2/SDL.h"
#include "vision.cpp"


timespec TimeDelta( timespec start, timespec end )
{
    timespec delta ;

    delta.tv_sec  = end.tv_sec  - start.tv_sec  ;
    delta.tv_nsec = end.tv_nsec - start.tv_nsec ;

    // rolled into secs ?
    if( delta.tv_nsec  < 0 ) {
        delta.tv_sec  -= 1 ;
        delta.tv_nsec += 1000000000 ;
    }

    return delta ;
}


int main( int argc, char* args[] ) {
    // Just your average typical first compiled program
    printf("Testing Connected components !\n" ) ;
    
    // init SDL
    if( SDL_Init( SDL_INIT_VIDEO ) < 0 ) {
        fprintf( stderr, "ERROR, SDL_Init() - '%s'.\n", SDL_GetError() ) ;
        return -1 ;
    }
    
    std::string img_path_fq = "/code/rpiCap/benchmarkData/testing_000_0000.bmp" ;
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
    
    fprintf( stdout,
             "Image '%s'\nx:%d, y:%d row:%d, bit-depth: %d, size: %d.\n",
             img_path_fq.c_str(), w, h, row, bpp, num_px
    ) ;
    
    // data from cc
    vision::RoiVec_t   regions ;
    vision::RoiIdSet_t gutters ;

    // Profile connected components 
    timespec tm_start, tm_end, tm_delta ;   

    // Test 1, just the whole image
    uint8_t* image_data = static_cast<uint8_t*>( bmp->pixels ) ;
    size_t   vec_size   = 50 ;
    uint8_t  threshold  = 167 ;

    clock_gettime( CLOCK_PROCESS_CPUTIME_ID, &tm_start ) ;
    
    regions = vision::ConnectedComponents( image_data, w, h, 0, num_px, threshold, vec_size, gutters ) ;

    clock_gettime( CLOCK_PROCESS_CPUTIME_ID, &tm_end ) ;

    tm_delta = TimeDelta( tm_start, tm_end ) ;
    printf( "Time %d.%d \n", tm_delta.tv_sec, tm_delta.tv_nsec / 1000 ) ;


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
    
    // Report detections
    SDL_SetRenderDrawColor( renderer, 255, 0, 0, 255 ) ;
    SDL_Rect r;
    for( size_t i = 0; i < regions.size(); i++ ) {
        vision::simpleROI R = regions[i] ;
        //fprintf( stdout, "{%d} %d: x%d, y%d, m%d, n%d\n", i, R.id, R.bb_x, R.bb_y, R.bb_m, R.bb_n ) ;
        r.x = R.bb_x ;
        r.y = R.bb_y ;
        r.w = R.bb_m - R.bb_x;
        r.h = R.bb_n - R.bb_y;
        SDL_RenderDrawRect( renderer, &r ) ;
    }

    // Swap to screen and show
    SDL_RenderPresent( renderer ) ;

    SDL_Delay( 15000 ) ;

    // SDL Cleanup
    SDL_DestroyWindow( win ) ;
    SDL_DestroyTexture( tex ) ;
    SDL_FreeSurface( bmp ) ;
    SDL_Quit() ;

    return 1 ;
}
