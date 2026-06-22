/*
 * FLUX Tokenizer Implementation
 *
 * BPE (Byte Pair Encoding) tokenizer for text-to-image generation.
 * Supports both WordPiece-style and SentencePiece-style tokenization.
 */

#include "flux.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

/* ========================================================================
 * Internal Data Structures
 * ======================================================================== */

/* Hash table entry for vocabulary lookup */
typedef struct {
    char *token;
    int id;
} vocab_entry_t;

/* BPE merge rule */
typedef struct {
    int left;
    int right;
    int result;
    int priority;  /* Lower = higher priority (earlier merge) */
} bpe_merge_t;

/* Tokenizer context */
struct flux_tokenizer {
    /* Vocabulary */
    char **vocab;           /* id -> token string */
    int vocab_size;
    vocab_entry_t *vocab_hash;  /* Hash table for token -> id lookup */
    int hash_size;

    /* BPE merges */
    bpe_merge_t *merges;
    int num_merges;

    /* Special token IDs */
    int pad_id;
    int unk_id;
    int bos_id;
    int eos_id;

    /* Configuration */
    int max_length;
    int add_bos;
    int add_eos;
};

/* Forward declarations */
void flux_tokenizer_free(flux_tokenizer *tok);

/* ========================================================================
 * Hash Table Functions
 * ======================================================================== */

/* FNV-1a hash */
static unsigned int hash_string(const char *str) {
    unsigned int hash = 2166136261u;
    while (*str) {
        hash ^= (unsigned char)*str++;
        hash *= 16777619u;
    }
    return hash;
}

/* Insert into hash table */
static void vocab_hash_insert(vocab_entry_t *table, int hash_size,
                              const char *token, int id) {
    unsigned int h = hash_string(token) % hash_size;
    while (table[h].token != NULL) {
        if (strcmp(table[h].token, token) == 0) {
            return;  /* Already exists */
        }
        h = (h + 1) % hash_size;
    }
    table[h].token = strdup(token);
    table[h].id = id;
}

/* Lookup in hash table, returns -1 if not found */
static int vocab_hash_lookup(const vocab_entry_t *table, int hash_size,
                             const char *token) {
    unsigned int h = hash_string(token) % hash_size;
    while (table[h].token != NULL) {
        if (strcmp(table[h].token, token) == 0) {
            return table[h].id;
        }
        h = (h + 1) % hash_size;
    }
    return -1;
}

/* ========================================================================
 * Unicode Utilities
 * ======================================================================== */

/* Get UTF-8 character length from first byte */
static int utf8_char_len(unsigned char c) {
    if ((c & 0x80) == 0) return 1;
    if ((c & 0xE0) == 0xC0) return 2;
    if ((c & 0xF0) == 0xE0) return 3;
    if ((c & 0xF8) == 0xF0) return 4;
    return 1;  /* Invalid, treat as 1 */
}

/* Decode UTF-8 character to codepoint - reserved for QWEN3 text encoder */
__attribute__((unused))
static int utf8_decode(const char *s, int *len) {
    unsigned char c = (unsigned char)s[0];
    if ((c & 0x80) == 0) {
        *len = 1;
        return c;
    }
    if ((c & 0xE0) == 0xC0) {
        *len = 2;
        return ((c & 0x1F) << 6) | (s[1] & 0x3F);
    }
    if ((c & 0xF0) == 0xE0) {
        *len = 3;
        return ((c & 0x0F) << 12) | ((s[1] & 0x3F) << 6) | (s[2] & 0x3F);
    }
    if ((c & 0xF8) == 0xF0) {
        *len = 4;
        return ((c & 0x07) << 18) | ((s[1] & 0x3F) << 12) |
               ((s[2] & 0x3F) << 6) | (s[3] & 0x3F);
    }
    *len = 1;
    return c;
}

/* Check if character is whitespace */
static int is_whitespace(int cp) {
    return cp == ' ' || cp == '\t' || cp == '\n' || cp == '\r';
}

/* Check if character is punctuation */
static int is_punctuation(int cp) {
    if (cp >= 33 && cp <= 47) return 1;   /* !"#$%&'()*+,-./ */
    if (cp >= 58 && cp <= 64) return 1;   /* :;<=>?@ */
    if (cp >= 91 && cp <= 96) return 1;   /* [\]^_` */
    if (cp >= 123 && cp <= 126) return 1; /* {|}~ */
    return 0;
}

/* ========================================================================
 * Pre-tokenization
 * ======================================================================== */

/* Split text into initial tokens (words, whitespace, punctuation) */
static char **pretokenize(const char *text, int *num_tokens) {
    int capacity = 64;
    char **tokens = malloc(capacity * sizeof(char *));
    int count = 0;

    const char *p = text;
    while (*p) {
        /* Skip leading whitespace (but could keep as token) */
        while (*p && is_whitespace((unsigned char)*p)) p++;
        if (!*p) break;

        const char *start = p;
        int char_len = utf8_char_len((unsigned char)*p);

        if (is_punctuation((unsigned char)*p)) {
            /* Single punctuation is a token */
            p += char_len;
        } else {
            /* Read until whitespace or punctuation */
            while (*p && !is_whitespace((unsigned char)*p) &&
                   !is_punctuation((unsigned char)*p)) {
                p += utf8_char_len((unsigned char)*p);
            }
        }

        /* Create token */
        int len = p - start;
        char *token = malloc(len + 1);
        memcpy(token, start, len);
        token[len] = '\0';

        if (count >= capacity) {
            capacity *= 2;
            tokens = realloc(tokens, capacity * sizeof(char *));
        }
        tokens[count++] = token;
    }

    *num_tokens = count;
    return tokens;
}

/* ========================================================================
 * BPE Tokenization
 * ======================================================================== */

/* Tokenize a single word using BPE */
static int *bpe_tokenize_word(flux_tokenizer *tok, const char *word,
                              int *num_tokens) {
    int len = strlen(word);
    if (len == 0) {
        *num_tokens = 0;
        return NULL;
    }

    /* Start with character-level tokens */
    int max_chars = len;  /* UTF-8 can have multi-byte chars */
    int *tokens = malloc(max_chars * sizeof(int));
    int n = 0;

    const char *p = word;
    while (*p) {
        int char_len = utf8_char_len((unsigned char)*p);
        char buf[8];
        memcpy(buf, p, char_len);
        buf[char_len] = '\0';

        int id = vocab_hash_lookup(tok->vocab_hash, tok->hash_size, buf);
        if (id < 0) {
            /* Unknown character, try byte fallback */
            for (int i = 0; i < char_len; i++) {
                char byte_buf[8];
                snprintf(byte_buf, sizeof(byte_buf), "<0x%02X>",
                         (unsigned char)p[i]);
                id = vocab_hash_lookup(tok->vocab_hash, tok->hash_size, byte_buf);
                if (id < 0) id = tok->unk_id;
                tokens[n++] = id;
            }
        } else {
            tokens[n++] = id;
        }
        p += char_len;
    }

    /* Apply BPE merges */
    int changed = 1;
    while (changed && n > 1) {
        changed = 0;
        int best_idx = -1;
        int best_priority = tok->num_merges + 1;

        /* Find best merge */
        for (int i = 0; i < n - 1; i++) {
            for (int m = 0; m < tok->num_merges; m++) {
                if (tok->merges[m].left == tokens[i] &&
                    tok->merges[m].right == tokens[i + 1]) {
                    if (tok->merges[m].priority < best_priority) {
                        best_priority = tok->merges[m].priority;
                        best_idx = i;
                    }
                    break;
                }
            }
        }

        /* Apply best merge */
        if (best_idx >= 0) {
            for (int m = 0; m < tok->num_merges; m++) {
                if (tok->merges[m].left == tokens[best_idx] &&
                    tok->merges[m].right == tokens[best_idx + 1]) {
                    tokens[best_idx] = tok->merges[m].result;
                    /* Shift remaining tokens */
                    for (int i = best_idx + 1; i < n - 1; i++) {
                        tokens[i] = tokens[i + 1];
                    }
                    n--;
                    changed = 1;
                    break;
                }
            }
        }
    }

    *num_tokens = n;
    return tokens;
}

/* ========================================================================
 * Tokenizer Loading
 * ======================================================================== */

#define TOK_MAGIC "FTOK"

flux_tokenizer *flux_tokenizer_load(const char *path) {
    FILE *f = fopen(path, "rb");
    if (!f) {
        fprintf(stderr, "flux_tokenizer_load: cannot open %s\n", path);
        return NULL;
    }

    /* Check magic */
    char magic[4];
    if (fread(magic, 1, 4, f) != 4 || memcmp(magic, TOK_MAGIC, 4) != 0) {
        fprintf(stderr, "flux_tokenizer_load: invalid magic\n");
        fclose(f);
        return NULL;
    }

    flux_tokenizer *tok = calloc(1, sizeof(flux_tokenizer));
    if (!tok) {
        fclose(f);
        return NULL;
    }

    /* Read config */
    uint32_t config[8];
    if (fread(config, sizeof(uint32_t), 8, f) != 8) goto error;

    tok->vocab_size = config[0];
    tok->num_merges = config[1];
    tok->pad_id = config[2];
    tok->unk_id = config[3];
    tok->bos_id = config[4];
    tok->eos_id = config[5];
    tok->max_length = config[6];
    tok->add_bos = config[7] & 1;
    tok->add_eos = (config[7] >> 1) & 1;

    tok->hash_size = tok->vocab_size * 2 + 1;
    if (tok->hash_size < FLUX_VOCAB_HASH_SIZE)
        tok->hash_size = FLUX_VOCAB_HASH_SIZE;

    /* Allocate vocabulary */
    tok->vocab = malloc(tok->vocab_size * sizeof(char *));
    tok->vocab_hash = calloc(tok->hash_size, sizeof(vocab_entry_t));
    if (!tok->vocab || !tok->vocab_hash) goto error;

    /* Read vocabulary */
    for (int i = 0; i < tok->vocab_size; i++) {
        uint16_t len;
        if (fread(&len, sizeof(uint16_t), 1, f) != 1) goto error;

        tok->vocab[i] = malloc(len + 1);
        if (!tok->vocab[i]) goto error;
        if (fread(tok->vocab[i], 1, len, f) != len) goto error;
        tok->vocab[i][len] = '\0';

        vocab_hash_insert(tok->vocab_hash, tok->hash_size, tok->vocab[i], i);
    }

    /* Read merges */
    if (tok->num_merges > 0) {
        tok->merges = malloc(tok->num_merges * sizeof(bpe_merge_t));
        if (!tok->merges) goto error;

        for (int i = 0; i < tok->num_merges; i++) {
            uint32_t merge[3];
            if (fread(merge, sizeof(uint32_t), 3, f) != 3) goto error;
            tok->merges[i].left = merge[0];
            tok->merges[i].right = merge[1];
            tok->merges[i].result = merge[2];
            tok->merges[i].priority = i;
        }
    }

    fclose(f);
    return tok;

error:
    fprintf(stderr, "flux_tokenizer_load: error reading tokenizer\n");
    fclose(f);
    flux_tokenizer_free(tok);
    return NULL;
}

void flux_tokenizer_free(flux_tokenizer *tok) {
    if (!tok) return;

    if (tok->vocab) {
        for (int i = 0; i < tok->vocab_size; i++) {
            free(tok->vocab[i]);
        }
        free(tok->vocab);
    }

    if (tok->vocab_hash) {
        for (int i = 0; i < tok->hash_size; i++) {
            free(tok->vocab_hash[i].token);
        }
        free(tok->vocab_hash);
    }

    free(tok->merges);
    free(tok);
}

/* ========================================================================
 * Main Tokenization API
 * ======================================================================== */

int *flux_tokenize(flux_tokenizer *tok, const char *text,
                   int *num_tokens, int max_len) {
    if (!tok || !text) {
        *num_tokens = 0;
        return NULL;
    }

    if (max_len <= 0) max_len = tok->max_length;

    /* Pre-tokenize */
    int num_words;
    char **words = pretokenize(text, &num_words);

    /* Collect all tokens */
    int capacity = 128;
    int *all_tokens = malloc(capacity * sizeof(int));
    int total = 0;

    /* Add BOS if configured */
    if (tok->add_bos && tok->bos_id >= 0) {
        all_tokens[total++] = tok->bos_id;
    }

    /* BPE tokenize each word */
    for (int w = 0; w < num_words && total < max_len - (tok->add_eos ? 1 : 0); w++) {
        int n;
        int *word_tokens = bpe_tokenize_word(tok, words[w], &n);

        for (int i = 0; i < n && total < max_len - (tok->add_eos ? 1 : 0); i++) {
            if (total >= capacity) {
                capacity *= 2;
                all_tokens = realloc(all_tokens, capacity * sizeof(int));
            }
            all_tokens[total++] = word_tokens[i];
        }

        free(word_tokens);
        free(words[w]);
    }
    free(words);

    /* Add EOS if configured */
    if (tok->add_eos && tok->eos_id >= 0 && total < max_len) {
        if (total >= capacity) {
            capacity *= 2;
            all_tokens = realloc(all_tokens, capacity * sizeof(int));
        }
        all_tokens[total++] = tok->eos_id;
    }

    *num_tokens = total;
    return all_tokens;
}

/* Decode tokens back to text */
char *flux_detokenize(flux_tokenizer *tok, const int *tokens, int num_tokens) {
    if (!tok || !tokens || num_tokens <= 0) {
        return strdup("");
    }

    /* Calculate total length */
    int total_len = 0;
    for (int i = 0; i < num_tokens; i++) {
        int id = tokens[i];
        if (id >= 0 && id < tok->vocab_size) {
            total_len += strlen(tok->vocab[id]);
        }
    }

    /* Build string */
    char *text = malloc(total_len + 1);
    char *p = text;
    for (int i = 0; i < num_tokens; i++) {
        int id = tokens[i];
        if (id >= 0 && id < tok->vocab_size) {
            /* Skip special tokens */
            if (id == tok->bos_id || id == tok->eos_id || id == tok->pad_id) {
                continue;
            }
            const char *token = tok->vocab[id];
            int len = strlen(token);
            memcpy(p, token, len);
            p += len;
        }
    }
    *p = '\0';

    return text;
}

/* Get vocabulary size */
int flux_tokenizer_vocab_size(flux_tokenizer *tok) {
    return tok ? tok->vocab_size : 0;
}

/* Get token string by ID */
const char *flux_tokenizer_get_token(flux_tokenizer *tok, int id) {
    if (!tok || id < 0 || id >= tok->vocab_size) return NULL;
    return tok->vocab[id];
}

/* Get token ID by string */
int flux_tokenizer_get_id(flux_tokenizer *tok, const char *token) {
    if (!tok || !token) return -1;
    return vocab_hash_lookup(tok->vocab_hash, tok->hash_size, token);
}

/* ========================================================================
 * Simple Tokenizer (Fallback)
 * ======================================================================== */

/*
 * Create a simple tokenizer that just does character-level tokenization.
 * Used as fallback or for testing.
 */
flux_tokenizer *flux_tokenizer_create_simple(void) {
    flux_tokenizer *tok = calloc(1, sizeof(flux_tokenizer));
    if (!tok) return NULL;

    /* ASCII printable characters + some special tokens */
    tok->vocab_size = 256 + 4;  /* All bytes + special tokens */
    tok->hash_size = 521;  /* Prime > vocab_size */
    tok->num_merges = 0;
    tok->pad_id = 256;
    tok->unk_id = 257;
    tok->bos_id = 258;
    tok->eos_id = 259;
    tok->max_length = FLUX_MAX_SEQ_LEN;
    tok->add_bos = 1;
    tok->add_eos = 1;

    tok->vocab = malloc(tok->vocab_size * sizeof(char *));
    tok->vocab_hash = calloc(tok->hash_size, sizeof(vocab_entry_t));
    if (!tok->vocab || !tok->vocab_hash) {
        flux_tokenizer_free(tok);
        return NULL;
    }

    /* Create vocabulary */
    for (int i = 0; i < 256; i++) {
        char buf[8];
        if (i >= 32 && i < 127) {
            buf[0] = (char)i;
            buf[1] = '\0';
        } else {
            snprintf(buf, sizeof(buf), "<0x%02X>", i);
        }
        tok->vocab[i] = strdup(buf);
        vocab_hash_insert(tok->vocab_hash, tok->hash_size, tok->vocab[i], i);
    }
    tok->vocab[256] = strdup("<pad>");
    tok->vocab[257] = strdup("<unk>");
    tok->vocab[258] = strdup("<bos>");
    tok->vocab[259] = strdup("<eos>");

    vocab_hash_insert(tok->vocab_hash, tok->hash_size, "<pad>", 256);
    vocab_hash_insert(tok->vocab_hash, tok->hash_size, "<unk>", 257);
    vocab_hash_insert(tok->vocab_hash, tok->hash_size, "<bos>", 258);
    vocab_hash_insert(tok->vocab_hash, tok->hash_size, "<eos>", 259);

    return tok;
}
