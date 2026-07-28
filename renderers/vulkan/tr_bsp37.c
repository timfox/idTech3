/*
===========================================================================
BSP37 compatibility compile unit.

The renderer currently routes GoldSrc-style BSP rendering through the single
R_LoadBSP30World implementation in tr_bsp30.c.  Keep this file in the build
list as a marker for future Titanfall-family loader work without duplicating
the BSP30 bridge symbols.
===========================================================================
*/

#include "tr_local.h"
