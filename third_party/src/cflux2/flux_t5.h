/*
 * T5 Text Encoder for FLUX.1
 *
 * Implements T5 (Text-to-Text Transfer Transformer) text encoder that produces
 * embeddings for FLUX.1 image generation.
 *
 * Note: This is a minimal implementation. Full T5 support requires
 * implementing the complete T5 architecture.
 */

#ifndef FLUX_T5_H
#define FLUX_T5_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ========================================================================
 * Architecture Constants (T5-XXL for FLUX.1)
 * ======================================================================== */

#define T5_HIDDEN_SIZE        4096
#define T5_INTERMEDIATE_SIZE  10240
#define T5_NUM_HEADS          32
#define T5_NUM_LAYERS         24
#define T5_VOCAB_SIZE         32128
#define T5_MAX_SEQ_LEN        512
#define T5_TEXT_DIM           4096  /* Single layer output for FLUX.1 */

/* ========================================================================
 * Forward Declarations
 * ======================================================================== */

typedef struct t5_model t5_model_t;
typedef struct t5_tokenizer t5_tokenizer_t;

/* ========================================================================
 * Tokenizer API
 * ======================================================================== */

/*
 * Load tokenizer from HuggingFace tokenizer directory.
 */
t5_tokenizer_t *t5_tokenizer_load(const char *tokenizer_dir);

/*
 * Free tokenizer resources.
 */
void t5_tokenizer_free(t5_tokenizer_t *tok);

/*
 * Tokenize text.
 * Returns token IDs array (caller must free).
 */
int *t5_tokenize(t5_tokenizer_t *tok, const char *text, int *num_tokens, int max_len);

/* ========================================================================
 * Combined Text Encoder API
 * ======================================================================== */

typedef struct t5_encoder {
    t5_tokenizer_t *tokenizer;
    t5_model_t *model;
} t5_encoder_t;

/*
 * Load complete text encoder (tokenizer + model).
 * model_dir should contain text_encoder/ and tokenizer/ subdirectories.
 * use_mmap: if true, use memory-mapped weights (saves memory, slower inference)
 */
t5_encoder_t *t5_encoder_load(const char *model_dir, int use_mmap);

/*
 * Free encoder resources.
 */
void t5_encoder_free(t5_encoder_t *enc);

/*
 * Encode text prompt to embeddings.
 * Returns: Embedding array [512, 4096] (caller must free)
 * 
 * Note: Currently returns dummy embeddings until full T5 implementation.
 */
float *t5_encode_text(t5_encoder_t *enc, const char *prompt);

#ifdef __cplusplus
}
#endif

#endif /* FLUX_T5_H */
