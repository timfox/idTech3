/*
===========================================================================
Crafting System Header

Simple 2-item combination recipe system.
===========================================================================
*/

#ifndef _G_CRAFTING_H
#define _G_CRAFTING_H

#include "g_local.h"
#include "g_inventory.h"

// Crafting system functions (implemented in g_inventory.c)
void G_Crafting_Init( void );
void G_Crafting_RegisterRecipe( int input1, int input2, int output, int outputQuantity, const char *name );
qboolean G_Crafting_CanCraft( int clientNum, int itemId1, int itemId2 );
qboolean G_Crafting_Craft( int clientNum, int itemId1, int itemId2 );
craft_recipe_t *G_Crafting_FindRecipe( int itemId1, int itemId2 );
qboolean G_Crafting_LoadRecipes( const char *filename );

#endif

