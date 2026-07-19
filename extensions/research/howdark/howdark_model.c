/*
===========================================================================
How Dark is Dark — calibrated material tables (Filip & Vávra 2026).

Relative constants reproduce paper ordering and ~10× coating vs fabric
albedo gap (Figs. 4–6, 8). Not measured BRDF samples.
===========================================================================
*/

#include "howdark/howdark_internal.h"

#include <ctype.h>
#include <stdio.h>
#include <string.h>

static void howdark_tolower_copy( char *dst, size_t dstSize, const char *src )
{
	size_t i;

	if ( !dst || dstSize == 0 ) {
		return;
	}
	for ( i = 0; src && src[i] && i + 1 < dstSize; i++ ) {
		dst[i] = (char)tolower( (unsigned char)src[i] );
	}
	dst[i] = '\0';
}

static int howdark_streq_ci( const char *a, const char *b )
{
	char aa[64];
	char bb[64];

	howdark_tolower_copy( aa, sizeof( aa ), a );
	howdark_tolower_copy( bb, sizeof( bb ), b );
	return strcmp( aa, bb ) == 0;
}

const float howdark_theta_i[HOWDARK_THETA_SAMPLES] = {
	0.0f, 15.0f, 30.0f, 45.0f, 60.0f, 75.0f, 80.0f, 85.0f
};

/*
 * Albedo / luminance: Vantablack ≈ Musou fabric < velvet ≪ coatings;
 * acrylic highest (~10× fabrics). Musou fabric wins P1/P50; Vantablack P99.
 */
const howdark_material_t howdark_materials[HOWDARK_MATERIAL_COUNT] = {
	{ 0, "Vantablack", "vantablack", HOWDARK_CLASS_ULTRA_BLACK, "ultra-black",
	  0.00080f, 0.00040f, 0.00070f, 0.00110f, 0.62f },
	{ 1, "Musou paint", "musou_paint", HOWDARK_CLASS_ULTRA_BLACK, "ultra-black coating",
	  0.00800f, 0.00350f, 0.00650f, 0.02200f, 0.74f },
	{ 2, "black velvet", "velvet", HOWDARK_CLASS_FABRIC, "fabric",
	  0.00150f, 0.00080f, 0.00130f, 0.00450f, 0.55f },
	{ 3, "Musou fabric", "musou_fabric", HOWDARK_CLASS_FABRIC, "fabric",
	  0.00090f, 0.00035f, 0.00065f, 0.00180f, 0.64f },
	{ 4, "acrylic paint", "acrylic", HOWDARK_CLASS_COATING, "coating",
	  0.01800f, 0.00800f, 0.01400f, 0.05500f, 0.68f },
	{ 5, "chalkboard paint", "chalkboard", HOWDARK_CLASS_COATING, "coating",
	  0.01200f, 0.00550f, 0.01000f, 0.03800f, 0.78f },
};

/*
 * Angular curves: fabrics + Vantablack keep low Rs; coatings rise at grazing.
 * Velvet has lowest mean TIS; acrylic/chalkboard THR elevated at low/mid θi.
 */
const howdark_curves_t howdark_curves[HOWDARK_MATERIAL_COUNT] = {
	/* 0 Vantablack */
	{
		{ 0.00070f, 0.00072f, 0.00075f, 0.00080f, 0.00090f, 0.00110f, 0.00130f, 0.00160f },
		{ 0.60f, 0.61f, 0.62f, 0.62f, 0.63f, 0.64f, 0.63f, 0.61f },
		{ 0.00010f, 0.00011f, 0.00012f, 0.00014f, 0.00018f, 0.00028f, 0.00035f, 0.00045f },
		{ 95.0f, 90.0f, 85.0f }
	},
	/* 1 Musou paint */
	{
		{ 0.00650f, 0.00680f, 0.00720f, 0.00800f, 0.00950f, 0.01300f, 0.01600f, 0.02200f },
		{ 0.78f, 0.77f, 0.76f, 0.74f, 0.72f, 0.68f, 0.65f, 0.60f },
		{ 0.00080f, 0.00090f, 0.00110f, 0.00150f, 0.00240f, 0.00450f, 0.00600f, 0.00900f },
		{ 85.0f, 75.0f, 62.0f }
	},
	/* 2 black velvet */
	{
		{ 0.00130f, 0.00135f, 0.00140f, 0.00150f, 0.00170f, 0.00210f, 0.00240f, 0.00290f },
		{ 0.52f, 0.53f, 0.54f, 0.55f, 0.56f, 0.57f, 0.56f, 0.54f },
		{ 0.00025f, 0.00028f, 0.00032f, 0.00040f, 0.00055f, 0.00085f, 0.00100f, 0.00130f },
		{ 88.0f, 78.0f, 65.0f }
	},
	/* 3 Musou fabric */
	{
		{ 0.00075f, 0.00078f, 0.00082f, 0.00090f, 0.00100f, 0.00120f, 0.00140f, 0.00170f },
		{ 0.62f, 0.63f, 0.64f, 0.64f, 0.65f, 0.66f, 0.65f, 0.63f },
		{ 0.00012f, 0.00013f, 0.00015f, 0.00018f, 0.00024f, 0.00035f, 0.00042f, 0.00055f },
		{ 96.0f, 91.0f, 84.0f }
	},
	/* 4 acrylic paint */
	{
		{ 0.01600f, 0.01650f, 0.01700f, 0.01800f, 0.02000f, 0.02800f, 0.03500f, 0.04800f },
		{ 0.72f, 0.71f, 0.70f, 0.68f, 0.64f, 0.55f, 0.50f, 0.42f },
		{ 0.00350f, 0.00400f, 0.00480f, 0.00600f, 0.00900f, 0.01600f, 0.02200f, 0.03200f },
		{ 65.0f, 35.0f, 15.0f }
	},
	/* 5 chalkboard paint */
	{
		{ 0.01050f, 0.01100f, 0.01150f, 0.01200f, 0.01400f, 0.01900f, 0.02400f, 0.03200f },
		{ 0.82f, 0.81f, 0.80f, 0.78f, 0.74f, 0.68f, 0.64f, 0.58f },
		{ 0.00200f, 0.00230f, 0.00280f, 0.00350f, 0.00500f, 0.00850f, 0.01100f, 0.01600f },
		{ 70.0f, 45.0f, 25.0f }
	},
};

int HowDark_MaterialCount( void )
{
	return HOWDARK_MATERIAL_COUNT;
}

const howdark_material_t *HowDark_GetMaterial( int id )
{
	if ( id < 0 || id >= HOWDARK_MATERIAL_COUNT ) {
		return NULL;
	}
	return &howdark_materials[id];
}

int HowDark_FindMaterial( const char *nameOrId )
{
	int i;
	int asInt;
	char buf[64];

	if ( !nameOrId || !nameOrId[0] ) {
		return -1;
	}

	/* Numeric id */
	asInt = 0;
	for ( i = 0; nameOrId[i]; i++ ) {
		if ( nameOrId[i] < '0' || nameOrId[i] > '9' ) {
			asInt = -1;
			break;
		}
		asInt = asInt * 10 + ( nameOrId[i] - '0' );
	}
	if ( asInt >= 0 && asInt < HOWDARK_MATERIAL_COUNT ) {
		return asInt;
	}

	howdark_tolower_copy( buf, sizeof( buf ), nameOrId );

	for ( i = 0; i < HOWDARK_MATERIAL_COUNT; i++ ) {
		if ( howdark_streq_ci( buf, howdark_materials[i].shortName ) ) {
			return i;
		}
		if ( howdark_streq_ci( buf, howdark_materials[i].name ) ) {
			return i;
		}
	}

	/* Loose aliases */
	if ( howdark_streq_ci( buf, "musou" ) || howdark_streq_ci( buf, "musoublack" ) ) {
		return 1;
	}
	if ( howdark_streq_ci( buf, "acryl" ) || howdark_streq_ci( buf, "acryl_paint" ) ) {
		return 4;
	}
	if ( howdark_streq_ci( buf, "chalk" ) || howdark_streq_ci( buf, "chalkb" ) ) {
		return 5;
	}
	if ( howdark_streq_ci( buf, "vanta" ) ) {
		return 0;
	}

	return -1;
}

float HowDark_Albedo( int id )
{
	const howdark_material_t *m = HowDark_GetMaterial( id );
	return m ? m->albedo : 0.0f;
}

void HowDark_LuminancePercentiles( int id, float *p1, float *p50, float *p99 )
{
	const howdark_material_t *m = HowDark_GetMaterial( id );

	if ( p1 ) {
		*p1 = m ? m->lumP1 : 0.0f;
	}
	if ( p50 ) {
		*p50 = m ? m->lumP50 : 0.0f;
	}
	if ( p99 ) {
		*p99 = m ? m->lumP99 : 0.0f;
	}
}

const char *HowDark_SelectAdvice( const char *useCase )
{
	char key[32];

	if ( !useCase || !useCase[0] ) {
		useCase = "optical";
	}
	howdark_tolower_copy( key, sizeof( key ), useCase );

	if ( howdark_streq_ci( key, "calibration" ) || howdark_streq_ci( key, "photometric" ) ||
		 howdark_streq_ci( key, "sensor" ) ) {
		return "Prefer ultra-black (Vantablack) or Musou fabric when residual "
			   "specular/off-specular must stay minimal across angles; coatings "
			   "are unsuitable near specular geometries.";
	}
	if ( howdark_streq_ci( key, "stray" ) || howdark_streq_ci( key, "baffle" ) ||
		 howdark_streq_ci( key, "shield" ) ) {
		return "Dark fabrics (Musou fabric, black velvet) offer robust stray-light "
			   "suppression with low angular reflectance when space/environment "
			   "allow; Vantablack is best attenuation but fragile/costly.";
	}
	if ( howdark_streq_ci( key, "aesthetic" ) || howdark_streq_ci( key, "design" ) ||
		 howdark_streq_ci( key, "visual" ) ) {
		return "Conventional matte coatings (acrylic, chalkboard) may suffice when "
			   "illumination/viewing avoid near-specular configs; Musou paint is "
			   "the strongest coating-class option at non-grazing angles.";
	}
	/* optical / default */
	return "Ultra-black materials give highest light attenuation; fabrics are a "
		   "robust alternative for optical benches; coatings are limited by "
		   "angle-dependent specular rise at grazing incidence (Filip & Vávra §6).";
}
