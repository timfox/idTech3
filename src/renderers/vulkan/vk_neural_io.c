/*
===========================================================================
Shared neural asset I/O (weights + volumes) for experimental Vulkan passes.
===========================================================================
*/

#include "tr_local.h"
#include "vk_neural_io.h"

qboolean vk_neural_load_mlp_rgb( uint32_t magic, const char *path, int featureDim, int hiddenDim,
	float *W1, int w1_stride, float *b1, float *W2, float *b2, int max_feature, int max_hidden )
{
	byte *buf;
	int len;
	uint32_t fileMagic;
	uint16_t fdim, hdim;
	float *dst;
	int count, i;

	if ( !path || !path[0] || !W1 || !b1 || !W2 || !b2 ) {
		return qfalse;
	}
	if ( featureDim < 1 || hiddenDim < 1 || featureDim > max_feature || hiddenDim > max_hidden ) {
		return qfalse;
	}

	len = ri.FS_ReadFile( path, (void **)&buf );
	if ( len < 16 || !buf ) {
		return qfalse;
	}

	Com_Memcpy( &fileMagic, buf, sizeof( fileMagic ) );
	if ( fileMagic != magic ) {
		ri.FS_FreeFile( buf );
		return qfalse;
	}

	Com_Memcpy( &fdim, buf + 4, sizeof( fdim ) );
	Com_Memcpy( &hdim, buf + 6, sizeof( hdim ) );
	if ( fdim != (uint16_t)featureDim || hdim != (uint16_t)hiddenDim ) {
		ri.Printf( PRINT_WARNING, "[NeuralIO] RGB weights dim mismatch in %s\n", path );
		ri.FS_FreeFile( buf );
		return qfalse;
	}

	dst = (float *)( buf + 16 );
	count = ( hiddenDim * featureDim ) + hiddenDim + ( 3 * hiddenDim ) + 3;
	if ( len < 16 + count * (int)sizeof( float ) ) {
		ri.FS_FreeFile( buf );
		return qfalse;
	}

	for ( i = 0; i < hiddenDim * w1_stride; i++ ) {
		W1[i] = 0.0f;
	}
	Com_Memcpy( W1, dst, (size_t)( hiddenDim * featureDim ) * sizeof( float ) );
	dst += hiddenDim * featureDim;
	Com_Memcpy( b1, dst, (size_t)hiddenDim * sizeof( float ) );
	dst += hiddenDim;
	Com_Memcpy( W2, dst, (size_t)( 3 * hiddenDim ) * sizeof( float ) );
	dst += 3 * hiddenDim;
	Com_Memcpy( b2, dst, 3 * sizeof( float ) );

	ri.FS_FreeFile( buf );
	ri.Printf( PRINT_ALL, "[NeuralIO] Loaded RGB MLP %s\n", path );
	return qtrue;
}

qboolean vk_neural_load_mlp_scalar( uint32_t magic, const char *path, int featureDim, int hiddenDim,
	float *W1, int w1_stride, float *b1, float *W2, float *b2, int max_feature, int max_hidden )
{
	byte *buf;
	int len;
	uint32_t fileMagic;
	uint16_t fdim, hdim;
	float *dst;
	int count, i;

	if ( !path || !path[0] || !W1 || !b1 || !W2 || !b2 ) {
		return qfalse;
	}
	if ( featureDim < 1 || hiddenDim < 1 || featureDim > max_feature || hiddenDim > max_hidden ) {
		return qfalse;
	}

	len = ri.FS_ReadFile( path, (void **)&buf );
	if ( len < 16 || !buf ) {
		return qfalse;
	}

	Com_Memcpy( &fileMagic, buf, sizeof( fileMagic ) );
	if ( fileMagic != magic ) {
		ri.FS_FreeFile( buf );
		return qfalse;
	}

	Com_Memcpy( &fdim, buf + 4, sizeof( fdim ) );
	Com_Memcpy( &hdim, buf + 6, sizeof( hdim ) );
	if ( fdim != (uint16_t)featureDim || hdim != (uint16_t)hiddenDim ) {
		ri.Printf( PRINT_WARNING, "[NeuralIO] Scalar weights dim mismatch in %s\n", path );
		ri.FS_FreeFile( buf );
		return qfalse;
	}

	dst = (float *)( buf + 16 );
	count = ( hiddenDim * featureDim ) + hiddenDim + hiddenDim + 1;
	if ( len < 16 + count * (int)sizeof( float ) ) {
		ri.FS_FreeFile( buf );
		return qfalse;
	}

	for ( i = 0; i < hiddenDim * w1_stride; i++ ) {
		W1[i] = 0.0f;
	}
	Com_Memcpy( W1, dst, (size_t)( hiddenDim * featureDim ) * sizeof( float ) );
	dst += hiddenDim * featureDim;
	Com_Memcpy( b1, dst, (size_t)hiddenDim * sizeof( float ) );
	dst += hiddenDim;
	Com_Memcpy( W2, dst, (size_t)hiddenDim * sizeof( float ) );
	dst += hiddenDim;
	*b2 = *dst;

	ri.FS_FreeFile( buf );
	ri.Printf( PRINT_ALL, "[NeuralIO] Loaded scalar MLP %s\n", path );
	return qtrue;
}

qboolean vk_neural_load_volume_f32( uint32_t magic, const char *path,
	int *gridX, int *gridY, int *gridZ, int *featureDim, float *out, size_t out_floats )
{
	byte *buf;
	int len;
	uint32_t fileMagic;
	uint16_t gx, gy, gz, fd;
	size_t need;

	if ( !path || !path[0] || !out || !gridX || !gridY || !gridZ || !featureDim ) {
		return qfalse;
	}

	len = ri.FS_ReadFile( path, (void **)&buf );
	if ( len < 12 || !buf ) {
		return qfalse;
	}

	Com_Memcpy( &fileMagic, buf, sizeof( fileMagic ) );
	if ( fileMagic != magic ) {
		ri.FS_FreeFile( buf );
		return qfalse;
	}

	Com_Memcpy( &gx, buf + 4, sizeof( gx ) );
	Com_Memcpy( &gy, buf + 6, sizeof( gy ) );
	Com_Memcpy( &gz, buf + 8, sizeof( gz ) );
	Com_Memcpy( &fd, buf + 10, sizeof( fd ) );

	if ( gx < 4 || gy < 4 || gz < 4 || fd < 1 ) {
		ri.FS_FreeFile( buf );
		return qfalse;
	}

	need = (size_t)gx * (size_t)gy * (size_t)gz * (size_t)fd;
	if ( need != out_floats || len < 12 + (int)( need * sizeof( float ) ) ) {
		ri.Printf( PRINT_WARNING,
			"[NeuralIO] Volume %s: file %ux%ux%ux%d needs %zu floats, buffer has %zu\n",
			path, gx, gy, gz, fd, need, out_floats );
		ri.FS_FreeFile( buf );
		return qfalse;
	}

	*gridX = gx;
	*gridY = gy;
	*gridZ = gz;
	*featureDim = fd;
	Com_Memcpy( out, buf + 12, need * sizeof( float ) );
	ri.FS_FreeFile( buf );
	ri.Printf( PRINT_ALL, "[NeuralIO] Loaded volume %s (%ux%ux%u dim=%d)\n", path, gx, gy, gz, fd );
	return qtrue;
}
