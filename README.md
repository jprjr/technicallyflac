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

technicallyflac f;
f.samplerate = 44100;
f.bitdepth = 16;
f.channels = 2;
f.blocksize = 1024;
f.write = my_write_callback;
f.userdata = my_userdata;

technicallyflac_init(&f);

// write out "fLaC" to your stream with fwrite or something

technicallyflac_streaminfo(&f,1);

for(i=0;i<blocks_of_audio;i++) {
    technicallyflac_encode(&f,blocks[i].data,blocks[i].size);
}
```

## LICENSE

BSD Zero Clause (see the `LICENSE` file).
