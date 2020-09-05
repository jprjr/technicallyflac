#include "example-shared.h"

#include <stdlib.h>
#include <string.h>

void
quit(int e, ...) {
    /* frees a bunch of stuff and exits */
    va_list ap;
    void *p = NULL;

    va_start(ap,e);
    do {
        p = va_arg(ap,void *);
        if(p != NULL) {
            free(p);
        }
    } while(p != NULL);

    EXAMPLE_EXIT(e);
}

int
write_buffer(uint8_t *bytes, uint32_t len, void *userdata) {
    membuffer *mem = (membuffer *)userdata;
    if(mem->pos + len > mem->len) return -1;
    memcpy(&mem->buf[mem->pos],bytes,len);
    mem->pos += len;
    return 0;
}

void
repack_samples_deinterleave(int32_t *d, int16_t *s, uint32_t channels, uint32_t num, uint8_t scale) {
    uint32_t i = 0;
    uint32_t c = 0;
    while(c < channels) {
        i = 0;
        while(i < num) {
            d[(c * num) + i] = (int16_t)(((uint16_t)s[(i*channels)+c]) >> scale);
            i++;
        }
        c++;
    }
}

void
repack_samples(int32_t *d, int16_t *s, uint32_t channels, uint32_t num, uint8_t scale) {
    uint32_t c = 0;
    while(num > 0) {
        c=0;
        while(c++ < channels) {
            *d++ = (int16_t)(((uint16_t)*s) >> scale);
            s++;
        }
        num--;
    }
}

void
pack_uint32le(uint8_t *d, uint32_t n) {
    d[0] = (uint8_t)(n       );
    d[1] = (uint8_t)(n >> 8  );
    d[2] = (uint8_t)(n >> 16 );
    d[3] = (uint8_t)(n >> 24 );
}

/* allocate and create a block of vorbis_comments */
uint8_t *
create_tags(uint32_t *outlen) {
    uint32_t len = 0;
    uint8_t *data;

    /* this is just hard-coded for this demo */
    /* vorbis_comments are:
     *   1. 32-bit, little-endian length for encoder name
     *   2. encoder name, string, not null terminated
     *   3. 32-bit, little-endian number of tags
     *   foreach tag:
     *     1. 32-bit, little-endian length of tag
     *     2. tag, string, not null terminated
     *
     * for this demo we'll just use:
     *   1. 4 bytes for encoder name length
     *   1. 15 bytes for "technicallyflac"
     *   1. 4 bytes for number of tags (just 2)
     *     1. 4 bytes for length of title tag
     *     2. 16 bytes for "TITLE=Demo Title"
     *     3. 4 bytes for length of artist tag
     *     4. 18 bytes for "ARTIST=Demo Artist"
     *   total is 65 bytes
     */

    data = (uint8_t *)malloc(65);

    pack_uint32le(&data[len],15);
    len += 4;

    memcpy(&data[len],"technicallyflac",15);
    len += 15;

    pack_uint32le(&data[len],2);
    len += 4;

    pack_uint32le(&data[len],16);
    len += 4;

    memcpy(&data[len],"TITLE=Demo Title",16);
    len += 16;

    pack_uint32le(&data[len],18);
    len += 4;

    memcpy(&data[len],"ARTIST=Demo Artist",18);
    len += 18;

    *outlen = len;
    return data;
}


