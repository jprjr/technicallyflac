#include "example-shared.h"

#define TECHNICALLYFLAC_IMPLEMENTATION
#include "../technicallyflac.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* example that reads in a headerless WAV file and writes
 * out a FLAC file with some tags. assumes WAV is 16-bit, 2channel, 44100Hz */

/* headerless wav can be created via ffmpeg like:
 *     ffmpeg -i your-audio.mp3 -ar 44100 -ac 2 -f s16le your-audio.raw
 */

/* example assumes we're on a little-endian system - would need to have
 * a proper decoder for input WAV data. */

#define BUFFER_SIZE 1


int main(int argc, const char *argv[]) {
    uint8_t buffer[BUFFER_SIZE];
    uint32_t bufferlen = BUFFER_SIZE;
    FILE *input;
    FILE *output;
    uint32_t frames;
    int16_t *raw_samples;
    int32_t *samples[2];
    int32_t *samplesbuf;
    uint8_t *tags;
    uint32_t tags_len;
    technicallyflac f;

    if(argc < 3) {
        printf("Usage: %s /path/to/raw /path/to/flac\n",argv[0]);
        return 1;
    }

    input = fopen(argv[1],"rb");
    if(input == NULL) return 1;

    output = fopen(argv[2],"wb");
    if(output == NULL) {
        fclose(input);
        return 1;
    }

    /* create a set of vorbis_comments */
    tags = create_tags(&tags_len);

    technicallyflac_init(&f,882,44100,2,16);

    raw_samples = (int16_t *)malloc(sizeof(int16_t) * f.channels * f.blocksize);
    if(!raw_samples) abort();
    samplesbuf = (int32_t *)malloc(sizeof(int32_t) * f.channels * f.blocksize);
    if(!samplesbuf) abort();
    samples[0] = &samplesbuf[0];
    samples[1] = &samplesbuf[f.blocksize];

    while(technicallyflac_streammarker(&f,buffer,&bufferlen)) {
        fwrite(buffer,1,bufferlen,output);
        bufferlen = BUFFER_SIZE;
    }

    fwrite(buffer,1,bufferlen,output);
    bufferlen = BUFFER_SIZE;

    while(technicallyflac_streaminfo(&f,buffer,&bufferlen,0)) {
        fwrite(buffer,1,bufferlen,output);
        bufferlen = BUFFER_SIZE;
    }

    fwrite(buffer,1,bufferlen,output);
    bufferlen = BUFFER_SIZE;

    while(technicallyflac_metadata(&f,buffer,&bufferlen,1, 4,tags_len, tags)) {
        fwrite(buffer,1,bufferlen,output);
        bufferlen = BUFFER_SIZE;
    }

    fwrite(buffer,1,bufferlen,output);
    bufferlen = BUFFER_SIZE;

    while((frames = fread(raw_samples,sizeof(int16_t) * 2, f.blocksize, input)) > 0) {
        repack_samples_deinterleave(samples,raw_samples,2,frames, 0);

        while(technicallyflac_frame(&f,buffer,&bufferlen,frames,samples)) {
            fwrite(buffer,1,bufferlen,output);
            bufferlen = BUFFER_SIZE;
        }
        fwrite(buffer,1,bufferlen,output);
        bufferlen = BUFFER_SIZE;
    }

    fclose(input);
    fclose(output);
    quit(0,tags,raw_samples,samplesbuf, NULL);

    return 0;
}

