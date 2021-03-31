/*
Copyright (c) 2020 John Regan

Permission to use, copy, modify, and/or distribute this software for any
purpose with or without fee is hereby granted.

THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES WITH
REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF MERCHANTABILITY
AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY SPECIAL, DIRECT,
INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES WHATSOEVER RESULTING FROM
LOSS OF USE, DATA OR PROFITS, WHETHER IN AN ACTION OF CONTRACT, NEGLIGENCE OR
OTHER TORTIOUS ACTION, ARISING OUT OF OR IN CONNECTION WITH THE USE OR
PERFORMANCE OF THIS SOFTWARE.
*/

#ifndef TECHNICALLYFLAC_H
#define TECHNICALLYFLAC_H

#include <stdint.h>
#include <stddef.h>
#include <assert.h>

typedef struct technicallyflac_s technicallyflac;

#define TECHNICALLYFLAC_STREAMINFO_SIZE 38

#ifdef __cplusplus
extern "C" {
#endif

/* returns the size of a technicallyflac object */
size_t technicallyflac_size(void);

/* initialize a technicallyflac object, should be called before any other function */
int technicallyflac_init(technicallyflac *f, uint32_t blocksize, uint32_t samplerate, uint8_t channels, uint8_t bitdepth);

/*
Below functions are for writing out parts of a FLAC stream.

All functions take the same common parameters:
  technicallyflac *f - pointer to an allocated technicallyflac object
  uint8_t *output    - an output buffer
  uint32_t *bytes    - the length of your output buffer, it will be updated with the
                       number of bytes placed in the buffer.

  If a function returns 1, call it again with the same parameters (it was
  only able to partially write the chunk).

  Returning 0 means that block is complete.

  You CAN call a function with OUTPUT set to NULL to find the required number of bytes,
  if you want to dynamically allocate space.

  Generally-speaking, every flac file will need:
    * 1 streammarker
    * 1 streaminfo
    * 0+ metadata blocks
    * 1+ audio blocks
*/


/* writes out the streammarker */
int technicallyflac_streammarker(technicallyflac *f, uint8_t *output, uint32_t *bytes);

/* write out the streaminfo block, set last_flag to 1 if this is the only metadata block */
int technicallyflac_streaminfo(technicallyflac *f, uint8_t *output, uint32_t *bytes, uint8_t last_flag);

/* write out other metadata blocks, set last_flag to 1 on the final block */
int technicallyflac_metadata(technicallyflac *f, uint8_t *output, uint32_t *bytes, uint8_t last_flag, uint8_t block_type, uint32_t block_length, uint8_t *block);

/* write out a frame of audio. num_frames should be equal to your pre-configured block size, except for the last flac frame (where it may be less). */
int technicallyflac_frame(technicallyflac *f, uint8_t *output, uint32_t *bytes, uint32_t num_frames, int32_t **frames);

struct technicallyflac_streammarker_state {
    uint32_t pos;
};

struct technicallyflac_streaminfo_state {
    uint32_t pos;
};

struct technicallyflac_metadata_state {
    uint32_t pos;
    uint8_t flag;
};

struct technicallyflac_frame_header_state {
    uint32_t pos;
    uint32_t len;
    uint32_t frameindexlen;
    uint8_t frameindex[6];
};

struct technicallyflac_frame_state {
    uint32_t channel;
    uint32_t frame;
    uint32_t subframe_header;
    uint32_t footer;
};

struct technicallyflac_bitwriter_s {
    uint64_t val;
    uint8_t  bits;
    uint8_t  crc8;
    uint16_t crc16;
    uint32_t pos;
    uint32_t len;
    uint8_t* buffer;
};

struct technicallyflac_s {
    /* block size (number of audio frames in a block) */
    uint32_t blocksize;

    /* samplerate in Hz */
    uint32_t samplerate;

    /* total channels, 1-8 */
    uint8_t channels;

    /* bit depth 4 - 32 */
    uint8_t bitdepth;

    /* current audio frame being encoded */
    uint32_t frameindex;

    /* stored as header value (8 = 001, 16 = 100, etc) */
    uint8_t bitdepth_header;

    /* value to use for the frame header samplerate */
    uint8_t samplerate_header;

    /* value to use for the end-of-frame-header samplerate */
    uint16_t samplerate_value;

    /* sample size in bytes */
    uint8_t samplesize;

    struct technicallyflac_bitwriter_s bw;

    struct technicallyflac_streammarker_state sm_state;
    struct technicallyflac_streaminfo_state   si_state;
    struct technicallyflac_metadata_state     md_state;
    struct technicallyflac_frame_header_state fh_state;
    struct technicallyflac_frame_state        fr_state;
};


#ifdef __cplusplus
}
#endif

#endif

#ifdef TECHNICALLYFLAC_IMPLEMENTATION

static int technicallyflac_frame_header(technicallyflac *f, uint8_t *output, uint32_t *bytes, uint32_t num_frames);

typedef struct technicallyflac_bitwriter_s technicallyflac_bitwriter;

static void technicallyflac_bitwriter_init(technicallyflac_bitwriter *bw);
static void technicallyflac_bitwriter_add(technicallyflac_bitwriter *bw, uint8_t bits, uint64_t val);
static void technicallyflac_bitwriter_align(technicallyflac_bitwriter *bw);

static const uint8_t technicallyflac_crc8_table[256] = {
  0x00, 0x07, 0x0e, 0x09, 0x1c, 0x1b, 0x12, 0x15,
  0x38, 0x3f, 0x36, 0x31, 0x24, 0x23, 0x2a, 0x2d,
  0x70, 0x77, 0x7e, 0x79, 0x6c, 0x6b, 0x62, 0x65,
  0x48, 0x4f, 0x46, 0x41, 0x54, 0x53, 0x5a, 0x5d,
  0xe0, 0xe7, 0xee, 0xe9, 0xfc, 0xfb, 0xf2, 0xf5,
  0xd8, 0xdf, 0xd6, 0xd1, 0xc4, 0xc3, 0xca, 0xcd,
  0x90, 0x97, 0x9e, 0x99, 0x8c, 0x8b, 0x82, 0x85,
  0xa8, 0xaf, 0xa6, 0xa1, 0xb4, 0xb3, 0xba, 0xbd,
  0xc7, 0xc0, 0xc9, 0xce, 0xdb, 0xdc, 0xd5, 0xd2,
  0xff, 0xf8, 0xf1, 0xf6, 0xe3, 0xe4, 0xed, 0xea,
  0xb7, 0xb0, 0xb9, 0xbe, 0xab, 0xac, 0xa5, 0xa2,
  0x8f, 0x88, 0x81, 0x86, 0x93, 0x94, 0x9d, 0x9a,
  0x27, 0x20, 0x29, 0x2e, 0x3b, 0x3c, 0x35, 0x32,
  0x1f, 0x18, 0x11, 0x16, 0x03, 0x04, 0x0d, 0x0a,
  0x57, 0x50, 0x59, 0x5e, 0x4b, 0x4c, 0x45, 0x42,
  0x6f, 0x68, 0x61, 0x66, 0x73, 0x74, 0x7d, 0x7a,
  0x89, 0x8e, 0x87, 0x80, 0x95, 0x92, 0x9b, 0x9c,
  0xb1, 0xb6, 0xbf, 0xb8, 0xad, 0xaa, 0xa3, 0xa4,
  0xf9, 0xfe, 0xf7, 0xf0, 0xe5, 0xe2, 0xeb, 0xec,
  0xc1, 0xc6, 0xcf, 0xc8, 0xdd, 0xda, 0xd3, 0xd4,
  0x69, 0x6e, 0x67, 0x60, 0x75, 0x72, 0x7b, 0x7c,
  0x51, 0x56, 0x5f, 0x58, 0x4d, 0x4a, 0x43, 0x44,
  0x19, 0x1e, 0x17, 0x10, 0x05, 0x02, 0x0b, 0x0c,
  0x21, 0x26, 0x2f, 0x28, 0x3d, 0x3a, 0x33, 0x34,
  0x4e, 0x49, 0x40, 0x47, 0x52, 0x55, 0x5c, 0x5b,
  0x76, 0x71, 0x78, 0x7f, 0x6a, 0x6d, 0x64, 0x63,
  0x3e, 0x39, 0x30, 0x37, 0x22, 0x25, 0x2c, 0x2b,
  0x06, 0x01, 0x08, 0x0f, 0x1a, 0x1d, 0x14, 0x13,
  0xae, 0xa9, 0xa0, 0xa7, 0xb2, 0xb5, 0xbc, 0xbb,
  0x96, 0x91, 0x98, 0x9f, 0x8a, 0x8d, 0x84, 0x83,
  0xde, 0xd9, 0xd0, 0xd7, 0xc2, 0xc5, 0xcc, 0xcb,
  0xe6, 0xe1, 0xe8, 0xef, 0xfa, 0xfd, 0xf4, 0xf3,
};

static const uint16_t technicallyflac_crc16_table[256] = {
  0x0000, 0x8005, 0x800f, 0x000a, 0x801b, 0x001e, 0x0014, 0x8011,
  0x8033, 0x0036, 0x003c, 0x8039, 0x0028, 0x802d, 0x8027, 0x0022,
  0x8063, 0x0066, 0x006c, 0x8069, 0x0078, 0x807d, 0x8077, 0x0072,
  0x0050, 0x8055, 0x805f, 0x005a, 0x804b, 0x004e, 0x0044, 0x8041,
  0x80c3, 0x00c6, 0x00cc, 0x80c9, 0x00d8, 0x80dd, 0x80d7, 0x00d2,
  0x00f0, 0x80f5, 0x80ff, 0x00fa, 0x80eb, 0x00ee, 0x00e4, 0x80e1,
  0x00a0, 0x80a5, 0x80af, 0x00aa, 0x80bb, 0x00be, 0x00b4, 0x80b1,
  0x8093, 0x0096, 0x009c, 0x8099, 0x0088, 0x808d, 0x8087, 0x0082,
  0x8183, 0x0186, 0x018c, 0x8189, 0x0198, 0x819d, 0x8197, 0x0192,
  0x01b0, 0x81b5, 0x81bf, 0x01ba, 0x81ab, 0x01ae, 0x01a4, 0x81a1,
  0x01e0, 0x81e5, 0x81ef, 0x01ea, 0x81fb, 0x01fe, 0x01f4, 0x81f1,
  0x81d3, 0x01d6, 0x01dc, 0x81d9, 0x01c8, 0x81cd, 0x81c7, 0x01c2,
  0x0140, 0x8145, 0x814f, 0x014a, 0x815b, 0x015e, 0x0154, 0x8151,
  0x8173, 0x0176, 0x017c, 0x8179, 0x0168, 0x816d, 0x8167, 0x0162,
  0x8123, 0x0126, 0x012c, 0x8129, 0x0138, 0x813d, 0x8137, 0x0132,
  0x0110, 0x8115, 0x811f, 0x011a, 0x810b, 0x010e, 0x0104, 0x8101,
  0x8303, 0x0306, 0x030c, 0x8309, 0x0318, 0x831d, 0x8317, 0x0312,
  0x0330, 0x8335, 0x833f, 0x033a, 0x832b, 0x032e, 0x0324, 0x8321,
  0x0360, 0x8365, 0x836f, 0x036a, 0x837b, 0x037e, 0x0374, 0x8371,
  0x8353, 0x0356, 0x035c, 0x8359, 0x0348, 0x834d, 0x8347, 0x0342,
  0x03c0, 0x83c5, 0x83cf, 0x03ca, 0x83db, 0x03de, 0x03d4, 0x83d1,
  0x83f3, 0x03f6, 0x03fc, 0x83f9, 0x03e8, 0x83ed, 0x83e7, 0x03e2,
  0x83a3, 0x03a6, 0x03ac, 0x83a9, 0x03b8, 0x83bd, 0x83b7, 0x03b2,
  0x0390, 0x8395, 0x839f, 0x039a, 0x838b, 0x038e, 0x0384, 0x8381,
  0x0280, 0x8285, 0x828f, 0x028a, 0x829b, 0x029e, 0x0294, 0x8291,
  0x82b3, 0x02b6, 0x02bc, 0x82b9, 0x02a8, 0x82ad, 0x82a7, 0x02a2,
  0x82e3, 0x02e6, 0x02ec, 0x82e9, 0x02f8, 0x82fd, 0x82f7, 0x02f2,
  0x02d0, 0x82d5, 0x82df, 0x02da, 0x82cb, 0x02ce, 0x02c4, 0x82c1,
  0x8243, 0x0246, 0x024c, 0x8249, 0x0258, 0x825d, 0x8257, 0x0252,
  0x0270, 0x8275, 0x827f, 0x027a, 0x826b, 0x026e, 0x0264, 0x8261,
  0x0220, 0x8225, 0x822f, 0x022a, 0x823b, 0x023e, 0x0234, 0x8231,
  0x8213, 0x0216, 0x021c, 0x8219, 0x0208, 0x820d, 0x8207, 0x0202,
};

static void technicallyflac_bitwriter_init(technicallyflac_bitwriter *bw) {
    bw->val    = 0;
    bw->bits   = 0;
    bw->crc8   = 0;
    bw->crc16  = 0;
}

/* flush out any bits that we can */
static void technicallyflac_bitwriter_flush(technicallyflac_bitwriter *bw) {
    uint64_t avail = ((bw->len - bw->pos) * 8);
    uint64_t mask  = -1LL;
    uint8_t  byte  = 0;

    while(avail && bw->bits > 7) {
        bw->bits -= 8;
        byte = (uint8_t)((bw->val >> bw->bits) & 0xFF);
        bw->buffer[bw->pos++] = byte;
        avail -= 8;
        bw->crc8 = technicallyflac_crc8_table[bw->crc8 ^ byte];
        bw->crc16 = technicallyflac_crc16_table[(bw->crc16 >> 8) ^ byte] & (( bw->crc16 & 0x00FF ) << 8);
    }
    if(bw->bits == 0) {
        bw->val = 0;
    } else {
        mask >>= 64 - bw->bits;
        bw->val &= mask;
    }
}

/* assumption: user called flush and validated
 * they can store the available bits */
static void technicallyflac_bitwriter_add(technicallyflac_bitwriter *bw, uint8_t bits, uint64_t val) {
    uint64_t mask  = -1LL;
    assert(bw->bits + bits <= 64);
    mask >>= (64 - bits);
    bw->val <<= bits;
    bw->val |= val & mask;
    bw->bits += bits;
}

static void technicallyflac_bitwriter_align(technicallyflac_bitwriter *bw) {
    uint8_t r = bw->bits % 8;
    if(r) {
        technicallyflac_bitwriter_add(bw,8-r,0);
    }
}

size_t technicallyflac_size(void) {
    return sizeof(technicallyflac);
}

int technicallyflac_init(technicallyflac *f, uint32_t blocksize, uint32_t samplerate, uint8_t channels, uint8_t bitdepth) {

    f->blocksize  = blocksize;
    f->samplerate = samplerate;
    f->channels   = channels;
    f->bitdepth   = bitdepth;

    if(f->bitdepth < 4 || f->bitdepth > 32 || f->bitdepth % 2 != 0) {
        return -1;
    }

    switch(f->bitdepth) {
        case 8:  {
            f->bitdepth_header = 1;
            break;
        }
        case 12:  {
            f->bitdepth_header = 2;
            break;
        }
        case 16: {
            f->bitdepth_header = 4;
            break;
        }
        case 20: {
            f->bitdepth_header = 5;
            break;
        }
        case 24: {
            f->bitdepth_header = 6;
            break;
        }
        default: f->bitdepth_header = 0;
    }

    if(f->samplerate % 10 == 0) {
        f->samplerate_header = 14;
        f->samplerate_value  = f->samplerate / 10;
    } else {
        f->samplerate_header = 13;
        f->samplerate_value  = f->samplerate;
    }

    f->samplesize = f->bitdepth / 8;
    f->frameindex = 0;

    f->sm_state.pos     = 0;
    f->si_state.pos     = 0;
    f->md_state.pos     = 0;
    f->md_state.flag    = 0;
    f->fh_state.pos     = 0;
    f->fh_state.len     = 0;
    f->fr_state.channel = 0;
    f->fr_state.frame   = 0;
    f->fr_state.subframe_header = 0;
    f->fr_state.footer  = 0;

    return 0;
}

int technicallyflac_streammarker(technicallyflac *f, uint8_t *output, uint32_t *bytes) {
    if(output == NULL || *bytes == 0) {
        return 4;
    }

    f->bw.buffer = output;
    f->bw.len = *bytes;
    f->bw.pos = 0;

    if(f->sm_state.pos >= 4) {
        f->sm_state.pos = 0;
    }

    if(f->sm_state.pos == 0) {
        technicallyflac_bitwriter_init(&f->bw);
        technicallyflac_bitwriter_add(&f->bw,8,'f');
        technicallyflac_bitwriter_add(&f->bw,8,'L');
        technicallyflac_bitwriter_add(&f->bw,8,'a');
        technicallyflac_bitwriter_add(&f->bw,8,'C');
    }

    technicallyflac_bitwriter_flush(&f->bw);
    f->sm_state.pos += f->bw.pos;
    assert(f->bw.pos > 0);
    *bytes = f->bw.pos;

    return f->sm_state.pos < 4;
}

int technicallyflac_streaminfo(technicallyflac *f,uint8_t *output, uint32_t *bytes, uint8_t last_flag) {
    int r = 0;
    uint32_t pos = 0;

    if(output == NULL || *bytes == 0) {
        return TECHNICALLYFLAC_STREAMINFO_SIZE;
    }

    f->bw.buffer = output;
    f->bw.len = *bytes;
    f->bw.pos = 0;

    if(f->si_state.pos >= TECHNICALLYFLAC_STREAMINFO_SIZE) {
        f->si_state.pos = 0;
    }

    if(f->si_state.pos == 0) {
        technicallyflac_bitwriter_init(&f->bw);
    }

    while(f->bw.pos < f->bw.len) {
        technicallyflac_bitwriter_flush(&f->bw);
        f->si_state.pos += f->bw.pos - pos;
        pos = f->bw.pos;

        if(f->si_state.pos >= TECHNICALLYFLAC_STREAMINFO_SIZE) break;

        switch(f->si_state.pos) {
            case 0: {
                /* last-metadata-block-flag */
                technicallyflac_bitwriter_add(&f->bw,1,last_flag);
                /* BLOCK_TYPE */
                technicallyflac_bitwriter_add(&f->bw,7,0);
                break;
            }
            /* 8 bits written */
            case 1: {
                /* BLOCK_LENGTH */
                technicallyflac_bitwriter_add(&f->bw,24,34);
                break;
            }
            /* 32 bits written */
            case 4: {
                /* min block size */
                technicallyflac_bitwriter_add(&f->bw,16,f->blocksize);
                break;
            }
            /* 48 bits written */
            case 6: {
                /* max block size */
                technicallyflac_bitwriter_add(&f->bw,16,f->blocksize);
                break;
            }
            /* 64 bits written */
            case 8: {
                /* min frame size */
                technicallyflac_bitwriter_add(&f->bw,24,0);
                break;
            }
            /* 88 bits written */
            case 11: {
                /* max frame size */
                technicallyflac_bitwriter_add(&f->bw,24,0);
                break;
            }
            /* 112 bits written */
            case 14: {
                /* samplerate in Hz */
                technicallyflac_bitwriter_add(&f->bw,20,f->samplerate);
                break;
            }
            /* 132 bits */
            case 16: {
                /* channels */
                technicallyflac_bitwriter_add(&f->bw,3,f->channels - 1);
                /* bit depth */
                technicallyflac_bitwriter_add(&f->bw,5,f->bitdepth - 1);
                break;
            }
            /* 140 bits */
            case 17: {
                /* total samples */
                technicallyflac_bitwriter_add(&f->bw,36,0);
                break;
            }
            /* 176 bits */
            case 22: {
                /* MD5 part 1 */
                technicallyflac_bitwriter_add(&f->bw, 32, 0);
                break;
            }
            /* 208 bits */
            case 26: {
                /* MD5 part 2 */
                technicallyflac_bitwriter_add(&f->bw, 32, 0);
                break;
            }
            /* 240 bits */
            case 30: {
                /* MD5 part 3 */
                technicallyflac_bitwriter_add(&f->bw, 32, 0);
                break;
            }
            /* 272 bits */
            case 34: {
                /* MD5 part 4 */
                technicallyflac_bitwriter_add(&f->bw, 32, 0);
                break;
            }

            default: break;
        }
    }

    r = f->si_state.pos < TECHNICALLYFLAC_STREAMINFO_SIZE;
    if( r == 0 ) {
        assert(f->bw.bits == 0);
    }
    assert(f->bw.pos > 0);
    *bytes = f->bw.pos;
    return r;
}

int technicallyflac_metadata(technicallyflac *f, uint8_t *output, uint32_t *bytes, uint8_t last_flag, uint8_t block_type, uint32_t block_length, uint8_t *block) {
    int r;
    uint32_t pos = 0;
    f->bw.buffer = output;
    f->bw.len = *bytes;
    f->bw.pos = 0;

    if(output == NULL || *bytes == 0) {
        return 4 + block_length;
    }

    if(f->md_state.flag) {
        f->md_state.pos = 0;
        f->md_state.flag = 0;
    }

    if(f->md_state.pos == 0) {
        technicallyflac_bitwriter_init(&f->bw);
        technicallyflac_bitwriter_add(&f->bw,1,last_flag);
        technicallyflac_bitwriter_add(&f->bw,7,block_type);
        technicallyflac_bitwriter_add(&f->bw,24,block_length);
    }

    while(f->bw.pos < f->bw.len) {
        technicallyflac_bitwriter_flush(&f->bw);
        f->md_state.pos += f->bw.pos - pos;
        pos = f->bw.pos;

        if(f->md_state.pos >= block_length + 4) break;

        if(f->md_state.pos >= 4) {
            technicallyflac_bitwriter_add(&f->bw,8,block[f->md_state.pos - 4]);
        }
    }

    r = f->md_state.pos < block_length + 4;
    if(r == 0) {
        assert(f->bw.bits == 0);
        f->md_state.flag = 1;
    }

    assert(f->bw.pos > 0);
    *bytes = f->bw.pos;
    return r;
}


int technicallyflac_frame_header(technicallyflac *f, uint8_t *output, uint32_t *bytes, uint32_t num_frames) {
    uint32_t frameindex;
    int r = 0;
    uint32_t pos = 0;

    f->bw.buffer = output;
    f->bw.len = *bytes;
    f->bw.pos = 0;

    if(f->fh_state.pos >= f->fh_state.len) {
        f->fh_state.pos = 0;
    }

    if(f->fh_state.pos == 0) {
        frameindex = f->frameindex;
        if(output != NULL) {
            /* only get 31 usable bits for frameindex */
            f->frameindex++;
            if(f->frameindex > 0x7FFFFFFF) {
                f->frameindex -= 0x80000000;
            }
        }

        technicallyflac_bitwriter_init(&f->bw);
        f->fh_state.frameindexlen = 0;
        f->fh_state.frameindex[0] = 0;
        f->fh_state.frameindex[1] = 0;
        f->fh_state.frameindex[2] = 0;
        f->fh_state.frameindex[3] = 0;
        f->fh_state.frameindex[4] = 0;
        f->fh_state.frameindex[5] = 0;
        assert(frameindex < ((uint32_t)1 << 31));
        if(frameindex < ((uint32_t)1<<7)) {
            f->fh_state.frameindex[0] = frameindex;
            f->fh_state.frameindexlen = 1;
        } else if(frameindex < ((uint32_t)1<<11)) {
            f->fh_state.frameindex[0] = 0xC0 | ((frameindex >> 6 ) & 0x1F);
            f->fh_state.frameindex[1] = 0x80 | ((frameindex      ) & 0x3F);
            f->fh_state.frameindexlen = 2;
        } else if(frameindex < ((uint32_t)1<<16)) {
            f->fh_state.frameindex[0] = 0xE0 | ((frameindex >> 12) & 0x0F);
            f->fh_state.frameindex[1] = 0x80 | ((frameindex >> 6 ) & 0x3F);
            f->fh_state.frameindex[2] = 0x80 | ((frameindex      ) & 0x3F);
            f->fh_state.frameindexlen = 3;
        } else if(frameindex < ((uint32_t)1 << 21)) {
            f->fh_state.frameindex[0] = 0xF0 | ((frameindex >> 18) & 0x07);
            f->fh_state.frameindex[1] = 0x80 | ((frameindex >> 12) & 0x3F);
            f->fh_state.frameindex[2] = 0x80 | ((frameindex >>  6) & 0x3F);
            f->fh_state.frameindex[3] = 0x80 | ((frameindex      ) & 0x3F);
            f->fh_state.frameindexlen = 4;
        }
        else if(frameindex < ((uint32_t)1 << 26)) {
            f->fh_state.frameindex[0] = 0xF8 | ((frameindex >> 24) & 0x03);
            f->fh_state.frameindex[1] = 0x80 | ((frameindex >> 18) & 0x3F);
            f->fh_state.frameindex[2] = 0x80 | ((frameindex >> 12) & 0x3F);
            f->fh_state.frameindex[3] = 0x80 | ((frameindex >>  6) & 0x3F);
            f->fh_state.frameindex[4] = 0x80 | ((frameindex      ) & 0x3F);
            f->fh_state.frameindexlen = 5;
        }
        else if(frameindex < ((uint32_t)1 << 31)) {
            f->fh_state.frameindex[0] = 0xFC | ((frameindex >> 30) & 0x01);
            f->fh_state.frameindex[1] = 0x80 | ((frameindex >> 24) & 0x3F);
            f->fh_state.frameindex[2] = 0x80 | ((frameindex >> 18) & 0x3F);
            f->fh_state.frameindex[3] = 0x80 | ((frameindex >> 12) & 0x3F);
            f->fh_state.frameindex[4] = 0x80 | ((frameindex >>  6) & 0x3F);
            f->fh_state.frameindex[5] = 0x80 | ((frameindex      ) & 0x3F);
            f->fh_state.frameindexlen = 6;
        }
        f->fh_state.len = f->fh_state.frameindexlen + 9;
    }

    if(output == NULL || *bytes == 0) {
        return f->fh_state.len;
    }

    while(f->bw.pos < f->bw.len && f->fh_state.pos < f->fh_state.len) {
        technicallyflac_bitwriter_flush(&f->bw);
        f->fh_state.pos += f->bw.pos - pos;
        pos = f->bw.pos;

        if(f->fh_state.pos == 0) {
            /* frame header, sync code 11111111111110 */
            technicallyflac_bitwriter_add(&f->bw,14,0x3FFE);
            /* reserved bit */
            technicallyflac_bitwriter_add(&f->bw,1,0);

            /* blocking strategy (fixed_size) */
            technicallyflac_bitwriter_add(&f->bw,1,0);
        }
        else if(f->fh_state.pos == 2) {
            /* block size */
            technicallyflac_bitwriter_add(&f->bw,4,7);

            /* sample rate */
            technicallyflac_bitwriter_add(&f->bw,4,f->samplerate_header);
        }
        else if(f->fh_state.pos == 3) {
            /* channels */
            technicallyflac_bitwriter_add(&f->bw,4,f->channels - 1);

            /* samplesize */
            technicallyflac_bitwriter_add(&f->bw,3,f->bitdepth_header);

            /* reserved bit */
            technicallyflac_bitwriter_add(&f->bw,1,0);

        }
        else if(f->fh_state.pos > 3 && f->fh_state.pos < f->fh_state.frameindexlen + 4) {
            technicallyflac_bitwriter_add(&f->bw,8,f->fh_state.frameindex[f->fh_state.pos-4]);
        }
        else if(f->fh_state.pos == f->fh_state.frameindexlen + 4) {
            /* blocksize - 1 */
            technicallyflac_bitwriter_add(&f->bw,16,num_frames-1);
        }
        else if(f->fh_state.pos == f->fh_state.frameindexlen + 6) {
            technicallyflac_bitwriter_add(&f->bw,16,f->samplerate_value);
        }
        else if(f->fh_state.pos == f->fh_state.frameindexlen + 8) {
            technicallyflac_bitwriter_add(&f->bw,8,f->bw.crc8);
        }
    }
    r = f->fh_state.pos < f->fh_state.len;
    if( r == 0 ) {
        assert(f->bw.bits == 0);
        assert(f->fh_state.pos == 9 + f->fh_state.frameindexlen);
    }
    assert(f->bw.pos > 0);
    *bytes = f->bw.pos;
    return r;
}

int technicallyflac_frame(technicallyflac *f, uint8_t *output, uint32_t *bytes, uint32_t num_frames, int32_t **frames) {
    int r = 0;
    f->bw.buffer = output;
    f->bw.len = *bytes;
    f->bw.pos = 0;

    if(f->fr_state.channel == f->channels && f->fr_state.footer == 2) {
        f->fh_state.pos = 0;
        f->fh_state.len = 0;
        f->fr_state.subframe_header = 0;
        f->fr_state.channel = 0;
        f->fr_state.frame = 0;
        f->fr_state.footer = 0;
    }

    if(f->fh_state.len == 0 ||
       f->fh_state.pos < f->fh_state.len) {
        technicallyflac_frame_header(f,output,bytes,num_frames);
    }

    if(f->fh_state.pos < f->fh_state.len) {
        return 1;
    }

    while(f->bw.pos < f->bw.len) {
        if(f->fr_state.channel == f->channels && f->fr_state.footer == 2) {
            break;
        }
        technicallyflac_bitwriter_flush(&f->bw);
        if(f->bw.pos < f->bw.len && f->bw.bits > 0) {
            continue;
        }
        if(f->bw.pos == f->bw.len && f->bw.bits > 0) {
            break;
        }

        if(f->fr_state.frame == 0 && f->fr_state.subframe_header == 0) {
            technicallyflac_bitwriter_add(&f->bw,1,0); /* zero-bit padding */
            technicallyflac_bitwriter_add(&f->bw,6,1); /* frame type (VERBATIM) */
            technicallyflac_bitwriter_add(&f->bw,1,0); /* wasted bits */
            f->fr_state.subframe_header = 1;
        }
        else if(f->fr_state.channel < f->channels) {
            technicallyflac_bitwriter_add(&f->bw,f->bitdepth,frames[f->fr_state.channel][f->fr_state.frame++]);
            if(f->fr_state.frame == num_frames) {
                f->fr_state.channel++;
                f->fr_state.frame = 0;
                f->fr_state.subframe_header = 0;
            }
        }
        else if(f->fr_state.channel == f->channels && f->fr_state.footer == 0) {
            technicallyflac_bitwriter_align(&f->bw);
            technicallyflac_bitwriter_add(&f->bw,16,f->bw.crc16);
            f->fr_state.footer = 1;
        }
        else if(f->fr_state.footer == 1) {
            f->fr_state.footer = 2;
        }
    }


    r = f->fr_state.channel < f->channels || f->fr_state.footer < 2;
    if(r == 0) {
        assert(f->bw.bits == 0);
    }
    assert(f->bw.pos > 0);
    *bytes = f->bw.pos;
    return r;
}

#endif
