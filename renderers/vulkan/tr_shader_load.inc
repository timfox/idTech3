/* Shader file loading, text hash construction, and shader bootstrap.
 * Included by tr_shader.c to keep shader parser state file-local. */

#define	MAX_SHADER_FILES 16384

static int loadShaderBuffers( char **shaderFiles, const int numShaderFiles, char **buffers )
{
	char filename[MAX_QPATH+8];
	char shaderName[MAX_QPATH];
	const char *p, *token;
	long summand, sum = 0;
	int shaderLine;
	int i;
	const char *shaderStart;
	qboolean denyErrors;

	// load and parse shader files
	for ( i = 0; i < numShaderFiles; i++ )
	{

		// look for a .mtr file first
		if( vk.pbrActive ){
			char *ext;
			Com_sprintf( filename, sizeof( filename ), "scripts/%s", shaderFiles[i] );
			if ( (ext = strrchr(filename, '.')) )
			{
				Q_strncpyz( ext, ".mtr", sizeof( filename ) - (int)( ext - filename ) );
			}

			if ( ri.FS_ReadFile( filename, NULL ) <= 0 )
			{
				Com_sprintf( filename, sizeof( filename ), "scripts/%s", shaderFiles[i] );
			}
		}else{
			Com_sprintf(filename, sizeof(filename), "scripts/%s", shaderFiles[i]);
		}
		//ri.Printf( PRINT_DEVELOPER, "...loading '%s'\n", filename );
		summand = ri.FS_ReadFile( filename, (void **)&buffers[i] );

		if ( !buffers[i] )
			ri.Error( ERR_DROP, "Couldn't load %s", filename );

		// comment some buggy shaders from pak0
		if ( summand == 35910 && strcmp( shaderFiles[i], "sky.shader" ) == 0 )
		{
			if ( memcmp( buffers[i] + 0x3D3E, "\tcloudparms ", 12 ) == 0 )
			{
				memcpy( buffers[i] + 0x27D7, "/*", 2 );
				memcpy( buffers[i] + 0x2A93, "*/", 2 );

				memcpy( buffers[i] + 0x3CA9, "/*", 2 );
				memcpy( buffers[i] + 0x3FC2, "*/", 2 );
			}
		}
		else if ( summand == 116073 && strcmp( shaderFiles[i], "sfx.shader" ) == 0 )
		{
			if ( memcmp( buffers[i] + 93457, "textures/sfx/xfinalfog\r\n", 24 ) == 0 )
			{
				memcpy( buffers[i] + 93457, "/*", 2 );
				memcpy( buffers[i] + 93663, "*/", 2 );
			}
		}

		p = buffers[i];
		COM_BeginParseSession( filename );

		shaderStart = NULL;
		denyErrors = qfalse;

		while ( 1 )
		{
			token = COM_ParseExt( &p, qtrue );

			if ( !*token )
				break;

			Q_strncpyz( shaderName, token, sizeof( shaderName ) );
			shaderLine = COM_GetCurrentParseLine();

			token = COM_ParseExt( &p, qtrue );
			if ( token[0] != '{' || token[1] != '\0' )
			{
				ri.Printf( PRINT_DEVELOPER, "File %s: shader \"%s\" " \
					"on line %d missing opening brace", filename, shaderName, shaderLine );
				if ( token[0] )
					ri.Printf( PRINT_DEVELOPER, " (found \"%s\" on line %d)\n", token, COM_GetCurrentParseLine() );
				else
					ri.Printf( PRINT_DEVELOPER, "\n" );

				if ( denyErrors || !p )
				{
					ri.Printf( PRINT_WARNING, "Ignoring entire file '%s' due to error.\n", filename );
					ri.FS_FreeFile( buffers[i] );
					buffers[i] = NULL;
					break;
				}

				SkipRestOfLine( &p );
				shaderStart = p;
				continue;
			}

			if ( !SkipBracedSection( &p, 1 ) )
			{
				ri.Printf(PRINT_WARNING, "WARNING: Ignoring shader file %s. Shader \"%s\" " \
					"on line %d missing closing brace.\n", filename, shaderName, shaderLine );
				ri.FS_FreeFile( buffers[i] );
				buffers[i] = NULL;
				break;
			}

			denyErrors = qtrue;
		}

		if ( buffers[ i ] ) {
			if ( shaderStart ) {
				summand -= (shaderStart - buffers[i]);
				if ( summand >= 0 ) {
					memmove( buffers[i], shaderStart, summand + 1 );
				}
			}
			//sum += summand;
			sum += COM_Compress( buffers[ i ] );
		}
	}

	return sum;
}


/*
====================
ScanAndLoadShaderFiles

Finds and loads all .shader files, combining them into
a single large text block that can be scanned for shader names
=====================
*/
static void ScanAndLoadShaderFiles( void )
{
	char **shaderFiles, **shaderxFiles;
	char *buffers[MAX_SHADER_FILES];
	char *xbuffers[MAX_SHADER_FILES];
	int numShaderFiles, numShaderxFiles;
	int i;
	const char *token;
	char *hashMem;
	char *textEnd;
	const char *p, *oldp;
	int shaderTextHashTableSizes[MAX_SHADERTEXT_HASH], hash, size;

	long sum = 0;

	// scan for legacy shader files
	shaderFiles = ri.FS_ListFiles( "scripts", ".shader", &numShaderFiles );

	shaderxFiles = ri.FS_ListFiles( "scripts", ".shaderx", &numShaderxFiles );

	if ( (!shaderFiles || !numShaderFiles) && (!shaderxFiles || !numShaderxFiles) ) {
		ri.Printf( PRINT_WARNING, "WARNING: no shader files found\n" );
		return;
	}

	if ( numShaderFiles > MAX_SHADER_FILES ) {
		numShaderFiles = MAX_SHADER_FILES;
	}
	if ( numShaderxFiles > MAX_SHADER_FILES ) {
		numShaderxFiles = MAX_SHADER_FILES;
	}

	sum = 0;
	sum += loadShaderBuffers( shaderxFiles, numShaderxFiles, xbuffers );
	sum += loadShaderBuffers( shaderFiles, numShaderFiles, buffers );

	// build single large buffer
	s_shaderText = ri.Hunk_Alloc( sum + numShaderxFiles*2 + numShaderFiles*2 + 1, h_low );
	s_shaderText[ 0 ] = s_shaderText[ sum + numShaderxFiles*2 + numShaderFiles*2 ] = '\0';

	textEnd = s_shaderText;

	// free in reverse order, so the temp files are all dumped
	// legacy shaders
	for ( i = numShaderFiles - 1; i >= 0 ; i-- ) {
		if ( buffers[ i ] ) {
			textEnd = Q_stradd( textEnd, buffers[ i ] );
			textEnd = Q_stradd( textEnd, "\n" );
			ri.FS_FreeFile( buffers[ i ] );
		}
	}

	// if shader text >= s_extensionOffset then it is an extended shader
	// normal shaders will never encounter that
	s_extensionOffset = textEnd;

	// extended shaders
	for ( i = numShaderxFiles - 1; i >= 0 ; i-- ) {
		if ( xbuffers[ i ] ) {
			textEnd = Q_stradd( textEnd, xbuffers[ i ] );
			textEnd = Q_stradd( textEnd, "\n" );
			ri.FS_FreeFile( xbuffers[ i ] );
		}
	}

	// free up memory
	if ( shaderxFiles )
		ri.FS_FreeFileList( shaderxFiles );
	if ( shaderFiles )
		ri.FS_FreeFileList( shaderFiles );

	//COM_Compress( s_shaderText );
	Com_Memset( shaderTextHashTableSizes, 0, sizeof( shaderTextHashTableSizes ) );
	size = 0;

	p = s_shaderText;
	// look for shader names
	while ( 1 ) {
		token = COM_ParseExt( &p, qtrue );
		if ( token[0] == 0 ) {
			break;
		}
		hash = generateHashValue(token, MAX_SHADERTEXT_HASH);
		shaderTextHashTableSizes[hash]++;
		size++;
		SkipBracedSection(&p, 0);
	}

	size += MAX_SHADERTEXT_HASH;

	hashMem = ri.Hunk_Alloc( size * sizeof(char *), h_low );

	for (i = 0; i < MAX_SHADERTEXT_HASH; i++) {
		shaderTextHashTable[i] = (const char **) hashMem;
		hashMem = hashMem + ((shaderTextHashTableSizes[i] + 1) * sizeof(char *));
	}

	p = s_shaderText;
	// look for shader names
	while ( 1 ) {
		oldp = p;
		token = COM_ParseExt( &p, qtrue );
		if ( token[0] == 0 ) {
			break;
		}

		hash = generateHashValue(token, MAX_SHADERTEXT_HASH);
		shaderTextHashTable[hash][--shaderTextHashTableSizes[hash]] = oldp;

		SkipBracedSection(&p, 0);
	}
}


/*
====================
CreateInternalShaders
====================
*/
static void CreateInternalShaders( void ) {
	tr.numShaders = 0;

	// init the default shader
	InitShader( "<default>", LIGHTMAP_NONE );
	stages[0].bundle[0].image[0] = tr.defaultImage;
	stages[0].bundle[0].tcGen = TCGEN_TEXTURE;
	stages[0].active = qtrue;
	stages[0].stateBits = GLS_DEFAULT;
	tr.defaultShader = FinishShader();

	InitShader( "<white>", LIGHTMAP_NONE );
	stages[0].bundle[0].image[0] = tr.whiteImage;
	stages[0].bundle[0].tcGen = TCGEN_TEXTURE;
	stages[0].active = qtrue;
	stages[0].bundle[0].rgbGen = CGEN_EXACT_VERTEX;
	stages[0].stateBits = GLS_DEPTHTEST_DISABLE | GLS_SRCBLEND_SRC_ALPHA | GLS_DSTBLEND_ONE_MINUS_SRC_ALPHA;
	tr.whiteShader = FinishShader();

	// shadow shader is just a marker
	InitShader( "<stencil shadow>", LIGHTMAP_NONE );
	stages[0].bundle[0].image[0] = tr.defaultImage;
	stages[0].bundle[0].tcGen = TCGEN_TEXTURE;
	stages[0].active = qtrue;
	stages[0].stateBits = GLS_DEFAULT;
	shader.sort = SS_STENCIL_SHADOW;
	tr.shadowShader = FinishShader();

	InitShader( "<cinematic>", LIGHTMAP_NONE );
	stages[0].bundle[0].image[0] = tr.defaultImage; // will be updated by specific cinematic images
	stages[0].bundle[0].tcGen = TCGEN_TEXTURE;
	stages[0].active = qtrue;
	stages[0].bundle[0].rgbGen = CGEN_IDENTITY_LIGHTING;
	stages[0].stateBits = GLS_DEPTHTEST_DISABLE | GLS_SRCBLEND_SRC_ALPHA | GLS_DSTBLEND_ONE_MINUS_SRC_ALPHA;
	tr.cinematicShader = FinishShader();

	InitShader( "<vectorfont>", LIGHTMAP_NONE );
	stages[0].bundle[0].image[0] = tr.whiteImage;
	stages[0].bundle[0].tcGen = TCGEN_TEXTURE;
	stages[0].active = qtrue;
	stages[0].bundle[0].rgbGen = CGEN_VERTEX;
	stages[0].uiVectorText = 1u;
	stages[0].stateBits = GLS_DEPTHTEST_DISABLE | GLS_SRCBLEND_SRC_ALPHA | GLS_DSTBLEND_ONE_MINUS_SRC_ALPHA;
	tr.vectorFontShader = FinishShader();
}


/*
====================
CreateExternalShaders
====================
*/
static void CreateExternalShaders( void ) {
	tr.projectionShadowShader = R_FindShader( "projectionShadow", LIGHTMAP_NONE, qtrue );
	tr.flareShader = R_FindShader( "flareShader", LIGHTMAP_NONE, qtrue );

	// Hack to make fogging work correctly on flares. Fog colors are calculated
	// in tr_flare.c already.
	if(!tr.flareShader->defaultShader)
	{
		int index;

		for(index = 0; index < tr.flareShader->numUnfoggedPasses; index++)
		{
			tr.flareShader->stages[index]->bundle[0].adjustColorsForFog = ACFF_NONE;
			tr.flareShader->stages[index]->stateBits |= GLS_DEPTHTEST_DISABLE;
		}
	}
}


/*
==================
R_InitShaders
==================
*/
void R_InitShaders( void ) {
	ri.Printf( PRINT_ALL, "Initializing Shaders\n" );

	Com_Memset(hashTable, 0, sizeof(hashTable));

	CreateInternalShaders();

	ScanAndLoadShaderFiles();

	CreateExternalShaders();
}
