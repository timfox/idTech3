/*
 * Unit test: clean-room Babble parser.
 */
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#include "q_shared.h"
#include "modules/dialogue/babble.h"

#define ASSERT(cond, msg) do { \
	if (!(cond)) { fprintf(stderr, "FAIL: %s\n", msg); return 1; } \
} while (0)

int main( void ) {
	babbleGraph_t graphs[4];
	char err[128];
	const char *src =
		"graph demo\n"
		"start hello\n"
		"node hello\n"
		"  speaker Guide\n"
		"  loc babble.demo.hello\n"
		"  duration 1.5\n"
		"  choice Continue next\n"
		"node next\n"
		"  speaker Guide\n"
		"  loc babble.demo.next\n";
	int n = Babble_ParseBuffer( src, (int)strlen( src ), graphs, 4, err, sizeof( err ) );
	ASSERT( n == 1, "one graph" );
	ASSERT( graphs[0].numNodes == 2, "two nodes" );
	ASSERT( !strcmp( graphs[0].startNode, "hello" ), "start node" );
	ASSERT( graphs[0].nodes[0].numChoices == 1, "one choice" );
	ASSERT( graphs[0].nodes[0].duration > 1.0f, "duration" );

	/* reject path traversal */
	{
		const char *bad =
			"graph x\nnode a\nvoice ../etc/passwd\n";
		n = Babble_ParseBuffer( bad, (int)strlen( bad ), graphs, 4, err, sizeof( err ) );
		ASSERT( n == 0, "reject unsafe voice path" );
	}

	printf( "unit_babble_parse: PASS\n" );
	return 0;
}
