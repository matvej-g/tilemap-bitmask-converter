#define STB_IMAGE_IMPLEMENTATION
#define STB_IMAGE_WRITE_IMPLEMENTATION   
#include "stb_image.h"
#include "stb_image_write.h"
#include <stdio.h>
#include <string.h>

//config variables
int config_tile_width;
int config_tile_height;
int config_border;
int config_spacing;
int config_output_border;
int config_output_spacing;
char config_output_format[8];

//image variables
    //input file
int img_width, img_height;
int channels;
    //output file
int output_width ;
int output_height;


stbi_uc *write_tile(stbi_uc *outFile, stbi_uc *tile, int tile_x, int tile_y) {
    int pixel_x = config_output_border + tile_x * (config_tile_width + config_output_spacing);
    int pixel_y = config_output_border + tile_y * (config_tile_height + config_output_spacing);
    for (int y = 0; y < config_tile_height; y++) {
        for (int x = 0; x < config_tile_width; x++) {
            if ((pixel_x + x) >= output_width || (pixel_y + y) >= output_height) {
                fprintf(stderr, "Error: tile out of bounds\n");
                return NULL;
            }  
            for (int i = 0; i < channels; i++) {
                int dst_index = ((pixel_y + y) * output_width + (pixel_x + x)) * channels + i;
                int src_index = (y * config_tile_width + x) * channels + i;
                outFile[dst_index] = tile[src_index];  
            }
        }
    }
    return outFile;
}



stbi_uc *read_tile(stbi_uc *file, int tile_x, int tile_y) {
    stbi_uc *tile;
    tile = malloc(sizeof(stbi_uc) * config_tile_height * config_tile_width * channels);
    if (!tile) {
        fprintf(stderr, "Error: read_tile(); failed malloc\n");
        return NULL;
    }

    int pixel_x = config_border + tile_x * (config_tile_width + config_spacing);
    int pixel_y = config_border + tile_y * (config_tile_height + config_spacing);
    for (int y = 0; y < config_tile_height; y++) {
        for (int x = 0; x < config_tile_width; x++) {
            if ((pixel_x + x) >= img_width || (pixel_y + y) >= img_height) {
                fprintf(stderr, "Error: tile out of bounds\n");
                free(tile);
                return NULL;
            }  
            for (int i = 0; i < channels; i++) {
                int src_index = ((pixel_y + y) * img_width + (pixel_x + x)) * channels + i;
                int dst_index = (y * config_tile_width + x) * channels + i;
                tile[dst_index] = file[src_index];  
            }
        }
    }
    return tile;
}

int main(int argc, char **argv) {
    if (argc < 2) {
        fprintf(stderr, "Error: use: ./convert filename.png [outfilename]\n");
        return 1;
    }
    stbi_uc *fileInput;
    fileInput = stbi_load (argv[1], &img_width, &img_height, &channels, 0);
    if (!fileInput) {
        fprintf(stderr, "Error: File format not valid. USE: JEPEG, PNG, BMP, TGA\n");
        return 1;
    }
    //printf("file width: %d\nfileHeight: %d\nchannels: %d\n", img_width, img_height, channels);

    //read config file
    FILE *config;
    config = fopen("config.ini", "r");
    if (!config) {
        fprintf(stderr, "Error: config.ini not found\n");
        return 1;
    }

    char readConfigLine[256];
    while(fgets(readConfigLine, 256, config)) {
        if (strncmp(readConfigLine, "//", 2) == 0) continue;
        char key[64];
        int value;
        if ((sscanf(readConfigLine, "%s = %d", key, &value)) == 2) {
            if (strcmp(key, "tile_width") == 0) config_tile_width = value;
            else if (strcmp(key, "tile_height") == 0) config_tile_height = value;
            else if (strcmp(key, "border") == 0) config_border = value;
            else if (strcmp(key, "spacing") == 0) config_spacing = value;
            else if (strcmp(key, "output_border") == 0) config_output_border = value;
            else if (strcmp(key, "output_spacing") == 0) config_output_spacing = value;
        }
        char str_value[64];
        if (sscanf(readConfigLine, "%s = %s", key, str_value) == 2) {
            if (strcmp(key, "output_format") == 0) strncpy(config_output_format, str_value, 8);                           
        } 
    }
    fclose(config);

    //build outputFile
    int bitmask_order[16][2] = {
        {0,3}, {1,3}, {0,0}, {3,0},  //row 0: 01 02 03 04 to -> 13,14,01,04
        {0,2}, {1,0}, {2,3}, {1,1},  //row 1: 05 06 07 08 to -> 09,02,15,06
        {3,3}, {0,1}, {3,2}, {2,0},  //row 2: 09 10 11 12 to -> 16,05,12,03
        {1,2}, {3,1}, {2,2}, {2,1}   //row 3: 13 14 15 16 to -> 10,08,11,07
    };

    output_width = (config_output_border*2) + (4*config_tile_width) + (3*config_output_spacing);
    output_height = (config_output_border*2) + (4*config_tile_height) + (3*config_output_spacing);
    
    stbi_uc *outputFile;
    outputFile = malloc(sizeof(stbi_uc) * output_width * output_height * channels);
    if (!outputFile) {
        fprintf(stderr, "Error: malloc outputFile\n");
        return 1;
    }

    for (int i = 0; i < 16; i++) {
        int out_x = i % 4;
        int out_y = i / 4;
        stbi_uc *tile = read_tile(fileInput, bitmask_order[i][0], bitmask_order[i][1]);
        if (!tile) {
            fprintf(stderr, "Error: malloc tile\n");
            free(outputFile);
            stbi_image_free(fileInput);
            return 1;
        }
        if(!write_tile(outputFile, tile, out_x, out_y)) {
            fprintf(stderr, "Error: write_tile return NULL\n");
            free(tile);
            free(outputFile);
            stbi_image_free(fileInput);
            return 1;
        }
        free(tile);
    }
    //check which extension .png or .jpeg
    
    char inputFileName[256];
    char outFileName[256];

    if (argc == 3) {
        strncpy(outFileName, argv[2], 256); 
    } else {
        strncpy(inputFileName, argv[1], 256);
        char *ext = strrchr(inputFileName, '.');
        if (ext) *ext = '\0';
        snprintf(outFileName, 256, "%s_mask.%s", inputFileName, config_output_format);
    }

    if (strcmp(config_output_format, "png") == 0)
        stbi_write_png(outFileName, output_width, output_height, channels, outputFile, output_width * channels);
    else if (strcmp(config_output_format, "jpg") == 0)
        stbi_write_jpg(outFileName, output_width, output_height, channels, outputFile, 100);
    free(outputFile);
    stbi_image_free(fileInput);
    return 0;
}