/*
===========================================================================
JSON-based input configuration system header.
===========================================================================
*/

#ifndef CL_INPUT_JSON_H
#define CL_INPUT_JSON_H

// Function prototypes
void IN_InitConfigurationJSON(void);
void IN_ShutdownConfigurationJSON(void);
qboolean IN_GetKeyboardBinding(const char *action, const char **key, const char **modifiers, int max_modifiers, qboolean *repeat);
void IN_SetKeyboardBinding(const char *action, const char *key, const char *modifiers, qboolean repeat);
void IN_GetMouseConfig(float *sensitivity, float *acceleration, qboolean *invert_y, qboolean *raw_input);
void IN_UpdateMouseConfig(float sensitivity, float acceleration, qboolean invert_y, qboolean raw_input);
void IN_GetControllerConfig(qboolean *enabled, const char **device, float *left_deadzone, float *right_deadzone, float *trigger_deadzone, const char **mapping_move_forward, const char **mapping_jump);
void IN_UpdateControllerConfig(qboolean enabled, const char *device, float left_deadzone, float right_deadzone, float trigger_deadzone);
const char *IN_GetControllerMapping(const char *action);
void IN_SetControllerMapping(const char *action, const char *mapping);

// JSON-based input configuration functions
void IN_InitConfigurationJSON(void);
void IN_ShutdownConfigurationJSON(void);

// Keyboard binding functions
qboolean IN_GetKeyboardBinding(const char *action, const char **key, const char **modifiers, int max_modifiers, qboolean *repeat);
void IN_SetKeyboardBinding(const char *action, const char *key, const char *modifiers, qboolean repeat);

// Mouse configuration functions
void IN_GetMouseConfig(float *sensitivity, float *acceleration, qboolean *invert_y, qboolean *raw_input);
void IN_UpdateMouseConfig(float sensitivity, float *acceleration, qboolean invert_y, qboolean raw_input);

// Controller configuration functions
void IN_GetControllerConfig(qboolean *enabled, const char **device,
						   float *left_deadzone, float *right_deadzone, float *trigger_deadzone,
						   const char **mapping_move_forward, const char **mapping_jump);
void IN_UpdateControllerConfig(qboolean enabled, const char *device,
							  float left_deadzone, float right_deadzone, float trigger_deadzone);

// Individual controller mapping functions
const char *IN_GetControllerMapping(const char *action);
void IN_SetControllerMapping(const char *action, const char *mapping);

#endif // CL_INPUT_JSON_H