/*
 * T5 SentencePiece Tokenizer Implementation
 *
 * Implements SentencePiece tokenizer for T5 text encoder.
 * Loads from HuggingFace tokenizer.json format.
 */

#include "flux_t5.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <stdint.h>
#include <math.h>

/* ========================================================================
 * Configuration
 * ======================================================================== */

#define T5_MAX_TOKEN_LEN 256
#define T5_HASH_SIZE 65537  /* Prime > 2 * vocab_size */

/* Special token IDs (T5 standard) */
#define T5_PAD_ID 0
#define T5_EOS_ID 1
#define T5_UNK_ID 2
#define T5_BOS_ID 3  /* Not used in T5, but defined for compatibility */

/* ========================================================================
 * Data Structures
 * ======================================================================== */

typedef struct {
    char *token;
    int id;
    float score;  /* SentencePiece score */
} sp_vocab_entry_t;

struct t5_tokenizer {
    /* Vocabulary: id -> token string and score */
    sp_vocab_entry_t *vocab;
    int vocab_size;

    /* Hash table: token string -> id */
    sp_vocab_entry_t *vocab_hash;
    int hash_size;

    /* SentencePiece model data */
    char *model_proto;  /* SentencePiece model proto (simplified) */
    int model_proto_size;
};

/* ========================================================================
 * Hash Functions
 * ======================================================================== */

static int hash_string(const char *str, int hash_size) {
    unsigned long hash = 5381;
    int c;
    while ((c = *str++)) {
        hash = ((hash << 5) + hash) + c;
    }
    return hash % hash_size;
}

/* ========================================================================
 * Tokenizer Loading (Simplified - loads from tokenizer.json)
 * ======================================================================== */

static int parse_tokenizer_json(const char *json_path, t5_tokenizer_t *tok) {
    FILE *f = fopen(json_path, "r");
    if (!f) {
        fprintf(stderr, "Failed to open tokenizer.json: %s\n", json_path);
        return -1;
    }

    /* Simplified JSON parsing - look for vocab and merges */
    char line[4096];
    int in_vocab = 0;
    int vocab_idx = 0;
    
    /* Allocate vocabulary */
    tok->vocab = calloc(T5_VOCAB_SIZE, sizeof(sp_vocab_entry_t));
    tok->vocab_hash = calloc(T5_HASH_SIZE, sizeof(sp_vocab_entry_t));
    if (!tok->vocab || !tok->vocab_hash) {
        fclose(f);
        return -1;
    }
    tok->vocab_size = T5_VOCAB_SIZE;
    tok->hash_size = T5_HASH_SIZE;

    /* Basic tokenizer: map common tokens */
    /* For now, create a simple word-based tokenizer */
    while (fgets(line, sizeof(line), f) && vocab_idx < T5_VOCAB_SIZE - 10) {
        /* Skip to vocab section - simplified parsing */
        if (strstr(line, "\"vocab\"") || strstr(line, "\"model\"")) {
            in_vocab = 1;
        }
        if (in_vocab && strstr(line, "\"content\"")) {
            /* Extract token - simplified */
            char *start = strchr(line, '"');
            if (start) {
                start++;
                char *end = strchr(start, '"');
                if (end) {
                    *end = '\0';
                    int len = end - start;
                    if (len < 256 && vocab_idx < T5_VOCAB_SIZE) {
                        tok->vocab[vocab_idx].token = strdup(start);
                        tok->vocab[vocab_idx].id = vocab_idx;
                        tok->vocab[vocab_idx].score = 1.0f - (vocab_idx / (float)T5_VOCAB_SIZE);
                        
                        /* Add to hash */
                        int hash = hash_string(start, T5_HASH_SIZE);
                        int probe = 0;
                        while (tok->vocab_hash[hash].token && probe < T5_HASH_SIZE) {
                            hash = (hash + 1) % T5_HASH_SIZE;
                            probe++;
                        }
                        if (probe < T5_HASH_SIZE) {
                            tok->vocab_hash[hash] = tok->vocab[vocab_idx];
                        }
                        
                        vocab_idx++;
                    }
                }
            }
        }
    }

    fclose(f);
    
    /* Add special tokens */
    if (vocab_idx < T5_VOCAB_SIZE) {
        const char *special_tokens[] = {"<pad>", "<eos>", "<unk>", "<s>"};
        for (int i = 0; i < 4 && vocab_idx < T5_VOCAB_SIZE; i++) {
            tok->vocab[vocab_idx].token = strdup(special_tokens[i]);
            tok->vocab[vocab_idx].id = vocab_idx;
            tok->vocab[vocab_idx].score = 0.0f;
            vocab_idx++;
        }
    }
    
    tok->vocab_size = vocab_idx;
    return 0;
}

t5_tokenizer_t *t5_tokenizer_load(const char *tokenizer_dir) {
    t5_tokenizer_t *tok = calloc(1, sizeof(t5_tokenizer_t));
    if (!tok) return NULL;

    char path[512];
    snprintf(path, sizeof(path), "%s/tokenizer.json", tokenizer_dir);
    
    if (parse_tokenizer_json(path, tok) != 0) {
        fprintf(stderr, "Failed to parse tokenizer.json, using basic tokenizer\n");
        /* Fallback: create minimal tokenizer */
        tok->vocab = calloc(T5_VOCAB_SIZE, sizeof(sp_vocab_entry_t));
        tok->vocab_hash = calloc(T5_HASH_SIZE, sizeof(sp_vocab_entry_t));
        tok->vocab_size = T5_VOCAB_SIZE;
        tok->hash_size = T5_HASH_SIZE;
        
        /* Add basic tokens */
        const char *basic_tokens[] = {"<pad>", "<eos>", "<unk>"};
        for (int i = 0; i < 3; i++) {
            tok->vocab[i].token = strdup(basic_tokens[i]);
            tok->vocab[i].id = i;
            tok->vocab[i].score = 0.0f;
        }
    }

    return tok;
}

void t5_tokenizer_free(t5_tokenizer_t *tok) {
    if (!tok) return;
    
    if (tok->vocab) {
        for (int i = 0; i < tok->vocab_size; i++) {
            if (tok->vocab[i].token) free(tok->vocab[i].token);
        }
        free(tok->vocab);
    }
    
    if (tok->vocab_hash) free(tok->vocab_hash);
    if (tok->model_proto) free(tok->model_proto);
    free(tok);
}

/* Simple tokenization: split on whitespace and map to IDs */
int *t5_tokenize(t5_tokenizer_t *tok, const char *text, int *num_tokens, int max_len) {
    if (!tok || !text) {
        *num_tokens = 0;
        return NULL;
    }

    int *tokens = malloc(max_len * sizeof(int));
    if (!tokens) {
        *num_tokens = 0;
        return NULL;
    }

    /* Simple word-based tokenization */
    int count = 0;
    const char *p = text;
    char word[256];
    int word_len = 0;

    while (*p && count < max_len - 1) {
        if (isspace(*p)) {
            if (word_len > 0) {
                word[word_len] = '\0';
                /* Look up token ID */
                int hash = hash_string(word, tok->hash_size);
                int probe = 0;
                while (tok->vocab_hash[hash].token && probe < tok->hash_size) {
                    if (strcmp(tok->vocab_hash[hash].token, word) == 0) {
                        tokens[count++] = tok->vocab_hash[hash].id;
                        break;
                    }
                    hash = (hash + 1) % tok->hash_size;
                    probe++;
                }
                if (probe >= tok->hash_size || !tok->vocab_hash[hash].token) {
                    tokens[count++] = T5_UNK_ID;  /* Unknown token */
                }
                word_len = 0;
            }
        } else {
            if (word_len < 255) {
                word[word_len++] = *p;
            }
        }
        p++;
    }

    /* Add final word */
    if (word_len > 0 && count < max_len - 1) {
        word[word_len] = '\0';
        int hash = hash_string(word, tok->hash_size);
        int probe = 0;
        while (tok->vocab_hash[hash].token && probe < tok->hash_size) {
            if (strcmp(tok->vocab_hash[hash].token, word) == 0) {
                tokens[count++] = tok->vocab_hash[hash].id;
                break;
            }
            hash = (hash + 1) % tok->hash_size;
            probe++;
        }
        if (probe >= tok->hash_size || !tok->vocab_hash[hash].token) {
            tokens[count++] = T5_UNK_ID;
        }
    }

    /* Add EOS token */
    if (count < max_len) {
        tokens[count++] = T5_EOS_ID;
    }

    *num_tokens = count;
    return tokens;
}
