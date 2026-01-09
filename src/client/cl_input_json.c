/*
===========================================================================
JSON-based input configuration system using enhanced cvar KVP functionality.
Provides structured configuration for key bindings with modifiers and
controller mappings.
===========================================================================
*/

#include "client.h"
#include "qcommon.h"

/*
================
Input Configuration JSON Schema

This defines the structure for input configuration using JSON cvars.
Example configuration:

{
  "keyboard": {
    "bindings": {
      "move_forward": {
        "key": "w",
        "modifiers": ["shift"],
        "repeat": true
      },
      "jump": {
        "key": "space",
        "modifiers": [],
        "repeat": false
      },
      "crouch": {
        "key": "ctrl",
        "modifiers": [],
        "repeat": true
      }
    }
  },
  "mouse": {
    "sensitivity": 1.5,
    "acceleration": 0.2,
    "invert_y": false,
    "raw_input": true
  },
  "controller": {
    "enabled": true,
    "device": "auto",
    "deadzone": {
      "left_stick": 0.1,
      "right_stick": 0.1,
      "triggers": 0.05
    },
    "mappings": {
      "move_forward": "left_stick_y+",
      "move_backward": "left_stick_y-",
      "strafe_left": "left_stick_x-",
      "strafe_right": "left_stick_x+",
      "look_horizontal": "right_stick_x",
      "look_vertical": "right_stick_y",
      "jump": "a_button",
      "crouch": "b_button",
      "attack": "right_trigger"
    }
  }
}
================
*/

static cvar_t *in_config_json = NULL;

// Function prototypes
void IN_InitConfigurationJSON(void);
qboolean IN_GetKeyboardBinding(const char *action, const char **key, const char **modifiers, int max_modifiers, qboolean *repeat);
void IN_SetKeyboardBinding(const char *action, const char *action_key, const char *modifiers, qboolean repeat);
void IN_GetMouseConfig(float *sensitivity, float *acceleration, qboolean *invert_y, qboolean *raw_input);
void IN_UpdateMouseConfig(float sensitivity, float acceleration, qboolean invert_y, qboolean raw_input);
void IN_GetControllerConfig(qboolean *enabled, const char **device,
    float *left_deadzone, float *right_deadzone, float *trigger_deadzone,
    const char **mapping_move_forward, const char **mapping_jump);
void IN_UpdateControllerConfig(qboolean enabled, const char *device,
    float left_deadzone, float right_deadzone, float trigger_deadzone);
const char *IN_GetControllerMapping(const char *action);
void IN_SetControllerMapping(const char *action, const char *mapping);
void IN_ShutdownConfigurationJSON(void);

/*
================
IN_InitConfigurationJSON

Initialize JSON-based input configuration
================
*/
void IN_InitConfigurationJSON(void) {
	const char *default_config = "{"
		"\"keyboard\": {"
			"\"bindings\": {"
				"\"move_forward\": {"
					"\"key\": \"w\","
					"\"modifiers\": [],"
					"\"repeat\": true"
				"},"
				"\"move_backward\": {"
					"\"key\": \"s\","
					"\"modifiers\": [],"
					"\"repeat\": true"
				"},"
				"\"strafe_left\": {"
					"\"key\": \"a\","
					"\"modifiers\": [],"
					"\"repeat\": true"
				"},"
				"\"strafe_right\": {"
					"\"key\": \"d\","
					"\"modifiers\": [],"
					"\"repeat\": true"
				"},"
				"\"jump\": {"
					"\"key\": \"space\","
					"\"modifiers\": [],"
					"\"repeat\": false"
				"},"
				"\"crouch\": {"
					"\"key\": \"ctrl\","
					"\"modifiers\": [],"
					"\"repeat\": true"
				"}"
			"}"
		"},"
		"\"mouse\": {"
			"\"sensitivity\": 1.0,"
			"\"acceleration\": 0.0,"
			"\"invert_y\": false,"
			"\"raw_input\": true"
		"},"
		"\"controller\": {"
			"\"enabled\": false,"
			"\"device\": \"auto\","
			"\"deadzone\": {"
				"\"left_stick\": 0.1,"
				"\"right_stick\": 0.1,"
				"\"triggers\": 0.05"
			"},"
			"\"mappings\": {"
				"\"move_forward\": \"left_stick_y+\","
				"\"move_backward\": \"left_stick_y-\","
				"\"strafe_left\": \"left_stick_x-\","
				"\"strafe_right\": \"left_stick_x+\","
				"\"look_horizontal\": \"right_stick_x\","
				"\"look_vertical\": \"right_stick_y\","
				"\"jump\": \"a_button\","
				"\"crouch\": \"b_button\","
				"\"attack\": \"right_trigger\""
			"}"
		"}"
	"}";

	in_config_json = Cvar_GetJSON("in_config_json", default_config, CVAR_ARCHIVE);
	Cvar_SetDescription(in_config_json, "JSON configuration for input settings (keyboard, mouse, controller)");
	Cvar_SetJSONValidator(in_config_json, CVJ_TYPE_CHECK, NULL);
}

/*
================
IN_GetKeyboardBinding

Retrieve keyboard binding configuration from JSON
================
*/
qboolean IN_GetKeyboardBinding(const char *action, const char **key, const char **modifiers, int max_modifiers __attribute__((unused)), qboolean *repeat) {
	if (!in_config_json || !action) {
		return qfalse;
	}

	char key_path[256];
	Com_sprintf(key_path, sizeof(key_path), "keyboard.bindings.%s.key", action);
	*key = Cvar_GetJSONString("in_config_json", key_path, NULL);

	if (!*key) {
		return qfalse;
	}

	Com_sprintf(key_path, sizeof(key_path), "keyboard.bindings.%s.repeat", action);
	*repeat = Cvar_GetJSONBoolean("in_config_json", key_path, qfalse);

	// For modifiers, we'd need to parse the JSON array
	// This is a simplified implementation - in practice you'd parse the array
	*modifiers = NULL;

	return qtrue;
}

/*
================
IN_SetKeyboardBinding

Update keyboard binding configuration in JSON
================
*/
void IN_SetKeyboardBinding(const char *action, const char *key, const char *modifiers, qboolean repeat) {
	if (!in_config_json || !action || !key) {
		return;
	}

	char json_buffer[512];
	Com_sprintf(json_buffer, sizeof(json_buffer),
		"{"
			"\"keyboard\": {"
				"\"bindings\": {"
					"\"%s\": {"
						"\"key\": \"%s\","
						"\"modifiers\": [%s],"
						"\"repeat\": %s"
					"}"
				"}"
			"}"
		"}",
		action,
		key,
		modifiers ? modifiers : "",
		repeat ? "true" : "false");

	Cvar_SetJSON("in_config_json", json_buffer);
}

/*
================
IN_GetMouseConfig

Retrieve mouse configuration from JSON
================
*/
void IN_GetMouseConfig(float *sensitivity, float *acceleration, qboolean *invert_y, qboolean *raw_input) {
	if (!in_config_json) {
		*sensitivity = 1.0f;
		*acceleration = 0.0f;
		*invert_y = qfalse;
		*raw_input = qtrue;
		return;
	}

	*sensitivity = (float)Cvar_GetJSONNumber("in_config_json", "mouse.sensitivity", 1.0);
	*acceleration = (float)Cvar_GetJSONNumber("in_config_json", "mouse.acceleration", 0.0);
	*invert_y = Cvar_GetJSONBoolean("in_config_json", "mouse.invert_y", qfalse);
	*raw_input = Cvar_GetJSONBoolean("in_config_json", "mouse.raw_input", qtrue);
}

/*
================
IN_UpdateMouseConfig

Update mouse configuration in JSON
================
*/
void IN_UpdateMouseConfig(float sensitivity, float acceleration, qboolean invert_y, qboolean raw_input) {
	if (!in_config_json) return;

	char json_buffer[512];
	Com_sprintf(json_buffer, sizeof(json_buffer),
		"{"
			"\"mouse\": {"
				"\"sensitivity\": %.2f,"
				"\"acceleration\": %.2f,"
				"\"invert_y\": %s,"
				"\"raw_input\": %s"
			"}"
		"}",
		sensitivity,
		acceleration,
		invert_y ? "true" : "false",
		raw_input ? "true" : "false");

	Cvar_SetJSON("in_config_json", json_buffer);
}

/*
================
IN_GetControllerConfig

Retrieve controller configuration from JSON
================
*/
void IN_GetControllerConfig(qboolean *enabled, const char **device,
						   float *left_deadzone, float *right_deadzone, float *trigger_deadzone,
						   const char **mapping_move_forward, const char **mapping_jump) {
	if (!in_config_json) {
		*enabled = qfalse;
		*device = "auto";
		*left_deadzone = 0.1f;
		*right_deadzone = 0.1f;
		*trigger_deadzone = 0.05f;
		*mapping_move_forward = "left_stick_y+";
		*mapping_jump = "a_button";
		return;
	}

	*enabled = Cvar_GetJSONBoolean("in_config_json", "controller.enabled", qfalse);
	*device = Cvar_GetJSONString("in_config_json", "controller.device", "auto");
	*left_deadzone = (float)Cvar_GetJSONNumber("in_config_json", "controller.deadzone.left_stick", 0.1);
	*right_deadzone = (float)Cvar_GetJSONNumber("in_config_json", "controller.deadzone.right_stick", 0.1);
	*trigger_deadzone = (float)Cvar_GetJSONNumber("in_config_json", "controller.deadzone.triggers", 0.05);
	*mapping_move_forward = Cvar_GetJSONString("in_config_json", "controller.mappings.move_forward", "left_stick_y+");
	*mapping_jump = Cvar_GetJSONString("in_config_json", "controller.mappings.jump", "a_button");
}

/*
================
IN_UpdateControllerConfig

Update controller configuration in JSON
================
*/
void IN_UpdateControllerConfig(qboolean enabled, const char *device,
							  float left_deadzone, float right_deadzone, float trigger_deadzone) {
	if (!in_config_json) return;

	char json_buffer[1024];
	Com_sprintf(json_buffer, sizeof(json_buffer),
		"{"
			"\"controller\": {"
				"\"enabled\": %s,"
				"\"device\": \"%s\","
				"\"deadzone\": {"
					"\"left_stick\": %.2f,"
					"\"right_stick\": %.2f,"
					"\"triggers\": %.2f"
				"}"
			"}"
		"}",
		enabled ? "true" : "false",
		device ? device : "auto",
		left_deadzone,
		right_deadzone,
		trigger_deadzone);

	Cvar_SetJSON("in_config_json", json_buffer);
}

/*
================
IN_GetControllerMapping

Retrieve a specific controller mapping from JSON
================
*/
const char *IN_GetControllerMapping(const char *action) {
	if (!in_config_json || !action) {
		return NULL;
	}

	char key_path[256];
	Com_sprintf(key_path, sizeof(key_path), "controller.mappings.%s", action);

	return Cvar_GetJSONString("in_config_json", key_path, NULL);
}

/*
================
IN_SetControllerMapping

Update a specific controller mapping in JSON
================
*/
void IN_SetControllerMapping(const char *action, const char *mapping) {
	if (!in_config_json || !action || !mapping) {
		return;
	}

	char json_buffer[512];
	Com_sprintf(json_buffer, sizeof(json_buffer),
		"{"
			"\"controller\": {"
				"\"mappings\": {"
					"\"%s\": \"%s\""
				"}"
			"}"
		"}",
		action,
		mapping);

	Cvar_SetJSON("in_config_json", json_buffer);
}

/*
================
IN_ShutdownConfigurationJSON

Shutdown JSON-based input configuration
================
*/
void IN_ShutdownConfigurationJSON(void) {
	// JSON cvars are automatically cleaned up by the cvar system
	in_config_json = NULL;
}