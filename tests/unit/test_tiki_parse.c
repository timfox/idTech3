/*
 * Unit test: clean-room TIKI parser + TAN header.
 */
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#include "q_shared.h"
#include "tr_model_tiki.h"

#define ASSERT(cond, msg) do { \
	if (!(cond)) { fprintf(stderr, "FAIL: %s\n", msg); return 1; } \
} while (0)

int main( void ) {
	tikiDef_t def;
	char err[128];
	const char *src =
		"setup\n{\n  path models/demo/body.md3\n  scale 1.0\n}\n"
		"animations\n{\n  idle models/demo/idle.tan\n"
		"  walk {\n    path models/demo/walk.tan\n"
		"    cmd 3 footstep sound/player/step.wav\n"
		"    cmd 1 exec quit\n"
		"  }\n}\n";
	ASSERT( R_Tiki_Parse( src, (int)strlen( src ), &def, err, sizeof( err ) ), "parse ok" );
	ASSERT( def.numAnims >= 1, "has anims" );
	ASSERT( !strcmp( def.mesh, "models/demo/body.md3" ), "mesh path" );

	/* allowlisted footstep kept; exec rejected */
	{
		int i, foundFoot = 0, foundExec = 0;
		for ( i = 0; i < def.numAnims; i++ ) {
			int c;
			for ( c = 0; c < def.anims[i].numCmds; c++ ) {
				if ( !Q_stricmp( def.anims[i].cmds[c].cmd, "footstep" ) ) {
					foundFoot = 1;
				}
				if ( !Q_stricmp( def.anims[i].cmds[c].cmd, "exec" ) ) {
					foundExec = 1;
				}
			}
		}
		ASSERT( foundFoot, "footstep allowed" );
		ASSERT( !foundExec, "exec rejected" );
	}

	{
		byte tan[24] = {
			'T','A','N',' ',
			1,0,0,0, /* version */
			10,0,0,0, /* frames */
			5,0,0,0, /* bones */
			20,0,0,0, /* rate */
			24,0,0,0 /* ofs */
		};
		tikiTanHeader_t h;
		ASSERT( R_Tiki_ParseTanHeader( tan, 24, &h ), "tan header" );
		ASSERT( h.numFrames == 10, "tan frames" );
	}

	/* path traversal rejected */
	{
		const char *bad = "setup\n{\n path ../secret.md3\n}\n";
		ASSERT( !R_Tiki_Parse( bad, (int)strlen( bad ), &def, err, sizeof( err ) ), "reject .." );
	}

	printf( "unit_tiki_parse: PASS\n" );
	return 0;
}
