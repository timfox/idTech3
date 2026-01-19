# Development Environment Setup

## Cursor/VS Code Project Commands

This project includes pre-configured tasks for common development workflows. To enable them:

### Option 1: Copy Configuration Files
```bash
# Copy the example configuration to your workspace
cp -r .vscode.example .vscode
```

### Option 2: Manual Setup
Create `.vscode/tasks.json` and `.vscode/settings.json` in your workspace with the following content:

### tasks.json
```json
{
    "version": "2.0.0",
    "tasks": [
        {
            "label": "build_release_vulkan",
            "type": "shell",
            "command": "./scripts/compile_engine.sh",
            "args": ["vulkan"],
            "group": "build"
        },
        {
            "label": "build_debug_opengl",
            "type": "shell",
            "command": "./scripts/compile_engine.sh",
            "args": ["opengl", "debug"],
            "group": "build"
        },
        {
            "label": "run_minimal_smoke_test",
            "type": "shell",
            "command": "./scripts/smoke_test.sh"
        },
        {
            "label": "format_all_changed_files",
            "type": "shell",
            "command": "bash",
            "args": ["-c", "git diff --name-only HEAD~1 -- '*.c' '*.cpp' '*.h' '*.hpp' | xargs -r clang-format -i"]
        }
        // ... more tasks available
    ]
}
```

### Keyboard Shortcuts (keybindings.json)
```json
[
    {
        "key": "ctrl+shift+b",
        "command": "workbench.action.tasks.runTask",
        "args": "build_release_vulkan"
    },
    {
        "key": "ctrl+shift+d",
        "command": "workbench.action.tasks.runTask",
        "args": "build_debug_opengl"
    },
    {
        "key": "ctrl+shift+t",
        "command": "workbench.action.tasks.runTask",
        "args": "run_minimal_smoke_test"
    },
    {
        "key": "ctrl+shift+f",
        "command": "workbench.action.tasks.runTask",
        "args": "format_all_changed_files"
    }
]
```

## Available Project Commands

### Build Commands
- `build_release_vulkan` - Build Vulkan renderer (Release)
- `build_debug_opengl` - Build OpenGL renderer (Debug)
- `build_release_full` - Build both renderers

### Code Quality
- `format_changed_files` - Show changed C/C++ files
- `format_all_changed_files` - Auto-format changed files
- `run_clang_tidy_changed` - Run clang-tidy on changed files
- `run_cppcheck` - Static analysis with cppcheck

### Testing & Validation
- `run_minimal_smoke_test` - 10-second basic functionality test
- `run_full_test_suite` - Comprehensive test suite
- `run_safe_mode_test` - Test with experimental features disabled
- `validate_ci_build` - Test all CI configurations locally
- `run_memory_safety_tests` - Valgrind memory checks
- `run_performance_tests` - Performance benchmarks

### Assets & Packaging
- `cook_assets` - Process assets for distribution
- `validate_assets` - Check asset integrity
- `package_release` - Create release packages

### Utilities
- `clean_build_artifacts` - Remove build files and logs
- `generate_documentation` - List available docs

## Usage

### Command Palette
1. `Ctrl+Shift+P` (or `Cmd+Shift+P` on Mac)
2. Type "Tasks: Run Task"
3. Select desired command

### Keyboard Shortcuts
- `Ctrl+Shift+B` - Build Vulkan (Release)
- `Ctrl+Shift+D` - Build OpenGL (Debug)
- `Ctrl+Shift+T` - Run smoke test
- `Ctrl+Shift+F` - Format changed files
- `Ctrl+Shift+C` - Run clang-tidy on changed files
- `Ctrl+Shift+V` - Validate CI build

## Development Workflow

### Daily Development
1. **Pull latest changes**: `git pull`
2. **Format code**: `Ctrl+Shift+F`
3. **Build and test**: `Ctrl+Shift+B` then `Ctrl+Shift+T`
4. **Run quality checks**: `Ctrl+Shift+C`

### Before Commit
1. **Format changed files**: `Ctrl+Shift+F`
2. **Run clang-tidy**: `Ctrl+Shift+C`
3. **Run smoke test**: `Ctrl+Shift+T`
4. **Validate build**: `Ctrl+Shift+V`

### Release Preparation
1. **Full test suite**: Run "run_full_test_suite"
2. **Performance tests**: Run "run_performance_tests"
3. **Package release**: Run "package_release"

## Troubleshooting

### Tasks Not Appearing
- Ensure `.vscode/tasks.json` exists in your workspace
- Reload VS Code window (`Ctrl+Shift+P` → "Developer: Reload Window")

### Scripts Not Found
- Ensure you're in the project root directory
- Check script permissions: `chmod +x scripts/*.sh`

### Build Failures
- Check available disk space
- Verify dependencies: `sudo apt-get install cmake clang-format clang-tidy cppcheck`
- Clean build artifacts: Run "clean_build_artifacts" task