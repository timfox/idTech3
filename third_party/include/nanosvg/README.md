# nanosvg - SVG Parser and Rasterizer

This directory should contain the nanosvg header files for SVG support.

## Download Instructions

nanosvg is a single-header SVG parser library. To enable SVG support:

1. Download nanosvg from: https://github.com/memononen/nanosvg

2. Copy the following files to this directory:
   - `src/nanosvg.h` → `libs/nanosvg/nanosvg.h`
   - `src/nanosvgrast.h` → `libs/nanosvg/nanosvgrast.h`

## Quick Setup

```bash
cd libs/nanosvg
git clone https://github.com/memononen/nanosvg.git temp
cp temp/src/nanosvg.h .
cp temp/src/nanosvgrast.h .
rm -rf temp
```

## License

nanosvg is licensed under the Zlib license. See the nanosvg repository for details.

