/*
===============================================================================

 Raycaster host tool

 Copyright (C) 2021-2022 gba-toolchain contributors
 For conditions of distribution and use, see copyright notice in LICENSE.md

===============================================================================
*/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define MAP_SIZE 24

static int taxi_distance(int x0, int y0, int x1, int y1);

int main(int argc, char* argv[]) {
    if (argc < 2) {
        printf("Missing input argument");
        return 1;
    }

    char binaryMap[MAP_SIZE * MAP_SIZE];
    unsigned char fieldMap[MAP_SIZE * MAP_SIZE];

    FILE* textFile = fopen(argv[1], "r");
    if (!textFile) {
        printf("Failed to open %s", argv[1]);
        return 1;
    }

    int y = 0, x = 0;
    while (y < MAP_SIZE) {
        const char ch = fgetc(textFile);
        if (ch == '\r' || ch == '\n') {
            continue;
        }

        binaryMap[(y * MAP_SIZE) + x] = ch - '0';
        ++x;
        if (x == MAP_SIZE) {
            x = 0;
            ++y;
        }
    }

    fclose(textFile);

    for (y = 0; y < MAP_SIZE; ++y) {
        for (x = 0; x < MAP_SIZE; ++x) {
            const int offset = (y * MAP_SIZE) + x;
            if (binaryMap[offset]) {
                fieldMap[offset] = 0;
                continue;
            }

            int minDist = MAP_SIZE + MAP_SIZE;
            for (int yy = 0; yy < MAP_SIZE; ++yy) {
                for (int xx = 0; xx < MAP_SIZE; ++xx) {
                    if (binaryMap[(yy * MAP_SIZE) + xx]) {
                        const int taxi = taxi_distance(x, y, xx, yy);
                        if (taxi < minDist) minDist = taxi;
                    }
                }
            }

            fieldMap[offset] = minDist;
        }
    }

    // Create bin name
    // Replace extension with .bin
    char path[260];
    strncpy(path, argv[1], sizeof(path));
    char* ext = path + sizeof(path) - 3;
    while (ext > path) {
        if (*ext == '.') {
            ext[1] = 'b';
            ext[2] = 'i';
            ext[3] = 'n';
            break;
        }
        ext--;
    }

    FILE* binaryFile = fopen(path, "wb");
    if (!binaryFile) {
        printf("Failed to open %s", path);
        return 1;
    }

    fwrite(binaryMap, 1, MAP_SIZE * MAP_SIZE, binaryFile);

    fclose(binaryFile);

    // Create fld name
    // Replace extension with .fld
    path[260];
    strncpy(path, argv[1], sizeof(path));
    ext = path + sizeof(path) - 3;
    while (ext > path) {
        if (*ext == '.') {
            ext[1] = 'f';
            ext[2] = 'l';
            ext[3] = 'd';
            break;
        }
        ext--;
    }

    binaryFile = fopen(path, "wb");
    if (!binaryFile) {
        printf("Failed to open %s", path);
        return 1;
    }

    fwrite(fieldMap, 1, MAP_SIZE * MAP_SIZE, binaryFile);

    fclose(binaryFile);

    return 0;
}

static int taxi_distance(int x0, int y0, int x1, int y1) {
    return abs(x0 - x1) + abs(y0 - y1);
}
