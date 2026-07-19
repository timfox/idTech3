/*
===========================================================================
RTFEM — island heuristic + status helpers.
===========================================================================
*/

#include "rtfem/rtfem_internal.h"

int RtFem_LargeIslandHeuristic( int islandNodes, int liveTotalNodes )
{
	/*
	 * Paper §4: "large" if at least sixty nodes and more than one quarter
	 * the total number of nodes in all live islands.
	 */
	if ( islandNodes < 60 ) {
		return 0;
	}
	if ( liveTotalNodes <= 0 ) {
		return 0;
	}
	return ( islandNodes * 4 > liveTotalNodes ) ? 1 : 0;
}
