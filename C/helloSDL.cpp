#include <stdio.h>
#include <string>
#include "SDL2/SDL.h"
#include "vision.cpp"


int main( int argc, char* args[] ) {
	// Just your average typical first compiled program
	printf("Hello, world!\n" ) ;

	int a, b, c ;
	a = 2 ;
	b = 6 ;
	c = vision::sum( a, b ) ;
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
	
	fprintf( stdout, "x:%d, y:%d row:%d, depth: %d.\n", w, h, row, bpp ) ;
	
	for( int i=0; i < 16; i++ ) {
		Uint8 pix = *((Uint8*) bmp->pixels + i) ;
		printf( "%d, ", pix ) ;
	}
	printf("\n") ;
	
	SDL_Window *my_win = SDL_CreateWindow( "Hello World!", 100, 100, 540, 380, SDL_WINDOW_SHOWN ) ;
	if( my_win == nullptr ) {
		fprintf( stderr, "ERROR, SDL_CreateWindow() - '%s'.\n", SDL_GetError() ) ;
		SDL_Quit() ;
		return -1 ;
	}
	
	SDL_Renderer *render = SDL_CreateRenderer( my_win, -1, SDL_RENDERER_SOFTWARE ) ;
	if( render == nullptr ) {
		fprintf( stderr, "ERROR, SDL_CreateRenderer() - '%s'.\n", SDL_GetError() ) ;
		SDL_DestroyWindow( my_win ) ;
		SDL_Quit() ;
		return -1 ;
	}
	
	SDL_Texture *tex = SDL_CreateTextureFromSurface( render, bmp ) ;
	SDL_FreeSurface( bmp ) ;
	if( tex == nullptr ) {
		fprintf( stderr, "ERROR, SDL_CreateTextureFromSurface() - '%s'.\n", SDL_GetError() ) ;
		SDL_DestroyRenderer( render ) ;
		SDL_DestroyWindow( my_win ) ;
		SDL_Quit() ;
		return -1 ;
	}
	
	// Attempt to Draw
	for( int i=0; i < 3; i++ ) {
		SDL_RenderClear( render ) ;
		SDL_RenderCopy( render, tex, NULL, NULL ) ;
		SDL_RenderPresent( render ) ;
		
		SDL_Delay( 1000 ) ;
	}
	
	// the end
	SDL_DestroyTexture( tex ) ;
	SDL_DestroyRenderer( render ) ;
	SDL_DestroyWindow( my_win ) ;
	SDL_Quit() ;
	return 1 ;
}
