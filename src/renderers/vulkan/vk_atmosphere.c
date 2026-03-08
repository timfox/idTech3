#include "tr_local.h"
#include "vk_atmosphere.h"
#include "vk_postfx.h"
#include <math.h>

void vk_atmosphere_build_push_constants( vkAtmospherePushConstants_t *pc )
{
	float tanHalfX;
	float tanHalfY;

	if ( !pc ) {
		return;
	}

	Com_Memset( pc, 0, sizeof( *pc ) );

	if ( tr.sunDirection[0] != 0.0f || tr.sunDirection[1] != 0.0f || tr.sunDirection[2] != 0.0f ) {
		pc->sunDir[0] = tr.sunDirection[0];
		pc->sunDir[1] = tr.sunDirection[1];
		pc->sunDir[2] = tr.sunDirection[2];
	} else {
		PostFX_Atmosphere_GetSunDirection( &pc->sunDir[0], &pc->sunDir[1], &pc->sunDir[2] );
	}
	pc->sunDir[3] = 0.0f;
	pc->sunColor[0] = 1.0f;
	pc->sunColor[1] = 1.0f;
	pc->sunColor[2] = 1.0f;
	pc->sunColor[3] = PostFX_Atmosphere_GetSunIntensity();
	pc->rayleigh[0] = 5.5e-6f;
	pc->rayleigh[1] = 13.0e-6f;
	pc->rayleigh[2] = 22.4e-6f;
	pc->rayleigh[3] = 1.0f;
	pc->mie[0] = 21e-6f;
	pc->mie[1] = PostFX_Atmosphere_GetMieG();
	pc->mie[2] = 0.0f;
	pc->mie[3] = 0.0f;
	pc->atmParams[0] = PostFX_Atmosphere_GetRayleighHeight();
	pc->atmParams[1] = PostFX_Atmosphere_GetMieHeight();
	pc->atmParams[2] = 0.0f;
	pc->atmParams[3] = PostFX_Atmosphere_GetScale();
	pc->viewOrigin[0] = backEnd.viewParms.or.origin[0];
	pc->viewOrigin[1] = backEnd.viewParms.or.origin[1];
	pc->viewOrigin[2] = backEnd.viewParms.or.origin[2];
	pc->viewOrigin[3] = 1.0f;
	pc->viewForward[0] = backEnd.viewParms.or.axis[0][0];
	pc->viewForward[1] = backEnd.viewParms.or.axis[0][1];
	pc->viewForward[2] = backEnd.viewParms.or.axis[0][2];
	pc->viewForward[3] = 0.0f;
	pc->viewRight[0] = backEnd.viewParms.or.axis[1][0];
	pc->viewRight[1] = backEnd.viewParms.or.axis[1][1];
	pc->viewRight[2] = backEnd.viewParms.or.axis[1][2];
	pc->viewRight[3] = 0.0f;
	pc->viewUp[0] = backEnd.viewParms.or.axis[2][0];
	pc->viewUp[1] = backEnd.viewParms.or.axis[2][1];
	pc->viewUp[2] = backEnd.viewParms.or.axis[2][2];
	pc->viewUp[3] = 0.0f;

	tanHalfX = tanf( DEG2RAD( backEnd.viewParms.fovX * 0.5f ) );
	tanHalfY = tanf( DEG2RAD( backEnd.viewParms.fovY * 0.5f ) );
	pc->viewParams[0] = tanHalfX > 0.0f ? tanHalfX : 1.0f;
	pc->viewParams[1] = tanHalfY > 0.0f ? tanHalfY : 1.0f;
	pc->viewParams[2] = 0.0f;
	pc->viewParams[3] = 0.0f;
}
