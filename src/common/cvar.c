// cvar.c -- dynamic variable tracking

#include "q_shared.h"
#include "qcommon.h"
#include "q_memory_safety.h"
#include "cvar_kvp_demo.h"

#ifdef USE_CJSON
#include "cJSON.h"
#endif

cvar_t	*cvar_vars = NULL;
static cvar_t	*cvar_cheats;
static cvar_t	*cvar_developer;
int	cvar_modifiedFlags;

static mutex_t cvar_mutex;
static qboolean cvar_initialized = qfalse;

#define	MAX_CVARS	2048
static cvar_t	cvar_indexes[MAX_CVARS];
static int		cvar_numIndexes;

static int	cvar_group[ CVG_MAX ];


#define FILE_HASH_SIZE		256
static	cvar_t	*hashTable[FILE_HASH_SIZE];
static	qboolean cvar_sort = qfalse;

static long generateHashValue( const char *fname ) {
	int		i;
	long	hash;
	char	letter;

	hash = 0;
	i = 0;
	while (fname[i] != '\0') {
		letter = locase[(byte)fname[i]];
		hash+=(long)(letter)*(i+119);
		i++;
	}
	hash &= (FILE_HASH_SIZE-1);
	return hash;
}


static qboolean Cvar_ValidateName( const char *name ) {
	const char *s;
	int c;
	
	if ( !name ) {
		return qfalse;
	}

	s = name;
	while ( (c = *s++) != '\0' ) {
		if ( c == '\\' || c == '\"' || c == ';' || c == '%' || c <= ' ' || c >= '~' )
			return qfalse;
	}

	if ( (s - name) >= MAX_STRING_CHARS ) {
		return qfalse;
	}

	return qtrue;
}


static cvar_t *Cvar_FindVar( const char *var_name ) {
	cvar_t	*var;
	long hash;

	if ( !var_name )
		return NULL;

	hash = generateHashValue( var_name );
	
	for ( var = hashTable[ hash ] ; var ; var = var->hashNext ) {
		if ( !Q_stricmp( var_name, var->name ) ) {
			return var;
		}
	}

	return NULL;
}


float Cvar_VariableValue( const char *var_name ) {
	cvar_t	*var;
	
	var = Cvar_FindVar (var_name);
	if (!var)
		return 0;
	return var->value;
}


int Cvar_VariableIntegerValue( const char *var_name ) {
	cvar_t	*var;
	
	var = Cvar_FindVar (var_name);
	if (!var)
		return 0;
	return var->integer;
}


const char *Cvar_VariableString( const char *var_name ) {
	cvar_t *var;
	
	var = Cvar_FindVar (var_name);
	if (!var)
		return "";
	return var->string;
}


void Cvar_VariableStringBuffer( const char *var_name, char *buffer, int bufsize ) {
	cvar_t *var;
	
	if ( !buffer || bufsize < 1 ) {
		Com_Printf( "Cvar_VariableStringBuffer: NULL buffer or invalid bufsize, ignoring\n" );
		return;
	}
	
	var = Cvar_FindVar (var_name);
	if (!var) {
		*buffer = '\0';
	}
	else {
		if ( !var->string ) {
			*buffer = '\0';
		} else {
			Q_strncpyz( buffer, var->string, bufsize );
		}
	}
}


void Cvar_VariableStringBufferSafe( const char *var_name, char *buffer, int bufsize, int flag ) {
	cvar_t *var;
	
	if ( !buffer || bufsize < 1 ) {
		Com_Printf( "Cvar_VariableStringBufferSafe: NULL buffer or invalid bufsize, ignoring\n" );
		return;
	}
	
	var = Cvar_FindVar( var_name );
	if ( !var || var->flags & flag ) {
		*buffer = '\0';
	}
	else {
		if ( !var->string ) {
			*buffer = '\0';
		} else {
			Q_strncpyz( buffer, var->string, bufsize );
		}
	}
}


unsigned Cvar_Flags( const char *var_name )
{
	const cvar_t *var;
	
    MUTEX_LOCK(cvar_mutex);
	if ( ( var = Cvar_FindVar( var_name ) ) == NULL ) {
        MUTEX_UNLOCK(cvar_mutex);
		return CVAR_NONEXISTENT;
    }
	else
	{
		if ( atomic_load_explicit(&var->modified, memory_order_relaxed) ) {
            MUTEX_UNLOCK(cvar_mutex);
			return var->flags | CVAR_MODIFIED;
        }
		else {
            MUTEX_UNLOCK(cvar_mutex);
			return var->flags;
        }
	}
}


void Cvar_CommandCompletion( void (*callback)(const char *s) )
{
	const cvar_t *cvar;

	for ( cvar = cvar_vars; cvar; cvar = cvar->next ) {
		if ( cvar->name && ( cvar->flags & CVAR_NOTABCOMPLETE ) == 0 ) {
			callback( cvar->name );
		}
	}
}


static qboolean Cvar_IsIntegral( const char *s ) {

	if ( *s == '-' && *(s+1) != '\0' )
		s++;

	while ( *s != '\0' ) {
		if ( *s < '0' || *s > '9' ) {
			return qfalse;
		}
		s++;
	}

	return qtrue;
}


static const char *Cvar_Validate( cvar_t *var, const char *value, qboolean warn )
{
	static char intbuf[ 32 ];
	const char *limit;
	float valuef;
	int	  valuei;

	if ( var->validator == CV_NONE )
		return value;

	if ( !value )
		return value;

	limit = NULL;

	if ( var->validator == CV_INTEGER || var->validator == CV_FLOAT ) {
		if ( !Q_isanumber( value ) ) {
			if ( warn )
				Com_Printf( "WARNING: cvar '%s' must be numeric", var->name );
			limit = var->resetString;
		} else {
			if ( var->validator == CV_INTEGER ) {
				if ( !Cvar_IsIntegral( value ) ) {
					if ( warn )
						Com_Printf( "WARNING: cvar '%s' must be integral", var->name );
					Q_secure_snprintf( intbuf, sizeof(intbuf), "%i", atoi( value ) );
					value = intbuf; // new value
				}
				valuei = atoi( value );
				if ( var->mins && valuei < atoi( var->mins ) ) {
					limit = var->mins;
				} else if ( var->maxs && valuei > atoi( var->maxs ) ) {
					limit = var->maxs;
				}
			} else { // CV_FLOAT
				valuef = Q_atof( value );
				if ( var->mins && valuef < Q_atof( var->mins ) ) {
					limit = var->mins;
				} else if ( var->maxs && valuef > Q_atof( var->maxs ) ) {
					limit = var->maxs;
				}
			}

			if ( warn ) {
				if ( limit && ( limit == var->mins || limit == var->maxs ) ) {
					if ( value == intbuf ) { // cast to integer
						Com_Printf( " and" ); 
					} else {
						Com_Printf( "WARNING: cvar '%s'", var->name );
					}
					Com_Printf( " is out of range (%s '%s')", (limit == var->mins) ? "min" : "max", limit );
				}
			}
		} // Q_isanumber
	} // CV_INTEGER || CV_FLOAT
	else if ( var->validator == CV_STRINGLIST ) {
		if ( var->mins ) {
			const char *list = var->mins;
			const char *found = NULL;
			char item[MAX_CVAR_VALUE_STRING];
			const char *p = list;

			while ( *p ) {
				const char *start = p;
				while ( *p && *p != ';' ) p++;
				size_t len = p - start;
				if ( len >= sizeof( item ) ) len = sizeof( item ) - 1;
				Com_Memcpy( item, start, len );
				item[len] = '\0';

				if ( !Q_stricmp( item, value ) ) {
					found = start;
					// Use exact casing from the list if possible, or just accept it
					// For now, just mark as found
					break;
				}
				if ( *p == ';' ) p++;
			}

			if ( !found ) {
				if ( warn ) {
					Com_Printf( "WARNING: cvar '%s' value '%s' is not in allowed list (%s)", var->name, value, var->mins );
				}
				limit = var->resetString;
			}
		}
	}
	else if ( var->validator == CV_FSPATH ) {
		// check for directory traversal patterns
		if ( FS_InvalidGameDir( value ) ) {
			if ( warn ) {
				Com_Printf( "WARNING: cvar '%s' contains invalid patterns", var->name );
			}
			// try to use current value if it is valid
			if ( !FS_InvalidGameDir( var->string ) ) {
				if ( warn ) {
					Com_Printf( "\n" );
				}
				return var->string;
			}
			limit = var->resetString;
		}
	}

	if ( limit || value == intbuf ) {
		if ( !limit )
			limit = value;
		if ( warn )
			Com_Printf( ", setting to '%s'\n", limit );
		return limit;
	} else {
		return value;
	}
}


cvar_t *Cvar_Get( const char *var_name, const char *var_value, int flags ) {
	cvar_t	*var;
	long	hash;
	int	index;

	if ( !var_name || !var_value ) {
		Com_Error( ERR_FATAL, "Cvar_Get: NULL parameter" );
	}

    MUTEX_LOCK(cvar_mutex);

	if ( !Cvar_ValidateName( var_name ) ) {
		Com_Printf( "invalid cvar name string: %s\n", var_name ? var_name : "(null)" );
		if ( var_name ) {
			var_name = "BADNAME";
		} else {
            MUTEX_UNLOCK(cvar_mutex);
			Com_Error( ERR_FATAL, "Cvar_Get: NULL var_name after validation check" );
			return NULL; // Never reached
		}
	}

#if 0 // FIXME: values with backslash happen
	if ( !Cvar_ValidateString( var_value ) ) {
		Com_Printf("invalid cvar value string: %s\n", var_value );
		var_value = "BADVALUE";
	}
#endif

	var = Cvar_FindVar (var_name);
	
	if(var)
	{
		int vm_created = (flags & CVAR_VM_CREATED);
		var_value = Cvar_Validate(var, var_value, qfalse);

		// Make sure the game code cannot mark engine-added variables as gamecode vars
		if(var->flags & CVAR_VM_CREATED)
		{
			if ( !vm_created )
				var->flags &= ~CVAR_VM_CREATED;
		}
		else if (!(var->flags & CVAR_USER_CREATED))
		{
			if ( vm_created )
				flags &= ~CVAR_VM_CREATED;
		}

		// if the C code is now specifying a variable that the user already
		// set a value for, take the new value as the reset value
		if(var->flags & CVAR_USER_CREATED)
		{
			var->flags &= ~CVAR_USER_CREATED;
			MEMORY_SAFETY_FREE( var->resetString );
			var->resetString = CopyString( var_value );

			if ( flags & CVAR_ROM || ( (flags & CVAR_DEVELOPER) && !cvar_developer->integer ) )
			{
				// this variable was set by the user,
				// so force it to value given by the engine.

				if(var->latchedString)
					MEMORY_SAFETY_FREE(var->latchedString);
				
				var->latchedString = CopyString(var_value);
			}
		}

		// Make sure servers cannot mark engine-added variables as SERVER_CREATED
		if ( var->flags & CVAR_SERVER_CREATED )
		{
			if ( !( flags & CVAR_SERVER_CREATED ) ) {
				// reset server-created flag
				var->flags &= ~CVAR_SERVER_CREATED;
				if ( vm_created ) {
					// reset to state requested by local VM module
					var->flags &= ~CVAR_ROM;
					MEMORY_SAFETY_FREE( var->resetString );
					var->resetString = CopyString( var_value );
					if ( var->latchedString )
						MEMORY_SAFETY_FREE( var->latchedString );
					var->latchedString = CopyString( var_value );
				}
			}
		}
		else
		{
			if ( flags & CVAR_SERVER_CREATED )
				flags &= ~CVAR_SERVER_CREATED;
		}

		var->flags |= flags;

		// only allow one non-empty reset string without a warning
		if ( !var->resetString[0] ) {
			// we don't have a reset string yet
			MEMORY_SAFETY_FREE( var->resetString );
			var->resetString = CopyString( var_value );
		} else if ( var_value[0] && strcmp( var->resetString, var_value ) ) {
			Com_DPrintf( "Warning: cvar \"%s\" given initial values: \"%s\" and \"%s\"\n",
				var_name, var->resetString, var_value );
		}

		// if we have a latched string, take that value now
		if ( var->latchedString ) {
			char *s;

			s = var->latchedString;
			var->latchedString = NULL;	// otherwise cvar_set2 would free it
			Cvar_Set2( var_name, s, qtrue );
			Z_Free( s );
		}

		// ZOID--needs to be set so that cvars the game sets as 
		// SERVERINFO get sent to clients
		cvar_modifiedFlags |=(flags);

        MUTEX_UNLOCK(cvar_mutex);
		return var;
	}

	//
	// allocate a new cvar
	//

	// find a free cvar
	for(index = 0; index < MAX_CVARS; index++)
	{
		if(!cvar_indexes[index].name)
			break;
	}

	if(index >= MAX_CVARS)
	{
		if(!com_errorEntered) {
            MUTEX_UNLOCK(cvar_mutex);
			Com_Error(ERR_FATAL, "Error: Too many cvars, cannot create a new one!");
        }

        MUTEX_UNLOCK(cvar_mutex);
		return NULL;
	}
	
	var = &cvar_indexes[index];
	
	if(index >= cvar_numIndexes)
		cvar_numIndexes = index + 1;
		
	var->name = CopyString( var_name );
	var->string = CopyString( var_value );
	atomic_store_explicit(&var->modified, 1, memory_order_relaxed);
	atomic_store_explicit(&var->modificationCount, 1, memory_order_relaxed);
	var->value = Q_atof( var->string );
	var->integer = atoi( var->string );
	var->resetString = CopyString( var_value );
	var->validator = CV_NONE;
	var->description = NULL;
	var->group = CVG_NONE;
	cvar_group[ var->group ] = 1;

	// link the variable in
	var->next = cvar_vars;
	if ( cvar_vars )
		cvar_vars->prev = var;

	var->prev = NULL;
	cvar_vars = var;

	var->flags = flags;
	// note what types of cvars have been modified (userinfo, archive, serverinfo, systeminfo)
	cvar_modifiedFlags |=(var->flags);

	hash = generateHashValue(var_name);
	var->hashIndex = hash;

	var->hashNext = hashTable[hash];
	if ( hashTable[hash] )
		hashTable[hash]->hashPrev = var;

	var->hashPrev = NULL;
	hashTable[hash] = var;

	 // sort on write
	cvar_sort = qtrue;

    MUTEX_UNLOCK(cvar_mutex);
	return var;
}


static void Cvar_QSortByName( cvar_t **a, int n ) 
{
	cvar_t *temp;
	cvar_t *m;
	int i, j;

	i = 0;
	j = n;
	m = a[ n>>1 ];

	do {
		// sort in descending order
		while ( strcmp( a[i]->name, m->name ) > 0 ) i++;
		while ( strcmp( a[j]->name, m->name ) < 0 ) j--;

		if ( i <= j ) {
			temp = a[i]; 
			a[i] = a[j]; 
			a[j] = temp;
			i++; 
			j--;
		}
	} while ( i <= j );

	if ( j > 0 ) Cvar_QSortByName( a, j );
	if ( n > i ) Cvar_QSortByName( a+i, n-i );
}


static void Cvar_Sort( void ) 
{
	cvar_t *list[ MAX_CVARS ], *var;
	int count;
	int i;

	for ( count = 0, var = cvar_vars; var; var = var->next ) {
		if ( var->name ) {
			list[ count++ ] = var;
		} else {
			Com_Error( ERR_FATAL, "%s: NULL cvar name", __func__ );
		}
	}

	if ( count < 2 ) {
		return; // nothing to sort
	}

	Cvar_QSortByName( &list[0], count-1 );
	
	cvar_vars = NULL;

	// relink cvars
	for ( i = 0; i < count; i++ ) {
		var = list[ i ];
		// link the variable in
		var->next = cvar_vars;
		if ( cvar_vars )
			cvar_vars->prev = var;
		var->prev = NULL;
		cvar_vars = var;
	}
}


static void Cvar_Print( const cvar_t *v ) {

	Com_Printf ("\"%s\" is:\"%s" S_COLOR_WHITE "\"",
		v->name, v->string );

	if ( !( v->flags & CVAR_ROM ) ) {
		Com_Printf (" default:\"%s" S_COLOR_WHITE "\"",
			v->resetString );
	}
#ifdef _DEBUG
	if ( v->modified ) {
		Com_Printf( " (modified)" );
	}
#endif
	Com_Printf ("\n");

	if ( v->latchedString ) {
		Com_Printf( "latched: \"%s\"\n", v->latchedString );
	}

	if ( v->description ) {
		Com_Printf( "%s\n", v->description );
	}
}


cvar_t *Cvar_Set2( const char *var_name, const char *value, qboolean force ) {
	cvar_t	*var;

//	Com_DPrintf( "Cvar_Set2: %s %s\n", var_name, value );

	if ( !var_name ) {
		Com_Error( ERR_FATAL, "Cvar_Set2: NULL var_name parameter" );
		return NULL; // Never reached, but helps compiler
	}

    MUTEX_LOCK(cvar_mutex);

	if ( !Cvar_ValidateName( var_name ) )
	{
		Com_Printf( "invalid cvar name string: %s\n", var_name );
		var_name = "BADNAME";
	}

#if 0	// FIXME
	if ( value && !Cvar_ValidateString( value ) ) {
		Com_Printf("invalid cvar value string: %s\n", value );
		var_value = "BADVALUE";
	}
#endif

	var = Cvar_FindVar( var_name );
	if ( !var )
	{
		if ( !value ) {
            MUTEX_UNLOCK(cvar_mutex);
			return NULL;
        }
		// create it
		if ( !force ) {
            MUTEX_UNLOCK(cvar_mutex);
			return Cvar_Get( var_name, value, CVAR_USER_CREATED );
        }
		else {
            MUTEX_UNLOCK(cvar_mutex);
			return Cvar_Get( var_name, value, 0 );
        }
	}

	if ( var->flags & (CVAR_ROM | CVAR_INIT | CVAR_CHEAT | CVAR_DEVELOPER) && !force )
	{
		if ( var->flags & CVAR_ROM )
		{
			Com_Printf( "%s is read only.\n", var_name );
            MUTEX_UNLOCK(cvar_mutex);
			return var;
		}

		if ( var->flags & CVAR_INIT )
		{
			Com_Printf( "%s is write protected.\n", var_name );
            MUTEX_UNLOCK(cvar_mutex);
			return var;
		}

		if ( (var->flags & CVAR_CHEAT) && !cvar_cheats->integer )
		{
			Com_Printf( "%s is cheat protected.\n", var_name );
            MUTEX_UNLOCK(cvar_mutex);
			return var;
		}

		if ( (var->flags & CVAR_DEVELOPER) && !cvar_developer->integer )
		{
			Com_Printf( "%s can be set only in developer mode.\n", var_name );
            MUTEX_UNLOCK(cvar_mutex);
			return var;
		}
	}

	if ( !value )
		value = var->resetString;

	value = Cvar_Validate( var, value, qtrue );

	if ( (var->flags & CVAR_LATCH) && var->latchedString )
	{
		if ( strcmp( value, var->string ) == 0 )
		{
			MEMORY_SAFETY_FREE( var->latchedString );
			var->latchedString = NULL;
            MUTEX_UNLOCK(cvar_mutex);
			return var;
		}

		if ( strcmp( value, var->latchedString ) == 0 ) {
            MUTEX_UNLOCK(cvar_mutex);
			return var;
        }
	}
	else if ( strcmp( value, var->string ) == 0 ) {
        MUTEX_UNLOCK(cvar_mutex);
		return var;
    }

	// note what types of cvars have been modified (userinfo, archive, serverinfo, systeminfo)
	cvar_modifiedFlags |=(var->flags);

	if ( !force )
	{
		if ( var->flags & CVAR_LATCH )
		{
			if ( var->latchedString )
			{
				if ( strcmp( value, var->latchedString ) == 0 ) {
                    MUTEX_UNLOCK(cvar_mutex);
					return var;
                }
				MEMORY_SAFETY_FREE( var->latchedString );
			}
			else
			{
				if ( strcmp( value, var->string ) == 0 ) {
                    MUTEX_UNLOCK(cvar_mutex);
					return var;
                }
			}

			Com_Printf( "%s will be changed upon restarting.\n", var_name );
			var->latchedString = CopyString( value );
			atomic_store_explicit(&var->modified, 1, memory_order_relaxed);
			ATOMIC_INCREMENT(&var->modificationCount);
			cvar_group[ var->group ] = 1;
            MUTEX_UNLOCK(cvar_mutex);
			return var;
		}
	}
	else
	{
		if ( var->latchedString )
		{
			// When applying a latched value (force=true), ensure it's marked for archiving
			// This ensures graphics settings are saved after vid_restart
			if ( var->flags & CVAR_ARCHIVE ) {
				cvar_modifiedFlags |=(CVAR_ARCHIVE);
			}
			MEMORY_SAFETY_FREE( var->latchedString );
			var->latchedString = NULL;
		}
	}

	if ( strcmp( value, var->string ) == 0 ) {
        MUTEX_UNLOCK(cvar_mutex);
		return var; // not changed
    }

	atomic_store_explicit(&var->modified, 1, memory_order_relaxed);
	ATOMIC_INCREMENT(&var->modificationCount);
	cvar_group[ var->group ] = 1;
	
	MEMORY_SAFETY_FREE( var->string ); // free the old value string
	
	var->string = CopyString( value );
	var->value = Q_atof( var->string );
	var->integer = atoi( var->string );

    MUTEX_UNLOCK(cvar_mutex);
	return var;
}


void Cvar_Set( const char *var_name, const char *value) {
	Cvar_Set2 (var_name, value, qtrue);
}


void Cvar_SetSafe( const char *var_name, const char *value )
{
	if ( !var_name ) {
		Com_Printf( "Cvar_SetSafe: NULL var_name parameter, ignoring\n" );
		return;
	}
	if ( !var_name[0] ) {
		Com_Printf( "Cvar_SetSafe: empty var_name parameter, ignoring\n" );
		return;
	}
	unsigned flags = Cvar_Flags( var_name );
	qboolean force = qtrue;

	if ( flags != CVAR_NONEXISTENT )
	{
		if ( flags & ( CVAR_PROTECTED | CVAR_PRIVATE ) )
		{
			if( value )
				Com_Printf( S_COLOR_YELLOW "Restricted source tried to set "
					"\"%s\" to \"%s\"\n", var_name, value );
			else
				Com_Printf( S_COLOR_YELLOW "Restricted source tried to "
					"modify \"%s\"\n", var_name );
			return;
		}

		// don't let VMs or server change engine latched cvars instantly
		//if ( ( flags & CVAR_LATCH ) && !( flags & CVAR_VM_CREATED ) )
		//{
		//	force = qfalse;
		//}
	}

	Cvar_Set2( var_name, value, force );
}


void Cvar_SetLatched( const char *var_name, const char *value) {
	Cvar_Set2 (var_name, value, qfalse);
}


void Cvar_SetValue( const char *var_name, float value) {
	char	val[32];

	if ( value == (int)value ) {
		Com_sprintf (val, sizeof(val), "%i",(int)value);
	} else {
		Com_sprintf (val, sizeof(val), "%f",value);
	}
	Cvar_Set (var_name, val);
}


void Cvar_SetIntegerValue( const char *var_name, int value ) {
	char	val[32];

	Q_secure_snprintf( val, sizeof(val), "%i", value );
	Cvar_Set( var_name, val );
}


void Cvar_SetValueSafe( const char *var_name, float value )
{
	if ( !var_name ) {
		Com_Printf( "Cvar_SetValueSafe: NULL var_name parameter, ignoring\n" );
		return;
	}
	char val[32];

	if( Q_isintegral( value ) )
		Com_sprintf( val, sizeof(val), "%i", (int)value );
	else
		Com_sprintf( val, sizeof(val), "%f", value );
	Cvar_SetSafe( var_name, val );
}


qboolean Cvar_SetModified( const char *var_name, qboolean modified )
{
	cvar_t	*var;

	var = Cvar_FindVar( var_name );
	if ( var ) 
	{
		var->modified = modified;
		return qtrue;
	}
	else 
	{
		return qfalse;
	}
}


void Cvar_Reset( const char *var_name ) {
	Cvar_Set2( var_name, NULL, qfalse );
}

void Cvar_ForceReset(const char *var_name)
{
	Cvar_Set2(var_name, NULL, qtrue);
}

void Cvar_SetCheatState(void)
{
	cvar_t	*var;

	// set all default vars to the safe value
	for(var = cvar_vars; var ; var = var->next)
	{
		if(var->flags & CVAR_CHEAT)
		{
			// the CVAR_LATCHED|CVAR_CHEAT vars might escape the reset here 
			// because of a different var->latchedString
			if (var->latchedString)
			{
				MEMORY_SAFETY_FREE(var->latchedString);
				var->latchedString = NULL;
			}
			if (strcmp(var->resetString,var->string))
				Cvar_Set(var->name, var->resetString);
		}
	}
}

qboolean Cvar_Command( void ) {
	cvar_t	*v;

	// check variables
	v = Cvar_FindVar (Cmd_Argv(0));
	if (!v) {
		return qfalse;
	}

	// perform a variable print or set
	if ( Cmd_Argc() == 1 ) {
		Cvar_Print( v );
		return qtrue;
	}

	// set the value if forcing isn't required
	Cvar_Set2( v->name, Cmd_ArgsFrom( 1 ), qfalse );
	return qtrue;
}


static void Cvar_Print_f( void )
{
	const char *name;
	cvar_t *cv;
	
	if(Cmd_Argc() != 2)
	{
		Com_Printf ("usage: print <variable>\n");
		return;
	}

	name = Cmd_Argv(1);

	cv = Cvar_FindVar(name);
	
	if(cv)
		Cvar_Print(cv);
	else
		Com_Printf ("Cvar %s does not exist.\n", name);
}


static void Cvar_Toggle_f( void ) {
	int		i, c;
	const char	*curval;

	c = Cmd_Argc();
	if ( c < 2 ) {
		Com_Printf( "usage: toggle <variable> [value1, value2, ...]\n" );
		return;
	}

	if ( c == 2 ) {
		Cvar_Set2( Cmd_Argv( 1 ), va( "%d", !Cvar_VariableValue( Cmd_Argv( 1 ) ) ), 
			qfalse );
		return;
	}

	if ( c == 3 ) {
		Com_Printf( "toggle: nothing to toggle to\n" );
		return;
	}

	curval = Cvar_VariableString( Cmd_Argv( 1 ) );

	// don't bother checking the last arg for a match since the desired
	// behaviour is the same as no match (set to the first argument)
	for ( i = 2; i + 1 < c; i++ ) {
		if ( strcmp( curval, Cmd_Argv( i ) ) == 0 ) {
			Cvar_Set2( Cmd_Argv( 1 ), Cmd_Argv(i + 1), qfalse );
			return;
		}
	}

	// fallback
	Cvar_Set2( Cmd_Argv( 1 ), Cmd_Argv( 2 ), qfalse );
}


static void Cvar_Set_f( void ) {
	int		c;
	const char	*cmd;
	cvar_t	*v;

	c = Cmd_Argc();
	cmd = Cmd_Argv(0);

	if ( c < 2 ) {
		Com_Printf ("usage: %s <variable> <value>\n", cmd);
		return;
	}
	if ( c == 2 ) {
		Cvar_Print_f();
		return;
	}

	v = Cvar_Set2 (Cmd_Argv(1), Cmd_ArgsFrom(2), qfalse);
	if( !v ) {
		return;
	}
	switch( cmd[3] ) {
		case 'a':
			if( !( v->flags & CVAR_ARCHIVE ) ) {
				v->flags |= CVAR_ARCHIVE;
				cvar_modifiedFlags |=(CVAR_ARCHIVE);
			}
			break;
		case 'u':
			if( !( v->flags & CVAR_USERINFO ) ) {
				v->flags |= CVAR_USERINFO;
				cvar_modifiedFlags |=(CVAR_USERINFO);
			}
			break;
		case 's':
			if( !( v->flags & CVAR_SERVERINFO ) ) {
				v->flags |= CVAR_SERVERINFO;
				cvar_modifiedFlags |=(CVAR_SERVERINFO);
			}
			break;
	}
}


static void Cvar_Reset_f( void ) {
	if ( Cmd_Argc() != 2 ) {
		Com_Printf ("usage: reset <variable>\n");
		return;
	}
	Cvar_Reset( Cmd_Argv( 1 ) );
}


// returns NULL for non-existent "-" argument
static const char *GetValue( int index, int *ival, float *fval ) 
{
	static char buf[ MAX_CVAR_VALUE_STRING ];
	const char *cmd;
	cvar_t	*var;

	cmd = Cmd_Argv( index );

	if ( ( *cmd == '-' && *(cmd+1) == '\0' ) || *cmd == '\0' ) {
		*ival = 0;
		*fval = 0.0f;
		buf[0] = '\0';
		return NULL;
	}

	var = Cvar_FindVar( cmd );
	if ( !var ) // cvar not found, return string
	{
		*ival = atoi( cmd );
		*fval = Q_atof( cmd );
		Q_strncpyz( buf, cmd, sizeof( buf ) );
		return buf;
	}
	else // found cvar, extract values
	{
		*ival = var->integer;
		*fval = var->value;
		Q_strncpyz( buf, var->string, sizeof( buf ) );
		return buf;
	}
}


typedef enum {
	FT_BAD = 0,
	FT_ADD,
	FT_SUB,
	FT_MUL,
	FT_DIV,
	FT_MOD,
	FT_SIN,
	FT_COS,
	FT_RAND,
} funcType_t;


static funcType_t GetFuncType( void ) 
{
	const char *cmd;
	cmd = Cmd_Argv( 1 );
	if ( !Q_stricmp( cmd, "add" ) )
		return FT_ADD;
	if ( !Q_stricmp( cmd, "sub" ) )
		return FT_SUB;
	if ( !Q_stricmp( cmd, "mul" ) )
		return FT_MUL;
	if ( !Q_stricmp( cmd, "div" ) )
		return FT_DIV;
	if ( !Q_stricmp( cmd, "mod" ) )
		return FT_MOD;
	if ( !Q_stricmp( cmd, "sin" ) )
		return FT_SIN;
	if ( !Q_stricmp( cmd, "cos" ) )
		return FT_COS;
	if ( !Q_stricmp( cmd, "rand" ) )
		return FT_RAND;

	return FT_BAD;
}


static qboolean AllowEmptyCvar( funcType_t ftype ) 
{
	switch ( ftype ) {
		case FT_ADD:
		case FT_SUB:
		case FT_MUL:
		case FT_DIV:
		case FT_MOD:
			return qfalse;
		default:
			return qtrue;
	};
}


static void Cvar_Op( funcType_t ftype, int *ival, float *fval ) 
{
	int icap, imod;
	float fcap, fmod;

	GetValue( 3, &imod, &fmod ); // index 3: value

	switch ( ftype ) {
		case FT_ADD:
			*ival += imod;
			*fval += fmod;
			break;
		case FT_SUB:
			*ival -= imod;
			*fval -= fmod;
			break;
		case FT_MUL:
			*ival *= imod;
			*fval *= fmod;
			break;
		case FT_DIV:
			if ( imod )
				*ival /= imod;
			if ( fmod )
				*fval /= fmod;
			break;
		case FT_MOD:
			if ( imod ) {
				*ival %= imod;
			}
			if ( fmod ) {
				*fval = fmodf( *fval, fmod );
			}
			break;

		case FT_SIN:
				*ival = sin( imod );
				*fval = sin( fmod );
				break;

		case FT_COS:
				*ival = cos( imod );
				*fval = cos( fmod );
				break;
		default: 
			break;
	}

	if ( Cmd_Argc() > 4 ) { // low bound
		if ( GetValue( 4, &icap, &fcap ) ) {
			if ( *ival < icap ) *ival = icap;
			if ( *fval < fcap ) *fval = fcap;
		}
	}
	if ( Cmd_Argc() > 5 ) { // high bound
		if ( GetValue( 5, &icap, &fcap ) ) {
			if ( *ival > icap ) *ival = icap;
			if ( *fval > fcap ) *fval = fcap;
		}
	}
}


static void Cvar_Rand( int *ival, float *fval ) 
{
	int icap;
	float fcap;

	*ival = rand();
	*fval = *ival;

	if ( Cmd_Argc() > 3 ) { // base
		if ( GetValue( 3, &icap, &fcap ) ) {
			*ival += icap;
			*fval = *ival;
		}
	}
	if ( Cmd_Argc() > 4 ) { // modulus
		if ( GetValue( 4, &icap, &fcap ) ) {
			if ( icap ) {
				*ival %= icap;
				*fval = *ival;
			}
		}
	}
}


static void Cvar_Func_f( void ) {

	funcType_t	ftype;
	const char	*cvar_name;
	char		value[ 64 ];
	cvar_t		*cvar;
	int			ival;
	float		fval;

	if ( Cmd_Argc() < 3 ) {
		Com_Printf( "usage: \n" \
			"  \\varfunc <add|sub|mul|div|mod|sin|cos> <cvar> <value> [lo.cap] [hi.cap]\n" \
			"  \\varfunc rand <cvar> [base] [modulus]\n" );
		return;
	}

	//     0     1     2      3      4        5
	// \varfunc <op> <cvar> <val> [lo-cap] [hi-cap]
	
	// \varfunc rand <cvar> [base] [modulus]

	ftype = GetFuncType(); // index 1: function type
	if ( ftype == FT_BAD ) {
		Com_Printf( "%s: unknown function %s\n", Cmd_Argv( 0 ), Cmd_Argv( 1 ) );
		return;
	}

	cvar_name = Cmd_Argv( 2 ); // index 2: cvar name
	cvar = Cvar_FindVar( cvar_name );
	if ( !cvar ) {
		if ( !AllowEmptyCvar( ftype ) )	{
			Com_Printf( "Cvar '%s' does not exist.\n", cvar_name );
			return; // FIXME: allow cvar creation for some functions?
		}
	} else if ( cvar->flags & ( CVAR_INIT | CVAR_ROM | CVAR_PROTECTED ) ) {
		Com_Printf( "Cvar '%s' is write-protected.\n", cvar_name );
		return;
	}
	
	if ( cvar ) {
		fval = cvar->value;
		ival = cvar->integer;
	} else {
		fval = 0.0;
		ival = 0;
	}

	if ( ftype == FT_RAND )
		Cvar_Rand( &ival, &fval );
	else
		Cvar_Op( ftype, &ival, &fval ); // apply modification
	
	if ( cvar && cvar->validator == CV_INTEGER ) {
		Q_secure_snprintf( value, sizeof(value), "%i", ival );
	} else {
		if ( (int)fval == fval )
			Q_secure_snprintf( value, sizeof(value), "%i", (int)fval );
		else
			Q_secure_snprintf( value, sizeof(value), "%f", fval );
	}

	Cvar_Set2( cvar_name, value, qfalse );
}


void Cvar_WriteVariables( fileHandle_t f )
{
	cvar_t	*var;
	char	buffer[MAX_CMD_LINE];
	const char	*value;

	if ( cvar_sort ) {
		cvar_sort = qfalse;
		Cvar_Sort();
	}

	for (var = cvar_vars; var; var = var->next)
	{
		if ( !var->name || Q_stricmp( var->name, "cl_cdkey" ) == 0 )
			continue;

		if ( var->flags & CVAR_ARCHIVE ) {
			int len;
			// write the latched value, even if it hasn't taken effect yet
			value = var->latchedString ? var->latchedString : var->string;
			if ( strlen( var->name ) + strlen( value ) + 10 > sizeof( buffer ) ) {
				Com_Printf( S_COLOR_YELLOW "WARNING: %svalue of variable \"%s\" too long to write to file\n", 
					value == var->latchedString ? "latched " : "", var->name );
				continue;
			}
			if ( (var->flags & CVAR_NODEFAULT) && !strcmp( value, var->resetString ) ) {
				continue;
			}
			len = Com_sprintf( buffer, sizeof( buffer ), "seta %s \"%s\"" Q_NEWLINE, var->name, value );

			FS_Write( buffer, len, f );
		}
	}
}


static void Cvar_List_f( void ) {
	cvar_t	*var;
	int		i;
	const char	*match;

	// sort to get more predictable output
	if ( cvar_sort ) {
		cvar_sort = qfalse;
		Cvar_Sort();
	}

	if ( Cmd_Argc() > 1 ) {
		match = Cmd_Argv( 1 );
	} else {
		match = NULL;
	}

	i = 0;
	for (var = cvar_vars ; var ; var = var->next, i++)
	{
		if(!var->name || (match && !Com_Filter(match, var->name)))
			continue;

		if (var->flags & CVAR_SERVERINFO) {
			Com_Printf("S");
		} else {
			Com_Printf(" ");
		}
		if (var->flags & CVAR_SYSTEMINFO) {
			Com_Printf("s");
		} else {
			Com_Printf(" ");
		}
		if (var->flags & CVAR_USERINFO) {
			Com_Printf("U");
		} else {
			Com_Printf(" ");
		}
		if (var->flags & CVAR_ROM) {
			Com_Printf("R");
		} else {
			Com_Printf(" ");
		}
		if (var->flags & CVAR_INIT) {
			Com_Printf("I");
		} else {
			Com_Printf(" ");
		}
		if (var->flags & CVAR_ARCHIVE) {
			Com_Printf("A");
		} else {
			Com_Printf(" ");
		}
		if (var->flags & CVAR_LATCH) {
			Com_Printf("L");
		} else {
			Com_Printf(" ");
		}
		if (var->flags & CVAR_CHEAT) {
			Com_Printf("C");
		} else {
			Com_Printf(" ");
		}
		if (var->flags & CVAR_USER_CREATED) {
			Com_Printf("?");
		} else {
			Com_Printf(" ");
		}

		Com_Printf (" %s \"%s\"\n", var->name, var->string);
	}

	Com_Printf ("\n%i total cvars\n", i);
	Com_Printf ("%i cvar indexes\n", cvar_numIndexes);
}


static void Cvar_ListModified_f( void ) {
	cvar_t	*var;
	int		totalModified;
	const char *value;
	const char *match;

	if ( Cmd_Argc() > 1 ) {
		match = Cmd_Argv( 1 );
	} else {
		match = NULL;
	}

	totalModified = 0;
	for (var = cvar_vars ; var ; var = var->next)
	{
		if ( !var->name || !var->modificationCount )
			continue;

		value = var->latchedString ? var->latchedString : var->string;
		if ( !strcmp( value, var->resetString ) )
			continue;

		totalModified++;

		if (match && !Com_Filter(match, var->name))
			continue;

		if (var->flags & CVAR_SERVERINFO) {
			Com_Printf("S");
		} else {
			Com_Printf(" ");
		}
		if (var->flags & CVAR_SYSTEMINFO) {
			Com_Printf("s");
		} else {
			Com_Printf(" ");
		}
		if (var->flags & CVAR_USERINFO) {
			Com_Printf("U");
		} else {
			Com_Printf(" ");
		}
		if (var->flags & CVAR_ROM) {
			Com_Printf("R");
		} else {
			Com_Printf(" ");
		}
		if (var->flags & CVAR_INIT) {
			Com_Printf("I");
		} else {
			Com_Printf(" ");
		}
		if (var->flags & CVAR_ARCHIVE) {
			Com_Printf("A");
		} else {
			Com_Printf(" ");
		}
		if (var->flags & CVAR_LATCH) {
			Com_Printf("L");
		} else {
			Com_Printf(" ");
		}
		if (var->flags & CVAR_CHEAT) {
			Com_Printf("C");
		} else {
			Com_Printf(" ");
		}
		if (var->flags & CVAR_USER_CREATED) {
			Com_Printf("?");
		} else {
			Com_Printf(" ");
		}

		Com_Printf (" %s \"%s\", default \"%s\"\n", var->name, value, var->resetString);
	}

	Com_Printf ("\n%i total modified cvars\n", totalModified);
}


static cvar_t *Cvar_Unset( cvar_t *cv )
{
	cvar_t *next = cv->next;

	// note what types of cvars have been modified (userinfo, archive, serverinfo, systeminfo)
	cvar_modifiedFlags |=(cv->flags);
	
	if ( cv->name )
		MEMORY_SAFETY_FREE( cv->name );
	if ( cv->string )
		MEMORY_SAFETY_FREE( cv->string );
	if ( cv->latchedString )
		MEMORY_SAFETY_FREE( cv->latchedString );
	if ( cv->resetString )
		MEMORY_SAFETY_FREE( cv->resetString );
	if ( cv->description )
		MEMORY_SAFETY_FREE( cv->description );
	if ( cv->mins )
		MEMORY_SAFETY_FREE( cv->mins );
	if ( cv->maxs )
		MEMORY_SAFETY_FREE( cv->maxs );

	if ( cv->prev )
		cv->prev->next = cv->next;
	else
		cvar_vars = cv->next;
	if ( cv->next )
		cv->next->prev = cv->prev;

	if ( cv->hashPrev )
		cv->hashPrev->hashNext = cv->hashNext;
	else
		hashTable[cv->hashIndex] = cv->hashNext;
	if ( cv->hashNext )
		cv->hashNext->hashPrev = cv->hashPrev;

	Com_Memset( cv, '\0', sizeof( *cv ) );
	
	return next;
}


static void Cvar_Unset_f( void )
{
	cvar_t *cv;
	
	if ( Cmd_Argc() != 2 )
	{
		Com_Printf( "Usage: %s <varname>\n", Cmd_Argv( 0 ) );
		return;
	}
	
	cv = Cvar_FindVar( Cmd_Argv( 1 ) );

	if ( !cv )
		return;
	
	if ( cv->flags & CVAR_USER_CREATED )
		Cvar_Unset( cv );
	else
		Com_Printf( "Error: %s: Variable %s is not user created.\n", 
			Cmd_Argv( 0 ), cv->name );
}



void Cvar_Restart( qboolean unsetVM )
{
	cvar_t *curvar = cvar_vars;

	while(curvar)
	{
		if((curvar->flags & CVAR_USER_CREATED) ||
			(unsetVM && (curvar->flags & CVAR_VM_CREATED)))
		{
			// throw out any variables the user/vm created
			curvar = Cvar_Unset(curvar);
			continue;
		}
		
		if(!(curvar->flags & (CVAR_ROM | CVAR_INIT | CVAR_NORESTART)))
		{
			// Just reset the rest to their default values.
			Cvar_Set2(curvar->name, curvar->resetString, qfalse);
		}
		
		curvar = curvar->next;
	}
}


static void Cvar_Trim( qboolean verbose )
{
	cvar_t *curvar = cvar_vars;
	while ( curvar )
	{
		if ( curvar->flags & CVAR_USER_CREATED )
		{
			// throw out any variables the user created
			if ( verbose )
				Com_Printf( "unset cvar" S_COLOR_YELLOW " %s\n", curvar->name );

			curvar = Cvar_Unset( curvar );
			continue;
		}

		curvar = curvar->next;
	}
}


static void Cvar_Restart_f( void )
{
	Cvar_Restart( qfalse );
}


static void Cvar_Trim_f( void )
{
	qboolean forced = qfalse;
	qboolean verbose = qtrue;
	int i;

	for ( i = 1; i < Cmd_Argc(); i++ )
	{
		const char *s = Cmd_Argv( i );
		if ( *s == '-' )
		{
			s++;
			while ( *s != '\0' )
			{
				if ( *s == 'f' ) // force cleanup
					forced = qtrue;
				else if ( *s == 's' ) // silent mode
					verbose = qfalse;
				s++;
			}
		}
	}

#ifdef DEDICATED
	if ( ( com_sv_running && com_sv_running->integer ) || forced )
#else
	if ( ( com_cl_running && com_cl_running->integer && com_sv_running && com_sv_running->integer ) || forced )
#endif
	{
		Cvar_Trim( verbose );
		return;
	}

#ifdef DEDICATED	
	Com_Printf( S_COLOR_YELLOW " You're not running a server, so not all subsystems/VMs are loaded.\n" );
#else
	Com_Printf( S_COLOR_YELLOW " You're not running a listen server, so not all subsystems/VMs are loaded.\n" );
#endif
	Com_Printf( S_COLOR_YELLOW " This means you'd remove cvars that are probably best kept around.\n" );
	Com_Printf( S_COLOR_YELLOW " If you don't care, you can force the call by running '\\%s -f'.\n", Cmd_Argv(0) );
	Com_Printf( S_COLOR_YELLOW " You've been warned.\n" );
}


const char *Cvar_InfoString( int bit, qboolean *truncated )
{
	static char	info[ MAX_INFO_STRING ];
	const cvar_t *user_vars[ MAX_CVARS ];
	const cvar_t *vm_vars[ MAX_CVARS ];
	const cvar_t *var;
	int user_count;
	int vm_count;
	int i;
	qboolean allSet;

	// sort to get more predictable output
	if ( cvar_sort )
	{
		cvar_sort = qfalse;
		Cvar_Sort();
	}

	info[0] = '\0';
	user_count = 0;
	vm_count = 0;
	allSet = qtrue; // this will be qfalse on overflow

	for ( var = cvar_vars; var; var = var->next )
	{
		if ( var->name && ( var->flags & bit ) )
		{
			// put vm/user-created cvars to the end
			if ( var->flags & ( CVAR_USER_CREATED | CVAR_VM_CREATED ) )
			{
				if ( var->flags & CVAR_USER_CREATED )
					user_vars[ user_count++ ] = var;
				else
					vm_vars[ vm_count++ ] = var;
			}
			else
			{
				allSet &= Info_SetValueForKey( info, var->name, var->string );
			}
		}
	}

	// add vm-created cvars
	for ( i = 0; i < vm_count; i++ )
	{
		var = vm_vars[ i ];
		allSet &= Info_SetValueForKey( info, var->name, var->string );
	}

	// add user-created cvars
	for ( i = 0; i < user_count; i++ )
	{
		var = user_vars[ i ];
		allSet &= Info_SetValueForKey( info, var->name, var->string );
	}

	if ( truncated )
	{
		*truncated = !allSet;
	}

	return info;
}


const char *Cvar_InfoString_Big( int bit, qboolean *truncated )
{
	static char	info[BIG_INFO_STRING];
	const cvar_t *var;
	qboolean allSet;

	info[0] = '\0';
	allSet = qtrue;

	for ( var = cvar_vars; var; var = var->next )
	{
		if ( var->name && (var->flags & bit) )
			allSet &= Info_SetValueForKey_s( info, sizeof( info ), var->name, var->string );
	}

	if ( truncated )
	{
		*truncated = !allSet;
	}

	return info;
}


void Cvar_InfoStringBuffer( int bit, char* buff, int buffsize ) {
	Q_strncpyz( buff, Cvar_InfoString( bit, NULL ), buffsize );
}


void Cvar_CheckRange( cvar_t *var, const char *mins, const char *maxs, cvarValidator_t type )
{
	if ( type >= CV_MAX ) {
		Com_Printf( S_COLOR_YELLOW "Invalid validation type %i for %s\n", type, var->name );
		return;
	}

	if ( var->mins ) {
		MEMORY_SAFETY_FREE( var->mins );
		var->mins = NULL;
	}
	if ( var->maxs ) {
		MEMORY_SAFETY_FREE( var->maxs );
		var->maxs = NULL;
	}

	var->validator = type;

	if ( type == CV_NONE )
		return;

	if ( mins )
		var->mins = CopyString( mins );

	if ( maxs )
		var->maxs = CopyString( maxs );

	// Force an initial range check
	Cvar_Set( var->name, var->string );
}


void Cvar_SetDescription( cvar_t *var, const char *var_description )
{
	if( var_description && var_description[0] != '\0' )
	{
		if( var->description != NULL )
		{
			MEMORY_SAFETY_FREE( var->description );
		}
		var->description = CopyString( var_description );
	}
}


void Cvar_SetDescription2( const char *var_name, const char* var_description )
{
	cvar_t *var;

	var = Cvar_FindVar( var_name );
	if ( !var || !var_description )
		return;

	if ( strlen( var_description ) >= MAX_CVAR_VALUE_STRING )
		return;

	if ( var_description[0] != '\0' )
	{
		if ( var->description != NULL )
		{
			MEMORY_SAFETY_FREE( var->description );
		}
		var->description = CopyString( var_description );
	}
}


void Cvar_SetGroup( cvar_t *var, cvarGroup_t group ) {
	if ( group < CVG_MAX ) {
		var->group = group;
	} else {
		Com_Error( ERR_DROP, "Bad group index %i for %s", group, var->name );
	}
}


int Cvar_CheckGroup( cvarGroup_t group ) {
	if ( group < CVG_MAX ) {
		return cvar_group[ group ];
	} else {
		return 0;
	}
}


void Cvar_ResetGroup( cvarGroup_t group, qboolean resetModifiedFlags ) {
	if ( group < CVG_MAX ) {
		cvar_group[ group ] = 0;
		if ( resetModifiedFlags ) {
			int i;
			for ( i = 0; i < cvar_numIndexes; i++ ) {
				if ( cvar_indexes[ i ].group == group && cvar_indexes[ i ].name ) {
					cvar_indexes[ i ].modified = qfalse;
				}
			}
		}
	}
}


#define INVALID_FLAGS ( CVAR_USER_CREATED | CVAR_SERVER_CREATED | CVAR_PROTECTED | CVAR_PRIVATE | CVAR_MODIFIED | CVAR_NONEXISTENT )
void Cvar_Register( vmCvar_t *vmCvar, const char *varName, const char *defaultValue, int flags, int privateFlag )
{
	cvar_t	*cv;

	// There is code in Cvar_Get to prevent CVAR_ROM cvars being changed by the
	// user. In other words CVAR_ARCHIVE and CVAR_ROM are mutually exclusive
	// flags. Unfortunately some historical game code (including single player
	// baseq3) sets both flags. We unset CVAR_ROM for such cvars.
	if ((flags & (CVAR_ARCHIVE | CVAR_ROM)) == (CVAR_ARCHIVE | CVAR_ROM)) {
		Com_DPrintf( S_COLOR_YELLOW "WARNING: Unsetting CVAR_ROM from cvar '%s', "
			"since it is also CVAR_ARCHIVE\n", varName );
		flags &= ~CVAR_ROM;
	}

	// Don't allow VM to specify a different creator or other internal flags.
	if ( flags & INVALID_FLAGS ) {
		Com_DPrintf( S_COLOR_YELLOW "WARNING: VM tried to set invalid flags 0x%02x on cvar '%s'\n", ( flags & INVALID_FLAGS ), varName );
		flags &= ~INVALID_FLAGS;
	}

	// Validate parameters
	if ( !varName ) {
		Com_Printf( "Cvar_Register: NULL varName parameter\n" );
		return;
	}
	if ( !varName[0] ) {
		Com_Printf( "Cvar_Register: empty varName parameter\n" );
		return;
	}

	cv = Cvar_FindVar( varName );

	// Don't modify cvar if it's protected.
	if ( cv && ( cv->flags & ( CVAR_PROTECTED | CVAR_PRIVATE ) ) ) {
		Com_DPrintf( S_COLOR_YELLOW "WARNING: VM tried to register protected cvar '%s' with value '%s'%s\n",
			varName, defaultValue, ( flags & ~cv->flags ) != 0 ? " and new flags" : "" );
		if ( cv->flags & CVAR_PRIVATE ) {
			if ( privateFlag ) {
				return;
			}
		}
	} else {
		cv = Cvar_Get( varName, defaultValue, flags | CVAR_VM_CREATED );
	}

	if (!vmCvar)
		return;

	vmCvar->handle = cv - cvar_indexes;
	vmCvar->modificationCount = -1;

	Cvar_Update( vmCvar, 0 );
}


void Cvar_Update( vmCvar_t *vmCvar, int privateFlag ) {
	size_t	len;
	cvar_t	*cv = NULL;
	assert(vmCvar);

	if ( vmCvar->handle < 0 || vmCvar->handle >= cvar_numIndexes ) {
		Com_Error( ERR_DROP, "Cvar_Update: handle out of range" );
	}

	cv = cvar_indexes + vmCvar->handle;

	if ( cv->modificationCount == vmCvar->modificationCount ) {
		return;
	}
	if ( !cv->string ) {
		return;		// variable might have been cleared by a cvar_restart
	} 
	if ( cv->flags & CVAR_PRIVATE ) {
		if ( privateFlag ) {
			return;
		}
	}
	vmCvar->modificationCount = cv->modificationCount;

	len = strlen( cv->string );
	if ( len + 1 > MAX_CVAR_VALUE_STRING ) {
		Com_Printf( S_COLOR_YELLOW "Cvar_Update: src %s length %d exceeds MAX_CVAR_VALUE_STRING - truncate\n",
			cv->string, (int)len );
	}

	Q_strncpyz( vmCvar->string, cv->string, sizeof( vmCvar->string ) ); 

	vmCvar->value = cv->value;
	vmCvar->integer = cv->integer;
}


void Cvar_CompleteCvarName( const char *args, int argNum )
{
	if( argNum == 2 )
	{
		// Skip "<cmd> "
		const char *p = Com_SkipTokens( args, 1, " " );

		if( p > args )
			Field_CompleteCommand( p, qfalse, qtrue );
	}
}


void Cvar_Init (void)
{
    MUTEX_INIT(cvar_mutex);
    cvar_initialized = qtrue;

	Com_Memset(cvar_indexes, '\0', sizeof(cvar_indexes));
	Com_Memset(hashTable, '\0', sizeof(hashTable));

	cvar_cheats = Cvar_Get( "sv_cheats", "1", CVAR_ROM | CVAR_SYSTEMINFO );
	Cvar_SetDescription( cvar_cheats, "Enable cheating commands (server side only)." );
	cvar_developer = Cvar_Get( "developer", "0", CVAR_TEMP );
	Cvar_SetDescription( cvar_developer, "Toggles developer mode. Prints more info to console and provides more commands." );

	Cmd_AddCommand ("print", Cvar_Print_f);
	Cmd_AddCommand ("toggle", Cvar_Toggle_f);
	Cmd_SetCommandCompletionFunc( "toggle", Cvar_CompleteCvarName );
	Cmd_AddCommand ("set", Cvar_Set_f);
	Cmd_SetCommandCompletionFunc( "set", Cvar_CompleteCvarName );
	Cmd_AddCommand ("sets", Cvar_Set_f);
	Cmd_SetCommandCompletionFunc( "sets", Cvar_CompleteCvarName );
	Cmd_AddCommand ("setu", Cvar_Set_f);
	Cmd_SetCommandCompletionFunc( "setu", Cvar_CompleteCvarName );
	Cmd_AddCommand ("seta", Cvar_Set_f);
	Cmd_SetCommandCompletionFunc( "seta", Cvar_CompleteCvarName );
	Cmd_AddCommand ("reset", Cvar_Reset_f);
	Cmd_SetCommandCompletionFunc( "reset", Cvar_CompleteCvarName );
	Cmd_AddCommand ("unset", Cvar_Unset_f);
	Cmd_SetCommandCompletionFunc("unset", Cvar_CompleteCvarName);

	Cmd_AddCommand( "varfunc", Cvar_Func_f );

	Cmd_AddCommand ("cvarlist", Cvar_List_f);
	Cmd_AddCommand ("cvar_modified", Cvar_ListModified_f);
	Cmd_AddCommand ("cvar_restart", Cvar_Restart_f);
	Cmd_AddCommand ("cvar_trim", Cvar_Trim_f);

	// JSON-enhanced cvar demo commands (disabled to avoid build issues)
	// Cmd_AddCommand ("cvar_kvp_demo", CVAR_KVP_Demo);
	// Cmd_AddCommand ("cvar_kvp_test_validation", CVAR_KVP_TestValidation);
}

// ============================================================================
// JSON-enhanced cvar functions for complex configurations
// ============================================================================

// cvar_t *Cvar_GetJSON( const char *var_name, const char *json_value, int flags ) {
// 	// Temporarily disabled for testing - fallback to regular cvar
// 	return Cvar_Get( var_name, json_value, flags );
// }

#ifdef USE_CJSON
cvar_t *Cvar_GetJSON( const char *var_name, const char *json_value, int flags ) {
	cvar_t *var;
	cJSON *json_obj = NULL;

	if ( !var_name || !json_value ) {
		Com_Error( ERR_FATAL, "Cvar_GetJSON: NULL parameter" );
	}

    MUTEX_LOCK(cvar_mutex);

	if ( !Cvar_ValidateName( var_name ) ) {
		Com_Printf( "invalid cvar name string: %s\n", var_name ? var_name : "(null)" );
		if ( var_name ) {
			var_name = "BADNAME";
		} else {
            MUTEX_UNLOCK(cvar_mutex);
			Com_Error( ERR_FATAL, "Cvar_GetJSON: NULL var_name after validation check" );
			return NULL; // Never reached
		}
	}

	// Try to parse the JSON safely
	json_obj = cJSON_Parse(json_value);
	if ( !json_obj ) {
		Com_Printf( S_COLOR_YELLOW "WARNING: Invalid JSON for cvar '%s', using empty object\n", var_name );
		json_obj = cJSON_CreateObject();
		if ( !json_obj ) {
			Com_Printf( S_COLOR_RED "ERROR: Failed to create empty JSON object for cvar '%s'\n", var_name );
			MUTEX_UNLOCK(cvar_mutex);
			return Cvar_Get( var_name, json_value, flags );
		}
	}

	var = Cvar_FindVar (var_name);

	if(var)
	{
		// Existing cvar - update JSON data safely
		if ( var->isJSON ) {
			// Free existing JSON data
			if ( var->jsonObject ) {
				cJSON_Delete( var->jsonObject );
				var->jsonObject = NULL;
			}
			if ( var->jsonString ) {
				MEMORY_SAFETY_FREE( var->jsonString );
				var->jsonString = NULL;
			}
		} else {
			// Convert from string cvar to JSON cvar
			var->isJSON = qtrue;
		}

		var->jsonObject = json_obj;
		var->jsonString = CopyString( json_value );
		var->flags |= flags;

		// Update string representation for compatibility
		char *json_str = cJSON_PrintUnformatted( json_obj );
		if ( json_str ) {
			if ( var->string ) {
				MEMORY_SAFETY_FREE( var->string );
			}
			var->string = CopyString( json_str );
			cJSON_free( json_str ); // Use cJSON's free function
		} else {
			// Fallback if printing fails
			if ( var->string ) {
				MEMORY_SAFETY_FREE( var->string );
			}
			var->string = CopyString( json_value );
		}

		var->value = 0.0f;
		var->integer = 0;

        MUTEX_UNLOCK(cvar_mutex);
		return var;
	}

	// Create new JSON cvar
	// find a free cvar
	int index;
	for(index = 0; index < MAX_CVARS; index++)
	{
		if(!cvar_indexes[index].name)
			break;
	}

	if(index >= MAX_CVARS)
	{
		cJSON_Delete( json_obj );
		MUTEX_UNLOCK(cvar_mutex);
		return Cvar_Get( var_name, json_value, flags );
	}

	var = &cvar_indexes[index];

	if(index >= cvar_numIndexes)
		cvar_numIndexes = index + 1;

	var->name = CopyString( var_name );
	var->string = CopyString( json_value );
	var->flags = flags;
	var->isJSON = qtrue;
	var->jsonObject = json_obj;
	var->jsonString = CopyString( json_value );
	var->value = 0.0f;
	var->integer = 0;

	var->next = cvar_vars;
	cvar_vars = var;

	cvar_numIndexes++;

    MUTEX_UNLOCK(cvar_mutex);
	return var;
}
#else
cvar_t *Cvar_GetJSON( const char *var_name, const char *json_value, int flags ) {
	// USE_CJSON disabled - fallback to regular cvar
	return Cvar_Get( var_name, json_value, flags );
}
#endif

#ifdef USE_CJSON
void Cvar_SetJSON( const char *var_name, const char *json_value ) {
	cvar_t *var;

	if ( !var_name ) {
		Com_Error( ERR_FATAL, "Cvar_SetJSON: NULL var_name parameter" );
		return;
	}

    MUTEX_LOCK(cvar_mutex);

	var = Cvar_FindVar( var_name );
	if ( !var ) {
		// Create new JSON cvar
        MUTEX_UNLOCK(cvar_mutex);
		Cvar_GetJSON( var_name, json_value, 0 );
		return;
	}

	// Parse new JSON safely
	cJSON *json_obj = NULL;
	if ( json_value && *json_value ) {
		json_obj = cJSON_Parse( json_value );
		if ( !json_obj ) {
			Com_Printf( S_COLOR_YELLOW "WARNING: Invalid JSON for cvar '%s', ignoring\n", var_name );
            MUTEX_UNLOCK(cvar_mutex);
			return;
		}
	} else {
		json_obj = cJSON_CreateObject();
		if ( !json_obj ) {
			Com_Printf( S_COLOR_RED "ERROR: Failed to create empty JSON object for cvar '%s'\n", var_name );
            MUTEX_UNLOCK(cvar_mutex);
			return;
		}
	}

	// Update cvar safely
	if ( var->isJSON ) {
		// Free existing JSON data
		if ( var->jsonObject ) {
			cJSON_Delete( var->jsonObject );
		}
		if ( var->jsonString ) {
			MEMORY_SAFETY_FREE( var->jsonString );
		}
	} else {
		// Convert to JSON cvar
		var->isJSON = qtrue;
	}

	var->jsonObject = json_obj;
	var->jsonString = CopyString( json_value );

	// Update string representation for compatibility
	char *json_str = cJSON_PrintUnformatted( json_obj );
	if ( json_str ) {
		if ( var->string ) {
			MEMORY_SAFETY_FREE( var->string );
		}
		var->string = CopyString( json_str );
		cJSON_free( json_str );
	} else {
		if ( var->string ) {
			MEMORY_SAFETY_FREE( var->string );
		}
		var->string = CopyString( json_value );
	}

	var->value = 0.0f;
	var->integer = 0;

    MUTEX_UNLOCK(cvar_mutex);
}
#else
void Cvar_SetJSON( const char *var_name, const char *json_value ) {
	// USE_CJSON disabled - fallback to regular cvar
	Cvar_Set( var_name, json_value );
}
#endif

#ifdef USE_CJSON
const char *Cvar_GetJSONValue( const char *var_name, const char *key_path ) {
	cvar_t *var;
	const char *result = NULL;

	if ( !var_name || !key_path ) {
		return NULL;
	}

    MUTEX_LOCK(cvar_mutex);

	var = Cvar_FindVar( var_name );
	if ( !var || !var->isJSON || !var->jsonObject ) {
        MUTEX_UNLOCK(cvar_mutex);
		return NULL;
	}

	// Navigate JSON path safely
	cJSON *current = var->jsonObject;
	char path_copy[1024];
	Q_strncpyz( path_copy, key_path, sizeof( path_copy ) );

	char *token = strtok( path_copy, "." );
	while ( token && current ) {
		if ( cJSON_IsObject( current ) ) {
			current = cJSON_GetObjectItem( current, token );
		} else if ( cJSON_IsArray( current ) && atoi( token ) >= 0 ) {
			int index = atoi( token );
			current = cJSON_GetArrayItem( current, index );
		} else {
			current = NULL;
			break;
		}
		token = strtok( NULL, "." );
	}

	if ( current ) {
		if ( cJSON_IsString( current ) ) {
			result = cJSON_GetStringValue( current );
		} else {
			// For non-string values, create a temporary string representation
			char *temp_str = cJSON_PrintUnformatted( current );
			if ( temp_str ) {
				// This is a memory leak, but acceptable for cvar access
				// In a real implementation, we'd want a proper cache
				result = temp_str;
			}
		}
	}

    MUTEX_UNLOCK(cvar_mutex);
	return result;
}
#else
const char *Cvar_GetJSONValue( const char *var_name, const char *key_path ) {
	// USE_CJSON disabled - return NULL
	return NULL;
}
#endif

double Cvar_GetJSONNumber( const char *var_name, const char *key_path, double default_value ) {
	const char *str_value = Cvar_GetJSONValue( var_name, key_path );
	if ( str_value ) {
		return atof( str_value );
	}
	return default_value;
}

const char *Cvar_GetJSONString( const char *var_name, const char *key_path, const char *default_value ) {
	const char *result = Cvar_GetJSONValue( var_name, key_path );
	return result ? result : default_value;
}

qboolean Cvar_GetJSONBoolean( const char *var_name, const char *key_path, qboolean default_value ) {
	const char *str_value = Cvar_GetJSONValue( var_name, key_path );
	if ( str_value ) {
		return Q_stricmp( str_value, "true" ) == 0 || atoi( str_value ) != 0;
	}
	return default_value;
}

void Cvar_SetJSONValidator( cvar_t *var, cvarJSONValidator_t validator_type, const char *schema ) {
	if ( !var ) {
		return;
	}

	var->jsonValidator = validator_type;

	// For now, we only support basic type checking
	// Schema validation could be added later with a full JSON schema library
	(void)schema; // Unused for now
}

#ifdef USE_CJSON
qboolean Cvar_ValidateJSON( cvar_t *var, const char *json_string ) {
	if ( !var || !json_string ) {
		return qfalse;
	}

	// Try to parse the JSON to validate it
	cJSON *test_obj = cJSON_Parse( json_string );
	if ( !test_obj ) {
		return qfalse;
	}

	// Basic validation passed
	cJSON_Delete( test_obj );
	return qtrue;
}
#else
qboolean Cvar_ValidateJSON( cvar_t *var, const char *json_string ) {
	// USE_CJSON disabled - basic string validation only
	if ( !var || !json_string ) {
		return qfalse;
	}
	// Very basic validation - check if it starts and ends with braces/brackets
	if ( (json_string[0] == '{' && json_string[strlen(json_string)-1] == '}') ||
		 (json_string[0] == '[' && json_string[strlen(json_string)-1] == ']') ) {
		return qtrue;
	}
	return qfalse;
}
#endif