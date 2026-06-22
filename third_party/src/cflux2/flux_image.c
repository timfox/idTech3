/*
 * FLUX Image I/O Implementation
 *
 * Pure C implementation for reading and writing images.
 * Supports: PNG (read/write), PPM (read/write), JPEG (read only, basic)
 *
 * PNG implementation uses zlib-style deflate compression.
 */

#include "flux.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

/* ========================================================================
 * Image Creation and Management
 * ======================================================================== */

flux_image *flux_image_create(int width, int height, int channels) {
    flux_image *img = (flux_image *)malloc(sizeof(flux_image));
    if (!img) return NULL;

    img->width = width;
    img->height = height;
    img->channels = channels;
    img->data = (uint8_t *)calloc(width * height * channels, sizeof(uint8_t));

    if (!img->data) {
        free(img);
        return NULL;
    }

    return img;
}

void flux_image_free(flux_image *img) {
    if (img) {
        free(img->data);
        free(img);
    }
}

flux_image *flux_image_clone(const flux_image *img) {
    if (!img) return NULL;

    flux_image *clone = flux_image_create(img->width, img->height, img->channels);
    if (!clone) return NULL;

    memcpy(clone->data, img->data, img->width * img->height * img->channels);
    return clone;
}

/* ========================================================================
 * PPM/PGM Format (Simple, uncompressed)
 * ======================================================================== */

static flux_image *load_ppm(FILE *f) {
    char magic[3];
    int width, height, maxval;

    if (fscanf(f, "%2s", magic) != 1) return NULL;

    /* Skip comments */
    int c;
    while ((c = fgetc(f)) == '#') {
        while ((c = fgetc(f)) != '\n' && c != EOF);
    }
    ungetc(c, f);

    if (fscanf(f, "%d %d %d", &width, &height, &maxval) != 3) return NULL;
    fgetc(f);  /* Skip single whitespace after maxval */

    int channels;
    if (strcmp(magic, "P6") == 0) {
        channels = 3;  /* RGB */
    } else if (strcmp(magic, "P5") == 0) {
        channels = 1;  /* Grayscale */
    } else {
        return NULL;  /* Unsupported format */
    }

    flux_image *img = flux_image_create(width, height, channels);
    if (!img) return NULL;

    size_t size = width * height * channels;
    if (fread(img->data, 1, size, f) != size) {
        flux_image_free(img);
        return NULL;
    }

    return img;
}

static int save_ppm(const flux_image *img, FILE *f) {
    if (img->channels == 1) {
        fprintf(f, "P5\n");
    } else {
        fprintf(f, "P6\n");
    }
    fprintf(f, "%d %d\n255\n", img->width, img->height);

    /* If 4 channels (RGBA), write only RGB */
    if (img->channels == 4) {
        for (int y = 0; y < img->height; y++) {
            for (int x = 0; x < img->width; x++) {
                uint8_t *p = img->data + (y * img->width + x) * 4;
                fwrite(p, 1, 3, f);
            }
        }
    } else {
        size_t size = img->width * img->height * img->channels;
        if (fwrite(img->data, 1, size, f) != size) {
            return -1;
        }
    }

    return 0;
}

/* ========================================================================
 * PNG Format
 * ======================================================================== */

/* CRC32 table for PNG */
static uint32_t crc_table[256];
static int crc_table_computed = 0;

static void make_crc_table(void) {
    for (int n = 0; n < 256; n++) {
        uint32_t c = (uint32_t)n;
        for (int k = 0; k < 8; k++) {
            if (c & 1)
                c = 0xedb88320u ^ (c >> 1);
            else
                c = c >> 1;
        }
        crc_table[n] = c;
    }
    crc_table_computed = 1;
}

static uint32_t update_crc(uint32_t crc, const uint8_t *buf, size_t len) {
    if (!crc_table_computed) make_crc_table();

    uint32_t c = crc;
    for (size_t n = 0; n < len; n++) {
        c = crc_table[(c ^ buf[n]) & 0xff] ^ (c >> 8);
    }
    return c;
}

static uint32_t png_crc(const uint8_t *buf, size_t len) {
    return update_crc(0xffffffffu, buf, len) ^ 0xffffffffu;
}

/* Adler-32 checksum for zlib */
static uint32_t adler32(const uint8_t *data, size_t len) {
    uint32_t a = 1, b = 0;
    for (size_t i = 0; i < len; i++) {
        a = (a + data[i]) % 65521;
        b = (b + a) % 65521;
    }
    return (b << 16) | a;
}

/* Simple uncompressed zlib/deflate (store mode) */
static uint8_t *deflate_store(const uint8_t *data, size_t len, size_t *out_len) {
    /* Zlib header (2 bytes) + deflate blocks + adler32 (4 bytes) */
    size_t max_block = 65535;
    size_t num_blocks = (len + max_block - 1) / max_block;
    size_t total = 2 + num_blocks * 5 + len + 4;

    uint8_t *out = (uint8_t *)malloc(total);
    if (!out) return NULL;

    size_t pos = 0;

    /* Zlib header: CMF=0x78 (deflate, 32K window), FLG=0x01 (no dict, level 0) */
    out[pos++] = 0x78;
    out[pos++] = 0x01;

    /* Deflate stored blocks */
    size_t remaining = len;
    const uint8_t *src = data;
    while (remaining > 0) {
        size_t block_len = (remaining > max_block) ? max_block : remaining;
        int is_final = (remaining <= max_block) ? 1 : 0;

        /* Block header: BFINAL (1 bit) + BTYPE=00 (2 bits) = stored */
        out[pos++] = is_final;

        /* LEN and NLEN (little-endian) */
        out[pos++] = block_len & 0xff;
        out[pos++] = (block_len >> 8) & 0xff;
        out[pos++] = (~block_len) & 0xff;
        out[pos++] = ((~block_len) >> 8) & 0xff;

        memcpy(out + pos, src, block_len);
        pos += block_len;

        src += block_len;
        remaining -= block_len;
    }

    /* Adler-32 checksum (big-endian) */
    uint32_t checksum = adler32(data, len);
    out[pos++] = (checksum >> 24) & 0xff;
    out[pos++] = (checksum >> 16) & 0xff;
    out[pos++] = (checksum >> 8) & 0xff;
    out[pos++] = checksum & 0xff;

    *out_len = pos;
    return out;
}

/* Write PNG chunk */
static void write_png_chunk(FILE *f, const char *type, const uint8_t *data, size_t len) {
    /* Length (big-endian) */
    uint8_t len_bytes[4] = {
        (len >> 24) & 0xff,
        (len >> 16) & 0xff,
        (len >> 8) & 0xff,
        len & 0xff
    };
    fwrite(len_bytes, 1, 4, f);

    /* Type */
    fwrite(type, 1, 4, f);

    /* Data */
    if (len > 0 && data) {
        fwrite(data, 1, len, f);
    }

    /* CRC (over type + data) */
    uint8_t *crc_data = (uint8_t *)malloc(4 + len);
    memcpy(crc_data, type, 4);
    if (len > 0 && data) {
        memcpy(crc_data + 4, data, len);
    }
    uint32_t crc = png_crc(crc_data, 4 + len);
    free(crc_data);

    uint8_t crc_bytes[4] = {
        (crc >> 24) & 0xff,
        (crc >> 16) & 0xff,
        (crc >> 8) & 0xff,
        crc & 0xff
    };
    fwrite(crc_bytes, 1, 4, f);
}

/* Write PNG tEXt chunk (uncompressed text metadata) */
static void write_png_text_chunk(FILE *f, const char *keyword, const char *text) {
    size_t key_len = strlen(keyword);
    size_t text_len = strlen(text);
    size_t data_len = key_len + 1 + text_len;  /* keyword + null + text */

    uint8_t *data = (uint8_t *)malloc(data_len);
    if (!data) return;

    memcpy(data, keyword, key_len);
    data[key_len] = 0;  /* Null separator */
    memcpy(data + key_len + 1, text, text_len);

    write_png_chunk(f, "tEXt", data, data_len);
    free(data);
}

static int save_png_with_metadata(const flux_image *img, FILE *f, int64_t seed, int has_seed) {
    /* PNG signature */
    const uint8_t signature[8] = {0x89, 0x50, 0x4e, 0x47, 0x0d, 0x0a, 0x1a, 0x0a};
    fwrite(signature, 1, 8, f);

    /* IHDR chunk */
    uint8_t ihdr[13];
    ihdr[0] = (img->width >> 24) & 0xff;
    ihdr[1] = (img->width >> 16) & 0xff;
    ihdr[2] = (img->width >> 8) & 0xff;
    ihdr[3] = img->width & 0xff;
    ihdr[4] = (img->height >> 24) & 0xff;
    ihdr[5] = (img->height >> 16) & 0xff;
    ihdr[6] = (img->height >> 8) & 0xff;
    ihdr[7] = img->height & 0xff;
    ihdr[8] = 8;  /* Bit depth */
    ihdr[9] = (img->channels == 4) ? 6 : (img->channels == 3) ? 2 :
              (img->channels == 2) ? 4 : 0;  /* Color type */
    ihdr[10] = 0;  /* Compression */
    ihdr[11] = 0;  /* Filter */
    ihdr[12] = 0;  /* Interlace */

    write_png_chunk(f, "IHDR", ihdr, 13);

    /* Write metadata as tEXt chunks */
    if (has_seed) {
        char seed_str[32];
        snprintf(seed_str, sizeof(seed_str), "%lld", (long long)seed);
        write_png_text_chunk(f, "flux:seed", seed_str);
        write_png_text_chunk(f, "Software", "flux (https://github.com/antirez/flux-c)");
        write_png_text_chunk(f, "flux:model", "FLUX.2-klein-4B");
    }

    /* Prepare raw image data with filter bytes */
    int channels = img->channels;
    if (channels == 4) channels = 4;  /* RGBA */
    else if (channels >= 3) channels = 3;  /* RGB */
    else channels = 1;  /* Grayscale */

    size_t row_bytes = 1 + img->width * channels;  /* +1 for filter byte */
    size_t raw_len = img->height * row_bytes;
    uint8_t *raw = (uint8_t *)malloc(raw_len);

    for (int y = 0; y < img->height; y++) {
        raw[y * row_bytes] = 0;  /* Filter: None */
        for (int x = 0; x < img->width; x++) {
            const uint8_t *src = img->data + (y * img->width + x) * img->channels;
            uint8_t *dst = raw + y * row_bytes + 1 + x * channels;

            for (int c = 0; c < channels; c++) {
                if (c < img->channels) {
                    dst[c] = src[c];
                } else {
                    dst[c] = 255;  /* Alpha = 255 */
                }
            }
        }
    }

    /* Compress with zlib (store mode) */
    size_t compressed_len;
    uint8_t *compressed = deflate_store(raw, raw_len, &compressed_len);
    free(raw);

    if (!compressed) return -1;

    /* IDAT chunk */
    write_png_chunk(f, "IDAT", compressed, compressed_len);
    free(compressed);

    /* IEND chunk */
    write_png_chunk(f, "IEND", NULL, 0);

    return 0;
}

static int save_png(const flux_image *img, FILE *f) {
    return save_png_with_metadata(img, f, 0, 0);
}

/* Read 4-byte big-endian integer */
static uint32_t read_be32(FILE *f) {
    uint8_t buf[4];
    if (fread(buf, 1, 4, f) != 4) return 0;
    return ((uint32_t)buf[0] << 24) | ((uint32_t)buf[1] << 16) |
           ((uint32_t)buf[2] << 8) | buf[3];
}

typedef struct {
    const uint8_t *data;
    size_t len;
    size_t bytepos;
    uint32_t bitbuf;
    int bitcount;
} png_bitstream;

static int png_bitstream_fill(png_bitstream *bs, int n) {
    while (bs->bitcount < n && bs->bytepos < bs->len) {
        bs->bitbuf |= (uint32_t)bs->data[bs->bytepos++] << bs->bitcount;
        bs->bitcount += 8;
    }
    return bs->bitcount >= n;
}

static int png_bitstream_get(png_bitstream *bs, int n, uint32_t *out) {
    if (n == 0) {
        *out = 0;
        return 1;
    }
    if (!png_bitstream_fill(bs, n)) return 0;
    *out = bs->bitbuf & ((1u << n) - 1u);
    bs->bitbuf >>= n;
    bs->bitcount -= n;
    return 1;
}

static int png_bitstream_align(png_bitstream *bs) {
    uint32_t discard;
    int skip = bs->bitcount & 7;
    if (skip == 0) return 1;
    return png_bitstream_get(bs, skip, &discard);
}

static int png_bitstream_read_bytes(png_bitstream *bs, uint8_t *out, size_t len) {
    if (bs->bitcount == 0) {
        if (bs->bytepos + len > bs->len) return 0;
        memcpy(out, bs->data + bs->bytepos, len);
        bs->bytepos += len;
        return 1;
    }
    for (size_t i = 0; i < len; i++) {
        uint32_t v;
        if (!png_bitstream_get(bs, 8, &v)) return 0;
        out[i] = (uint8_t)v;
    }
    return 1;
}

#define PNG_MAXBITS 15

typedef struct {
    uint16_t count[PNG_MAXBITS + 1];
    uint16_t symbol[288];
} png_huffman;

static int png_huffman_build(png_huffman *h, const uint8_t *lengths, int n) {
    uint16_t offs[PNG_MAXBITS + 1];
    int left = 1;

    memset(h->count, 0, sizeof(h->count));
    for (int i = 0; i < n; i++) {
        if (lengths[i] > PNG_MAXBITS) return 0;
        h->count[lengths[i]]++;
    }

    for (int len = 1; len <= PNG_MAXBITS; len++) {
        left <<= 1;
        left -= h->count[len];
        if (left < 0) return 0;
    }

    offs[1] = 0;
    for (int len = 1; len < PNG_MAXBITS; len++) {
        offs[len + 1] = offs[len] + h->count[len];
    }

    for (int i = 0; i < n; i++) {
        int len = lengths[i];
        if (len) {
            h->symbol[offs[len]++] = (uint16_t)i;
        }
    }

    return 1;
}

static int png_huffman_decode(png_bitstream *bs, const png_huffman *h, int *symbol) {
    uint32_t code = 0;
    uint32_t first = 0;
    uint32_t index = 0;

    for (int len = 1; len <= PNG_MAXBITS; len++) {
        uint32_t bit;
        if (!png_bitstream_get(bs, 1, &bit)) return 0;
        code |= bit;
        uint32_t count = h->count[len];
        if (code < first + count) {
            *symbol = h->symbol[index + (code - first)];
            return 1;
        }
        index += count;
        first += count;
        first <<= 1;
        code <<= 1;
    }
    return 0;
}

static int png_build_fixed_huffman(png_huffman *litlen, png_huffman *dist) {
    uint8_t litlen_lengths[288];
    uint8_t dist_lengths[32];

    for (int i = 0; i <= 143; i++) litlen_lengths[i] = 8;
    for (int i = 144; i <= 255; i++) litlen_lengths[i] = 9;
    for (int i = 256; i <= 279; i++) litlen_lengths[i] = 7;
    for (int i = 280; i <= 287; i++) litlen_lengths[i] = 8;
    for (int i = 0; i < 32; i++) dist_lengths[i] = 5;

    if (!png_huffman_build(litlen, litlen_lengths, 288)) return 0;
    if (!png_huffman_build(dist, dist_lengths, 32)) return 0;
    return 1;
}

static int png_build_dynamic_huffman(png_bitstream *bs, png_huffman *litlen, png_huffman *dist) {
    static const uint8_t order[19] = {
        16, 17, 18, 0, 8, 7, 9, 6, 10, 5, 11, 4, 12, 3, 13, 2, 14, 1, 15
    };
    uint32_t hlit, hdist, hclen;
    uint8_t code_lengths[19] = {0};
    png_huffman code_huff;

    if (!png_bitstream_get(bs, 5, &hlit)) return 0;
    if (!png_bitstream_get(bs, 5, &hdist)) return 0;
    if (!png_bitstream_get(bs, 4, &hclen)) return 0;
    int nlen = (int)hlit + 257;
    int ndist = (int)hdist + 1;
    int ncode = (int)hclen + 4;

    if (nlen > 288 || ndist > 32) return 0;

    for (int i = 0; i < ncode; i++) {
        uint32_t v;
        if (!png_bitstream_get(bs, 3, &v)) return 0;
        code_lengths[order[i]] = (uint8_t)v;
    }

    if (!png_huffman_build(&code_huff, code_lengths, 19)) return 0;

    uint8_t lengths[320];
    int total = nlen + ndist;
    int i = 0;
    int prev = 0;

    while (i < total) {
        int sym;
        if (!png_huffman_decode(bs, &code_huff, &sym)) return 0;
        if (sym <= 15) {
            lengths[i++] = (uint8_t)sym;
            prev = sym;
        } else if (sym == 16) {
            uint32_t repeat;
            if (i == 0) return 0;
            if (!png_bitstream_get(bs, 2, &repeat)) return 0;
            repeat += 3;
            if (i + (int)repeat > total) return 0;
            for (uint32_t r = 0; r < repeat; r++) lengths[i++] = (uint8_t)prev;
        } else if (sym == 17) {
            uint32_t repeat;
            if (!png_bitstream_get(bs, 3, &repeat)) return 0;
            repeat += 3;
            if (i + (int)repeat > total) return 0;
            for (uint32_t r = 0; r < repeat; r++) lengths[i++] = 0;
            prev = 0;
        } else if (sym == 18) {
            uint32_t repeat;
            if (!png_bitstream_get(bs, 7, &repeat)) return 0;
            repeat += 11;
            if (i + (int)repeat > total) return 0;
            for (uint32_t r = 0; r < repeat; r++) lengths[i++] = 0;
            prev = 0;
        } else {
            return 0;
        }
    }

    if (!png_huffman_build(litlen, lengths, nlen)) return 0;
    if (!png_huffman_build(dist, lengths + nlen, ndist)) return 0;

    return 1;
}

/* Zlib inflate (stored, fixed, and dynamic blocks) */
static uint8_t *inflate_zlib(const uint8_t *data, size_t len, size_t expected_len) {
    if (len < 6) return NULL;

    uint8_t cmf = data[0];
    uint8_t flg = data[1];
    if ((cmf & 0x0f) != 8) return NULL;
    if (((cmf << 8) + flg) % 31 != 0) return NULL;

    size_t pos = 2;
    if (flg & 0x20) {
        if (len < 10) return NULL;
        pos += 4;
    }
    if (len < pos + 4) return NULL;

    size_t deflate_len = len - pos - 4;
    png_bitstream bs = {data + pos, deflate_len, 0, 0, 0};

    uint8_t *out = (uint8_t *)malloc(expected_len);
    if (!out) return NULL;
    size_t out_pos = 0;

    static const int len_base[29] = {
        3, 4, 5, 6, 7, 8, 9, 10, 11, 13,
        15, 17, 19, 23, 27, 31, 35, 43, 51, 59,
        67, 83, 99, 115, 131, 163, 195, 227, 258
    };
    static const int len_extra[29] = {
        0, 0, 0, 0, 0, 0, 0, 0, 1, 1,
        1, 1, 2, 2, 2, 2, 3, 3, 3, 3,
        4, 4, 4, 4, 5, 5, 5, 5, 0
    };
    static const int dist_base[30] = {
        1, 2, 3, 4, 5, 7, 9, 13, 17, 25,
        33, 49, 65, 97, 129, 193, 257, 385, 513, 769,
        1025, 1537, 2049, 3073, 4097, 6145, 8193, 12289, 16385, 24577
    };
    static const int dist_extra[30] = {
        0, 0, 0, 0, 1, 1, 2, 2, 3, 3,
        4, 4, 5, 5, 6, 6, 7, 7, 8, 8,
        9, 9, 10, 10, 11, 11, 12, 12, 13, 13
    };

    int final = 0;
    while (!final) {
        uint32_t bfinal, btype;
        if (!png_bitstream_get(&bs, 1, &bfinal)) goto fail;
        if (!png_bitstream_get(&bs, 2, &btype)) goto fail;
        final = (int)bfinal;

        if (btype == 0) {
            if (!png_bitstream_align(&bs)) goto fail;
            uint32_t stored_len, stored_nlen;
            if (!png_bitstream_get(&bs, 16, &stored_len)) goto fail;
            if (!png_bitstream_get(&bs, 16, &stored_nlen)) goto fail;
            if ((stored_len ^ 0xffffu) != stored_nlen) goto fail;
            if (out_pos + stored_len > expected_len) goto fail;
            if (!png_bitstream_read_bytes(&bs, out + out_pos, stored_len)) goto fail;
            out_pos += stored_len;
        } else if (btype == 1 || btype == 2) {
            png_huffman litlen, dist;
            if (btype == 1) {
                if (!png_build_fixed_huffman(&litlen, &dist)) goto fail;
            } else {
                if (!png_build_dynamic_huffman(&bs, &litlen, &dist)) goto fail;
            }

            for (;;) {
                int sym;
                if (!png_huffman_decode(&bs, &litlen, &sym)) goto fail;
                if (sym < 256) {
                    if (out_pos >= expected_len) goto fail;
                    out[out_pos++] = (uint8_t)sym;
                } else if (sym == 256) {
                    break;
                } else if (sym <= 285) {
                    int len_sym = sym - 257;
                    uint32_t extra, dist_extra_bits;
                    int dist_sym;
                    int length = len_base[len_sym];
                    if (len_extra[len_sym]) {
                        if (!png_bitstream_get(&bs, len_extra[len_sym], &extra)) goto fail;
                        length += (int)extra;
                    }
                    if (!png_huffman_decode(&bs, &dist, &dist_sym)) goto fail;
                    if (dist_sym >= 30) goto fail;
                    int distance = dist_base[dist_sym];
                    if (dist_extra[dist_sym]) {
                        if (!png_bitstream_get(&bs, dist_extra[dist_sym], &dist_extra_bits)) goto fail;
                        distance += (int)dist_extra_bits;
                    }
                    if (distance <= 0 || (size_t)distance > out_pos) goto fail;
                    if (out_pos + length > expected_len) goto fail;
                    for (int i = 0; i < length; i++) {
                        out[out_pos] = out[out_pos - distance];
                        out_pos++;
                    }
                } else {
                    goto fail;
                }
            }
        } else {
            goto fail;
        }
    }

    if (out_pos != expected_len) goto fail;

    uint32_t expected_adler = ((uint32_t)data[len - 4] << 24) |
                              ((uint32_t)data[len - 3] << 16) |
                              ((uint32_t)data[len - 2] << 8) |
                              (uint32_t)data[len - 1];
    if (adler32(out, expected_len) != expected_adler) goto fail;

    return out;

fail:
    free(out);
    return NULL;
}

/* Apply PNG filter to reconstructed row */
static void png_unfilter_row(uint8_t *row, const uint8_t *prev_row,
                             int filter, int width, int channels) {
    int bpp = channels;

    switch (filter) {
        case 0:  /* None */
            break;
        case 1:  /* Sub */
            for (int i = bpp; i < width * channels; i++) {
                row[i] = row[i] + row[i - bpp];
            }
            break;
        case 2:  /* Up */
            if (prev_row) {
                for (int i = 0; i < width * channels; i++) {
                    row[i] = row[i] + prev_row[i];
                }
            }
            break;
        case 3:  /* Average */
            for (int i = 0; i < width * channels; i++) {
                int a = (i >= bpp) ? row[i - bpp] : 0;
                int b = prev_row ? prev_row[i] : 0;
                row[i] = row[i] + (a + b) / 2;
            }
            break;
        case 4:  /* Paeth */
            for (int i = 0; i < width * channels; i++) {
                int a = (i >= bpp) ? row[i - bpp] : 0;
                int b = prev_row ? prev_row[i] : 0;
                int c = (prev_row && i >= bpp) ? prev_row[i - bpp] : 0;
                int p = a + b - c;
                int pa = abs(p - a);
                int pb = abs(p - b);
                int pc = abs(p - c);
                int pr = (pa <= pb && pa <= pc) ? a : (pb <= pc) ? b : c;
                row[i] = row[i] + pr;
            }
            break;
    }
}

static flux_image *load_png(FILE *f) {
    /* Verify signature */
    uint8_t sig[8];
    if (fread(sig, 1, 8, f) != 8) return NULL;

    const uint8_t expected[8] = {0x89, 0x50, 0x4e, 0x47, 0x0d, 0x0a, 0x1a, 0x0a};
    if (memcmp(sig, expected, 8) != 0) return NULL;

    int width = 0, height = 0, color_type = 0;
    uint8_t *idat_data = NULL;
    size_t idat_len = 0;

    /* Read chunks */
    while (1) {
        uint32_t chunk_len = read_be32(f);
        char chunk_type[5] = {0};
        if (fread(chunk_type, 1, 4, f) != 4) break;

        if (strcmp(chunk_type, "IHDR") == 0) {
            width = read_be32(f);
            height = read_be32(f);
            (void)fgetc(f);  /* bit_depth - skip */
            color_type = fgetc(f);
            fseek(f, 3, SEEK_CUR);  /* Skip compression, filter, interlace */
            fseek(f, 4, SEEK_CUR);  /* Skip CRC */
        } else if (strcmp(chunk_type, "IDAT") == 0) {
            /* Accumulate IDAT chunks */
            idat_data = (uint8_t *)realloc(idat_data, idat_len + chunk_len);
            if (fread(idat_data + idat_len, 1, chunk_len, f) != chunk_len) {
                free(idat_data);
                return NULL;
            }
            idat_len += chunk_len;
            fseek(f, 4, SEEK_CUR);  /* Skip CRC */
        } else if (strcmp(chunk_type, "IEND") == 0) {
            break;
        } else {
            /* Skip unknown chunk */
            fseek(f, chunk_len + 4, SEEK_CUR);
        }
    }

    if (width == 0 || height == 0 || !idat_data) {
        free(idat_data);
        return NULL;
    }

    /* Determine channels from color type */
    int channels;
    switch (color_type) {
        case 0: channels = 1; break;  /* Grayscale */
        case 2: channels = 3; break;  /* RGB */
        case 4: channels = 2; break;  /* Grayscale + Alpha */
        case 6: channels = 4; break;  /* RGBA */
        default:
            free(idat_data);
            return NULL;
    }

    /* Decompress */
    size_t raw_len = height * (1 + width * channels);
    uint8_t *raw = inflate_zlib(idat_data, idat_len, raw_len);
    free(idat_data);

    if (!raw) return NULL;

    /* Create image and apply filters */
    flux_image *img = flux_image_create(width, height, channels);
    if (!img) {
        free(raw);
        return NULL;
    }

    int row_bytes = 1 + width * channels;
    uint8_t *prev_row = NULL;

    for (int y = 0; y < height; y++) {
        uint8_t *row_data = raw + y * row_bytes;
        int filter = row_data[0];
        uint8_t *row = row_data + 1;

        png_unfilter_row(row, prev_row, filter, width, channels);

        memcpy(img->data + y * width * channels, row, width * channels);
        prev_row = row;
    }

    free(raw);
    return img;
}

/* ========================================================================
 * Public API
 * ======================================================================== */

static const char *get_extension(const char *path) {
    const char *dot = strrchr(path, '.');
    if (!dot || dot == path) return "";
    return dot + 1;
}

flux_image *flux_image_load(const char *path) {
    FILE *f = fopen(path, "rb");
    if (!f) return NULL;

    flux_image *img = NULL;
    const char *ext = get_extension(path);

    if (strcasecmp(ext, "png") == 0) {
        img = load_png(f);
    } else if (strcasecmp(ext, "ppm") == 0 || strcasecmp(ext, "pgm") == 0) {
        img = load_ppm(f);
    } else {
        /* Try to detect by magic bytes */
        uint8_t magic[8];
        if (fread(magic, 1, 8, f) == 8) {
            fseek(f, 0, SEEK_SET);
            if (magic[0] == 0x89 && magic[1] == 'P' && magic[2] == 'N' && magic[3] == 'G') {
                img = load_png(f);
            } else if (magic[0] == 'P' && (magic[1] == '5' || magic[1] == '6')) {
                img = load_ppm(f);
            }
        }
    }

    fclose(f);
    return img;
}

int flux_image_save(const flux_image *img, const char *path) {
    if (!img || !path) return -1;

    FILE *f = fopen(path, "wb");
    if (!f) return -1;

    int result;
    const char *ext = get_extension(path);

    if (strcasecmp(ext, "png") == 0) {
        result = save_png(img, f);
    } else if (strcasecmp(ext, "ppm") == 0 || strcasecmp(ext, "pgm") == 0) {
        result = save_ppm(img, f);
    } else {
        /* Default to PNG */
        result = save_png(img, f);
    }

    fclose(f);
    return result;
}

int flux_image_save_with_seed(const flux_image *img, const char *path, int64_t seed) {
    if (!img || !path) return -1;

    FILE *f = fopen(path, "wb");
    if (!f) return -1;

    int result;
    const char *ext = get_extension(path);

    if (strcasecmp(ext, "png") == 0) {
        result = save_png_with_metadata(img, f, seed, 1);
    } else if (strcasecmp(ext, "ppm") == 0 || strcasecmp(ext, "pgm") == 0) {
        /* PPM doesn't support metadata, just save normally */
        result = save_ppm(img, f);
    } else {
        /* Default to PNG with seed */
        result = save_png_with_metadata(img, f, seed, 1);
    }

    fclose(f);
    return result;
}

/* ========================================================================
 * Image Manipulation
 * ======================================================================== */

flux_image *flux_image_resize(const flux_image *img, int new_width, int new_height) {
    if (!img || new_width <= 0 || new_height <= 0) return NULL;

    flux_image *resized = flux_image_create(new_width, new_height, img->channels);
    if (!resized) return NULL;

    float scale_x = (float)img->width / new_width;
    float scale_y = (float)img->height / new_height;

    for (int y = 0; y < new_height; y++) {
        for (int x = 0; x < new_width; x++) {
            float src_x = (x + 0.5f) * scale_x - 0.5f;
            float src_y = (y + 0.5f) * scale_y - 0.5f;

            int x0 = (int)floorf(src_x);
            int y0 = (int)floorf(src_y);
            int x1 = x0 + 1;
            int y1 = y0 + 1;

            float wx = src_x - x0;
            float wy = src_y - y0;

            x0 = (x0 < 0) ? 0 : (x0 >= img->width) ? img->width - 1 : x0;
            x1 = (x1 < 0) ? 0 : (x1 >= img->width) ? img->width - 1 : x1;
            y0 = (y0 < 0) ? 0 : (y0 >= img->height) ? img->height - 1 : y0;
            y1 = (y1 < 0) ? 0 : (y1 >= img->height) ? img->height - 1 : y1;

            for (int c = 0; c < img->channels; c++) {
                float v00 = img->data[(y0 * img->width + x0) * img->channels + c];
                float v01 = img->data[(y0 * img->width + x1) * img->channels + c];
                float v10 = img->data[(y1 * img->width + x0) * img->channels + c];
                float v11 = img->data[(y1 * img->width + x1) * img->channels + c];

                float v = v00 * (1 - wx) * (1 - wy) +
                          v01 * wx * (1 - wy) +
                          v10 * (1 - wx) * wy +
                          v11 * wx * wy;

                resized->data[(y * new_width + x) * img->channels + c] = (uint8_t)(v + 0.5f);
            }
        }
    }

    return resized;
}

/* Convert image to specific number of channels */
flux_image *flux_image_convert(const flux_image *img, int new_channels) {
    if (!img || new_channels < 1 || new_channels > 4) return NULL;

    flux_image *converted = flux_image_create(img->width, img->height, new_channels);
    if (!converted) return NULL;

    for (int y = 0; y < img->height; y++) {
        for (int x = 0; x < img->width; x++) {
            const uint8_t *src = img->data + (y * img->width + x) * img->channels;
            uint8_t *dst = converted->data + (y * img->width + x) * new_channels;

            if (img->channels == 1 && new_channels >= 3) {
                /* Grayscale to RGB(A) */
                dst[0] = dst[1] = dst[2] = src[0];
                if (new_channels == 4) dst[3] = 255;
            } else if (img->channels >= 3 && new_channels == 1) {
                /* RGB to grayscale */
                dst[0] = (uint8_t)(0.299f * src[0] + 0.587f * src[1] + 0.114f * src[2]);
            } else {
                /* Copy available channels */
                for (int c = 0; c < new_channels; c++) {
                    if (c < img->channels) {
                        dst[c] = src[c];
                    } else {
                        dst[c] = 255;  /* Alpha or missing = 255 */
                    }
                }
            }
        }
    }

    return converted;
}

/* strcasecmp for portability */
#ifdef _WIN32
#define strcasecmp _stricmp
#endif
