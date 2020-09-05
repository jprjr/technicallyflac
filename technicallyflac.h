#ifndef TECHNICALLYFLAC_H
#define TECHNICALLYFLAC_H

#include <stdint.h>
#include <stddef.h>


/* by default, an internal 2048-byte buffer
 * is used for the bitstream writer (it won't
 * call your write callback until this is full
 * or asked to flush specifically).
 * This buffer is ultimately stack-allocated, so
 * if it's too large just define a new, smaller
 * buffer size.
 */

#ifndef TECHNICALLYFLAC_BUFFER_SIZE
#define TECHNICALLYFLAC_BUFFER_SIZE 2048
#endif

/* in case you want to statically allocate a buffer
 * for storing the STREAMINFO metadata block - this
 * is the number of bytes you'll need */
#define TECHNICALLYFLAC_STREAMINFO_SIZE 38


/* callback used to write out bytes
 * should return a 0 on success
 *
 * Technically any non-zero is a failure, but you
 * should use a negative number - this return value is
 * bubbled up back to your application's call to technicallyflac_encode,
 * and technicallyflac_encode returns positive values to indicate
 * # of bytes needed/used.
 *
 * If your callback returns a negative value, you can catch that in your
 * application and report the kind of error.
 */

typedef int (*technicallyflac_write_callback)(uint8_t *bytes, uint32_t len, void *userdata);

/* struct for the "flac" encoder */
struct technicallyflac_s {

    /* the following fields need to be set by the user:
     *
     * blocksize
     * samplerate
     * channels
     * bitdepth
     * write
     * userdata
     *
     * everything else is set by calling technicallyflac_init */

    /* block size (number of audio frames in a block) */
    uint32_t blocksize;

    /* samplerate in Hz */
    uint32_t samplerate;

    /* total channels, 1-8 */
    uint8_t channels;

    /* bit depth 4 - 32 */
    uint8_t bitdepth;

    /* write callback for encoding, writing streaminfo, etc */
    technicallyflac_write_callback write;

    /* userdata for write callback */
    void *userdata;

    /* the follwing will be overwritten so don't worry about setting them */

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
};
typedef struct technicallyflac_s technicallyflac;

/* struct for buffering and writing out bits
 * used internally, but available in your application too
 * for creating metadata blocks */
struct technicallyflac_bitwriter_s {
    uint64_t val;
    uint8_t bits;
    uint8_t crc8;
    uint16_t crc16;
    uint32_t pos;
    uint32_t len;
    uint8_t buffer[TECHNICALLYFLAC_BUFFER_SIZE];
    technicallyflac_write_callback write;
    void *userdata;
};
typedef struct technicallyflac_bitwriter_s technicallyflac_bitwriter;

#ifdef __cplusplus
extern "C" {
#endif

/* BEGIN FLAC ENCODER FUNCTIONS */

/* returns number of bytes necessary to encode a block of audio given
 * a number of channels, bit depth, and number of audio frames */
uint32_t
technicallyflac_frame_size(uint8_t channels, uint8_t bitdepth, uint32_t num_frames);


/* initialize the flac encoder, validate settings */
/* returns 0 on success */
int
technicallyflac_init(technicallyflac *f);

/* writes out the STREAMINFO metadata block (including header)
 * you can pass NULL to check number of bytes needed at runtime,
 * otherwise use TECHNICALLYFLAC_STREAMINFO_SIZE.
 * last_flag should be true if this is your only metadata block.
 */
int
technicallyflac_streaminfo(technicallyflac *f, uint8_t last_flag);

/* technicallyflac only has support for the STREAMINFO metadata block,
 * since that's a required block. All other blocks are up to the application
 * to implement.
 * This function just writes out the METADATA_BLOCK_HEADER and your data.
 * See https://xiph.org/flac/format.html#metadata_block_header for information on the
 * block header, and information on the different metadata blocks
 */
int
technicallyflac_metadata_block(technicallyflac *f, uint8_t lastflag, uint8_t block_type, const uint8_t *data, uint32_t length);

/* encode num_frames worth of audio frames.
 * the frames buffer is expected to be interleaved.
 * returns number of bytes written out, including block headers, frame headers, etc
 * num_frames should be equal to block size until final block (where it can be less) */
int
technicallyflac_encode_interleaved(technicallyflac *f, const int32_t *frames, uint32_t num_frames);

/* encode num_frames worth of audio frames.
 * the frames buffer is expected to be planar.
 * returns number of bytes written out, including block headers, frame headers, etc
 * num_frames should be equal to block size until final block (where it can be less) */
int
technicallyflac_encode_planar(technicallyflac *f, const int32_t *frames, uint32_t num_frames);

/* END FLAC ENCODER FUNCTIONS */

/* BEGIN BITWRITER FUNCTIONS */

/* initialize a bitwriter */
void
technicallyflac_bitwriter_init(technicallyflac_bitwriter *, technicallyflac_write_callback, void *);

/* "write" some bits into the bitwriter */
/* returns 0 on success */
int
technicallyflac_bitwriter_write(technicallyflac_bitwriter *, uint8_t bits, uint64_t val);

/* ensure the bitwriter is byte-aligned */
int
technicallyflac_bitwriter_align(technicallyflac_bitwriter *);

/* flush any data in the buffer (manually calls the write callback) */
/* returns 0 on success */
int
technicallyflac_bitwriter_flush(technicallyflac_bitwriter *);

/* END BITWRITER FUNCTIONS */

#ifdef __cplusplus
}
#endif

#endif

/* end header, begin implementation */

#ifdef TECHNICALLYFLAC_IMPLEMENTATION

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

void
technicallyflac_bitwriter_init(technicallyflac_bitwriter *bw, technicallyflac_write_callback w, void *userdata) {
    bw->val      = 0;
    bw->bits     = 0;
    bw->crc8     = 0;
    bw->crc16    = 0;
    bw->pos      = 0;
    bw->len      = TECHNICALLYFLAC_BUFFER_SIZE;
    bw->write    = w;
    bw->userdata = userdata;
}

int
technicallyflac_bitwriter_flush(technicallyflac_bitwriter *bw) {
    int r = bw->write(bw->buffer,bw->pos,bw->userdata);
    bw->pos = 0;
    return r;
}

int
technicallyflac_bitwriter_align(technicallyflac_bitwriter *bw) {
    uint8_t bits = (uint8_t)((-1 * ((int)bw->bits)) % 8);
    return bits == 0 ? 0 : technicallyflac_bitwriter_write(bw, bits, 0);
}

int
technicallyflac_bitwriter_write(technicallyflac_bitwriter *bw, uint8_t bits, uint64_t val) {
    uint8_t byte = 0;
    uint64_t mask  = -1LL;
    uint64_t imask = -1LL;
    int r = 0;

    if(bits == 64) {
        bw->val = val;
    } else {
        mask >>= (64 - bits);
        bw->val <<= bits;
        bw->val |= val & mask;
    }
    bw->bits += bits;

    while(bw->bits >= 8) {
        bw->bits -= 8;
        byte = (uint8_t)((bw->val >> bw->bits) & 0xFF);
        if(bw->pos == bw->len) {
            if(( r = technicallyflac_bitwriter_flush(bw)) != 0) return r;
        }
        bw->buffer[bw->pos++] = byte;
        bw->crc8  = technicallyflac_crc8_table[bw->crc8 ^ byte];
        bw->crc16 = technicallyflac_crc16_table[(bw->crc16 >> 8) ^ byte] ^ (( bw->crc16 & 0x00FF) << 8);
    }

    if(bw->bits == 0) {
        bw->val = 0;
    } else {
        imask >>= 64 - bw->bits;
        bw->val &= imask;
    }

    return 0;
}

int
technicallyflac_init(technicallyflac *f) {
    /* TODO:
     * is it possible to have odd bit-depths (7-bit, 9-bit, etc?)
     */

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

    return 0;
}

int
technicallyflac_metadata_block(technicallyflac *f, uint8_t lastflag, uint8_t block_type, const uint8_t *data, uint32_t length) {
    int r;
    technicallyflac_bitwriter bw;
    uint32_t n;

    if(f == NULL) return length + 4;

    technicallyflac_bitwriter_init(&bw,f->write,f->userdata);

    /* METADATA_BLOCK_HEADER */
    /* last-metadata-block-flag */
    if((r = technicallyflac_bitwriter_write(&bw,1,lastflag)) != 0) return r;

    /* BLOCK_TYPE */
    if((r = technicallyflac_bitwriter_write(&bw,7,block_type)) != 0) return r;

    /* BLOCK_LENGTH */
    if((r = technicallyflac_bitwriter_write(&bw,24,length)) != 0) return r;

    /* METADATA_BLOCK_DATA */
    n = length;
    while(n > 0) {
        if(( r = technicallyflac_bitwriter_write(&bw,8,*data++)) != 0) return r;
        n--;
    }
    if((r = technicallyflac_bitwriter_align(&bw)) != 0) return r;
    if((r = technicallyflac_bitwriter_flush(&bw)) != 0) return r;

    return length + 4;
}


/* bit of duplication here, STREAMINFO is required, want to make it
 * easy for the user. For other metadata, I expect app to prepare an array
 * of bytes and use the above method */
int
technicallyflac_streaminfo(technicallyflac *f, uint8_t lastflag) {
    int r;
    technicallyflac_bitwriter bw;

    if(f == NULL) return TECHNICALLYFLAC_STREAMINFO_SIZE;

    technicallyflac_bitwriter_init(&bw,f->write,f->userdata);

    /* METADATA_BLOCK_HEADER */
    /* last-metadata-block-flag */
    if((r = technicallyflac_bitwriter_write(&bw,1,lastflag)) != 0) return r;

    /* BLOCK_TYPE */
    if((r = technicallyflac_bitwriter_write(&bw,7,0)) != 0) return r;

    /* BLOCK_LENGTH */
    if((r = technicallyflac_bitwriter_write(&bw,24,34)) != 0) return r;

    /* min block size */
    if((r = technicallyflac_bitwriter_write(&bw,16,f->blocksize)) != 0) return r;

    /* max block size */
    if((r = technicallyflac_bitwriter_write(&bw,16,f->blocksize)) != 0) return r;

    /* min frame size */
    if((r = technicallyflac_bitwriter_write(&bw,24,0)) != 0) return r;

    /* max frame size */
    if((r = technicallyflac_bitwriter_write(&bw,24,0)) != 0) return r;

    /* samplerate in Hz */
    if((r = technicallyflac_bitwriter_write(&bw,20,f->samplerate)) != 0) return r;

    /* (number of channels) - 1 */
    if((r = technicallyflac_bitwriter_write(&bw,3,f->channels - 1)) != 0) return r;

    /* (bits per sample) - 1 */
    if((r = technicallyflac_bitwriter_write(&bw,5,f->bitdepth - 1)) != 0) return r;

    /* (total samples) */
    if((r = technicallyflac_bitwriter_write(&bw,36,0)) != 0) return r;

    /* MD5 signature */
    if((r = technicallyflac_bitwriter_write(&bw,32,0)) != 0) return r;
    if((r = technicallyflac_bitwriter_write(&bw,32,0)) != 0) return r;
    if((r = technicallyflac_bitwriter_write(&bw,32,0)) != 0) return r;
    if((r = technicallyflac_bitwriter_write(&bw,32,0)) != 0) return r;

    if((r = technicallyflac_bitwriter_align(&bw)) != 0) return r;
    if((r = technicallyflac_bitwriter_flush(&bw)) != 0) return r;

    return TECHNICALLYFLAC_STREAMINFO_SIZE;
}

uint32_t
technicallyflac_frame_size(uint8_t channels, uint8_t bitdepth, uint32_t num_frames) {
    /* frame size is:
     *   number of bytes needed to encode PCM audio (sample size * channels * samples)
     *   + 1 byte per channel (for the subframe headers)
     *   + 17 bytes of frame headers/footers
     */
    return    ((num_frames * ((uint32_t)channels) * ((uint32_t)bitdepth)) / 8)
            + ((uint32_t)channels)
            + 17;
}

static int
technicallyflac_header(technicallyflac *f, technicallyflac_bitwriter *bw, uint32_t num_frames) {
    uint32_t frameindex;
    int bits = 24;
    int r = 0;

    frameindex = f->frameindex++;
    /* only get 31 usable bits for frameindex */
    if(f->frameindex == 0x80000000) {
        f->frameindex = 0;
    }

    technicallyflac_bitwriter_init(bw,f->write,f->userdata);

    /* frame header, sync code 11111111111110 */
    if((r = technicallyflac_bitwriter_write(bw,14,0x3FFE)) != 0) return r;

    /* reserved bit */
    if((r = technicallyflac_bitwriter_write(bw,1,0)) != 0) return r;

    /* blocking strategy (fixed-size) */
    if((r = technicallyflac_bitwriter_write(bw,1,0)) != 0) return r;

    /* block size (7 = get 16-bit from end of header */
    if((r = technicallyflac_bitwriter_write(bw,4, 7)) != 0) return r;

    /* samplerate (get from end of header) */
    if((r = technicallyflac_bitwriter_write(bw,4,f->samplerate_header)) != 0) return r;

    /* channels */
    if((r = technicallyflac_bitwriter_write(bw,4,f->channels - 1)) != 0) return r;

    /* samplesize */
    if((r = technicallyflac_bitwriter_write(bw,3,f->bitdepth_header)) != 0) return r;

    /* reserved bit */
    if((r = technicallyflac_bitwriter_write(bw,1, 0)) != 0) return r;

    /* first byte of frameindex */
    /* see https://stackoverflow.com/questions/53267434/cant-understand-flac-frame-header-format */
    /* just going to always use 48 bits */
    if((r = technicallyflac_bitwriter_write(bw,8, 0xFC | ((frameindex >> 30)))) != 0) return r;
    while(bits >= 0) {
        if((r = technicallyflac_bitwriter_write(bw, 8, 0x80 | ((frameindex >> bits) & 0x3F))) != 0) return r;
        bits -= 6;
    }
    if((r = technicallyflac_bitwriter_write(bw,16,num_frames-1)) != 0) return r;
    if((r = technicallyflac_bitwriter_write(bw,16,f->samplerate_value)) != 0) return r;
    if((r = technicallyflac_bitwriter_write(bw,8,bw->crc8)) != 0) return r;

    return 0;

}

int
technicallyflac_encode_planar(technicallyflac *f, const int32_t *frames, uint32_t num_frames) {
    uint32_t i;
    uint32_t c;
    technicallyflac_bitwriter bw;

    int r = 0;

    if(( r = technicallyflac_header(f,&bw,num_frames)) != 0) {
        return r;
    }

    c = f->channels;
    while(c--) {

        /* subframe zero-bit padding */
        if((r = technicallyflac_bitwriter_write(&bw,1,0)) != 0) return r;

        /* subframe type (verbatim) */
        if(( r = technicallyflac_bitwriter_write(&bw,6,1)) != 0) return r;

        /* wasted bits flag */
        if(( r = technicallyflac_bitwriter_write(&bw,1,0)) != 0) return r;

        i = num_frames;
        while(i--) {
            if((r = technicallyflac_bitwriter_write(&bw,f->bitdepth,*frames++)) != 0) return r;
        }
    }

    if(( r = technicallyflac_bitwriter_align(&bw)) != 0) return r;
    if(( r = technicallyflac_bitwriter_write(&bw,16,bw.crc16)) != 0) return r;
    if(( r = technicallyflac_bitwriter_flush(&bw)) != 0) return r;

    return technicallyflac_frame_size(f->channels, f->bitdepth, num_frames);
}

int
technicallyflac_encode_interleaved(technicallyflac *f, const int32_t *frames, uint32_t num_frames) {
    uint32_t i;
    uint32_t c;
    technicallyflac_bitwriter bw;

    int r = 0;

    if(( r = technicallyflac_header(f,&bw,num_frames)) != 0) {
        return r;
    }

    /* need to convert interleaved to planar */
    for(c=0;c<f->channels;c++) {
        /* subframe zero-bit padding */
        if((r = technicallyflac_bitwriter_write(&bw,1,0)) != 0) return r;

        /* subframe type (verbatim) */
        if(( r = technicallyflac_bitwriter_write(&bw,6,1)) != 0) return r;

        /* wasted bits flag */
        if(( r = technicallyflac_bitwriter_write(&bw,1,0)) != 0) return r;

        for(i=0;i<num_frames;i++) {
            if((r = technicallyflac_bitwriter_write(&bw,f->bitdepth,frames[(i*f->channels) + c])) != 0) return r;
        }
    }
    if(( r = technicallyflac_bitwriter_align(&bw)) != 0) return r;
    if(( r = technicallyflac_bitwriter_write(&bw,16,bw.crc16)) != 0) return r;
    if(( r = technicallyflac_bitwriter_flush(&bw)) != 0) return r;

    return technicallyflac_frame_size(f->channels, f->bitdepth, num_frames);
}

#endif
