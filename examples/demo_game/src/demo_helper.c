/*
 * Tiny host utility: print how to install and run idtech3_demo.pk3.
 * Built only when BUILD_EXAMPLE_DEMO_GAME=ON (see examples/demo_game/CMakeLists.txt).
 */
#include <stdio.h>
#include <stdlib.h>

int main(int argc, char **argv) {
	(void)argc;
	(void)argv;

	printf("idtech3_demo_helper - example mod launcher hints\n\n");
	printf("1. Build the pack: cmake --build <build-dir> --target demo_game_pk3\n");
	printf("   Output: <build-dir>/idtech3_demo.pk3\n\n");
	printf("2. Install: copy idtech3_demo.pk3 into <install>/idtech3_demo/\n");
	printf("   beside your full base/ (or baseq3/) tree.\n\n");
	printf("3. Run:\n");
	printf("   idtech3 +set fs_basepath <install> +set fs_game idtech3_demo +set cl_renderer vulkan\n\n");
	printf("4. Load a map from your base game, e.g. +map q3dm1\n\n");
	printf("See examples/demo_game/README.md in the engine repo.\n");
	return EXIT_SUCCESS;
}
