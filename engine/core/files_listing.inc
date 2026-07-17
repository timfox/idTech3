/* Filesystem listing, mod discovery, and directory console commands.
 * Included by files.c to preserve static search-path access. */

/*
===============
FS_ListFilteredFiles

Returns a unique list of files that match the given criteria
from all search paths
===============
*/
static char **FS_ListFilteredFiles( const char *path, const char *extension, const char *filter, int *numfiles, int flags ) {
	int				nfiles;
	char			**listCopy;
	char			*list[MAX_FOUND_FILES];
	const searchpath_t	*search;
	int				i;
	int				pathLength;
	int				extLen;
	int				length, pathDepth, temp;
	pack_t			*pak;
	fileInPack_t	*buildBuffer;
	char			zpath[MAX_ZPATH];
	qboolean		hasPatterns;
	const char		*x;

	if ( !fs_searchpaths ) {
		Com_Error( ERR_FATAL, "Filesystem call made without initialization" );
	}

	if  ( fs_numServerPaks && ( flags & FS_MATCH_STICK ) == 0 ) {
		flags &= ~FS_MATCH_UNPURE;
		if ( !FS_AllowListExternal( extension ) ) {
			flags &= ~FS_MATCH_EXTERN;
		}
	}

	if ( !path ) {
		*numfiles = 0;
		return NULL;
	}

	if ( !extension ) {
		extension = "";
	}

	extLen = (int)strlen( extension );
	hasPatterns = Com_HasPatterns( extension );
	if ( hasPatterns && extension[0] == '.' && extension[1] != '\0' ) {
		extension++;
	}

	pathLength = strlen( path );
	if ( pathLength > 0 && ( path[pathLength-1] == '\\' || path[pathLength-1] == '/' ) ) {
		pathLength--;
	}

	nfiles = 0;
	FS_ReturnPath( path, zpath, sizeof( zpath ), &pathDepth );

	//
	// search through the path, one element at a time, adding to list
	//
	for ( search = fs_searchpaths; search; search = search->next ) {
		// is the element a pak file?
		if ( search->pack && ( flags & FS_MATCH_PK3s ) ) {

			//ZOID:  If we are pure, don't search for files on paks that
			// aren't on the pure list
			if ( !FS_PakIsPure( search->pack ) && !( flags & FS_MATCH_UNPURE ) ) {
				continue;
			}

			// look through all the pak file elements
			pak = search->pack;
			buildBuffer = pak->buildBuffer;
			for (i = 0; i < pak->numfiles; i++) {
				const char *name;
				int zpathLen, depth;

				// check for directory match
				name = buildBuffer[i].name;
				//
				if ( filter ) {
					// case insensitive
					if ( !Com_FilterPath( filter, name ) )
						continue;
					// unique the match
					nfiles = FS_AddFileToList( name, list, nfiles );
				}
				else {

					zpathLen = FS_ReturnPath( name, zpath, sizeof( zpath ), &depth );

					if ( (depth-pathDepth)>2 || pathLength > zpathLen || Q_stricmpn( name, path, pathLength ) ) {
						continue;
					}

					// check for extension match
					length = (int)strlen( name );

					if ( fnamecallback ) {
						// use custom filter
						if ( !fnamecallback( name, length ) )
							continue;
					} else {
						if ( length < extLen )
							continue;
						if ( *extension ) {
							if ( hasPatterns ) {
								x = strrchr( name, '.' );
								if ( !x || !Com_FilterExt( extension, x+1 ) ) {
									continue;
								}
							} else {
								if ( Q_stricmp( name + length - extLen, extension ) ) {
									continue;
								}
							}
						}
					}
					// unique the match

					temp = pathLength;
					if (pathLength) {
						temp++;		// include the '/'
					}
					nfiles = FS_AddFileToList( name + temp, list, nfiles );
				}
			}
		} else if ( search->dir && ( search->policy != DIR_DENY || (flags & FS_MATCH_EXTERN) != 0 ) ) { // scan for files in the filesystem
			const char *netpath;
			int		numSysFiles;
			char	**sysFiles;
			const char *name;

			netpath = FS_BuildOSPath( search->dir->path, search->dir->gamedir, path );
			sysFiles = Sys_ListFiles( netpath, extension, filter, &numSysFiles, (flags & FS_MATCH_SUBDIRS) ? FS_MAX_SUBDIRS : 0);
			for ( i = 0; i < numSysFiles; i++ ) {
				// unique the match
				name = sysFiles[ i ];
				length = strlen( name );
				if ( fnamecallback ) {
					// use custom filter
					if ( !fnamecallback( name, length ) )
						continue;
				} // else - should be already filtered by Sys_ListFiles

				nfiles = FS_AddFileToList( name, list, nfiles );
			}
			Sys_FreeFileList( sysFiles );
		}		
	}

	// return a copy of the list
	*numfiles = nfiles;

	if ( nfiles == 0 ) {
		return NULL;
	}

	listCopy = Z_Malloc( ( nfiles + 1 ) * sizeof( listCopy[0] ) );
	for ( i = 0; i < nfiles; i++ ) {
		listCopy[i] = list[i];
	}
	listCopy[i] = NULL;

	return listCopy;
}


/*
=================
FS_ListFiles
=================
*/
char **FS_ListFiles( const char *path, const char *extension, int *numfiles ) 
{
	return FS_ListFilteredFiles( path, extension, NULL, numfiles, FS_MATCH_ANY );
}


/*
=================
FS_FreeFileList
=================
*/
void FS_FreeFileList( char **list ) {
	int		i;

	if ( !fs_searchpaths ) {
		Com_Error( ERR_FATAL, "Filesystem call made without initialization" );
	}

	if ( !list ) {
		return;
	}

	for ( i = 0 ; list[i] ; i++ ) {
		Z_Free( list[i] );
	}

	Z_Free( list );
}


/*
================
FS_GetFileList
================
*/
int	FS_GetFileList( const char *path, const char *extension, char *listbuf, int bufsize ) {
	int		nFiles, i, nTotal, nLen;
	char **pFiles = NULL;

	*listbuf = '\0';
	nFiles = 0;
	nTotal = 0;

	if (Q_stricmp(path, "$modlist") == 0) {
		return FS_GetModList(listbuf, bufsize);
	}

	pFiles = FS_ListFiles(path, extension, &nFiles);

	for (i =0; i < nFiles; i++) {
		nLen = strlen(pFiles[i]) + 1;
		if (nTotal + nLen + 1 < bufsize) {
			Com_Memcpy( listbuf, pFiles[i], (size_t)nLen );
			listbuf += nLen;
			nTotal += nLen;
		}
		else {
			nFiles = i;
			break;
		}
	}

	FS_FreeFileList(pFiles);

	return nFiles;
}


/*
=======================
Sys_ConcatenateFileLists

mkv: Naive implementation. Concatenates three lists into a
     new list, and frees the old lists from the heap.
bk001129 - from cvs1.17 (mkv)

Note: Could move to common.c next to Sys_ListFiles.
=======================
 */
static unsigned int Sys_CountFileList( char **list )
{
	int i = 0;

	if ( list )
	{
		while ( *list )
		{
			list++;
			i++;
		}
	}

	return i;
}


static char** FS_ConcatenateFileLists( char **list0, char **list1 )
{
	int totalLength;
	char **src, **dst, **cat;

	totalLength = Sys_CountFileList( list0 );
	totalLength += Sys_CountFileList( list1 );

	/* Create new list. */
	dst = cat = Z_Malloc( ( totalLength + 1 ) * sizeof( char* ) );

	/* Copy over lists. */
	if ( list0 )
	{
		for (src = list0; *src; src++, dst++)
			*dst = *src;
	}

	if ( list1 )
	{
		for ( src = list1; *src; src++, dst++ )
			*dst = *src;
	}

	// Terminate the list
	*dst = NULL;

	// Free our old lists.
	// NOTE: not freeing their content, it's been merged in dst and still being used
	if ( list0 ) Z_Free( list0 );
	if ( list1 ) Z_Free( list1 );

	return cat;
}


/*
================
FS_GetModDescription
================
*/
static void FS_GetModDescription( const char *modDir, char *description, int descriptionLen ) {
	fileHandle_t	descHandle;
	char			descPath[MAX_QPATH];
	int				nDescLen;

	Com_sprintf( descPath, sizeof ( descPath ), "%s%cdescription.txt", modDir, PATH_SEP );
	FS_ReplaceSeparators( descPath );
	nDescLen = FS_SV_FOpenFileRead( descPath, &descHandle );

	if ( descHandle != FS_INVALID_HANDLE ) {
		if ( nDescLen > 0 ) {
			if ( nDescLen > descriptionLen - 1 )
				nDescLen = descriptionLen - 1;
			nDescLen = FS_Read( description, nDescLen, descHandle );
			if ( nDescLen >= 0 ) {
				description[ nDescLen ] = '\0';
				while ( nDescLen > 0 && description[ nDescLen - 1 ] == '\n' ) {
					// strip ending newlines
					description[ nDescLen - 1 ] = '\0';
					nDescLen--;
				}
			}
		} else {
			Q_strncpyz( description, modDir, descriptionLen );
		}
		FS_FCloseFile( descHandle ); 
	} else {
		Q_strncpyz( description, modDir, descriptionLen );
	}
}


/*
================
FS_IsBaseGame
================
*/
static qboolean FS_IsBaseGame( const char *game )
{
	int i;

	if ( game == NULL || *game == '\0' ) {
		return qtrue;
	}

	for ( i = 0; i < basegame_cnt; i++ ) {
		if ( Q_stricmp( basegames[i], game ) == 0 ) {
			return qtrue;
		}
	}

	return qfalse;
}


/*
===========
FS_PathCmp

Ignore case and separator char distinctions
===========
*/
static int FS_PathCmp( const char* s1, const char* s2 ) {
	int		c1, c2;

	do {
		c1 = *s1++;
		c2 = *s2++;

		if ( c1 >= 'a' && c1 <= 'z' ) {
			c1 -= ('a' - 'A');
		}
		if ( c2 >= 'a' && c2 <= 'z' ) {
			c2 -= ('a' - 'A');
		}

		if ( c1 == '\\' || c1 == ':' ) {
			c1 = '/';
		}
		if ( c2 == '\\' || c2 == ':' ) {
			c2 = '/';
		}

		if ( c1 < c2 ) {
			return -1;		// strings not equal
		}
		if ( c1 > c2 ) {
			return 1;
		}
	}
	while ( c1 );

	return 0;		// strings are equal
}


/*
================
FS_SortFileList
================
*/
static void FS_SortFileList( char** list, int n ) {
	const char* m;
	char* temp;
	int i, j;
	i = 0;
	j = n;
	m = list[n >> 1];
	do {
		while ( FS_PathCmp( list[i], m ) < 0 ) i++;
		while ( FS_PathCmp( list[j], m ) > 0 ) j--;
		if ( i <= j ) {
			temp = list[i];
			list[i] = list[j];
			list[j] = temp;
			i++;
			j--;
		}
	}
	while ( i <= j );
	if ( j > 0 ) FS_SortFileList( list, j );
	if ( n > i ) FS_SortFileList( list + i, n - i );
}


/*
================
FS_GetModList

Returns a list of mod directory names
A mod directory is a peer to baseq3 with a pk3 in it
================
*/
static int FS_GetModList( char *listbuf, int bufsize ) {
	int i, j, k;
	int	nMods, nTotal, nLen, nPaks, nPotential, nDescLen;
	int nDirs, nPakDirs;
	char **pFiles = NULL;
	char **pPaks = NULL;
	char **pDirs = NULL;
	const char *name, *path;
	char description[ MAX_OSPATH ];

	int dummy;
	char **pFiles0 = NULL;
	qboolean bDrop = qfalse;

	// paths to search for mods
	cvar_t *const *paths[] = { &fs_basepath, &fs_homepath, &fs_steampath };

	*listbuf = '\0';
	nMods = nTotal = 0;

	// iterate through paths and get list of potential mods
	for (i = 0; (size_t) i < ARRAY_LEN( paths ); i++) {
		if ( !*paths[ i ] || !(*paths[i])->string[0] )
			continue;
		pFiles0 = Sys_ListFiles( (*paths[i])->string, "/", NULL, &dummy, 1 );
		// Sys_ConcatenateFileLists frees the lists so Sys_FreeFileList isn't required
		pFiles = FS_ConcatenateFileLists( pFiles, pFiles0 );
	}

	nPotential = Sys_CountFileList( pFiles );

	if ( nPotential >= 2 ) {
		FS_SortFileList( pFiles, nPotential - 1 );
	}

	for ( i = 0; i < nPotential; i++ ) {
		name = pFiles[i];
		// NOTE: cleaner would involve more changes
		// ignore duplicate mod directories
		if ( i != 0 ) {
			bDrop = qfalse;
			for ( j = 0; j < i; j++ ) {
				if ( Q_stricmp( pFiles[j], name ) == 0 ) {
					// this one can be dropped
					bDrop = qtrue;
					break;
				}
			}
		}

		// we also drop BASEGAME, "." and ".."
		if ( bDrop || strcmp(name, "." ) == 0 || strcmp( name, ".." ) == 0 ) {
			continue;
		}

		if ( FS_IsBaseGame( name ) ) {
			continue;
		}

		// in order to be a valid mod the directory must contain at least one .pk3
		// we didn't keep the information when we merged the directory names, as to what OS Path it was found under
		// so we will try each of them here
		nPaks = nPakDirs = 0;
		for ( j = 0; (size_t) j < ARRAY_LEN( paths ); j++ ) {
			if ( !*paths[ j ] || !(*paths[ j ])->string[0] )
				break;
			path = FS_BuildOSPath( (*paths[j])->string, name, NULL );

			nPaks = nDirs = nPakDirs = 0;
			pPaks = Sys_ListFiles( path, ".pk3", NULL, &nPaks, 0 );
			pDirs = Sys_ListFiles( path, "/", NULL, &nDirs, 0 );
			for ( k = 0; k < nDirs; k++ ) {
				// we only want to count directories ending with ".pk3dir"
				if ( FS_IsExt( pDirs[k], ".pk3dir", strlen( pDirs[k] ) ) ) {
					nPakDirs++;
				}
			}

			// we only use Sys_ListFiles to check whether files are present
			Sys_FreeFileList( pDirs );
			Sys_FreeFileList( pPaks );
			if ( nPaks > 0 || nPakDirs > 0 ) {
				break;
			}
		}

		if ( nPaks > 0 || nPakDirs > 0 ) {
			nLen = strlen( name ) + 1;
			// nLen is the length of the mod path
			// we need to see if there is a description available
			FS_GetModDescription( name, description, sizeof( description ) );
			nDescLen = strlen( description ) + 1;

			if ( nTotal + nLen + 1 + nDescLen + 1 < bufsize ) {
				Com_Memcpy( listbuf, name, (size_t)nLen );
				listbuf += nLen;
				Com_Memcpy( listbuf, description, (size_t)nDescLen );
				listbuf += nDescLen;
				nTotal += nLen + nDescLen;
				nMods++;
			} else {
				break;
			}
		}
	}
	Sys_FreeFileList( pFiles );

	return nMods;
}


//============================================================================

/*
================
FS_Dir_f
================
*/
static void FS_Dir_f( void ) {
	const char *path;
	const char *extension;
	char **dirnames;
	int ndirs;
	int i;

	if ( Cmd_Argc() < 2 || Cmd_Argc() > 3 ) {
		Com_Printf( "usage: dir <directory> [extension]\n" );
		return;
	}

	if ( Cmd_Argc() == 2 ) {
		path = Cmd_Argv( 1 );
		extension = "";
	} else {
		path = Cmd_Argv( 1 );
		extension = Cmd_Argv( 2 );
	}

	Com_Printf( "Directory of %s %s\n", path, extension );
	Com_Printf( "---------------\n" );

	dirnames = FS_ListFiles( path, extension, &ndirs );

	for ( i = 0; i < ndirs; i++ ) {
		Com_Printf( "%s\n", dirnames[i] );
	}
	FS_FreeFileList( dirnames );
}


/*
===========
FS_ConvertPath
===========
*/
static void FS_ConvertPath( char *s ) {
	while (*s) {
		if ( *s == '\\' || *s == ':' ) {
			*s = '/';
		}
		s++;
	}
}


/*
================
FS_NewDir_f
================
*/
static void FS_NewDir_f( void ) {
	const char *filter;
	char	**dirnames;
	char	dirname[ MAX_STRING_CHARS ];
	int		ndirs;
	int		i;

	if ( Cmd_Argc() < 2 ) {
		Com_Printf( "usage: fdir <filter>\n" );
		Com_Printf( "example: fdir *q3dm*.bsp\n");
		return;
	}

	filter = Cmd_Argv( 1 );

	Com_Printf( "---------------\n" );

	dirnames = FS_ListFilteredFiles( "", "", filter, &ndirs, FS_MATCH_ANY | FS_MATCH_SUBDIRS );

	if ( ndirs >= 2 ) {
		FS_SortFileList( dirnames, ndirs - 1 );
	}

	for ( i = 0; i < ndirs; i++ ) {
		Q_strncpyz( dirname, dirnames[i], sizeof( dirname ) );
		FS_ConvertPath( dirname );
		Com_Printf( "%s\n", dirname );
	}

	Com_Printf( "%d files listed\n", ndirs );
	FS_FreeFileList( dirnames );
}


/*
============
FS_Path_f
============
*/
static void FS_Path_f( void ) {
	const searchpath_t *s;
	int i;

	Com_Printf( "Current search path:\n" );
	for ( s = fs_searchpaths; s; s = s->next ) {
		if ( s->pack ) {
			Com_Printf( "%s (%i files)\n", s->pack->pakFilename, s->pack->numfiles );
			if ( fs_numServerPaks ) {
				if ( !FS_PakIsPure( s->pack ) ) {
					Com_Printf( S_COLOR_YELLOW "    not on the pure list\n" );
				} else {
					Com_Printf( "    on the pure list\n" );
				}
			}
		} else {
			Com_Printf( "%s%c%s\n", s->dir->path, PATH_SEP, s->dir->gamedir );
		}
	}

	Com_Printf( "\n" );
	for ( i = 1 ; i < MAX_FILE_HANDLES ; i++ ) {
		if ( fsh[i].handleFiles.file.o ) {
			Com_Printf( "handle %i: %s\n", i, fsh[i].name );
		}
	}
}


/*
============
FS_TouchFile_f

The only purpose of this function is to allow game script files to copy
arbitrary files furing an "fs_copyfiles 1" run.
============
*/
static void FS_TouchFile_f( void ) {
	fileHandle_t	f;

	if ( Cmd_Argc() != 2 ) {
		Com_Printf( "Usage: touchFile <file>\n" );
		return;
	}

	FS_FOpenFileRead( Cmd_Argv( 1 ), &f, qfalse );
	if ( f != FS_INVALID_HANDLE ) {
		FS_FCloseFile( f );
	}
}


/*
============
FS_CompleteFileName
============
*/
static void FS_CompleteFileName( const char *args, int argNum ) {
	(void)args;
	(void)argNum;
	if( argNum == 2 ) {
		Field_CompleteFilename( "", "", qfalse, FS_MATCH_ANY );
	}
}


/*
============
FS_Which_f
============
*/
static void FS_Which_f( void ) {
	const searchpath_t *search;
	char			*netpath;
	pack_t			*pak;
	fileInPack_t	*pakFile;
	directory_t		*dir;
	long			hash;
	FILE			*temp;
	const char			*filename;
	char			buf[ MAX_OSPATH*2 + 1 ];
	int				numfound;

	hash = 0;
	numfound = 0;
	filename = Cmd_Argv(1);

	if ( !filename[0] ) {
		Com_Printf( "Usage: which <file>\n" );
		return;
	}

	// qpaths are not supposed to have a leading slash
	if ( filename[0] == '/' || filename[0] == '\\' ) {
		filename++;
	}

	// just wants to see if file is there
	for ( search = fs_searchpaths ; search ; search = search->next ) {
		if ( search->pack ) {
			hash = FS_HashFileName(filename, search->pack->hashSize);
		}
		// is the element a pak file?
		if ( search->pack && search->pack->hashTable[hash] ) {
			// look through all the pak file elements
			pak = search->pack;
			pakFile = pak->hashTable[hash];
			do {
				// case and separator insensitive comparisons
				if ( !FS_FilenameCompare( pakFile->name, filename ) ) {
					// found it!
					Com_Printf( "File \"%s\" found in \"%s\"\n", filename, pak->pakFilename );
					if ( ++numfound >= 32 ) {
						return;
					}
				}
				pakFile = pakFile->next;
			} while(pakFile != NULL);
		} else if ( search->dir ) {
			dir = search->dir;

			netpath = FS_BuildOSPath( dir->path, dir->gamedir, filename );
			temp = Sys_FOpen( netpath, "rb" );
			if ( !temp ) {
				continue;
			}
			fclose(temp);
			Com_sprintf( buf, sizeof( buf ), "%s%c%s", dir->path, PATH_SEP, dir->gamedir );
			FS_ReplaceSeparators( buf );
			Com_Printf( "File \"%s\" found at \"%s\"\n", filename, buf );
			if ( ++numfound >= 32 ) {
				return;
			}
		}
	}

	if ( !numfound ) {
		Com_Printf( "File not found: \"%s\"\n", filename );
	}
}


//===========================================================================
