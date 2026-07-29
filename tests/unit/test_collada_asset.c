/*
 * Unit test: engine-owned Collada asset support.
 * Run: ctest -R unit_collada_asset
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "collada_public.h"

#define ASSERT(cond, msg) do { \
	if (!(cond)) { \
		fprintf(stderr, "FAIL: %s\n", msg); \
		return 1; \
	} \
} while (0)

typedef struct outputBuffer_s {
	char *data;
	size_t size;
	size_t capacity;
} outputBuffer_t;

static void CaptureOutput( void *userData, const char *data, unsigned length ) {
	outputBuffer_t *buffer = (outputBuffer_t *)userData;
	char *newData;
	size_t required = buffer->size + length;

	if ( required > buffer->capacity ) {
		size_t newCapacity = buffer->capacity ? buffer->capacity * 2 : 4096;
		while ( newCapacity < required ) {
			newCapacity *= 2;
		}
		newData = (char *)realloc( buffer->data, newCapacity );
		if ( !newData ) {
			return;
		}
		buffer->data = newData;
		buffer->capacity = newCapacity;
	}

	memcpy( buffer->data + buffer->size, data, length );
	buffer->size += length;
}

static char *ReadWholeFile( const char *path ) {
	FILE *file;
	long length;
	char *data;

	file = fopen( path, "rb" );
	if ( !file ) {
		return NULL;
	}
	if ( fseek( file, 0, SEEK_END ) != 0 ) {
		fclose( file );
		return NULL;
	}
	length = ftell( file );
	if ( length < 0 || fseek( file, 0, SEEK_SET ) != 0 ) {
		fclose( file );
		return NULL;
	}

	data = (char *)malloc( (size_t)length + 1 );
	if ( !data ) {
		fclose( file );
		return NULL;
	}
	if ( fread( data, 1, (size_t)length, file ) != (size_t)length ) {
		free( data );
		fclose( file );
		return NULL;
	}
	data[length] = '\0';
	fclose( file );
	return data;
}

static const char *const kJav2Bones[] = {
	"Bip01_Pelvis", "Bip01_Spine", "Bip01_Spine1", "Bip01_Neck",
	"Bip01_Head", "Bip01_HeadNub", "helmet", "head_extra", "head",
	"head_dude", "Bip01_L_Clavicle", "Bip01_L_UpperArm",
	"Bip01_L_Forearm", "Bip01_L_Hand", "Bip01_L_Finger0",
	"Bip01_L_Finger0Nub", "shield", "l_hand", "l_forearm",
	"l_shoulder", "Bip01_R_Clavicle", "Bip01_R_UpperArm",
	"Bip01_R_Forearm", "Bip01_R_Hand", "Bip01_R_Finger0",
	"Bip01_R_Finger0Nub", "r_hand", "jav", "r_forearm",
	"r_shoulder", "shoulders", "chest", "back", "Bip01_L_Thigh",
	"Bip01_L_Calf", "Bip01_L_Foot", "Bip01_L_Toe0",
	"Bip01_L_Toe0Nub", "l_leg", "Bip01_R_Thigh", "Bip01_R_Calf",
	"Bip01_R_Foot", "Bip01_R_Toe0", "Bip01_R_Toe0Nub", "r_leg",
	"r_hip", "hip", "l_hip"
};

static void AppendText( outputBuffer_t *buffer, const char *text ) {
	CaptureOutput( buffer, text, (unsigned)strlen( text ) );
}

static char *CreateJav2SkeletonDefinitions( void ) {
	outputBuffer_t buffer = { 0 };
	size_t i;

	AppendText( &buffer, "<skeletons><standard_skeleton id=\"jav2-test\">" );
	for ( i = 0; i < sizeof( kJav2Bones ) / sizeof( kJav2Bones[0] ); ++i ) {
		AppendText( &buffer, "<bone name=\"" );
		AppendText( &buffer, kJav2Bones[i] );
		AppendText( &buffer, "\"/>" );
	}
	AppendText( &buffer, "</standard_skeleton><skeleton target=\"jav2-test\"><identifier><root>Bip01_Pelvis</root></identifier>" );
	for ( i = 0; i < sizeof( kJav2Bones ) / sizeof( kJav2Bones[0] ); ++i ) {
		AppendText( &buffer, "<bone name=\"" );
		AppendText( &buffer, kJav2Bones[i] );
		AppendText( &buffer, "\"><target>" );
		AppendText( &buffer, kJav2Bones[i] );
		AppendText( &buffer, "</target></bone>" );
	}
	AppendText( &buffer, "</skeleton></skeletons>" );
	CaptureOutput( &buffer, "", 1 );
	return buffer.data;
}

int main(void) {
	char path[128];
	char *meshDae;
	char *animDae;
	char *skeletonsXml;
	outputBuffer_t meshOutput = { 0 };
	outputBuffer_t animOutput = { 0 };

	ASSERT(Collada_IsSourcePath("art/meshes/unit.dae") == 1, "detect dae source");
	ASSERT(Collada_IsSourcePath("art/meshes/unit.DAE") == 1, "detect uppercase dae source");
	ASSERT(Collada_IsSourcePath("art/meshes/unit.pmd") == 0, "reject runtime mesh as source");
	ASSERT(Collada_ClassifyRuntimePath("art/meshes/unit.pmd") == COLLADA_ASSET_MESH, "classify pmd mesh");
	ASSERT(Collada_ClassifyRuntimePath("art/anims/walk.psa") == COLLADA_ASSET_ANIMATION, "classify psa animation");
	ASSERT(Collada_ClassifyRuntimePath("art/anims/walk.dae") == COLLADA_ASSET_UNKNOWN, "dae is not runtime output");

	ASSERT(Collada_GetRuntimePath("art/meshes/unit.dae", COLLADA_ASSET_MESH, path, sizeof(path)) == 1, "mesh runtime path");
	ASSERT(strcmp(path, "art/meshes/unit.pmd") == 0, "mesh runtime extension");
	ASSERT(Collada_GetRuntimePath("art/anims/walk.dae", COLLADA_ASSET_ANIMATION, path, sizeof(path)) == 1, "animation runtime path");
	ASSERT(strcmp(path, "art/anims/walk.psa") == 0, "animation runtime extension");
	ASSERT(Collada_GetRuntimePath("art/meshes/unit.pmd", COLLADA_ASSET_MESH, path, sizeof(path)) == 0, "reject non-source conversion");
	ASSERT(Collada_GetRuntimePath("art/meshes/unit.dae", COLLADA_ASSET_MESH, path, 8) == 0, "reject short output buffer");
	ASSERT(Collada_ConvertDaeToPmd("<html>This is not COLLADA</html>", NULL, NULL) != 0, "converter rejects invalid mesh dae");
	ASSERT(Collada_ConvertDaeToPsa("<html>This is not COLLADA</html>", NULL, NULL) != 0, "converter rejects invalid animation dae");

	meshDae = ReadWholeFile( "/home/tim/Desktop/rts/binaries/data/mods/_test.dae/art/meshes/jav2.dae" );
	animDae = ReadWholeFile( "/home/tim/Desktop/rts/binaries/data/mods/_test.dae/art/animation/jav2.dae" );
	skeletonsXml = CreateJav2SkeletonDefinitions();
	ASSERT( meshDae != NULL, "read real 0ad mesh dae fixture" );
	ASSERT( animDae != NULL, "read real 0ad animation dae fixture" );
	ASSERT( skeletonsXml != NULL, "read real 0ad skeleton definitions fixture" );
	ASSERT( Collada_SetSkeletonDefinitions( skeletonsXml, (int)strlen( skeletonsXml ) ) == 0, "load real 0ad skeleton definitions" );

	ASSERT( Collada_ConvertDaeToPmd( meshDae, CaptureOutput, &meshOutput ) == 0, "convert real 0ad mesh dae to pmd" );
	ASSERT( meshOutput.size > 8, "real pmd output is non-empty" );
	ASSERT( memcmp( meshOutput.data, "PSMD", 4 ) == 0, "real pmd output signature" );

	ASSERT( Collada_ConvertDaeToPsa( animDae, CaptureOutput, &animOutput ) == 0, "convert real 0ad animation dae to psa" );
	ASSERT( animOutput.size > 8, "real psa output is non-empty" );
	ASSERT( memcmp( animOutput.data, "PSSA", 4 ) == 0, "real psa output signature" );

	free( meshDae );
	free( animDae );
	free( skeletonsXml );
	free( meshOutput.data );
	free( animOutput.data );

	printf("PASS: unit_collada_asset\n");
	return 0;
}
