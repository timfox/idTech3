/*
 * T5 Encoder Model Implementation for FLUX.1
 *
 * Implements T5-XXL encoder-only model for text encoding.
 * - 24 transformer layers
 * - 4096 hidden dimension
 * - 32 attention heads
 * - Relative position embeddings
 * - Layer normalization
 */

#include "flux_t5.h"
#include "flux_safetensors.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

/* Forward declarations from flux_t5_tokenizer.c */
extern t5_tokenizer_t *t5_tokenizer_load(const char *tokenizer_dir);
extern void t5_tokenizer_free(t5_tokenizer_t *tok);
extern int *t5_tokenize(t5_tokenizer_t *tok, const char *text, int *num_tokens, int max_len);

/* Forward declarations - t5_model_free is used by flux_t5.c */
void t5_model_free(t5_model_t *model);

#ifdef USE_BLAS
#ifdef __APPLE__
#include <Accelerate/Accelerate.h>
#else
#include <cblas.h>
#endif
#endif

/* ========================================================================
 * Data Structures
 * ======================================================================== */

typedef struct {
    float *q_weight;      /* [num_heads * head_dim, hidden] = [4096, 4096] */
    float *k_weight;      /* [num_heads * head_dim, hidden] = [4096, 4096] */
    float *v_weight;      /* [num_heads * head_dim, hidden] = [4096, 4096] */
    float *o_weight;       /* [hidden, num_heads * head_dim] = [4096, 4096] */
} t5_attention_t;

typedef struct {
    float *wi_0_weight;   /* [intermediate, hidden] = [10240, 4096] */
    float *wi_1_weight;   /* [intermediate, hidden] = [10240, 4096] */
    float *wo_weight;     /* [hidden, intermediate] = [4096, 10240] */
} t5_ff_t;

typedef struct {
    float *layer_norm_weight;      /* [hidden] */
    float *SelfAttention_layer_norm_weight;  /* [hidden] */
    t5_attention_t self_attn;
    t5_ff_t ff;
} t5_layer_t;

struct t5_model {
    /* Embedding layer */
    float *shared;         /* [vocab_size, hidden] = [32128, 4096] */
    
    /* Encoder layers */
    t5_layer_t *layers;   /* [num_layers] = [24] */
    int num_layers;
    
    /* Final layer norm */
    float *final_layer_norm_weight;  /* [hidden] */
    
    /* Relative position bias (simplified) */
    float *relative_attention_bias; /* [num_buckets, num_heads] */
    int num_buckets;
    
    /* Working memory */
    float *hidden_state;  /* [seq_len, hidden] */
    float *residual;      /* [seq_len, hidden] */
    float *q_buf;         /* [seq_len, num_heads * head_dim] */
    float *k_buf;         /* [seq_len, num_heads * head_dim] */
    float *v_buf;         /* [seq_len, num_heads * head_dim] */
    float *attn_scores;   /* [num_heads, seq_len, seq_len] */
    float *attn_out;      /* [seq_len, num_heads * head_dim] */
    float *ff_out;        /* [seq_len, hidden] */
    float *norm_buf;      /* [seq_len, hidden] */
    
    /* Mmap mode */
    int use_mmap;
    safetensors_file_t *sf_file;
};

/* ========================================================================
 * Helper Functions
 * ======================================================================== */

static float *load_tensor(safetensors_file_t *sf, const char *name) {
    if (!sf) return NULL;
    const safetensor_t *t = safetensors_find(sf, name);
    if (!t) return NULL;
    
    /* Use the same helper as transformer */
    float *tensor = safetensors_get_f32(sf, t);
    return tensor;
}

static void layer_norm(float *out, const float *in, const float *weight,
                       int seq_len, int hidden, float eps) {
    for (int s = 0; s < seq_len; s++) {
        const float *x = in + s * hidden;
        float *y = out + s * hidden;
        
        /* Compute mean */
        float mean = 0.0f;
        for (int i = 0; i < hidden; i++) {
            mean += x[i];
        }
        mean /= hidden;
        
        /* Compute variance */
        float var = 0.0f;
        for (int i = 0; i < hidden; i++) {
            float diff = x[i] - mean;
            var += diff * diff;
        }
        var /= hidden;
        
        /* Normalize */
        float inv_std = 1.0f / sqrtf(var + eps);
        for (int i = 0; i < hidden; i++) {
            y[i] = (x[i] - mean) * inv_std * weight[i];
        }
    }
}

static void linear(float *y, const float *x, const float *W,
                   int seq_len, int in_dim, int out_dim) {
    /* y[seq, out] = x[seq, in] @ W[out, in]^T */
#ifdef USE_BLAS
    cblas_sgemm(CblasRowMajor, CblasNoTrans, CblasTrans,
                seq_len, out_dim, in_dim,
                1.0f, x, in_dim, W, in_dim, 0.0f, y, out_dim);
#else
    for (int s = 0; s < seq_len; s++) {
        for (int o = 0; o < out_dim; o++) {
            float sum = 0.0f;
            for (int i = 0; i < in_dim; i++) {
                sum += x[s * in_dim + i] * W[o * in_dim + i];
            }
            y[s * out_dim + o] = sum;
        }
    }
#endif
}

static void attention(float *out, const float *q, const float *k, const float *v,
                      float *attn_scores, int seq_len, int num_heads, int head_dim, const float *bias) {
    int hidden = num_heads * head_dim;
    float scale = 1.0f / sqrtf((float)head_dim);
    
    /* Compute attention scores */
    for (int h = 0; h < num_heads; h++) {
        float *scores = attn_scores + h * seq_len * seq_len;
        for (int i = 0; i < seq_len; i++) {
            for (int j = 0; j < seq_len; j++) {
                float score = 0.0f;
                for (int d = 0; d < head_dim; d++) {
                    score += q[i * hidden + h * head_dim + d] *
                             k[j * hidden + h * head_dim + d];
                }
                score *= scale;
                if (bias) {
                    score += bias[h * seq_len * seq_len + i * seq_len + j];
                }
                scores[i * seq_len + j] = score;
            }
        }
        
        /* Apply softmax */
        for (int i = 0; i < seq_len; i++) {
            float *row = scores + i * seq_len;
            float max_val = row[0];
            for (int j = 1; j < seq_len; j++) {
                if (row[j] > max_val) max_val = row[j];
            }
            
            float sum = 0.0f;
            for (int j = 0; j < seq_len; j++) {
                row[j] = expf(row[j] - max_val);
                sum += row[j];
            }
            for (int j = 0; j < seq_len; j++) {
                row[j] /= sum;
            }
        }
        
        /* Compute attention output */
        for (int i = 0; i < seq_len; i++) {
            float *out_head = out + i * hidden + h * head_dim;
            memset(out_head, 0, head_dim * sizeof(float));
            for (int j = 0; j < seq_len; j++) {
                float attn_weight = scores[i * seq_len + j];
                const float *v_head = v + j * hidden + h * head_dim;
                for (int d = 0; d < head_dim; d++) {
                    out_head[d] += attn_weight * v_head[d];
                }
            }
        }
    }
}

static void t5_layer_forward(t5_model_t *model, t5_layer_t *layer, int seq_len) {
    int hidden = T5_HIDDEN_SIZE;
    
    /* Self-attention with layer norm */
    layer_norm(model->norm_buf, model->hidden_state, layer->layer_norm_weight,
               seq_len, hidden, 1e-6f);
    
    /* Q, K, V projections */
    linear(model->q_buf, model->norm_buf, layer->self_attn.q_weight,
           seq_len, hidden, hidden);
    linear(model->k_buf, model->norm_buf, layer->self_attn.k_weight,
           seq_len, hidden, hidden);
    linear(model->v_buf, model->norm_buf, layer->self_attn.v_weight,
           seq_len, hidden, hidden);
    
    /* Attention */
    attention(model->attn_out, model->q_buf, model->k_buf, model->v_buf,
              model->attn_scores, seq_len, 32, 128, NULL);
    
    /* Output projection */
    linear(model->residual, model->attn_out, layer->self_attn.o_weight,
           seq_len, hidden, hidden);
    
    /* Residual connection */
    for (int i = 0; i < seq_len * hidden; i++) {
        model->hidden_state[i] += model->residual[i];
    }
    
    /* Feed-forward */
    layer_norm(model->norm_buf, model->hidden_state,
               layer->SelfAttention_layer_norm_weight, seq_len, hidden, 1e-6f);
    
    linear(model->ff_out, model->norm_buf, layer->ff.wi_0_weight,
           seq_len, hidden, T5_INTERMEDIATE_SIZE);
    /* Apply GELU activation */
    for (int i = 0; i < seq_len * T5_INTERMEDIATE_SIZE; i++) {
        float x = model->ff_out[i];
        model->ff_out[i] = 0.5f * x * (1.0f + tanhf(0.7978845608f * (x + 0.044715f * x * x * x)));
    }
    
    linear(model->residual, model->ff_out, layer->ff.wo_weight,
           seq_len, T5_INTERMEDIATE_SIZE, hidden);
    
    /* Residual connection */
    for (int i = 0; i < seq_len * hidden; i++) {
        model->hidden_state[i] += model->residual[i];
    }
}

/* ========================================================================
 * Model Loading
 * ======================================================================== */

t5_model_t *t5_model_load(const char *model_dir, int use_mmap) {
    t5_model_t *model = calloc(1, sizeof(t5_model_t));
    if (!model) return NULL;
    
    model->num_layers = T5_NUM_LAYERS;
    model->use_mmap = use_mmap;
    
    /* Open safetensors file */
    char path[512];
    snprintf(path, sizeof(path), "%s/model.safetensors", model_dir);
    
    model->sf_file = safetensors_open(path);
    if (!model->sf_file) {
        fprintf(stderr, "t5_model_load: failed to open %s\n", path);
        free(model);
        return NULL;
    }
    
    /* Allocate layers */
    model->layers = calloc(model->num_layers, sizeof(t5_layer_t));
    if (!model->layers) {
        safetensors_close(model->sf_file);
        free(model);
        return NULL;
    }
    
    /* Load shared embeddings */
    model->shared = load_tensor(model->sf_file, "shared.weight");
    if (!model->shared) {
        fprintf(stderr, "t5_model_load: failed to load shared embeddings\n");
        goto error;
    }
    
    /* Load encoder layers */
    for (int i = 0; i < model->num_layers; i++) {
        char name[256];
        t5_layer_t *layer = &model->layers[i];
        
        snprintf(name, sizeof(name), "encoder.block.%d.layer.0.layer_norm.weight", i);
        layer->layer_norm_weight = load_tensor(model->sf_file, name);
        
        snprintf(name, sizeof(name), "encoder.block.%d.layer.1.layer_norm.weight", i);
        layer->SelfAttention_layer_norm_weight = load_tensor(model->sf_file, name);
        
        snprintf(name, sizeof(name), "encoder.block.%d.layer.0.SelfAttention.q.weight", i);
        layer->self_attn.q_weight = load_tensor(model->sf_file, name);
        
        snprintf(name, sizeof(name), "encoder.block.%d.layer.0.SelfAttention.k.weight", i);
        layer->self_attn.k_weight = load_tensor(model->sf_file, name);
        
        snprintf(name, sizeof(name), "encoder.block.%d.layer.0.SelfAttention.v.weight", i);
        layer->self_attn.v_weight = load_tensor(model->sf_file, name);
        
        snprintf(name, sizeof(name), "encoder.block.%d.layer.0.SelfAttention.o.weight", i);
        layer->self_attn.o_weight = load_tensor(model->sf_file, name);
        
        snprintf(name, sizeof(name), "encoder.block.%d.layer.1.DenseReluDense.wi_0.weight", i);
        layer->ff.wi_0_weight = load_tensor(model->sf_file, name);
        
        snprintf(name, sizeof(name), "encoder.block.%d.layer.1.DenseReluDense.wi_1.weight", i);
        layer->ff.wi_1_weight = load_tensor(model->sf_file, name);
        
        snprintf(name, sizeof(name), "encoder.block.%d.layer.1.DenseReluDense.wo.weight", i);
        layer->ff.wo_weight = load_tensor(model->sf_file, name);
    }
    
    /* Load final layer norm */
    model->final_layer_norm_weight = load_tensor(model->sf_file, "encoder.final_layer_norm.weight");
    
    /* Allocate working buffers */
    int max_seq = T5_MAX_SEQ_LEN;
    int hidden = T5_HIDDEN_SIZE;
    model->hidden_state = malloc(max_seq * hidden * sizeof(float));
    model->residual = malloc(max_seq * hidden * sizeof(float));
    model->q_buf = malloc(max_seq * hidden * sizeof(float));
    model->k_buf = malloc(max_seq * hidden * sizeof(float));
    model->v_buf = malloc(max_seq * hidden * sizeof(float));
    model->attn_scores = malloc(32 * max_seq * max_seq * sizeof(float));
    model->attn_out = malloc(max_seq * hidden * sizeof(float));
    model->ff_out = malloc(max_seq * T5_INTERMEDIATE_SIZE * sizeof(float));
    model->norm_buf = malloc(max_seq * hidden * sizeof(float));
    
    if (!model->hidden_state || !model->residual || !model->q_buf ||
        !model->k_buf || !model->v_buf || !model->attn_out || !model->ff_out ||
        !model->norm_buf) {
        fprintf(stderr, "t5_model_load: failed to allocate working buffers\n");
        goto error;
    }
    
    if (!use_mmap) {
        safetensors_close(model->sf_file);
        model->sf_file = NULL;
    }
    
    return model;
    
error:
    t5_model_free(model);
    return NULL;
}

void t5_model_free(t5_model_t *model) {
    if (!model) return;
    
    if (model->shared) free(model->shared);
    if (model->final_layer_norm_weight) free(model->final_layer_norm_weight);
    
    if (model->layers) {
        for (int i = 0; i < model->num_layers; i++) {
            t5_layer_t *layer = &model->layers[i];
            if (layer->layer_norm_weight) free(layer->layer_norm_weight);
            if (layer->SelfAttention_layer_norm_weight) free(layer->SelfAttention_layer_norm_weight);
            if (layer->self_attn.q_weight) free(layer->self_attn.q_weight);
            if (layer->self_attn.k_weight) free(layer->self_attn.k_weight);
            if (layer->self_attn.v_weight) free(layer->self_attn.v_weight);
            if (layer->self_attn.o_weight) free(layer->self_attn.o_weight);
            if (layer->ff.wi_0_weight) free(layer->ff.wi_0_weight);
            if (layer->ff.wi_1_weight) free(layer->ff.wi_1_weight);
            if (layer->ff.wo_weight) free(layer->ff.wo_weight);
        }
        free(model->layers);
    }
    
    if (model->hidden_state) free(model->hidden_state);
    if (model->residual) free(model->residual);
    if (model->q_buf) free(model->q_buf);
    if (model->k_buf) free(model->k_buf);
    if (model->v_buf) free(model->v_buf);
    if (model->attn_scores) free(model->attn_scores);
    if (model->attn_out) free(model->attn_out);
    if (model->ff_out) free(model->ff_out);
    if (model->norm_buf) free(model->norm_buf);
    
    if (model->sf_file) safetensors_close(model->sf_file);
    free(model);
}

/* ========================================================================
 * Forward Pass
 * ======================================================================== */

float *t5_forward(t5_model_t *model, const int *input_ids, int seq_len) {
    int hidden = T5_HIDDEN_SIZE;
    
    /* Embedding lookup */
    for (int s = 0; s < seq_len; s++) {
        int token_id = input_ids[s];
        if (token_id >= 0 && token_id < T5_VOCAB_SIZE) {
            memcpy(model->hidden_state + s * hidden,
                   model->shared + token_id * hidden,
                   hidden * sizeof(float));
        } else {
            memset(model->hidden_state + s * hidden, 0, hidden * sizeof(float));
        }
    }
    
    /* Run through encoder layers */
    for (int i = 0; i < model->num_layers; i++) {
        t5_layer_forward(model, &model->layers[i], seq_len);
    }
    
    /* Final layer norm */
    layer_norm(model->hidden_state, model->hidden_state,
               model->final_layer_norm_weight, seq_len, hidden, 1e-6f);
    
    /* Return copy of hidden state */
    float *output = malloc(seq_len * hidden * sizeof(float));
    if (output) {
        memcpy(output, model->hidden_state, seq_len * hidden * sizeof(float));
    }
    
    return output;
}
