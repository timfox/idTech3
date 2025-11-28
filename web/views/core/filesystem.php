<?php
/**
 * File System - id Tech 3 Engine Documentation  
 */
$title = 'File System - id Tech 3 Documentation';
$breadcrumbs = [
    '/core' => 'Core Engine',
    '/core/filesystem' => 'File System'
];
?>

<h1>File System</h1>

<div class="section">
    <h2>Overview</h2>
    <p>The id Tech 3 file system implements a Virtual File System (VFS) that abstracts file access across multiple sources including PAK files, directories, and network downloads. It provides transparent access to game assets while supporting modding, content distribution, and platform-specific file handling.</p>
    
    <div class="feature-list">
        <h3>Key Features</h3>
        <ul>
            <li><strong>Virtual File System:</strong> Unified interface for multiple data sources</li>
            <li><strong>PAK File Support:</strong> Compressed asset archives with efficient access</li>
            <li><strong>Search Path System:</strong> Hierarchical asset resolution</li>
            <li><strong>Mod Support:</strong> Override and extension mechanisms</li>
        </ul>
    </div>
</div>

<div class="section">
    <h2>File System Architecture</h2>
    
    <h3>Core Structures</h3>
    <div class="code-block">
        <pre><code>// files.h - File system core definitions
typedef struct fileInPack_s {
    char*                   name;           // File name
    unsigned long           pos;            // File position in PAK
    unsigned long           len;            // Uncompressed file size
    struct fileInPack_s*    next;           // Hash table chain
} fileInPack_t;

typedef struct pack_s {
    char                pakFilename[MAX_OSPATH];    // PAK file path
    char                pakBasename[MAX_OSPATH];    // PAK base name
    char                pakGamename[MAX_OSPATH];    // Game directory
    unzFile             handle;                     // ZIP file handle
    int                 checksum;                   // PAK checksum
    int                 pure_checksum;              // Pure server checksum
    int                 numfiles;                   // Number of files
    int                 referenced;                 // Reference count
    int                 hashSize;                   // Hash table size
    fileInPack_t*       hashTable[MAX_FILES_IN_PACK];  // File hash table
    fileInPack_t*       buildBuffer;                // Temp build buffer
} pack_t;

typedef struct searchpath_s {
    struct searchpath_s* next;          // Next in search chain
    pack_t*             pack;           // PAK file (if NULL, use dir)
    char*               dir;            // Directory path
} searchpath_t;

typedef struct {
    FILE*               o;              // File handle for writing
    unzFile             z;              // ZIP handle for reading
    qboolean            zipFile;        // Reading from ZIP
    int                 zipFilePos;     // Position in ZIP
    int                 zipFileLen;     // Remaining bytes in ZIP
    char                name[MAX_ZPATH]; // File name
} fileHandleData_t;

// Global file system state
static searchpath_t*    fs_searchpaths;     // Search path chain
static pack_t*          fs_loadedPaks;      // Loaded PAK files
static fileHandleData_t fsh[MAX_FILE_HANDLES]; // File handles
static qboolean         fs_debug;          // Debug mode
static cvar_t*          fs_basepath;       // Base game path
static cvar_t*          fs_basegame;       // Base game name
static cvar_t*          fs_game;           // Current game/mod</code></pre>
    </div>
    
    <h3>File System Initialization</h3>
    <div class="code-block">
        <pre><code>// File system startup and configuration
void FS_Startup(const char* gameName) {
    Com_Printf("------ File System Initialization ------\n");
    
    // Initialize CVars
    fs_debug = Cvar_Get("fs_debug", "0", 0);
    fs_basepath = Cvar_Get("fs_basepath", Sys_DefaultBasePath(), CVAR_INIT);
    fs_basegame = Cvar_Get("fs_basegame", "", CVAR_INIT);
    fs_game = Cvar_Get("fs_game", gameName, CVAR_INIT | CVAR_SYSTEMINFO);
    fs_copyfiles = Cvar_Get("fs_copyfiles", "0", CVAR_INIT);
    fs_restrict = Cvar_Get("fs_restrict", "", CVAR_INIT);
    
    // Clear search paths
    FS_ClearSearchPaths();
    
    // Add base path
    FS_AddGameDirectory(fs_basepath->string, gameName);
    
    // Add base game if different
    if (fs_basegame->string[0] && Q_stricmp(fs_basegame->string, gameName)) {
        FS_AddGameDirectory(fs_basepath->string, fs_basegame->string);
    }
    
    // Add current game/mod
    if (fs_game->string[0] && Q_stricmp(fs_game->string, gameName) && 
        Q_stricmp(fs_game->string, fs_basegame->string)) {
        FS_AddGameDirectory(fs_basepath->string, fs_game->string);
    }
    
    // Print loaded PAKs
    FS_DisplaySearchPaths();
    
    Com_Printf("File system initialized.\n");
    Com_Printf("--------------------------------------\n");
}

void FS_AddGameDirectory(const char* path, const char* dir) {
    searchpath_t* sp;
    pack_t* pak;
    char* pakfile;
    int numfiles;
    char** pakfiles;
    int i;
    
    // Get all PAK files in directory
    pakfiles = Sys_ListFiles(va("%s/%s", path, dir), ".pk3", NULL, &numfiles, qfalse);
    
    // Sort PAK files
    qsort(pakfiles, numfiles, sizeof(char*), pakcmp);
    
    // Load PAK files
    for (i = 0; i < numfiles; i++) {
        pakfile = FS_BuildOSPath(path, dir, pakfiles[i]);
        pak = FS_LoadPakFile(pakfile);
        
        if (!pak) {
            continue;
        }
        
        // Add to search path
        sp = Z_Malloc(sizeof(*sp));
        sp->pack = pak;
        sp->next = fs_searchpaths;
        fs_searchpaths = sp;
    }
    
    // Free file list
    Sys_FreeFileList(pakfiles);
    
    // Add directory path
    sp = Z_Malloc(sizeof(*sp));
    sp->dir = CopyString(va("%s/%s", path, dir));
    sp->next = fs_searchpaths;
    fs_searchpaths = sp;
}</code></pre>
    </div>
</div>

<div class="section">
    <h2>PAK File System</h2>
    
    <h3>PAK File Loading</h3>
    <div class="code-block">
        <pre><code>// PAK (PK3/ZIP) file handling
pack_t* FS_LoadPakFile(const char* pakfile) {
    unzFile             uf;
    int                 err;
    unz_global_info     gi;
    char                filename_inzip[MAX_ZPATH];
    unz_file_info       file_info;
    int                 i, len;
    long                hash;
    int                 fs_numServerPaks;
    pack_t*             pack;
    fileInPack_t*       buildBuffer;
    
    fs_numServerPaks = 0;
    
    uf = unzOpen(pakfile);
    err = unzGetGlobalInfo(uf, &gi);
    
    if (err != UNZ_OK) {
        return NULL;
    }
    
    len = 0;
    unzGoToFirstFile(uf);
    for (i = 0; i < gi.number_entry; i++) {
        err = unzGetCurrentFileInfo(uf, &file_info, filename_inzip, sizeof(filename_inzip), NULL, 0, NULL, 0);
        if (err != UNZ_OK) {
            break;
        }
        len += strlen(filename_inzip) + 1;
        unzGoToNextFile(uf);
    }
    
    buildBuffer = Z_Malloc((gi.number_entry * sizeof(fileInPack_t)) + len);
    namePtr = ((char*)buildBuffer) + gi.number_entry * sizeof(fileInPack_t);
    
    // Build the hash table
    pack = Z_Malloc(sizeof(pack_t));
    pack->hashSize = gi.number_entry;
    pack->hashTable = Z_Malloc(gi.number_entry * sizeof(fileInPack_t*));
    pack->handle = uf;
    pack->numfiles = gi.number_entry;
    pack->buildBuffer = buildBuffer;
    Q_strncpyz(pack->pakFilename, pakfile, sizeof(pack->pakFilename));
    Q_strncpyz(pack->pakBasename, COM_SkipPath(pakfile), sizeof(pack->pakBasename));
    
    unzGoToFirstFile(uf);
    
    for (i = 0; i < gi.number_entry; i++) {
        err = unzGetCurrentFileInfo(uf, &file_info, filename_inzip, sizeof(filename_inzip), NULL, 0, NULL, 0);
        if (err != UNZ_OK) {
            break;
        }
        
        if (file_info.uncompressed_size > 0) {
            buildBuffer[i].name = namePtr;
            strcpy(buildBuffer[i].name, filename_inzip);
            namePtr += strlen(filename_inzip) + 1;
            
            // Convert to lowercase for hash
            Q_strlwr(buildBuffer[i].name);
            
            // Hash the file name
            hash = FS_HashFileName(buildBuffer[i].name, pack->hashSize);
            buildBuffer[i].next = pack->hashTable[hash];
            pack->hashTable[hash] = &buildBuffer[i];
            
            // Store file info
            buildBuffer[i].pos = unzGetOffset(uf);
            buildBuffer[i].len = file_info.uncompressed_size;
        }
        unzGoToNextFile(uf);
    }
    
    pack->checksum = FS_ChecksumPak(pakfile);
    pack->pure_checksum = FS_PureChecksumPak(pakfile);
    
    return pack;
}

long FS_HashFileName(const char* fname, int hashSize) {
    int i;
    long hash;
    char letter;
    
    hash = 0;
    i = 0;
    while (fname[i] != '\0') {
        letter = tolower(fname[i]);
        if (letter == '.') break;              // Don't include extension
        if (letter == '\\') letter = '/';      // Convert backslashes
        if (letter == PATH_SEP) letter = '/';  // Convert path separators
        hash += (long)(letter) * (i + 119);
        i++;
    }
    hash = (hash ^ (hash >> 10) ^ (hash >> 20));
    hash &= (hashSize - 1);
    return hash;
}</code></pre>
    </div>
    
    <h3>File Access and Caching</h3>
    <div class="code-block">
        <pre><code>// File reading and handle management
fileHandle_t FS_FOpenFileRead(const char* filename, qboolean unpure) {
    searchpath_t*   search;
    char*           netpath;
    pack_t*         pak;
    fileInPack_t*   pakFile;
    directory_t*    dir;
    long            hash;
    FILE*           temp;
    int             l;
    fileHandle_t    f;
    
    if (!filename) {
        Com_Error(ERR_FATAL, "FS_FOpenFileRead: NULL 'filename' parameter passed");
    }
    
    // qpaths are not supposed to have a leading slash
    if (filename[0] == '/' || filename[0] == '\\') {
        filename++;
    }
    
    // Make sure the filename is not too long
    if (strlen(filename) >= MAX_QPATH) {
        Com_Printf("FS_FOpenFileRead: %s is too long\n", filename);
        return 0;
    }
    
    f = FS_HandleForFile();
    fsh[f].zipFile = qfalse;
    
    Q_strncpyz(fsh[f].name, filename, sizeof(fsh[f].name));
    
    // Search through the path, one element at a time
    for (search = fs_searchpaths; search; search = search->next) {
        // Is the element a pak file?
        if (search->pack) {
            hash = FS_HashFileName(filename, search->pack->hashSize);
            
            // Look through all files in the pak
            for (pakFile = search->pack->hashTable[hash]; pakFile; pakFile = pakFile->next) {
                // Case and separator insensitive comparisons
                if (!FS_FilenameCompare(pakFile->name, filename)) {
                    // Found it!
                    
                    // Open a new file on the pakfile
                    fsh[f].zipFile = qtrue;
                    fsh[f].zipFilePos = pakFile->pos;
                    fsh[f].zipFileLen = pakFile->len;
                    
                    if (unzSetOffset(search->pack->handle, pakFile->pos) != UNZ_OK) {
                        Com_Error(ERR_FATAL, "FS_FOpenFileRead: unzSetOffset failed");
                    }
                    
                    if (unzOpenCurrentFile(search->pack->handle) != UNZ_OK) {
                        Com_Error(ERR_FATAL, "FS_FOpenFileRead: unzOpenCurrentFile failed");
                    }
                    
                    fsh[f].z = search->pack->handle;
                    
                    if (fs_debug->integer) {
                        Com_Printf("FS_FOpenFileRead: %s (found in '%s')\n", 
                                  filename, search->pack->pakFilename);
                    }
                    return f;
                }
            }
        } else if (search->dir) {
            // Check a file in the directory tree
            netpath = FS_BuildOSPath(search->dir, filename);
            
            temp = fopen(netpath, "rb");
            if (!temp) {
                continue;
            }
            
            fsh[f].o = temp;
            fsh[f].zipFile = qfalse;
            
            if (fs_debug->integer) {
                Com_Printf("FS_FOpenFileRead: %s (found in '%s')\n", 
                          filename, search->dir);
            }
            
            return f;
        }
    }
    
    if (fs_debug->integer) {
        Com_Printf("FS_FOpenFileRead: can't find %s\n", filename);
    }
    
    fsh[f].name[0] = 0;
    return 0;
}

int FS_Read(void* buffer, int len, fileHandle_t f) {
    byte*   buf;
    qboolean isConfig;
    
    if (!f) {
        return 0;
    }
    
    buf = (byte*)buffer;
    
    if (fsh[f].zipFile == qfalse) {
        // Reading from regular file
        return fread(buf, 1, len, fsh[f].o);
    } else {
        // Reading from ZIP file
        int remaining = len;
        int block, read;
        
        // Don't read past end of file
        if (fsh[f].zipFilePos + len > fsh[f].zipFileLen) {
            len = fsh[f].zipFileLen - fsh[f].zipFilePos;
        }
        
        read = unzReadCurrentFile(fsh[f].z, buf, len);
        fsh[f].zipFilePos += read;
        
        return read;
    }
}</code></pre>
    </div>
</div>

<div class="section">
    <h2>Search Path System</h2>
    
    <h3>Path Resolution</h3>
    <div class="code-block">
        <pre><code>// Search path management and resolution
char** FS_ListFiles(const char* path, const char* extension, int* numfiles) {
    int                 nfiles;
    char**              listCopy;
    char*               list[MAX_FOUND_FILES];
    searchpath_t*       search;
    int                 i;
    int                 pathLength;
    int                 extensionLength;
    int                 length, pathDepth, temp;
    pack_t*             pak;
    fileInPack_t*       buildBuffer;
    char                zpath[MAX_ZPATH];
    
    if (!path) {
        *numfiles = 0;
        return NULL;
    }
    
    if (!extension) {
        extension = "";
    }
    
    pathLength = strlen(path);
    if (path[pathLength-1] == '\\' || path[pathLength-1] == '/') {
        pathLength--;
    }
    extensionLength = strlen(extension);
    nfiles = 0;
    
    for (search = fs_searchpaths; search; search = search->next) {
        // Search PAK files
        if (search->pack) {
            buildBuffer = search->pack->buildBuffer;
            for (i = 0; i < search->pack->numfiles; i++) {
                char* name = buildBuffer[i].name;
                
                // Check if in specified path
                if (pathLength) {
                    if (Q_stricmpn(name, path, pathLength)) {
                        continue;
                    }
                    if (name[pathLength] != '/') {
                        continue;
                    }
                    name += pathLength + 1;
                }
                
                // Check for subdirectories
                if (strchr(name, '/')) {
                    continue;
                }
                
                // Check extension
                if (extensionLength) {
                    length = strlen(name);
                    if (length < extensionLength) {
                        continue;
                    }
                    if (Q_stricmp(name + length - extensionLength, extension)) {
                        continue;
                    }
                }
                
                // Add to list if not already present
                temp = 0;
                for (j = 0; j < nfiles; j++) {
                    if (!Q_stricmp(name, list[j])) {
                        temp = 1;
                        break;
                    }
                }
                
                if (!temp) {
                    list[nfiles] = CopyString(name);
                    nfiles++;
                    if (nfiles == MAX_FOUND_FILES - 1) {
                        break;
                    }
                }
            }
        } else if (search->dir) {
            // Search directory
            char* netpath = FS_BuildOSPath(search->dir, path);
            char** dirnames = Sys_ListFiles(netpath, extension, NULL, &ndirnames, qfalse);
            
            for (i = 0; i < ndirnames; i++) {
                // Add to list if not already present
                temp = 0;
                for (j = 0; j < nfiles; j++) {
                    if (!Q_stricmp(dirnames[i], list[j])) {
                        temp = 1;
                        break;
                    }
                }
                
                if (!temp) {
                    list[nfiles] = CopyString(dirnames[i]);
                    nfiles++;
                    if (nfiles == MAX_FOUND_FILES - 1) {
                        break;
                    }
                }
            }
            
            Sys_FreeFileList(dirnames);
        }
    }
    
    // Return a copy of the list
    *numfiles = nfiles;
    
    if (!nfiles) {
        return NULL;
    }
    
    listCopy = Z_Malloc((nfiles + 1) * sizeof(*listCopy));
    for (i = 0; i < nfiles; i++) {
        listCopy[i] = list[i];
    }
    listCopy[i] = NULL;
    
    return listCopy;
}</code></pre>
    </div>
    
    <h3>Pure Server System</h3>
    <div class="code-block">
        <pre><code>// Pure server PAK validation
qboolean FS_ComparePaks(char* neededpaks, int len, qboolean dlstring) {
    searchpath_t*   sp;
    qboolean        havepak;
    char*           origpos = neededpaks;
    char*           token;
    
    if (!fs_numServerPaks) {
        return qfalse; // Server didn't send any pak information
    }
    
    while ((token = COM_ParseExt(&neededpaks, qfalse)) && token[0]) {
        havepak = qfalse;
        
        // Check if we have this pak
        for (sp = fs_searchpaths; sp; sp = sp->next) {
            if (sp->pack && sp->pack->checksum == atoi(token)) {
                havepak = qtrue;
                break;
            }
        }
        
        if (!havepak && fs_numServerPaks) {
            if (dlstring) {
                // Server wants us to download this pak
                if (strlen(token) + (origpos - neededpaks) >= len - 1) {
                    // No more room in the buffer
                    break;
                }
                
                strcat(origpos, va("@%s@%s/", token, token));
            } else {
                Com_Printf("WARNING: Missing pak %s\n", token);
                return qfalse;
            }
        }
    }
    
    if (dlstring) {
        // Generate download string for missing paks
        return qtrue;
    }
    
    return qtrue;
}

void FS_PureServerSetReferencedPaks(const char* pakSums, const char* pakNames) {
    int i, c, d;
    
    Cmd_TokenizeString(pakSums);
    c = Cmd_Argc();
    
    if (c > MAX_SEARCH_PATHS) {
        c = MAX_SEARCH_PATHS;
    }
    
    fs_numServerPaks = c;
    
    for (i = 0; i < c; i++) {
        fs_serverPaks[i] = atoi(Cmd_Argv(i));
    }
    
    if (pakNames && *pakNames) {
        Cmd_TokenizeString(pakNames);
        d = Cmd_Argc();
        
        if (d > MAX_SEARCH_PATHS) {
            d = MAX_SEARCH_PATHS;
        }
        
        for (i = 0; i < d; i++) {
            if (i < c) {
                Q_strncpyz(fs_serverPakNames[i], Cmd_Argv(i), sizeof(fs_serverPakNames[i]));
            }
        }
    }
}</code></pre>
    </div>
</div>

<div class="section">
    <h2>Asset Loading and Streaming</h2>
    
    <h3>File Loading Utilities</h3>
    <div class="code-block">
        <pre><code>// High-level file loading functions
int FS_ReadFile(const char* qpath, void** buffer) {
    fileHandle_t    h;
    byte*           buf;
    qboolean        isConfig;
    int             len;
    
    if (!qpath || !qpath[0]) {
        Com_Error(ERR_FATAL, "FS_ReadFile with empty name");
    }
    
    buf = NULL; // Quiet compiler warning
    
    // Look for it in the filesystem or pack files
    len = FS_FOpenFileRead(qpath, &h, qfalse);
    
    if (h == 0) {
        if (buffer) {
            *buffer = NULL;
        }
        return -1;
    }
    
    if (!buffer) {
        FS_FCloseFile(h);
        return len;
    }
    
    isConfig = !strcmp(COM_GetExtension(qpath), "cfg");
    
    if (isConfig) {
        buf = Z_Malloc(len + 1);
        *buffer = buf;
        buf[len] = 0;
    } else {
        buf = Hunk_AllocateTempMemory(len + 1);
        *buffer = buf;
        buf[len] = 0;
    }
    
    FS_Read(buf, len, h);
    
    // Guarantee that it will have a trailing 0 for string operations
    buf[len] = 0;
    FS_FCloseFile(h);
    
    return len;
}

void FS_FreeFile(void* buffer) {
    if (!buffer) {
        Com_Error(ERR_FATAL, "FS_FreeFile( NULL )");
    }
    Hunk_FreeTempMemory(buffer);
}

int FS_WriteFile(const char* qpath, const void* buffer, int size) {
    fileHandle_t f;
    
    if (!qpath || !buffer) {
        Com_Error(ERR_FATAL, "FS_WriteFile: NULL parameter");
    }
    
    f = FS_FOpenFileWrite(qpath);
    if (!f) {
        Com_Printf("Failed to open %s\n", qpath);
        return 0;
    }
    
    FS_Write(buffer, size, f);
    FS_FCloseFile(f);
    
    return 1;
}

// Streaming file access for large assets
typedef struct fileStream_s {
    fileHandle_t    handle;
    int             size;
    int             pos;
    byte*           buffer;
    int             bufferSize;
    int             bufferPos;
    int             bufferLen;
    qboolean        eof;
} fileStream_t;

fileStream_t* FS_OpenStream(const char* filename, int bufferSize) {
    fileStream_t* stream;
    int len;
    
    len = FS_FOpenFileRead(filename, &stream->handle, qfalse);
    if (!stream->handle) {
        return NULL;
    }
    
    stream = Z_Malloc(sizeof(*stream));
    stream->size = len;
    stream->pos = 0;
    stream->bufferSize = bufferSize;
    stream->buffer = Z_Malloc(bufferSize);
    stream->bufferPos = 0;
    stream->bufferLen = 0;
    stream->eof = qfalse;
    
    return stream;
}

int FS_ReadStream(fileStream_t* stream, void* buffer, int size) {
    byte* buf = (byte*)buffer;
    int totalRead = 0;
    int toRead;
    
    while (size > 0 && !stream->eof) {
        // Refill buffer if empty
        if (stream->bufferPos >= stream->bufferLen) {
            stream->bufferLen = FS_Read(stream->buffer, stream->bufferSize, stream->handle);
            stream->bufferPos = 0;
            
            if (stream->bufferLen == 0) {
                stream->eof = qtrue;
                break;
            }
        }
        
        // Copy from buffer
        toRead = Q_min(size, stream->bufferLen - stream->bufferPos);
        memcpy(buf, stream->buffer + stream->bufferPos, toRead);
        
        stream->bufferPos += toRead;
        stream->pos += toRead;
        buf += toRead;
        size -= toRead;
        totalRead += toRead;
    }
    
    return totalRead;
}</code></pre>
    </div>
</div>

<div class="section">
    <h2>Related Topics</h2>
    <ul>
        <li><a href="/core/engine-subsystems">Engine Subsystems</a></li>
        <li><a href="/core/memory-management">Memory Management</a></li>
        <li><a href="/development/modding">Modding Guide</a></li>
        <li><a href="/tools/asset-tools">Asset Tools</a></li>
        <li><a href="/platform/cross-platform">Cross-Platform Development</a></li>
    </ul>
</div>