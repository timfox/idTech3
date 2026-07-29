# Vulkan Diagnostics

`diagnostics/` contains renderer init/status helpers that are included by
`tr_init.c` to preserve existing static linkage and console-command ownership.

| File | Role |
|------|------|
| `tr_init_capture.inc` | capture and screenshot-oriented init helpers |
| `tr_init_info.inc` | Vulkan/device info console output |
| `tr_init_diagnostics.inc` | renderer profile, status, compatibility warnings, recovery commands |

Keep these files focused on diagnostics and console control. Renderer pass
implementation should stay in the owning root module or extension folder.

