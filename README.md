# QOI.H

QOI (Quite Okay Image) format implementation in C.
Inlcudes two (2) executables to convert from and to PNG.

## Quick start
### Build
```console
$ cc nob.c -o nob
```
### PNG to QOI
```console
$ ./build/png_to_qoi <input PNG image path> <output QOI image path>
```
### QOI to PNG
```console
$ ./build/qoi_to_png <input QOI image path> <output PNG image path>
```

## References
- [QOI offical site](https://qoiformat.org/)
- [QOI specification](https://qoiformat.org/qoi-specification.pdf)
- [QOI tests](https://qoiformat.org/qoi_test_images.zip)
- [QOI Github offical site](https://github.com/phoboslab/qoi)