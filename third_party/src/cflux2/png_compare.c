#include "flux.h"
#include <stdio.h>
#include <string.h>

static int compare_images(const flux_image *a, const flux_image *b) {
    if (!a || !b) return -1;
    if (a->width != b->width || a->height != b->height || a->channels != b->channels) {
        fprintf(stderr, "image mismatch: %dx%d/%d vs %dx%d/%d\n",
                a->width, a->height, a->channels,
                b->width, b->height, b->channels);
        return 1;
    }

    size_t size = (size_t)a->width * (size_t)a->height * (size_t)a->channels;
    if (memcmp(a->data, b->data, size) != 0) {
        for (size_t i = 0; i < size; i++) {
            if (a->data[i] != b->data[i]) {
                fprintf(stderr, "pixel mismatch at byte %zu: %u vs %u\n",
                        i, a->data[i], b->data[i]);
                break;
            }
        }
        return 1;
    }

    return 0;
}

int main(int argc, char **argv) {
    if (argc != 3) {
        fprintf(stderr, "usage: %s <image_a.png> <image_b.png>\n", argv[0]);
        return 1;
    }

    flux_image *a = flux_image_load(argv[1]);
    if (!a) {
        fprintf(stderr, "failed to load: %s\n", argv[1]);
        return 1;
    }

    flux_image *b = flux_image_load(argv[2]);
    if (!b) {
        fprintf(stderr, "failed to load: %s\n", argv[2]);
        flux_image_free(a);
        return 1;
    }

    int result = compare_images(a, b);
    if (result == 0) {
        printf("images match: %dx%d (%d channels)\n", a->width, a->height, a->channels);
    }

    flux_image_free(a);
    flux_image_free(b);
    return result;
}
