<!DOCTYPE html>
<html lang="en">
<head>
    <meta charset="UTF-8">
    <title>Custom Cursor in Quake3e</title>
    <style>
        body {
            font-family: Arial, sans-serif;
            max-width: 800px;
            margin: 0 auto;
            padding: 20px;
            line-height: 1.6;
        }
        code {
            background: #f4f4f4;
            padding: 2px 5px;
            border-radius: 3px;
        }
        pre {
            background: #f4f4f4;
            padding: 15px;
            border-radius: 5px;
            overflow-x: auto;
        }
    </style>
</head>
<body>
    <h1>Custom Cursor in Quake3e</h1>
    
    <h2>Overview</h2>
    <p>Quake3e allows you to customize the cursor appearance by modifying the UI code. The cursor functionality is primarily handled in ui_atoms.c.</p>

    <h2>Implementation Steps</h2>
    
    <h3>1. Load Custom Cursor Image</h3>
    <p>First, you need to register and load your custom cursor image. Add this to your UI initialization code:</p>
    <pre><code>// In UI_Init() function
uis.cursor = trap_R_RegisterShaderNoMip("path/to/your/cursor.tga");</code></pre>

    <h3>2. Draw Custom Cursor</h3>
    <p>To draw the custom cursor, you'll need to modify the UI drawing code. Here's a basic implementation:</p>
    <pre><code>void UI_DrawCustomCursor(void) {
    int x, y;
    float scale;
    
    // Get cursor position
    trap_GetCursorPos(&x, &y);
    
    // Apply UI scaling
    scale = uis.scale;
    x = x * scale + uis.bias;
    y = y * scale;
    
    // Draw cursor
    trap_R_SetColor(NULL);
    trap_R_DrawStretchPic(x, y, 32, 32, 0, 0, 1, 1, uis.cursor);
}</code></pre>

    <h3>3. Integration</h3>
    <p>Add the cursor drawing call to your main UI rendering function:</p>
    <pre><code>void UI_DrawActiveMenu(void) {
    // ... existing menu drawing code ...
    
    // Draw custom cursor last so it appears on top
    UI_DrawCustomCursor();
}</code></pre>

    <h2>Important Notes</h2>
    <ul>
        <li>Cursor image should be a TGA file with transparency</li>
        <li>Recommended size is 32x32 pixels</li>
        <li>Make sure to handle cursor visibility based on menu state</li>
        <li>Consider adding cursor animation if desired</li>
    </ul>

    <h2>Example Cursor Image Format</h2>
    <pre><code>// Example cursor.tga header
typedef struct {
    unsigned char identsize;          // Size of ID field that follows header (0)
    unsigned char colourmaptype;      // 0 = None, 1 = paletted
    unsigned char imagetype;          // 0 = none, 1 = indexed, 2 = rgb, 3 = grey, +8=rle
    unsigned short colourmapstart;    // First colour map entry
    unsigned short colourmaplength;   // Number of colours
    unsigned char colourmapbits;      // Bits per palette entry
    unsigned short xstart;            // Image x origin
    unsigned short ystart;            // Image y origin
    unsigned short width;             // Image width in pixels
    unsigned short height;            // Image height in pixels
    unsigned char bits;               // Bits per pixel (8/16/24/32)
    unsigned char descriptor;         // Image descriptor
} TGA_HEADER;</code></pre>

    <h2>Additional Tips</h2>
    <ul>
        <li>Use <code>trap_Key_SetCatcher()</code> to properly handle cursor input</li>
        <li>Consider adding cursor states (normal, hover, click)</li>
        <li>Test cursor behavior across different resolutions</li>
        <li>Remember to handle cursor visibility during loading screens</li>
    </ul>
</body>
</html>
