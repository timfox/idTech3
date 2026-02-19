This directory holds the TrueType fonts used for FreeType font generation.

Source Sans 3 (regular + bold) is downloaded automatically when you configure the project with FreeType enabled. The renderer falls back to those fonts when rendering UI text.

If your environment lacks network access or you prefer different fonts, place replacement `.ttf` files here and rerun the renderer. The font loader will scan this folder before checking system paths.
This folder hosts the TrueType fonts used by the renderer when `BUILD_FREETYPE` is enabled.

Two Source Sans 3 fonts (`SourceSans3-Regular.ttf` and `SourceSans3-Bold.ttf`) are downloaded automatically by CMake when they are missing, so you usually do not need to add files manually.

If you prefer other fonts, place them here and update your UI scripts accordingly. The renderer will pick the first available font in `fonts/` before falling back to common system fonts (DejaVu, Liberation, Arial, etc.).
