/*
===========================================================================
DaX evaluated foundation models (Table 2) and benchmark scores.
===========================================================================
*/

#include "dax/dax.h"

#include <string.h>

static const dax_model_row_t model_table[DAX_MODEL_COUNT] = {
	{ DAX_MODEL_DAX, "DaX", "ViT-L/16", 304, 104569, 41.2f },
	{ DAX_MODEL_DAX_BASE, "DaX-Base", "ViT-B/16", 86, 104569, 38.5f },
	{ DAX_MODEL_H_OPTIMUS_1, "H-Optimus-1", "ViT-G/14", 1100, 1000000, 39.8f },
	{ DAX_MODEL_UNI2, "UNI2", "ViT-H/14", 632, 350000, 37.6f },
	{ DAX_MODEL_VIRCHOW2, "Virchow2", "ViT-H/14", 632, 3100000, 36.9f },
	{ DAX_MODEL_UNI, "UNI", "ViT-L/16", 304, 100000, 35.4f },
	{ DAX_MODEL_CONCH, "CONCH", "ViT-B/16", 90, 0, 34.8f },
	{ DAX_MODEL_GPFM, "GPFM", "ViT-L/14", 307, 72280, 34.2f },
	{ DAX_MODEL_MUSK, "MUSK", "BEiT-3", 675, 33000, 33.7f },
	{ DAX_MODEL_DINOV3_VITL, "DINOv3 ViT-L", "ViT-L/16", 300, 0, 32.1f },
	{ DAX_MODEL_RESNET50, "ResNet-50", "ResNet-50", 26, 0, 28.4f },
};

const char *Dax_ModelName( dax_model_id_t id )
{
	if ( id >= 0 && id < DAX_MODEL_COUNT ) {
		return model_table[id].name;
	}
	return "unknown";
}

const dax_model_row_t *Dax_ModelTable( int *count )
{
	if ( count ) {
		*count = DAX_MODEL_COUNT;
	}
	return model_table;
}

void Dax_ModelLookup( dax_model_id_t id, dax_model_row_t *out )
{
	if ( !out ) {
		return;
	}
	if ( id >= 0 && id < DAX_MODEL_COUNT ) {
		*out = model_table[id];
		return;
	}
	memset( out, 0, sizeof( *out ) );
}
