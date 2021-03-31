# technicallyflac

A single-file C library for creating FLAC streams. Does not use any C library functions,
does not allocate any heap memory.

The streams are not compressed, hence the name "technically" FLAC.

Use case: you want to store/stream audio in a format that
supports tags, embedded art, etc and don't care about
space savings.

## Usage

In one C file define `TECHNICALLYFLAC_IMPLEMENTATION` before including `technicallyflac.h`.

`libflac` and `libflake` both handle all the metadata-writing automatically, this library
gives a bit more control over when/where data is written. You should be familiar with
the [FLAC format](https://xiph.org/flac/format.html).

See `technicallyflac.h` for details on how to use the library, also see `examples/example-flac.c` and `examples/example-ogg.c`. Most applications will probably do something like:

```C

#define BUFFER_LEN 8192 /* some buffer length */
technicallyflac f;
uint8_t buffer[BUFFER_LEN];
uint32_t bufferlen = BUFFER_LEN;

technicallyflac_init(&f, 1024, 44100, 2, 16); /* validates parameters */

technicallyflac_streammarker(&f,buffer,&bufferlen); /* writes the string "fLaC" into buffer */
fwrite(buffer,1,bufferlen,output);

bufferlen = BUFFER_LEN;
technicallyflac_streaminfo(&f,buffer,&bufferlen,1); /* writes out the streaminfo block */
fwrite(buffer,1,bufferlen,output);

for(i=0;i<blocks_of_audio;i++) {
    bufferlen = BUFFER_LEN;
    technicallyflac_frame(&f,buffer,&bufferlen,blocks[i].size,blocks[i].data);
    fwrite(buffer,1,bufferlen,output);
}
```

## LICENSE

BSD Zero Clause (see the `LICENSE` file).
