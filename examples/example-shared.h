#ifndef EXAMPLE_SHARED_H
#define EXAMPLE_SHARED_H

#include <stdint.h>
#include <stdarg.h>

#if defined(_WIN32) || defined(_WIN64) || defined(_MSC_VER)
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN 1
#endif
#include <windows.h>
#define EXAMPLE_EXIT(e) ExitProcess(e)
#else
#include <unistd.h>
#define EXAMPLE_EXIT(e) exit(e)
#endif

/* shared struct defs, function headers, etc for the flac and ogg examples */

struct membuffer_s {
    uint8_t *buf;
    uint32_t pos;
    uint32_t len;
};

typedef struct membuffer_s membuffer;

#ifdef __cplusplus
extern "C" {
#endif

int
write_buffer(uint8_t *bytes, uint32_t len, void *userdata);

void
repack_samples_deinterleave(int32_t **d, int16_t *s, uint32_t channels, uint32_t num, uint8_t scale);

void
repack_samples(int32_t *d, int16_t *s, uint32_t channels, uint32_t num, uint8_t scale);

void
pack_uint32le(uint8_t *d, uint32_t n);

uint8_t *
create_tags(uint32_t *outlen);

void
quit(int e, ...);

#ifdef __cplusplus
}
#endif

#endif
