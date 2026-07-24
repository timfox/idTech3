/*
 * Offline: load surf_aztec.bsp and measure exterior-triangle rate for fan vs ear-clip.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>

#include "tr_bsp30_triangulate.h"
#include "../../engine/core/qfiles_bsp30.h"

static int32_t le32( int32_t v ) {
	unsigned char *p = (unsigned char *)&v;
	return (int32_t)( p[0] | ( p[1] << 8 ) | ( p[2] << 16 ) | ( p[3] << 24 ) );
}
static int16_t le16( int16_t v ) {
	unsigned char *p = (unsigned char *)&v;
	return (int16_t)( p[0] | ( p[1] << 8 ) );
}
static float lef( float v ) {
	uint32_t u;
	memcpy( &u, &v, 4 );
	u = (uint32_t)le32( (int32_t)u );
	memcpy( &v, &u, 4 );
	return v;
}

static void fan( int n, int *idx ) {
	int j;
	for ( j = 0; j < n - 2; j++ ) {
		idx[j * 3 + 0] = 0;
		idx[j * 3 + 1] = j + 1;
		idx[j * 3 + 2] = j + 2;
	}
}

int main( int argc, char **argv ) {
	const char *path = argc > 1 ? argv[1] : "/home/tim/Desktop/Surf/maps/surf_aztec.bsp";
	FILE *f = fopen( path, "rb" );
	uint8_t *buf;
	long size;
	bsp30_header_t *hdr;
	bsp30_face_t *faces;
	bsp30_vertex_t *verts;
	bsp30_edge_t *edges;
	int32_t *surfedges;
	int nfaces, nverts, nedges, nsurfedges;
	int i, fanBad = 0, earBad = 0, earOk = 0, fanOk = 0, skipped = 0;
	int maxPts = 0, exteriorWorst = 0;

	if ( !f ) {
		fprintf( stderr, "cannot open %s\n", path );
		return 1;
	}
	fseek( f, 0, SEEK_END );
	size = ftell( f );
	fseek( f, 0, SEEK_SET );
	buf = (uint8_t *)malloc( (size_t)size );
	if ( fread( buf, 1, (size_t)size, f ) != (size_t)size ) {
		fprintf( stderr, "read fail\n" );
		return 1;
	}
	fclose( f );
	hdr = (bsp30_header_t *)buf;
	if ( le32( hdr->version ) != 30 ) {
		fprintf( stderr, "not bsp30\n" );
		return 1;
	}

	{
		bsp30_lump_t *L = &hdr->lumps[BSP30_LUMP_FACES];
		faces = (bsp30_face_t *)( buf + le32( L->fileofs ) );
		nfaces = le32( L->filelen ) / (int)sizeof( bsp30_face_t );
	}
	{
		bsp30_lump_t *L = &hdr->lumps[BSP30_LUMP_VERTEXES];
		verts = (bsp30_vertex_t *)( buf + le32( L->fileofs ) );
		nverts = le32( L->filelen ) / (int)sizeof( bsp30_vertex_t );
	}
	{
		bsp30_lump_t *L = &hdr->lumps[BSP30_LUMP_EDGES];
		edges = (bsp30_edge_t *)( buf + le32( L->fileofs ) );
		nedges = le32( L->filelen ) / (int)sizeof( bsp30_edge_t );
	}
	{
		bsp30_lump_t *L = &hdr->lumps[BSP30_LUMP_SURFEDGES];
		surfedges = (int32_t *)( buf + le32( L->fileofs ) );
		nsurfedges = le32( L->filelen ) / (int)sizeof( int32_t );
	}

	printf( "map=%s faces=%d verts=%d\n", path, nfaces, nverts );

	for ( i = 0; i < nfaces; i++ ) {
		int numPoints = le16( faces[i].numedges );
		int firstEdge = le32( faces[i].firstedge );
		float xyz[BSP30_TRIANGULATE_MAX_POINTS * 3];
		int ear[BSP30_TRIANGULATE_MAX_POINTS * 3];
		int fanIdx[BSP30_TRIANGULATE_MAX_POINTS * 3];
		int j, nIdx, earInside, fanInside;

		if ( numPoints < 3 ) {
			skipped++;
			continue;
		}
		if ( numPoints > maxPts ) {
			maxPts = numPoints;
		}
		if ( numPoints > BSP30_TRIANGULATE_MAX_POINTS ) {
			skipped++;
			continue;
		}
		for ( j = 0; j < numPoints; j++ ) {
			int se = firstEdge + j;
			int edgeIndex, vi;
			if ( se < 0 || se >= nsurfedges ) {
				fprintf( stderr, "face %d bad surfedge\n", i );
				return 1;
			}
			edgeIndex = le32( surfedges[se] );
			if ( edgeIndex >= 0 ) {
				vi = le16( (int16_t)edges[edgeIndex].v[0] );
			} else {
				vi = le16( (int16_t)edges[-edgeIndex].v[1] );
			}
			if ( vi < 0 || vi >= nverts ) {
				fprintf( stderr, "face %d bad vert %d\n", i, vi );
				return 1;
			}
			xyz[j * 3 + 0] = lef( verts[vi].point[0] );
			xyz[j * 3 + 1] = lef( verts[vi].point[1] );
			xyz[j * 3 + 2] = lef( verts[vi].point[2] );
		}

		nIdx = ( numPoints - 2 ) * 3;
		fan( numPoints, fanIdx );
		fanInside = R_Bsp30_TriangleCentroidInside( xyz, numPoints, fanIdx, nIdx );
		if ( fanInside ) {
			fanOk++;
		} else {
			fanBad++;
		}

		nIdx = R_Bsp30_TriangulateFace( xyz, numPoints, ear, nIdx );
		earInside = ( nIdx > 0 ) &&
			R_Bsp30_TriangleCentroidInside( xyz, numPoints, ear, nIdx );
		if ( earInside ) {
			earOk++;
		} else {
			earBad++;
			if ( !fanInside ) {
				exteriorWorst++;
			}
		}
	}

	printf( "maxPoints=%d skipped=%d\n", maxPts, skipped );
	printf( "fan  ok=%d bad=%d (%.1f%% exterior)\n", fanOk, fanBad,
		100.0 * fanBad / ( ( fanOk + fanBad ) > 0 ? ( fanOk + fanBad ) : 1 ) );
	printf( "ear  ok=%d bad=%d (%.1f%% exterior)\n", earOk, earBad,
		100.0 * earBad / ( ( earOk + earBad ) > 0 ? ( earOk + earBad ) : 1 ) );
	printf( "both exterior=%d\n", exteriorWorst );
	return earBad > 0 ? 2 : 0;
}
