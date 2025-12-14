/*
===========================================================================
files_internal.h - Internal filesystem type definitions
Shared between files.c and files_v2.c
===========================================================================
*/

#ifndef __FILES_INTERNAL_H__
#define __FILES_INTERNAL_H__

#include "q_shared.h"
#include "qcommon.h"
#include "unzip.h"

// File path constants
#ifndef MAX_ZPATH
#define MAX_ZPATH			256		// max length of a filesystem pathname
#endif

// Feature flags (must be defined before pack_t struct)
#ifndef USE_HANDLE_CACHE
#define USE_HANDLE_CACHE
#endif
#ifndef USE_PK3_CACHE
#define USE_PK3_CACHE
#endif

// Forward declarations
typedef struct pack_s pack_t;
typedef struct fileInPack_s fileInPack_t;
typedef struct directory_s directory_t;
typedef struct fileHandleData_s fileHandleData_t;

// File in pack structure
typedef struct fileInPack_s {
	char					*name;		// name of the file
	unsigned long			pos;		// file info position in zip
	unsigned long			size;		// file size
	struct	fileInPack_s*	next;		// next file in the hash
} fileInPack_t;

// Pack structure
typedef struct pack_s {
	char			*pakFilename;				// c:\quake3\baseq3\pak0.pk3
	char			*pakBasename;				// pak0
	const char		*pakGamename;				// baseq3
	unzFile			handle;						// handle to zip file
	int				checksum;					// regular checksum
	int				pure_checksum;				// checksum for pure
	int				numfiles;					// number of files in pk3
	int				referenced;					// referenced file flags
	qboolean		exclude;					// found in \fs_excludeReference list
	int				hashSize;					// hash table size (power of 2)
	fileInPack_t*	*hashTable;					// hash table
	fileInPack_t*	buildBuffer;				// buffer with the filenames etc.
	int				index;

	int				handleUsed;

#ifdef USE_HANDLE_CACHE
	struct pack_s	*next_h;						// double-linked list of unreferenced paks with open file handles
	struct pack_s	*prev_h;
#endif

	// caching subsystem
#ifdef USE_PK3_CACHE
	unsigned int	namehash;
	fileOffset_t	size;
	fileTime_t		mtime;
	fileTime_t		ctime;
	qboolean		touched;
	struct pack_s	*next;
	struct pack_s	*prev;
	int				checksumFeed;
	int				*headerLongs;
	int				numHeaderLongs;
#endif
} pack_t;

// Directory structure
typedef struct directory_s {
	char		*path;		// c:\quake3
	char		*gamedir;	// baseq3
} directory_t;

// Directory policy
typedef enum {
	DIR_STATIC = 0,	// always allowed, never changes
	DIR_ALLOW,
	DIR_DENY
} dirPolicy_t;

// File handle data structure
typedef union qfile_gus {
	FILE*		o;
	unzFile		z;
	void*		v;
} qfile_gut;

typedef struct qfile_us {
	qfile_gut	file;
	qboolean	unique;
} qfile_ut;

typedef struct fileHandleData_s {
	qfile_ut	handleFiles;
	qboolean	handleSync;
	qboolean	zipFile;
	int			zipFilePos;
	int			zipFileLen;
	char		name[MAX_ZPATH];
	handleOwner_t	owner;
	int			pakIndex;
	pack_t		*pak;
} fileHandleData_t;

// Searchpath structure (needed for migration)
typedef struct searchpath_s {
	struct searchpath_s *next;
	pack_t		*pack;		// only one of pack / dir will be non NULL
	directory_t	*dir;
	dirPolicy_t	policy;
} searchpath_t;

// External declarations from files.c
extern fileHandleData_t fsh[];
extern searchpath_t *fs_searchpaths;
extern int fs_checksumFeed;

// Function declarations from files.c
qboolean FS_PakIsPure(const pack_t *pack);
int FS_OpenFileInPak(fileHandle_t *file, pack_t *pak, fileInPack_t *pakFile, qboolean uniqueFILE);
pack_t *FS_LoadZipFile(const char *zipfile);
char *FS_BuildOSPath(const char *base, const char *game, const char *qpath);
int FS_FileLength(FILE *h);
fileHandle_t FS_HandleForFile(void);
void FS_InitHandle(fileHandleData_t *fd);
qboolean FS_FilenameCompare(const char *s1, const char *s2);
FILE *Sys_FOpen(const char *ospath, const char *mode);

#endif // __FILES_INTERNAL_H__
