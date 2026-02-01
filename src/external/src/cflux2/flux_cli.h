/*
 * FLUX Interactive CLI Mode
 */

#ifndef FLUX_CLI_H
#define FLUX_CLI_H

#include "flux.h"

/*
 * Run the interactive CLI. Called when flux is invoked without a prompt.
 * Returns 0 on success, non-zero on error.
 */
int flux_cli_run(flux_ctx *ctx, const char *model_dir);

#endif /* FLUX_CLI_H */
