#include "example-shared.h"

#define TECHNICALLYFLAC_IMPLEMENTATION
#include "../technicallyflac.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define QUIT { \
        fclose(input); \
        fclose(output); \
        quit(1,tags,raw_samples,samples, mem.buf, NULL); \
        return 1; }

/* example that reads in a headerless WAV file and writes
 * out a FLAC file with some tags. assumes WAV is 16-bit, 2channel, 44100Hz */

/* headerless wav can be created via ffmpeg like:
 *     ffmpeg -i your-audio.mp3 -ar 44100 -ac 2 -f s16le your-audio.raw
 */

/* example assumes we're on a little-endian system - would need to have
 * a proper decoder for input WAV data. */


int main(int argc, const char *argv[]) {
    membuffer mem;
    FILE *input;
    FILE *output;
    uint32_t frames;
    int16_t *raw_samples;
    int32_t *samples;
    uint8_t *tags;
    uint32_t tags_len;
    int fsize;
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

    /* setup flac parameters */
    f.samplerate = 44100;
    f.bitdepth = 16;
    f.channels = 2;
    f.blocksize = 882; /* 20ms */

    mem.len = technicallyflac_frame_size(f.channels,f.bitdepth,f.blocksize);
    mem.buf = (uint8_t *)malloc(mem.len);
    mem.pos = 0;
    memset(mem.buf,0,mem.len);

    raw_samples = (int16_t *)malloc(sizeof(int16_t) * f.channels * f.blocksize);
    samples = (int32_t *)malloc(sizeof(int32_t) * f.channels * f.blocksize);


    if(technicallyflac_init(&f)) {
        fclose(input);
        fclose(output);
        quit(1,tags,raw_samples,samples, mem.buf, NULL);
        return 1;
    }

    f.write = write_buffer;
    f.userdata = &mem;

    if(fwrite("fLaC",1,4,output) != 4) QUIT

    fsize = technicallyflac_streaminfo(&f,0);
    if(fwrite(mem.buf,1,fsize,output) != (size_t)fsize) QUIT
    mem.pos = 0;

    fsize = technicallyflac_metadata_block(&f, 1, 4, tags, tags_len);
    if(fwrite(mem.buf,1,fsize,output) != (size_t)fsize) QUIT
    mem.pos = 0;

    while((frames = fread(raw_samples,sizeof(int16_t) * 2, f.blocksize, input)) > 0) {
        repack_samples(samples,raw_samples,2,frames, 0);
        fsize = technicallyflac_encode_interleaved(&f,samples,frames);
        if(fwrite(mem.buf,1,fsize,output) != (size_t)fsize) QUIT
        mem.pos = 0;
    }

    fclose(input);
    fclose(output);
    quit(0,tags,raw_samples,samples,mem.buf, NULL);

    return 0;
}

