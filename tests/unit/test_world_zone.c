#include "world_zone.h"

#define CHECK(condition) do { if ( !(condition) ) return 1; } while ( 0 )

int main( void ) {
	worldZone_t zones[2] = { 0 };
	vec3_t inside = { 10.0f, 10.0f, 10.0f };
	vec3_t outside = { 5000.0f, 5000.0f, 0.0f };

	WorldZone_Init();
	zones[0].active = qtrue;
	Q_strncpyz( zones[0].name, "Atrium", sizeof( zones[0].name ) );
	VectorSet( zones[0].boundsMin, 0, 0, 0 );
	VectorSet( zones[0].boundsMax, 100, 100, 100 );
	zones[0].loadRadius = 256.0f;
	zones[0].unloadRadius = 512.0f;
	WorldZone_Import( 1, zones );
	CHECK( WorldZone_GetCount() == 1 );
	CHECK( WorldZone_FindAtPoint( inside ) == 0 );
	CHECK( WorldZone_FindAtPoint( outside ) == -1 );
	WorldZone_UpdateView( inside );
	CHECK( WorldZone_Get( 0 )->state == WZ_STATE_RESIDENT );
	WorldZone_UpdateView( outside );
	CHECK( WorldZone_Get( 0 )->state == WZ_STATE_INACTIVE );
	WorldZone_Shutdown();
	return 0;
}
