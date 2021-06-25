#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int main( int argc, char * argv[] ) {
    if ( argc < 2 ) {
        printf( "Missing map argument\n" );
        return 1;
    }

    char binaryMap[24 * 24];

    FILE * textFile = fopen( argv[1], "r" );

    int y = 0, x = 0;
    while ( y < 24 ) {
        const char ch = fgetc( textFile );
        if ( ch == '\r' || ch == '\n' ) {
            continue;
        }

        binaryMap[( y * 24 ) + x] = ch - '0';
        ++x;
        if ( x == 24 ) {
            x = 0;
            ++y;
        }
    }

    fclose( textFile );

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

    printf( "Generating map -> %s\n", fileName );

    FILE * mapFile = fopen( fileName, "wb" );
    free( fileName );

    fwrite( binaryMap, 1, 24 * 24, mapFile );

    fclose( mapFile );

    return 0;
}
