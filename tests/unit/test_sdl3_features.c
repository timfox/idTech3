#include <SDL3/SDL.h>

#include <stdio.h>
#include <string.h>

static int failures;

static void expect_true( const char *label, int value )
{
	if ( !value ) {
		fprintf( stderr, "FAIL %s: %s\n", label, SDL_GetError() );
		failures++;
	} else {
		fprintf( stderr, "ok   %s\n", label );
	}
}

int main( int argc, char **argv )
{
	SDL_JoystickID *gamepads = NULL;
	SDL_CameraID *cameras = NULL;
	int gamepadCount = 0;
	int cameraCount = 0;
	int mappingResult;
	(void)argc;
	(void)argv;

	SDL_SetHint( SDL_HINT_VIDEO_DRIVER, "dummy" );
	SDL_SetHint( SDL_HINT_AUDIO_DRIVER, "dummy" );

	expect_true( "SDL_Init video/gamepad/camera",
		SDL_Init( SDL_INIT_VIDEO | SDL_INIT_GAMEPAD | SDL_INIT_CAMERA ) );

	SDL_SetGamepadEventsEnabled( true );
	expect_true( "gamepad events enabled", SDL_GamepadEventsEnabled() );

	mappingResult = SDL_AddGamepadMapping( "03000000000000000000000000000000,IdTech3 Test Pad,a:b0,b:b1,back:b2,start:b3,leftx:a0,lefty:a1" );
	expect_true( "gamepad mapping accepted", mappingResult >= 0 );

	gamepads = SDL_GetGamepads( &gamepadCount );
	expect_true( "gamepad enumeration call", gamepads != NULL || gamepadCount == 0 );
	SDL_free( gamepads );

	cameras = SDL_GetCameras( &cameraCount );
	expect_true( "camera enumeration call", cameras != NULL || cameraCount == 0 );
	SDL_free( cameras );

	SDL_QuitSubSystem( SDL_INIT_CAMERA | SDL_INIT_GAMEPAD | SDL_INIT_VIDEO );
	SDL_Quit();

	if ( failures ) {
		fprintf( stderr, "unit_sdl3_features: %d failure(s)\n", failures );
		return 1;
	}

	printf( "unit_sdl3_features: ok\n" );
	return 0;
}
