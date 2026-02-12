/*
 * SPDX-License-Identifier: GPL-2.0-or-later
 * engine_tool commandlet host for automation tasks.
 */
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#if !defined(PATH_MAX)
# define PATH_MAX 4096
#endif

#if !defined(_WIN32)
# include <sys/wait.h>
#endif

static int shell_exit_code(int status) {
#if defined(_WIN32)
    return status;
#else
    if (WIFEXITED(status)) {
        return WEXITSTATUS(status);
    }
    return status;
#endif
}

static int run_string(const char *command) {
    printf("engine_tool> %s\n", command);
    int status = system(command);
    if (status < 0) {
        perror("engine_tool: system");
        return 127;
    }
    return shell_exit_code(status);
}

static int run_validate_assets(void) {
    return run_string("python3 scripts/asset_validation.py validate");
}

static int run_ddc(int argc, char **argv) {
    if (argc < 3) {
        fprintf(stderr, "engine_tool ddc: subcommand required\n");
        return 1;
    }
    char command[PATH_MAX];
    strncpy(command, "bash scripts/asset_ddc.sh", sizeof(command));
    command[sizeof(command) - 1] = '\0';
    for (int i = 2; i < argc; ++i) {
        strncat(command, " ", sizeof(command) - strlen(command) - 1);
        strncat(command, argv[i], sizeof(command) - strlen(command) - 1);
    }
    return run_string(command);
}

static int run_cook_map(const char *map_name, int force_build) {
    char command[PATH_MAX];
    if (!force_build) {
        snprintf(command, sizeof(command), "bash scripts/asset_ddc.sh needs-rebuild %s", map_name);
        int needs = run_string(command);
        if (needs == 1) {
            printf("engine_tool: %s is cached, skipping cook\n", map_name);
            return 0;
        }
        if (needs != 0) {
            printf("engine_tool: DDC reported code %d, forcing rebuild\n", needs);
        }
    }

    snprintf(command, sizeof(command), "bash scripts/build_maps.sh %s", map_name);
    int result = run_string(command);
    if (result != 0) {
        return result;
    }

    snprintf(command, sizeof(command), "bash scripts/asset_ddc.sh update %s", map_name);
    return run_string(command);
}

static void print_usage(const char *prog) {
    fprintf(stderr,
            "Usage: %s <command> [args...]\n"
            "Commands:\n"
            "  validate-assets          Run the asset validation lints\n"
            "  cook-map <map> [--force] Run the map-tool pipeline for <map> (checks DDC)\n"
            "  ddc <subcommand> [...]   Proxy to scripts/asset_ddc.sh\n",
            prog);
}

int main(int argc, char **argv) {
    if (argc < 2) {
        print_usage(argv[0]);
        return 1;
    }

    if (strcmp(argv[1], "validate-assets") == 0) {
        return run_validate_assets();
    } else if (strcmp(argv[1], "cook-map") == 0) {
        if (argc < 3) {
            fprintf(stderr, "engine_tool cook-map: map name required\n");
            return 1;
        }
        int force = 0;
        const char *map_name = NULL;
        for (int i = 2; i < argc; ++i) {
            if (strcmp(argv[i], "--force") == 0 || strcmp(argv[i], "-f") == 0) {
                force = 1;
                continue;
            }
            if (!map_name) {
                map_name = argv[i];
            }
        }
        if (!map_name) {
            fprintf(stderr, "engine_tool cook-map: map name required\n");
            return 1;
        }
        return run_cook_map(map_name, force);
    } else if (strcmp(argv[1], "ddc") == 0) {
        return run_ddc(argc, argv);
    }

    fprintf(stderr, "engine_tool: unknown command '%s'\n", argv[1]);
    print_usage(argv[0]);
    return 1;
}
