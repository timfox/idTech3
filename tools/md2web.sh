#!/bin/bash

# md2web.sh: Convert Markdown documentation to web formats (HTML + PHP)
# Converts .md files from docs/ to both HTML and PHP files in web/views/
#
# Organization:
#   docs/*.md          - Source markdown files
#   web/views/*.html   - Standalone HTML documentation (offline access)
#   web/views/*.php    - PHP web views (online app integration)
#
# Usage: ./md2web.sh [filename.md] [--all] [--offline-only] [--online-only]

set -e

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="$(dirname "$SCRIPT_DIR")"
DOCS_DIR="$PROJECT_ROOT/docs"
WEB_DIR="$PROJECT_ROOT/web"
VIEWS_DIR="$WEB_DIR/views"

# Colors for output
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
NC='\033[0m' # No Color

# Create output directories
mkdir -p "$VIEWS_DIR"

# Check if PHP is available
if ! command -v php &> /dev/null; then
    echo -e "${RED}Error: PHP is required but not installed.${NC}"
    exit 1
fi

# Function to convert markdown to HTML
convert_md_to_html() {
    local md_file="$1"
    local html_file="$2"
    local title="$3"

    echo -e "${BLUE}Converting ${md_file} to ${html_file}${NC}"

    # Use PHP to convert markdown to HTML
    cat > /tmp/md_converter.php << 'EOF'
<?php
// Get arguments
$md_file = $argv[1];
$html_file = $argv[2];
$title = $argv[3] ?? 'Documentation';

// Read markdown
$md_content = file_get_contents($md_file);

// Basic markdown to HTML conversion
function markdown_to_html($md) {
    // Use league/commonmark if available
    if (class_exists('League\CommonMark\CommonMarkConverter')) {
        $converter = new League\CommonMark\CommonMarkConverter();
        return $converter->convert($md);
    }
    // Use parsedown if available
    if (class_exists('Parsedown')) {
        $pd = new Parsedown();
        return $pd->text($md);
    }
    // Fallback: basic conversion
    $html = htmlspecialchars($md, ENT_QUOTES, 'UTF-8');

    // Headings
    $html = preg_replace('/^###### (.*?)$/m', '<h6>$1</h6>', $html);
    $html = preg_replace('/^##### (.*?)$/m', '<h5>$1</h5>', $html);
    $html = preg_replace('/^#### (.*?)$/m', '<h4>$1</h4>', $html);
    $html = preg_replace('/^### (.*?)$/m', '<h3>$1</h3>', $html);
    $html = preg_replace('/^## (.*?)$/m', '<h2>$1</h2>', $html);
    $html = preg_replace('/^# (.*?)$/m', '<h1>$1</h1>', $html);

    // Bold/italic
    $html = preg_replace('/\*\*(.*?)\*\*/', '<strong>$1</strong>', $html);
    $html = preg_replace('/\*(.*?)\*/', '<em>$1</em>', $html);

    // Code blocks and inline code
    $html = preg_replace('/```([a-z]*)\n(.*?)\n```/s', '<pre><code class="language-$1">$2</code></pre>', $html);
    $html = preg_replace('/`([^`]+)`/', '<code>$1</code>', $html);

    // Lists
    $html = preg_replace('/^\s*-\s+(.*)$/m', '<li>$1</li>', $html);
    $html = preg_replace('/^\s*\d+\.\s+(.*)$/m', '<li>$1</li>', $html);

    // Paragraphs
    $html = preg_replace('/\n{2,}/', "</p><p>", $html);
    $html = '<p>' . $html . '</p>';

    // Wrap lists
    $html = preg_replace_callback('#((<li>.*?</li>\s*)+)#s', function($m) {
        return '<ul>' . $m[1] . '</ul>';
    }, $html);

    // Links
    $html = preg_replace('/\[(.+?)\]\((.+?)\)/', '<a href="$2">$1</a>', $html);

    return $html;
}

// Generate standalone HTML
function generate_html($title, $content) {
    return <<<HTML
<!DOCTYPE html>
<html lang="en">
<head>
    <meta charset="UTF-8">
    <meta name="viewport" content="width=device-width, initial-scale=1.0">
    <title>{$title} - idTech3 Documentation</title>
    <meta name="description" content="idTech3 engine documentation: {$title}">
    <meta name="generator" content="md2web.sh">
    <style>
        body {
            font-family: -apple-system, BlinkMacSystemFont, 'Segoe UI', Roboto, sans-serif;
            line-height: 1.6;
            color: #333;
            max-width: 800px;
            margin: 0 auto;
            padding: 20px;
            background: #fafbfc;
        }
        .header {
            text-align: center;
            border-bottom: 1px solid #e1e4e8;
            padding-bottom: 20px;
            margin-bottom: 30px;
        }
        .header h1 {
            color: #2c3e50;
            margin: 0;
            font-size: 2.5em;
        }
        .header p {
            color: #666;
            margin: 10px 0 0 0;
        }
        .content {
            background: white;
            padding: 30px;
            border-radius: 8px;
            box-shadow: 0 2px 10px rgba(0,0,0,0.1);
        }
        h1, h2, h3, h4, h5, h6 {
            color: #2c3e50;
            margin-top: 1.5em;
            margin-bottom: 0.5em;
        }
        h1 { border-bottom: 2px solid #3498db; padding-bottom: 10px; }
        h2 { border-bottom: 1px solid #bdc3c7; padding-bottom: 5px; }
        code {
            background: #f8f9fa;
            padding: 2px 6px;
            border-radius: 3px;
            font-family: 'SF Mono', Monaco, 'Cascadia Code', monospace;
            font-size: 0.9em;
        }
        pre {
            background: #f8f9fa;
            padding: 15px;
            border-radius: 5px;
            overflow-x: auto;
            border-left: 4px solid #3498db;
        }
        pre code {
            background: none;
            padding: 0;
        }
        blockquote {
            border-left: 4px solid #3498db;
            padding-left: 15px;
            margin-left: 0;
            color: #666;
            font-style: italic;
        }
        table {
            border-collapse: collapse;
            width: 100%;
            margin: 20px 0;
        }
        th, td {
            border: 1px solid #ddd;
            padding: 8px 12px;
            text-align: left;
        }
        th {
            background: #f8f9fa;
            font-weight: 600;
        }
        tr:nth-child(even) {
            background: #f8f9fa;
        }
        ul, ol {
            margin-left: 20px;
        }
        a {
            color: #3498db;
            text-decoration: none;
        }
        a:hover {
            text-decoration: underline;
        }
        .footer {
            text-align: center;
            margin-top: 40px;
            padding-top: 20px;
            border-top: 1px solid #e1e4e8;
            color: #666;
            font-size: 0.9em;
        }
        .back-link {
            display: inline-block;
            margin-bottom: 20px;
            color: #666;
            text-decoration: none;
        }
        .back-link:hover {
            color: #333;
        }
    </style>
</head>
<body>
    <div class="header">
        <h1>{$title}</h1>
        <p>idTech3 Engine Documentation</p>
        <a href="../index.html" class="back-link">← Back to Documentation Index</a>
    </div>
    <div class="content">
{$content}
    </div>
    <div class="footer">
        <p>Generated from Markdown source using md2web.sh</p>
        <p><a href="https://github.com/idtech3">idTech3 Project</a></p>
    </div>
</body>
</html>
HTML;
}

// Convert and save
$content = markdown_to_html($md_content);
$html = generate_html($title, $content);
file_put_contents($html_file, $html);

echo "Converted: {$md_file} -> {$html_file}\n";
EOF

    php /tmp/md_converter.php "$md_file" "$html_file" "$title"
}

# Function to convert markdown to PHP web view
convert_md_to_php() {
    local md_file="$1"
    local php_file="$2"
    local title="$3"

    echo -e "${BLUE}Converting ${md_file} to PHP web view${NC}"

    # The md2html.php script now generates both formats, so just ensure it exists
    if [ ! -f "$php_file" ]; then
        cd "$WEB_DIR/utils"
        php md2html.php > /dev/null 2>&1
    fi
}

# Function to process a single file
process_file() {
    local md_file="$1"
    local base_name="$(basename "$md_file" .md)"

    # Convert to lowercase with hyphens for web URLs
    local web_name="$(echo "$base_name" | tr '[:upper:]' '[:lower:]' | sed 's/_/-/g')"

    # Extract title from first heading
    local title="Documentation"
    if head -10 "$md_file" | grep -q "^# "; then
        title="$(head -10 "$md_file" | grep "^# " | head -1 | sed 's/^# //' | sed 's/^[[:space:]]*//')"
    fi

    echo -e "${GREEN}Processing: ${base_name}.md${NC}"
    echo "  Title: $title"
    echo "  Web URL: $web_name"

    # For single files, we regenerate all files since md2html.php processes everything
    echo "  Note: Regenerating all documentation files..."
    cd "$WEB_DIR/utils"
    php md2html.php > /dev/null 2>&1

    echo ""
}

# Function to process all files
process_all() {
    echo -e "${YELLOW}Processing all markdown files in $DOCS_DIR${NC}"

    # Use the md2html.php script directly since it handles all files
    cd "$WEB_DIR/utils"
    if php md2html.php; then
        echo -e "${GREEN}Successfully processed all documentation files${NC}"
    else
        echo -e "${RED}Error processing documentation files${NC}"
        exit 1
    fi
}

# Function to show usage
show_usage() {
    cat << EOF
md2web.sh - Convert Markdown documentation to web formats

USAGE:
    $0 [filename.md]           # Process specific file
    $0 --all                   # Process all .md files in docs/
    $0 --offline-only          # Generate only offline HTML files
    $0 --online-only           # Generate only online PHP views

OUTPUT (all in web/views/):
    HTML: [filename].html        # Standalone/offline documentation
    PHP:  [filename].php         # Web application integration

EXAMPLES:
    $0 configurable-naming.md    # Process one file
    $0 --all                     # Process all .md files in docs/
    $0 --offline-only            # Only HTML files

DEPENDENCIES:
    - PHP (with optional league/commonmark or parsedown)
    - Bash shell

EOF
}

# Main logic
case "${1:-}" in
    "")
        show_usage
        ;;
    "--help"|"-h")
        show_usage
        ;;
    "--all")
        process_all
        ;;
    "--offline-only")
        echo "Offline-only mode not implemented yet"
        ;;
    "--online-only")
        echo "Online-only mode not implemented yet"
        ;;
    *.md)
        if [ -f "$DOCS_DIR/$1" ]; then
            process_file "$DOCS_DIR/$1"
        else
            echo -e "${RED}Error: File $DOCS_DIR/$1 not found${NC}"
            exit 1
        fi
        ;;
    *)
        echo -e "${RED}Error: Unknown option '$1'${NC}"
        echo ""
        show_usage
        exit 1
        ;;
esac

echo -e "${GREEN}Conversion complete!${NC}"
echo ""
echo "All documentation files generated in: $VIEWS_DIR/"
echo "  - Standalone HTML: [filename].html (full documents)"
echo "  - Web PHP content: [filename].php (content-only, no HTML wrapper)"
echo ""
echo "Test standalone docs: open $VIEWS_DIR/configurable-naming.html"
echo "Test web app docs:   visit /app/core/content-handler.php?page=configurable-naming"
