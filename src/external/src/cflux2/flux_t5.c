/*
 * T5 Text Encoder Implementation for FLUX.1
 *
 * Wrapper functions that use the full T5 implementation.
 */

#include "flux_t5.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

/* Forward declarations from flux_t5_tokenizer.c and flux_t5_model.c */
extern t5_tokenizer_t *t5_tokenizer_load(const char *tokenizer_dir);
extern void t5_tokenizer_free(t5_tokenizer_t *tok);
extern int *t5_tokenize(t5_tokenizer_t *tok, const char *text, int *num_tokens, int max_len);
extern t5_model_t *t5_model_load(const char *model_dir, int use_mmap);
extern void t5_model_free(t5_model_t *model);
extern float *t5_forward(t5_model_t *model, const int *input_ids, int seq_len);

/*
 * Load complete T5 encoder
 */
t5_encoder_t *t5_encoder_load(const char *model_dir, int use_mmap) {
    t5_encoder_t *enc = calloc(1, sizeof(t5_encoder_t));
    if (!enc) return NULL;

    /* Load tokenizer */
    char tok_path[512];
    snprintf(tok_path, sizeof(tok_path), "%s/tokenizer", model_dir);
    enc->tokenizer = t5_tokenizer_load(tok_path);
    if (!enc->tokenizer) {
        fprintf(stderr, "t5_encoder_load: failed to load tokenizer\n");
        free(enc);
        return NULL;
    }

    /* Load model */
    char model_path[512];
    snprintf(model_path, sizeof(model_path), "%s/text_encoder", model_dir);
    enc->model = t5_model_load(model_path, use_mmap);
    if (!enc->model) {
        fprintf(stderr, "t5_encoder_load: failed to load model\n");
        t5_tokenizer_free(enc->tokenizer);
        free(enc);
        return NULL;
    }

    return enc;
}

/*
 * Free T5 encoder
 */
void t5_encoder_free(t5_encoder_t *enc) {
    if (!enc) return;
    if (enc->tokenizer) t5_tokenizer_free(enc->tokenizer);
    if (enc->model) t5_model_free(enc->model);
    free(enc);
}

/*
 * Encode text prompt to embeddings
 */
float *t5_encode_text(t5_encoder_t *enc, const char *prompt) {
    if (!enc || !prompt || !enc->tokenizer || !enc->model) return NULL;

    /* Tokenize text */
    int num_tokens = 0;
    int *token_ids = t5_tokenize(enc->tokenizer, prompt, &num_tokens, T5_MAX_SEQ_LEN);
    if (!token_ids || num_tokens == 0) {
        fprintf(stderr, "t5_encode_text: tokenization failed\n");
        return NULL;
    }

    /* Run forward pass */
    float *embeddings = t5_forward(enc->model, token_ids, num_tokens);
    free(token_ids);

    if (!embeddings) {
        fprintf(stderr, "t5_encode_text: forward pass failed\n");
        return NULL;
    }

    /* T5 outputs [seq_len, hidden_size], FLUX.1 expects [512, 4096] */
    /* Pad or truncate to T5_MAX_SEQ_LEN if needed */
    if (num_tokens != T5_MAX_SEQ_LEN) {
        float *padded = calloc(T5_MAX_SEQ_LEN * T5_TEXT_DIM, sizeof(float));
        if (padded) {
            int copy_len = (num_tokens < T5_MAX_SEQ_LEN) ? num_tokens : T5_MAX_SEQ_LEN;
            memcpy(padded, embeddings, copy_len * T5_TEXT_DIM * sizeof(float));
            free(embeddings);
            embeddings = padded;
        }
    }

    return embeddings;
}
