#pragma once

/*
 * Color Pipeline Phase 2.2 — source-alpha encoding & WBOIT normalization.
 * Distinct from frozen oitContract_t.sourceAlphaEncoding (internal accum form).
 * See docs/WBOIT_ALPHA_ENCODING.md.
 */


#include "../common/tr_types.h"

struct shader_s;
typedef struct shader_s shader_t;

/* Must match oit_source_normalize.glsl */
typedef enum oitSourceAlphaEncoding_e {
	OIT_SOURCE_ALPHA_STRAIGHT = 0,
	OIT_SOURCE_ALPHA_PREMULTIPLIED,
	OIT_SOURCE_ALPHA_OPAQUE,
	OIT_SOURCE_ALPHA_ADDITIVE,
	OIT_SOURCE_ALPHA_MASKED,
	OIT_SOURCE_ALPHA_MULTIPLICATIVE,
	OIT_SOURCE_ALPHA_UNKNOWN
} oitSourceAlphaEncoding_t;

typedef enum oitAlphaReason_e {
	ALPHA_ENCODING_EXPLICIT_STRAIGHT = 0,
	ALPHA_ENCODING_EXPLICIT_PREMULTIPLIED,
	ALPHA_ENCODING_CLASSIC_COMPATIBILITY,
	ALPHA_ENCODING_INFERRED_UNSAFE,
	ALPHA_ENCODING_UNKNOWN,
	ALPHA_ROUTE_NOT_WBOIT_COMPATIBLE
} oitAlphaReason_t;

typedef enum oitTransparentFilterMode_e {
	OIT_FILTER_STRAIGHT_SOURCE = 0,
	OIT_FILTER_PREMULTIPLIED_SOURCE,
	OIT_FILTER_EDGE_DILATED_STRAIGHT
} oitTransparentFilterMode_t;

typedef enum oitEmissiveOpacityPolicy_e {
	EMISSIVE_SCALED_BY_SURFACE_ALPHA = 0,
	EMISSIVE_INDEPENDENT_OF_SURFACE_ALPHA,
	EMISSIVE_ADDITIVE_ROUTE
} oitEmissiveOpacityPolicy_t;

typedef enum oitAlphaCertLevel_e {
	OIT_ALPHA_NONE = 0,
	OIT_ALPHA_DECLARED,
	OIT_ALPHA_NORMALIZED,
	OIT_ALPHA_ACCUMULATION_VALID,
	OIT_ALPHA_EDGE_CERTIFIED
} oitAlphaCertLevel_t;

typedef enum transparencyPath_e {
	TRANSP_PATH_WBOIT = 0,
	TRANSP_PATH_ADDITIVE,
	TRANSP_PATH_ALPHA_TESTED,
	TRANSP_PATH_MULTIPLICATIVE,
	TRANSP_PATH_REFRACTIVE,
	TRANSP_PATH_SORTED_ALPHA,
	TRANSP_PATH_REJECTED
} transparencyPath_t;

typedef struct materialTransparencyInfo_s {
	oitSourceAlphaEncoding_t sourceEncoding;
	transparencyPath_t path;
	oitAlphaReason_t reason;
	oitTransparentFilterMode_t filterMode;
	oitEmissiveOpacityPolicy_t emissivePolicy;

	qboolean alphaFromTexture;
	qboolean colorAlreadyAssociated;
	qboolean emissiveIndependentOfOpacity;
	qboolean preserveTransparentRgb;
	qboolean wboitEligible;
	qboolean vertexAlpha;
	qboolean entityAlpha;
	qboolean constAlpha;
	qboolean waveformAlphaUnsupported;
	qboolean portalAlphaUnsupported;
	qboolean multiStageUnsupported;

	float alphaScale;
	float alphaBias;
	float alphaCutoff;

	char declarationSource[64];
	char fallbackReason[96];
} materialTransparencyInfo_t;

/* CPU reference sample (mirrors GLSL OitSurfaceSample). */
typedef struct oitSurfaceSample_s {
	float unassociatedRadiance[3];
	float associatedRadiance[3];
	float opacity;
	unsigned sourceEncoding;
	unsigned flags;
} oitSurfaceSample_t;

#define OIT_SAMPLE_FLAG_CLAMPED_DIV   (1u << 0)
#define OIT_SAMPLE_FLAG_ZERO_ALPHA_RGB (1u << 1)
#define OIT_SAMPLE_FLAG_NEAR_ZERO_BRIGHT (1u << 2)
#define OIT_SAMPLE_FLAG_NON_FINITE    (1u << 3)
#define OIT_SAMPLE_FLAG_REJECTED      (1u << 4)

typedef struct oitSourcePolicy_s {
	float epsilon;
	int edgePolicy; /* r_transparentEdgePolicy */
	qboolean allowEmissiveAtZeroAlpha;
} oitSourcePolicy_t;

typedef struct oitAlphaCounters_s {
	uint32_t materialsWboit;
	uint32_t sourcesStraight;
	uint32_t sourcesPremul;
	uint32_t sourcesUnknown;
	uint32_t routesRejected;
	uint32_t zeroAlphaColored;
	uint32_t nearZeroBright;
	uint32_t blackEdgeRisk;
	uint32_t colorBleedRisk;
	uint32_t premulDivClamps;
	uint32_t alphaOutOfRange;
	uint32_t nonFinite;
	uint32_t doublePremulSuspects;
	uint32_t missingAlphaMultiplySuspects;
	uint32_t faultHits;
} oitAlphaCounters_t;

void vk_oit_alpha_register( void );
void vk_oit_alpha_begin_frame( void );

void vk_oit_alpha_query_shader( const shader_t *shader, materialTransparencyInfo_t *out );
qboolean vk_oit_alpha_wboit_eligible( const shader_t *shader, materialTransparencyInfo_t *outOpt );

void vk_oit_normalize_source( const float rgba[4], oitSourceAlphaEncoding_t encoding,
	const oitSourcePolicy_t *policy, oitSurfaceSample_t *out );

/* Reference source-over for single-layer certification. */
void vk_oit_reference_source_over( const float unassoc[3], float opacity,
	const float opaque[3], float outRgb[3] );

const char *vk_oit_source_alpha_name( oitSourceAlphaEncoding_t e );
const char *vk_oit_alpha_reason_name( oitAlphaReason_t r );
const char *vk_oit_alpha_cert_level_name( oitAlphaCertLevel_t l );
const char *vk_oit_transparency_path_name( transparencyPath_t p );

oitAlphaCertLevel_t vk_oit_alpha_certification_level( void );
qboolean vk_oit_alpha_validate( char *errBuf, int errBufSize );
const oitAlphaCounters_t *vk_oit_alpha_counters( void );

void vk_oit_alpha_note_sample_flags( unsigned flags );
void vk_oit_alpha_note_route( oitSourceAlphaEncoding_t enc, transparencyPath_t path );

/* Push-constant pack: encoding | (alphaDebug<<8) | (edgePolicy<<16) | (emissive<<24) */
int vk_oit_alpha_pack_push( oitSourceAlphaEncoding_t enc );

