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

    int a, b, c ;
    a = 2 ;
    b = 6 ;
    c = vision::sum( a, b ) ; // have I linked vision correctly?
    printf( "%d + %d = %d", a, b, c ) ;
    
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

    int num_px = sizeof( *pixfmt ) ;
    
    fprintf( stdout,
             "Image '%s'\nx:%d, y:%d row:%d, bit-depth: %d, size: %d.\n",
             img_path_fq, w, h, row, bpp, num_px
    ) ;
    
    for( int i=0; i < 16; i++ ) {
        Uint8 pix = *((Uint8*) bmp->pixels + i) ;
        printf( "%d, ", pix ) ;
    }
    printf("\n") ;
    
    // data from cc
    vision::tROIvec regions ;
    vision::tROIset gutters ;

    unsigned char threshold = 136 ;

    // Profile connected components 
    timespec tm_start, tm_end, tm_delta ;   

    clock_gettime( CLOCK_PROCESS_CPUTIME_ID, &tm_start ) ;

    // Test 1, just the whole image
    regions = vision::ConnectedComponents( bmp->pixels, w, h, 0, num_px, threshold, 50, gutters ) ;

    clock_gettime( CLOCK_PROCESS_CPUTIME_ID, &tm_end ) ;

    tm_delta = TimeDelta( tm_start, tm_end ) ;
    fprintf( stdout, "Time %d:%d \n", tm_delta.tv_sec, tm_delta.tv_nsec ) ;

    SDL_FreeSurface( bmp ) ;

    SDL_Quit() ;
    return 1 ;
}
