Startup prerequisites and known issues
-------------------------------------
- Vulkan is preferred for rendering when available. If Vulkan initialization fails, the launcher will attempt a GL path fallback if available.
- Console history: On first startup, a history file is created in the user's home dir (the exact path is determined by the FS home path). If it cannot be created, history loading will gracefully fall back to an empty history.
- GPU sanity: On startup, Vulkan will check for suitable GPUs. If none are found (or if only CPU fallback paths exist), Vulkan is disabled and rendering falls back to the non-Vulkan path if available.
- Logging: Verbose Vulkan diagnostics can be silenced by setting the environment variable `IDTECH3_VULKAN_SILENT=1`.
- The renderer can be forced via `+set cl_renderer <renderer>` (e.g., `opengl`, `vulkan`); a subsequent vid_restart is required.
- On first run, ensure your environment has a writable home directory for history/config files.

