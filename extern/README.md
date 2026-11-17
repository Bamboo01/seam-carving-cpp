# Third-party code

## stb

- `stb/stb_image.h` — stb_image v2.32
- `stb/stb_image_write.h` — stb_image_write v1.16

Source: https://github.com/nothings/stb (unmodified copies of `master`, fetched 2026-11-17).

License: dual public domain / MIT. See the end of each header for the full text.

Both are single-header libraries. Only `src/core/image.cpp` defines the
implementation macros, so exactly one translation unit carries the ~350 KB of
compiled code; every other file just includes the declarations.
