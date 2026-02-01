#ifndef EMBCACHE_H
#define EMBCACHE_H

#include <stdint.h>
#include <stddef.h>

/* ========================================================================
 * Embedding Cache with 4-bit Quantization
 *
 * Caches text embeddings in 4-bit quantized form to save memory.
 * Used in CLI mode to avoid re-encoding when the same prompt is used.
 *
 * 4-bit quantization uses block-wise min/max scaling:
 * - Divide embedding into blocks of 32 values
 * - Store min/max (scale) per block
 * - Quantize each value to 4 bits (0-15)
 * - Pack two 4-bit values per byte
 *
 * Memory usage: ~2 MB per cached embedding (vs 15.7 MB uncompressed)
 * ======================================================================== */

#define EMB_QUANT_BLOCK_SIZE 32  /* Values per quantization block */

/* Quantized embedding structure */
typedef struct {
    uint8_t *data;       /* Packed 4-bit values (2 values per byte) */
    float *scales;       /* Scale (max-min) per block */
    float *offsets;      /* Offset (min) per block */
    int num_elements;    /* Total number of float values */
    int num_blocks;      /* Number of quantization blocks */
} emb_quantized_t;

/* Single cache entry */
typedef struct {
    char *prompt;              /* The prompt string (owned) */
    emb_quantized_t *emb;      /* Quantized embedding */
    uint64_t hash;             /* Hash of prompt for quick comparison */
} emb_cache_entry_t;

/* ========================================================================
 * Quantization Functions
 * ======================================================================== */

/* Quantize float embedding to 4-bit representation */
emb_quantized_t *emb_quantize_4bit(const float *data, int num_elements);

/* Dequantize back to float array (caller must free result) */
float *emb_dequantize_4bit(const emb_quantized_t *q);

/* Free quantized embedding */
void emb_quantized_free(emb_quantized_t *q);

/* ========================================================================
 * Cache Functions
 * ======================================================================== */

/* Initialize the embedding cache (call once at startup) */
void emb_cache_init(void);

/* Store embedding in cache for given prompt (takes ownership of embedding) */
void emb_cache_store(const char *prompt, const float *embedding, int num_elements);

/* Lookup embedding for prompt. Returns dequantized embedding if found, NULL otherwise.
 * Caller must free the returned embedding. */
float *emb_cache_lookup(const char *prompt);

/* Check if prompt is in cache without dequantizing */
int emb_cache_has(const char *prompt);

/* Clear the cache */
void emb_cache_clear(void);

/* Free all cache resources */
void emb_cache_free(void);

/* Get cache statistics */
void emb_cache_stats(int *num_entries, size_t *memory_used);

#endif /* EMBCACHE_H */
