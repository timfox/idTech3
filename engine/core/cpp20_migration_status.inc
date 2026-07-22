/* Auto-maintained snapshot for cpp20_status (update when converting files).
 * Shell: ./scripts/cpp20_status.sh can regenerate approximate filesystem counts.
 */
#ifndef CPP20_MIGRATION_STATUS_INC
#define CPP20_MIGRATION_STATUS_INC

/* First-party tracked approx (engine+runtime+renderers+modules+tests), foundation milestone.
 * Filesystem snapshot (scripts/cpp20_status.sh): ~623 .c / ~45 .cpp under tracked trees.
 */
#define CPP20_STATUS_TOTAL_C              623
#define CPP20_STATUS_TOTAL_CXX             45
/* Leaf files converted in foundation batch (md4/md5/huffman_static/q_utf8/vk_cluster_math). */
#define CPP20_STATUS_CONVERTED              5
#define CPP20_STATUS_BLOCKED               48
#define CPP20_STATUS_MIXED_BOUNDARIES      24
#define CPP20_STATUS_C_ABI_BOUNDARIES      16
#define CPP20_STATUS_HIGH_RISK_REMAINING   40

#endif
