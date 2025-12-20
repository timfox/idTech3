/*
===========================================================================
Copyright (C) 1999-2005 Id Software, Inc.

This file is part of Quake III Arena source code.

Quake III Arena source code is free software; you can redistribute it
and/or modify it under the terms of the GNU General Public License as
published by the Free Software Foundation; either version 2 of the License,
or (at your option) any later version.

Quake III Arena source code is distributed in the hope that it will be
useful, but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with Quake III Arena source code; if not, write to the Free Software
Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
===========================================================================
*/

#include "client.h"
#include "cl_imgui_debug.h"
#include "cl_net_enhanced.h"
#include "../common/q_memtrack.h"
#include "../common/q_log.h"
#include "../common/qcommon.h"
#include "../common/event_system.h"
#include "../common/performance_counters.h"

#ifdef USE_CIMGUI

#include <stdbool.h>
#include <string.h>
#include <stdlib.h>

#define CIMGUI_DEFINE_ENUMS_AND_STRUCTS
#include "cimgui.h"

// Frame time history for graphs
#define FRAME_HISTORY_SIZE 120
static float frame_times[FRAME_HISTORY_SIZE];
static int frame_history_offset = 0;
static float fps_history[FRAME_HISTORY_SIZE];

// CVars for debug overlays
cvar_t *cl_imgui_debug_performance;
static cvar_t *cl_imgui_debug_memory;
static cvar_t *cl_imgui_debug_network;
static cvar_t *cl_imgui_debug_renderer;
static cvar_t *cl_imgui_debug_cvars;
static cvar_t *cl_imgui_debug_console;
static cvar_t *cl_imgui_debug_events;
static cvar_t *cl_imgui_debug_profiler;
static cvar_t *cl_imgui_debug_mainmenu;

// CVar browser state
static char cvar_filter[256] = "";
static char cvar_edit_value[256] = "";
static cvar_t *cvar_selected = NULL;

// Console overlay state
static char console_filter[256] = "";
static bool console_auto_scroll = true;

/*
================
CL_ImGui_Debug_RegisterCvars
================
*/
static void CL_ImGui_Debug_RegisterCvars(void) {
	cl_imgui_debug_performance = Cvar_Get("cl_imgui_debug_performance", "0", CVAR_ARCHIVE_ND);
	Cvar_SetDescription(cl_imgui_debug_performance, "Display FPS, frame times, and performance metrics");
	
	cl_imgui_debug_memory = Cvar_Get("cl_imgui_debug_memory", "0", CVAR_ARCHIVE_ND);
	Cvar_SetDescription(cl_imgui_debug_memory, "Display memory usage statistics and leak information");
	
	cl_imgui_debug_network = Cvar_Get("cl_imgui_debug_network", "0", CVAR_ARCHIVE_ND);
	Cvar_SetDescription(cl_imgui_debug_network, "Display network statistics and connection info");
	
	cl_imgui_debug_renderer = Cvar_Get("cl_imgui_debug_renderer", "0", CVAR_ARCHIVE_ND);
	Cvar_SetDescription(cl_imgui_debug_renderer, "Display renderer statistics and draw call info");
	
	cl_imgui_debug_cvars = Cvar_Get("cl_imgui_debug_cvars", "0", CVAR_ARCHIVE_ND);
	Cvar_SetDescription(cl_imgui_debug_cvars, "Browse and edit console variables");
	
	cl_imgui_debug_console = Cvar_Get("cl_imgui_debug_console", "0", CVAR_ARCHIVE_ND);
	Cvar_SetDescription(cl_imgui_debug_console, "Display console output in ImGui window");
	
	cl_imgui_debug_events = Cvar_Get("cl_imgui_debug_events", "0", CVAR_ARCHIVE_ND);
	Cvar_SetDescription(cl_imgui_debug_events, "Display event system statistics and subscriptions");
	
	cl_imgui_debug_profiler = Cvar_Get("cl_imgui_debug_profiler", "0", CVAR_ARCHIVE_ND);
	Cvar_SetDescription(cl_imgui_debug_profiler, "Display profiler and performance counter information");
	
	cl_imgui_debug_mainmenu = Cvar_Get("cl_imgui_debug_mainmenu", "1", CVAR_ARCHIVE_ND);
	Cvar_SetDescription(cl_imgui_debug_mainmenu, "Show main debug menu bar");
}

/*
================
CL_ImGui_Debug_ShowPerformanceOverlay
================
*/
void CL_ImGui_Debug_ShowPerformanceOverlay(void) {
	if (!cl_imgui_debug_performance || !cl_imgui_debug_performance->integer) {
		return;
	}
	
	bool open = true;
	if (!igBegin("Performance", &open, 0)) {
		igEnd();
		if (!open) {
			Cvar_Set("cl_imgui_debug_performance", "0");
		}
		return;
	}
	
	// Update frame time history
	const float frameMs = (cls.realFrametime > 0) ? (float)cls.realFrametime : 1.0f;
	const float fps = 1000.0f / frameMs;
	
	frame_times[frame_history_offset] = frameMs;
	fps_history[frame_history_offset] = fps;
	frame_history_offset = (frame_history_offset + 1) % FRAME_HISTORY_SIZE;
	
	// Current stats
	igText("FPS: %.1f", fps);
	igText("Frame Time: %.2f ms", frameMs);
	igSameLine(0, 20);
	igText("Min: %.2f ms", frameMs < 16.67f ? frameMs : 16.67f);
	igSameLine(0, 20);
	igText("Max: %.2f ms", frameMs > 16.67f ? frameMs : 16.67f);
	
	igSeparator();
	
	// Frame time graph
	igText("Frame Time History (ms)");
	ImVec2 graph_size = { -1, 100 };
	igPlotLines_FloatPtr("##frametime", frame_times, FRAME_HISTORY_SIZE, frame_history_offset, NULL, 0.0f, 50.0f, graph_size, sizeof(float));
	
	// FPS graph
	igText("FPS History");
	igPlotLines_FloatPtr("##fps", fps_history, FRAME_HISTORY_SIZE, frame_history_offset, NULL, 0.0f, 200.0f, graph_size, sizeof(float));
	
	igSeparator();
	
	// Additional stats
	igText("Client State: %s", cls.state == CA_DISCONNECTED ? "Disconnected" :
	                          cls.state == CA_CONNECTING ? "Connecting" :
	                          cls.state == CA_CHALLENGING ? "Challenging" :
	                          cls.state == CA_CONNECTED ? "Connected" :
	                          cls.state == CA_LOADING ? "Loading" :
	                          cls.state == CA_PRIMED ? "Primed" :
	                          cls.state == CA_ACTIVE ? "Active" : "Unknown");
	
		if (cls.state >= CA_CONNECTED) {
			igText("Client Time: %d ms", cls.realtime);
		}
	
	igEnd();
}

/*
================
CL_ImGui_Debug_ShowMemoryOverlay
================
*/
void CL_ImGui_Debug_ShowMemoryOverlay(void) {
	if (!cl_imgui_debug_memory || !cl_imgui_debug_memory->integer) {
		return;
	}
	
	bool open = true;
	if (!igBegin("Memory Statistics", &open, 0)) {
		igEnd();
		if (!open) {
			Cvar_Set("cl_imgui_debug_memory", "0");
		}
		return;
	}
	
#ifdef ENABLE_MEMORY_TRACKING
	memstats_t total_stats;
	Q_MemTrack_GetTotalStats(&total_stats);
	
	igText("Total Memory");
	igSeparator();
	igText("Allocated: %.2f MB", (float)total_stats.allocated / (1024.0f * 1024.0f));
	igText("Freed: %.2f MB", (float)total_stats.freed / (1024.0f * 1024.0f));
	igText("Current: %.2f MB", (float)total_stats.current / (1024.0f * 1024.0f));
	igText("Peak: %.2f MB", (float)total_stats.peak / (1024.0f * 1024.0f));
	igText("Allocations: %lld", (long long)total_stats.count);
	igText("Frees: %lld", (long long)total_stats.free_count);
	igText("Leaks: %d", Q_MemTrack_GetLeakCount());
	
	igSeparator();
	
	// Per-type breakdown
	const char *type_names[] = {
		"HUNK", "ZONE", "TEMP", "SOUND", "RENDERER",
		"NETWORK", "FILESYSTEM", "SCRIPT", "BOTLIB", "OTHER"
	};
	
	if (igCollapsingHeader_BoolPtr("Memory by Type", NULL, 0)) {
		for (int i = 0; i < MEMTYPE_COUNT; i++) {
			memstats_t stats;
			Q_MemTrack_GetStats((memtype_t)i, &stats);
			
			if (stats.current > 0 || stats.peak > 0) {
				igText("%s:", type_names[i]);
				igIndent(20);
				igText("Current: %.2f KB", (float)stats.current / 1024.0f);
				igText("Peak: %.2f KB", (float)stats.peak / 1024.0f);
				igText("Allocations: %lld", (long long)stats.count);
				igUnindent(20);
			}
		}
	}
	
	if (igButton("Report Leaks", (ImVec2){-1, 0})) {
		Q_MemTrack_ReportLeaks();
	}
#else
	igText("Memory tracking not enabled");
	igText("Build with ENABLE_MEMORY_TRACKING=ON to enable");
#endif
	
	igEnd();
}

/*
================
CL_ImGui_Debug_ShowNetworkOverlay
================
*/
void CL_ImGui_Debug_ShowNetworkOverlay(void) {
	if (!cl_imgui_debug_network || !cl_imgui_debug_network->integer) {
		return;
	}
	
	bool open = true;
	if (!igBegin("Network Statistics", &open, 0)) {
		igEnd();
		if (!open) {
			Cvar_Set("cl_imgui_debug_network", "0");
		}
		return;
	}
	
	igText("Network Statistics");
	igSeparator();
	
#ifdef USE_CURL
	extern net_stats_t *NET_Stats_Get(void);
	net_stats_t *stats_ptr = NET_Stats_Get();
	if (stats_ptr) {
		net_stats_t stats = *stats_ptr;
		
		igText("Bytes Sent: %.2f MB", (float)stats.bytes_sent / (1024.0f * 1024.0f));
		igText("Bytes Received: %.2f MB", (float)stats.bytes_received / (1024.0f * 1024.0f));
		igText("Total Requests: %llu", (unsigned long long)stats.requests_total);
		igText("Successful: %llu", (unsigned long long)stats.requests_success);
		igText("Failed: %llu", (unsigned long long)stats.requests_failed);
		
		if (stats.requests_total > 0) {
			float success_rate = ((float)stats.requests_success / (float)stats.requests_total) * 100.0f;
			igText("Success Rate: %.1f%%", success_rate);
		}
		
		igSeparator();
		igText("Response Times");
		igText("Average: %d ms", stats.average_response_time);
		igText("Last: %d ms", stats.last_response_time);
		
		igSeparator();
		igText("Protocol Usage");
		igText("HTTP/2: %d", (int)stats.http2_requests);
		igText("HTTP/1.1: %d", (int)stats.http1_requests);
		igText("IPv6: %d", (int)stats.ipv6_requests);
		igText("IPv4: %d", (int)stats.ipv4_requests);
	} else {
		igText("Network statistics not available");
	}
#else
	igText("Network statistics require curl support");
#endif
	
	if (cls.state >= CA_CONNECTED) {
		igSeparator();
		igText("Connection Info");
		igText("State: Connected");
		igText("Client Time: %d ms", cls.realtime);
	}
	
	igEnd();
}

/*
================
CL_ImGui_Debug_ShowRendererOverlay
================
*/
void CL_ImGui_Debug_ShowRendererOverlay(void) {
	if (!cl_imgui_debug_renderer || !cl_imgui_debug_renderer->integer) {
		return;
	}
	
	bool open = true;
	if (!igBegin("Renderer Statistics", &open, 0)) {
		igEnd();
		if (!open) {
			Cvar_Set("cl_imgui_debug_renderer", "0");
		}
		return;
	}
	
	igText("Renderer: %s", cls.glconfig.renderer_string);
	igText("Vendor: %s", cls.glconfig.vendor_string);
	igText("Version: %s", cls.glconfig.version_string);
	igText("Extensions: %s", cls.glconfig.extensions_string);
	
	igSeparator();
	igText("Display");
	igText("Resolution: %d x %d", cls.glconfig.vidWidth, cls.glconfig.vidHeight);
	igText("Color Bits: %d", cls.glconfig.colorBits);
	igText("Depth Bits: %d", cls.glconfig.depthBits);
	igText("Stencil Bits: %d", cls.glconfig.stencilBits);
	
	// Renderer performance counters (if available)
	// Note: r_speeds is a renderer CVAR, access via Cvar_Get
	cvar_t *r_speeds = Cvar_Get("r_speeds", "0", 0);
	if (r_speeds && r_speeds->integer) {
		igSeparator();
		igText("Performance Counters");
		igText("(Enable r_speeds for detailed stats)");
	}
	
	igEnd();
}

/*
================
CL_ImGui_Debug_ShowCVarBrowser
================
*/
void CL_ImGui_Debug_ShowCVarBrowser(void) {
	if (!cl_imgui_debug_cvars || !cl_imgui_debug_cvars->integer) {
		return;
	}
	
	bool open = true;
	if (!igBegin("CVar Browser", &open, 0)) {
		igEnd();
		if (!open) {
			Cvar_Set("cl_imgui_debug_cvars", "0");
		}
		return;
	}
	
	// Filter input
	igInputText("Filter", cvar_filter, sizeof(cvar_filter), 0, NULL, NULL);
	
	igSameLine(0, 10);
	if (igButton("Clear", (ImVec2){0, 0})) {
		cvar_filter[0] = '\0';
	}
	
	igSeparator();
	
	// CVar list
	if (igBeginChild_Str("##cvarlist", (ImVec2){0, -60}, false, 0)) {
		extern cvar_t *cvar_vars;
		cvar_t *var = cvar_vars;
		int count = 0;
		
		while (var) {
			// Apply filter
			if (cvar_filter[0] != '\0' && !Q_stristr(var->name, cvar_filter)) {
				var = var->next;
				continue;
			}
			
			// Selectable row
			bool selected = (var == cvar_selected);
			if (igSelectable_Bool(var->name, selected, 0, (ImVec2){0, 0})) {
				cvar_selected = var;
				Q_strncpyz(cvar_edit_value, var->string, sizeof(cvar_edit_value));
			}
			
			// Show value on same line
			igSameLine(200, 0);
			igText("%s", var->string);
			
			// Show flags
			igSameLine(400, 0);
			if (var->flags & CVAR_ARCHIVE) igText("[A]");
			if (var->flags & CVAR_USERINFO) igText("[U]");
			if (var->flags & CVAR_SERVERINFO) igText("[S]");
			if (var->flags & CVAR_ROM) igText("[R]");
			if (var->flags & CVAR_INIT) igText("[I]");
			if (var->flags & CVAR_LATCH) igText("[L]");
			
			var = var->next;
			count++;
		}
		
		igText("Total: %d CVars", count);
		igEndChild();
	}
	
	igSeparator();
	
	// Edit selected CVar
	if (cvar_selected) {
		igText("Edit: %s", cvar_selected->name);
		if (cvar_selected->description) {
			igTextWrapped("%s", cvar_selected->description);
		}
		
		igInputText("##value", cvar_edit_value, sizeof(cvar_edit_value), 0, NULL, NULL);
		
		igSameLine(0, 10);
		if (igButton("Set", (ImVec2){0, 0})) {
			if (!(cvar_selected->flags & CVAR_ROM)) {
				Cvar_Set(cvar_selected->name, cvar_edit_value);
			}
		}
		
		igSameLine(0, 10);
		if (igButton("Reset", (ImVec2){0, 0})) {
			if (cvar_selected->resetString) {
				Cvar_Set(cvar_selected->name, cvar_selected->resetString);
				Q_strncpyz(cvar_edit_value, cvar_selected->resetString, sizeof(cvar_edit_value));
			}
		}
		
		igText("Value: %s", cvar_selected->string);
		// Show numeric interpretation if applicable
		{
			float float_val = Q_atof(cvar_selected->string);
			if (float_val != 0.0f || (cvar_selected->string[0] >= '0' && cvar_selected->string[0] <= '9')) {
				igText("Numeric: %.2f", float_val);
			}
		}
	} else {
		igText("Select a CVar to edit");
	}
	
	igEnd();
}

/*
================
CL_ImGui_Debug_ShowConsoleOverlay
================
*/
void CL_ImGui_Debug_ShowConsoleOverlay(void) {
	if (!cl_imgui_debug_console || !cl_imgui_debug_console->integer) {
		return;
	}
	
	bool open = true;
	if (!igBegin("Console", &open, 0)) {
		igEnd();
		if (!open) {
			Cvar_Set("cl_imgui_debug_console", "0");
		}
		return;
	}
	
	// Filter input
	igInputText("Filter", console_filter, sizeof(console_filter), 0, NULL, NULL);
	
	igSameLine(0, 10);
	igCheckbox("Auto-scroll", &console_auto_scroll);
	
	igSeparator();
	
	// Console output (simplified - would need to hook into console system)
	if (igBeginChild_Str("##consoleoutput", (ImVec2){0, -30}, false, 0)) {
		igText("Console output would appear here");
		igText("(Full integration requires console system hooks)");
		
		if (console_auto_scroll) {
			igSetScrollHereY(1.0f);
		}
		
		igEndChild();
	}
	
	igSeparator();
	
	// Command input
	static char command_input[256] = "";
	igInputText("##command", command_input, sizeof(command_input), 
		ImGuiInputTextFlags_EnterReturnsTrue, NULL, NULL);
	
	igSameLine(0, 10);
	if (igButton("Execute", (ImVec2){0, 0})) {
		if (command_input[0] != '\0') {
			Cbuf_AddText(command_input);
			Cbuf_AddText("\n");
			command_input[0] = '\0';
		}
	}
	
	igEnd();
}

/*
================
CL_ImGui_Debug_ShowEventSystemOverlay
================
*/
void CL_ImGui_Debug_ShowEventSystemOverlay(void) {
	if (!cl_imgui_debug_events || !cl_imgui_debug_events->integer) {
		return;
	}
	
	bool open = true;
	if (!igBegin("Event System", &open, 0)) {
		igEnd();
		if (!open) {
			Cvar_Set("cl_imgui_debug_events", "0");
		}
		return;
	}
	
	eventSystemStats_t stats;
	Event_GetStats(&stats);
	
	igText("Event System Statistics");
	igSeparator();
	
	igText("Events Published: %u", stats.eventsPublished);
	igText("Events Processed: %u", stats.eventsProcessed);
	igText("Registered Event Types: %u", stats.registeredEventTypes);
	
	igSeparator();
	igText("Subscriptions");
	igText("  Created: %u", stats.subscriptionsCreated);
	igText("  Destroyed: %u", stats.subscriptionsDestroyed);
	igText("  Active: %u", stats.activeSubscriptions);
	
	igSeparator();
	igText("Queue Status");
	igText("  Immediate Queue: %u / %u", stats.immediateQueueSize, 256);
	igText("  Deferred Queue: %u / %u", stats.deferredQueueSize, 256);
	
	// Show queue status with color coding
	if (stats.immediateQueueSize > 200) {
		igTextColored((ImVec4){1.0f, 0.0f, 0.0f, 1.0f}, "  WARNING: Immediate queue nearly full!");
	}
	if (stats.deferredQueueSize > 200) {
		igTextColored((ImVec4){1.0f, 0.0f, 0.0f, 1.0f}, "  WARNING: Deferred queue nearly full!");
	}
	
	igSeparator();
	if (igButton("Print Stats to Console", (ImVec2){-1, 0})) {
		Event_PrintStats();
	}
	
	igEnd();
}

/*
================
CL_ImGui_Debug_ShowProfilerOverlay
================
*/
void CL_ImGui_Debug_ShowProfilerOverlay(void) {
	if (!cl_imgui_debug_profiler || !cl_imgui_debug_profiler->integer) {
		return;
	}
	
	bool open = true;
	if (!igBegin("Profiler", &open, 0)) {
		igEnd();
		if (!open) {
			Cvar_Set("cl_imgui_debug_profiler", "0");
		}
		return;
	}
	
	extern performanceCounters_t perfCounters;
	
	igText("Performance Counters");
	igSeparator();
	
	igText("FPS: %.1f (avg: %.1f)", perfCounters.currentFPS, perfCounters.averageFPS);
	igText("Frame Time: %.2f ms (avg: %.2f ms)", perfCounters.currentFrameTime, perfCounters.averageFrameTime);
	igText("  Min: %.2f ms, Max: %.2f ms", perfCounters.minFrameTime, perfCounters.maxFrameTime);
	
	if (perfCounters.gpuTimingAvailable && perfCounters.gpuFrameTime > 0.0f) {
		igText("GPU Frame Time: %.2f ms", perfCounters.gpuFrameTime);
		float ratio = (perfCounters.currentFrameTime > 0.0f) ? 
			(perfCounters.currentFrameTime / perfCounters.gpuFrameTime) : 0.0f;
		igText("CPU/GPU Ratio: %.2f", ratio);
	} else {
		igTextColored((ImVec4){0.7f, 0.7f, 0.7f, 1.0f}, "GPU Timing: Not available");
	}
	
	igSeparator();
	igText("Draw Calls");
	igText("  Current Frame: %d", perfCounters.drawCallsThisFrame);
	igText("  Average: %.1f", perfCounters.averageDrawCallsPerFrame);
	igText("  Min: %d, Max: %d", perfCounters.minDrawCallsPerFrame, perfCounters.maxDrawCallsPerFrame);
	igText("  Total: %d", perfCounters.totalDrawCalls);
	
	igSeparator();
	igText("Frame Time History");
	igText("  Samples: %d / 60", perfCounters.frameTimeHistoryCount);
	
	// Frame time graph
	if (perfCounters.frameTimeHistoryCount > 0) {
		ImVec2 graph_size = {-1, 100};
		igPlotLines_FloatPtr("##frametime", perfCounters.frameTimeHistory, 
			perfCounters.frameTimeHistoryCount, perfCounters.frameTimeHistoryIndex,
			NULL, 0.0f, 50.0f, graph_size, sizeof(float));
	}
	
	igSeparator();
	if (igButton("Print Stats to Console", (ImVec2){-1, 0})) {
		Perf_DisplayInfo_f();
	}
	
	igEnd();
}

/*
================
CL_ImGui_Debug_ShowMainMenu
================
*/
void CL_ImGui_Debug_ShowMainMenu(void) {
	if (!cl_imgui_debug_mainmenu || !cl_imgui_debug_mainmenu->integer) {
		return;
	}
	
	if (igBeginMainMenuBar()) {
		if (igBeginMenu("Debug", true)) {
			if (igMenuItem_Bool("Performance", NULL, cl_imgui_debug_performance && cl_imgui_debug_performance->integer, true)) {
				Cvar_Set("cl_imgui_debug_performance", cl_imgui_debug_performance->integer ? "0" : "1");
			}
			if (igMenuItem_Bool("Memory", NULL, cl_imgui_debug_memory && cl_imgui_debug_memory->integer, true)) {
				Cvar_Set("cl_imgui_debug_memory", cl_imgui_debug_memory->integer ? "0" : "1");
			}
			if (igMenuItem_Bool("Network", NULL, cl_imgui_debug_network && cl_imgui_debug_network->integer, true)) {
				Cvar_Set("cl_imgui_debug_network", cl_imgui_debug_network->integer ? "0" : "1");
			}
			if (igMenuItem_Bool("Renderer", NULL, cl_imgui_debug_renderer && cl_imgui_debug_renderer->integer, true)) {
				Cvar_Set("cl_imgui_debug_renderer", cl_imgui_debug_renderer->integer ? "0" : "1");
			}
			if (igMenuItem_Bool("CVar Browser", NULL, cl_imgui_debug_cvars && cl_imgui_debug_cvars->integer, true)) {
				Cvar_Set("cl_imgui_debug_cvars", cl_imgui_debug_cvars->integer ? "0" : "1");
			}
			if (igMenuItem_Bool("Console", NULL, cl_imgui_debug_console && cl_imgui_debug_console->integer, true)) {
				Cvar_Set("cl_imgui_debug_console", cl_imgui_debug_console->integer ? "0" : "1");
			}
			if (igMenuItem_Bool("Event System", NULL, cl_imgui_debug_events && cl_imgui_debug_events->integer, true)) {
				Cvar_Set("cl_imgui_debug_events", cl_imgui_debug_events->integer ? "0" : "1");
			}
			if (igMenuItem_Bool("Profiler", NULL, cl_imgui_debug_profiler && cl_imgui_debug_profiler->integer, true)) {
				Cvar_Set("cl_imgui_debug_profiler", cl_imgui_debug_profiler->integer ? "0" : "1");
			}
			igEndMenu();
		}
		
		if (igBeginMenu("Help", true)) {
			if (igMenuItem_Bool("About", NULL, false, true)) {
				// Could show about dialog
			}
			igEndMenu();
		}
		
		igEndMainMenuBar();
	}
}

/*
================
CL_ImGui_Debug_RenderAll
================
*/
void CL_ImGui_Debug_RenderAll(void) {
	CL_ImGui_Debug_ShowMainMenu();
	CL_ImGui_Debug_ShowPerformanceOverlay();
	CL_ImGui_Debug_ShowMemoryOverlay();
	CL_ImGui_Debug_ShowNetworkOverlay();
	CL_ImGui_Debug_ShowRendererOverlay();
	CL_ImGui_Debug_ShowCVarBrowser();
	CL_ImGui_Debug_ShowConsoleOverlay();
	CL_ImGui_Debug_ShowEventSystemOverlay();
	CL_ImGui_Debug_ShowProfilerOverlay();
}

/*
================
CL_ImGui_Debug_Init
================
*/
void CL_ImGui_Debug_Init(void) {
	CL_ImGui_Debug_RegisterCvars();
	Com_Memset(frame_times, 0, sizeof(frame_times));
	Com_Memset(fps_history, 0, sizeof(fps_history));
}

/*
================
CL_ImGui_Debug_Shutdown
================
*/
void CL_ImGui_Debug_Shutdown(void) {
	// Cleanup if needed
}

#else // USE_CIMGUI

void CL_ImGui_Debug_Init(void) {}
void CL_ImGui_Debug_Shutdown(void) {}
void CL_ImGui_Debug_RenderAll(void) {}

#endif // USE_CIMGUI

