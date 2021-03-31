#include "example-shared.h"

#define TECHNICALLYFLAC_IMPLEMENTATION
#include "../technicallyflac.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <time.h>
#include <ogg/ogg.h>

#define QUIT { \
        ogg_stream_clear(&os); \
        fclose(input); \
        fclose(output); \
        quit(1,tags,raw_samples,samplesbuf, NULL); \
        return 1; }

/* re-define this if you'd like to experiment with other bit-depths (must be <= 16) */
#define BIT_DEPTH  16
#define BIT_SCALE (16 - BIT_DEPTH)

/* example that reads in a headerless WAV file and writes
 * out an FLAC-in-Ogg file with some tags. assumes WAV is 16-bit, 2channel, 44100Hz */

/* headerless wav can be created via ffmpeg like:
 *     ffmpeg -i your-audio.mp3 -ar 44100 -ac 2 -f s16le your-audio.raw
 */

/* example assumes we're on a little-endian system - would need to have
 * a proper decoder for input WAV data. */

static int
write_ogg_page(ogg_page *og, FILE *out) {
    int h;
    int b;
    if((h = fwrite(og->header,1,og->header_len,out)) != og->header_len) return h;
    if((b = fwrite(og->body,1,og->body_len,out)) != og->body_len) return b;
    return h + b;
}

int main(int argc, const char *argv[]) {
    uint8_t *buffer;
    uint32_t bufferlen = 0;
    uint32_t buffersize = 0;
    uint32_t bufferpos = 0;
    FILE *input;
    FILE *output;
    uint32_t frames;
    int16_t *raw_samples;
    int32_t *samples[2];
    int32_t *samplesbuf;
    uint8_t *tags;
    uint32_t tags_len;
    int serial;

    technicallyflac f;

    ogg_stream_state os;
    ogg_packet op;
    ogg_page og;

    if(argc < 3) {
        printf("Usage: %s /path/to/raw /path/to/ogg\n",argv[0]);
        return 1;
    }

    if(technicallyflac_init(&f,882,44100,2,BIT_DEPTH)) {
        printf("init failed, bail\n");
        return 1;
    }

    /* seed random number generator for making an ogg serial */
    srand((unsigned int)time(NULL));
    serial = rand();
    if(ogg_stream_init(&os,serial) != 0) return 1;

    input = fopen(argv[1],"rb");
    if(input == NULL) return 1;

    output = fopen(argv[2],"wb");
    if(output == NULL) {
        fclose(input);
        return 1;
    }

    /* create a set of vorbis_comments */
    tags = create_tags(&tags_len);

    raw_samples = (int16_t *)malloc(sizeof(int16_t) * f.channels * f.blocksize);
    if(!raw_samples) abort();
    samplesbuf = (int32_t *)malloc(sizeof(int32_t) * f.channels * f.blocksize);
    if(!samplesbuf) abort();
    samples[0] = &samplesbuf[0];
    samples[1] = &samplesbuf[f.blocksize];

    /* find our max packet size */
    buffersize = technicallyflac_size_frame(882,2,BIT_DEPTH);
    buffer = malloc(buffersize);
    if(!buffer) abort();
    bufferlen = buffersize;

    /* ogg packet will always just use our memory buffer */
    op.packet = buffer;

    /* set defaults for ogg_packet */
    op.bytes = 0;
    op.b_o_s = 0;
    op.e_o_s = 0;
    op.granulepos = 0;
    op.packetno = 0;

    /* everything is allocated - let's create the first packet */
    /* going to just add stuff into the memory buffer and update fields */

    /* one-byte packet type: 0x7F */
    buffer[0] = 0x7F;

    /* 4-byte ASCII signature FLAC */
    buffer[1] = 'F';
    buffer[2] = 'L';
    buffer[3] = 'A';
    buffer[4] = 'C';

    /* 1-byte major version number (1.0) */
    buffer[5] = 0x01;
    /* 1-byte minor version number (1.0) */
    buffer[6] = 0x00;

    /* 2-byte, big endian number of header packets (not including first) */
    buffer[7] = 0x00;
    buffer[8] = 0x01;

    bufferpos = 9;
    bufferlen = buffersize - bufferpos;

    if(technicallyflac_streammarker(&f,&buffer[bufferpos],&bufferlen) != 0) abort(); /* this should happen in one call */
    bufferpos += bufferlen;
    bufferlen = buffersize - bufferpos;

    if(technicallyflac_streaminfo(&f,&buffer[bufferpos],&bufferlen,0) != 0) abort() ;
    bufferpos += bufferlen;
    bufferlen = buffersize - bufferpos;

    /* we now have the first packet, feed it in */
    op.bytes = bufferpos;
    op.b_o_s = 1;

    if(ogg_stream_packetin(&os,&op) != 0) QUIT

    /* packet sent to stream successfully */
    op.packetno++;
    op.b_o_s = 0;
    bufferpos = 0;
    bufferlen = buffersize;

    /* force a flush, first page should only have streaminfo packet */
    if(ogg_stream_flush(&os,&og) == 0) QUIT

    if(write_ogg_page(&og,output) != (og.header_len + og.body_len)) QUIT


    /* write out the vorbis comment page */
    if(technicallyflac_metadata(&f,&buffer[bufferpos],&bufferlen,1,4,tags_len,tags) != 0) abort ();
    bufferpos += bufferlen;
    bufferlen = buffersize - bufferpos;

    op.bytes = bufferpos;
    if(ogg_stream_packetin(&os,&op) != 0) QUIT

    op.packetno++;
    bufferpos = 0;
    bufferlen = buffersize;

    if(ogg_stream_flush(&os,&og) == 0) QUIT
    if(write_ogg_page(&og,output) != (og.header_len + og.body_len)) QUIT

    while((frames = fread(raw_samples,sizeof(int16_t) * 2, f.blocksize, input)) > 0) {

        /* first check for and write out any pages */
        while(ogg_stream_pageout(&os,&og) != 0) {
            if(write_ogg_page(&og,output) != (og.header_len + og.body_len)) QUIT
        }

        repack_samples_deinterleave(samples,raw_samples,2,frames,BIT_SCALE);

        bufferpos = 0;
        bufferlen = buffersize;

        if(technicallyflac_frame(&f,&buffer[bufferpos],&bufferlen,frames,samples) != 0) abort();
        bufferpos += bufferlen;
        bufferlen -= bufferpos;

        op.bytes = bufferpos;
        op.granulepos += frames;

        if(frames != f.blocksize) {
            /* this means we're at end-of-file, set the end-of-stream flag */
            op.e_o_s = 1;
        }

        if(ogg_stream_packetin(&os,&op) != 0) QUIT

        op.packetno++;
    }

    /* flush out the final page */
    if(ogg_stream_flush(&os,&og) != 0) {
        if(write_ogg_page(&og,output) != (og.header_len + og.body_len)) QUIT
    }

    ogg_stream_clear(&os);
    fclose(input);
    fclose(output);
    quit(0,tags,raw_samples,samplesbuf, NULL);
    return 0;
}

