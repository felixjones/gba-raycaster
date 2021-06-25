#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"

typedef unsigned short gBGR1555_type;

static gBGR1555_type read_color( const stbi_uc * data );

int main( int argc, char * argv[] ) {
    if ( argc < 2 ) {
        printf( "Missing image argument\n" );
        return 1;
    }

    int width, height;
    stbi_uc * data = stbi_load( argv[1], &width, &height, NULL, 3 );
    if ( !data ) {
        printf( "Failed to read image %s\n", argv[1] );
        return 2;
    }

    // Create bin name
    char * slashF = strrchr( argv[1], '/' );
    char * slashB = strrchr( argv[1], '\\' );
    char * fileStart;
    if ( slashF != NULL && slashB != NULL ) {
        fileStart = ( slashF > slashB ? slashF : slashB ) + 1;
    } else if ( slashF != NULL ) {
        fileStart = slashF + 1;
    } else if ( slashB != NULL ) {
        fileStart = slashB + 1;
    } else {
        fileStart = argv[1];
    }

    char * extension = strrchr( fileStart, '.' );
    ptrdiff_t length = ( ptrdiff_t ) extension - ( ptrdiff_t ) fileStart;
    char * fileName = malloc( length + 5 );
    strncpy( fileName, fileStart, length );
    strcpy( &fileName[length], ".bin" );

    printf( "Generating %d %dx%d textures -> %s\n", width / height, height, height, fileName );

    FILE * textureFile = fopen( fileName, "wb" );
    free( fileName );

    gBGR1555_type palette[256];
    memset( palette, 0, sizeof( palette ) );
    int paletteSize = 0;

    for ( int xx = 0; xx < width; ++xx ) {
        for ( int yy = 0; yy < height; ++yy ) {
            const gBGR1555_type color = read_color( &data[( yy * width + xx ) * 3] );

            int ii;
            for ( ii = 0; ii < paletteSize; ++ii ) {
                if ( palette[ii] == color ) {
                    const stbi_uc byte = ( stbi_uc ) ii;
                    fwrite( &byte, sizeof( byte ), 1, textureFile );
                    break;
                }
            }

            if ( ii == paletteSize ) {
                if ( paletteSize == 256 ) {
                    printf( "Too many colors for palette!\n" );
                    return 3;
                }

                const stbi_uc byte = ( stbi_uc ) ii;
                fwrite( &byte, sizeof( byte ), 1, textureFile );
                palette[paletteSize++] = color;
            }
        }
    }

    fclose( textureFile );

    char * paletteName = malloc( length + 9 );
    strncpy( paletteName, fileStart, length );
    strcpy( &paletteName[length], ".pal.bin" );

    printf( "Generating palette -> %s\n", paletteName );

    FILE * paletteFile = fopen( fileName, "wb" );
    free( paletteName );

    fwrite( palette, sizeof( gBGR1555_type ), 256, paletteFile );

    fclose( paletteFile );

    stbi_image_free( data );

    return 0;
}

gBGR1555_type read_color( const stbi_uc * data ) {
    stbi_uc red = data[0], green = data[1], blue = data[2];

    gBGR1555_type color = 0;
    color |= ( green & 1 ) << 15; // LSB green
    color |= red >> 3;
    color |= ( green >> 3 ) << 5;
    color |= ( blue >> 3 ) << 10;
    return color;
}
