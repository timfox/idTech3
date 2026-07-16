/* Filesystem handle allocation and FILE* utility helpers.
 * Included by files.c to preserve static handle table access. */

/*
=================
FS_HandleForFile
=================
*/
static fileHandle_t	FS_HandleForFile( void ) 
{
	int		i;

	for ( i = 1 ; i < MAX_FILE_HANDLES ; i++ ) 
	{
		if ( fsh[i].handleFiles.file.v == NULL )
			return i;
	}

	Com_Error( ERR_DROP, "FS_HandleForFile: none free" );
	return FS_INVALID_HANDLE;
}


static FILE	*FS_FileForHandle( fileHandle_t f ) {
	if ( f <= 0 || f >= MAX_FILE_HANDLES ) {
		Com_Error( ERR_DROP, "FS_FileForHandle: out of range" );
	}
	if ( fsh[f].zipFile ) {
		Com_Error( ERR_DROP, "FS_FileForHandle: can't get FILE on zip file" );
	}
	if ( ! fsh[f].handleFiles.file.o ) {
		Com_Error( ERR_DROP, "FS_FileForHandle: NULL" );
	}
	
	return fsh[f].handleFiles.file.o;
}


void FS_ForceFlush( fileHandle_t f ) {
	FILE *file;

	file = FS_FileForHandle(f);
	setvbuf( file, NULL, _IONBF, 0 );
}


/*
================
FS_FileLengthByHandle

If this is called on a non-unique FILE (from a pak file),
it will return the size of the pak file, not the expected
size of the file.
================
*/
#if 0
static int FS_FileLengthByHandle( fileHandle_t f ) {
	int		pos;
	int		end;
	FILE*	h;

	h = FS_FileForHandle( f );
	pos = ftell( h );
	fseek( h, 0, SEEK_END );
	end = ftell( h );
	fseek( h, pos, SEEK_SET );

	return end;
}
#endif


/*
================
FS_FileLength
================
*/
static int FS_FileLength( FILE* h ) 
{
	int		pos;
	int		end;

	pos = ftell( h );
	fseek( h, 0, SEEK_END );
	end = ftell( h );
	fseek( h, pos, SEEK_SET );

	return end;
}


/*
====================
FS_PakIndexForHandle
====================
*/
int FS_PakIndexForHandle( fileHandle_t f ) {

	if ( f <= FS_INVALID_HANDLE || f >= MAX_FILE_HANDLES )
		return -1;

	return fsh[ f ].pakIndex;
}
