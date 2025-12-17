<?php
/**
 * Script to convert PHP views to HTML, then to Markdown.
 * - Reads all .php files in /web/views/
 * - Renders them (with minimal context)
 * - Converts produced HTML to Markdown
 * - Writes .md files to /docs/
 */

// Get all .php files in views directory
$viewsDir = __DIR__ . '/../views/';
$docsDir = __DIR__ . '/../../docs/';

// Ensure output directory exists
if (!is_dir($docsDir)) {
    mkdir($docsDir, 0755, true);
}

$phpFiles = glob($viewsDir . '*.php');

// Helper: convert HTML to Markdown (uses a basic method if no library)
function html_to_markdown($html) {
    // Uses league/html-to-markdown if available
    if (class_exists('League\HTMLToMarkdown\HtmlConverter')) {
        $converter = new League\HTMLToMarkdown\HtmlConverter();
        return $converter->convert($html);
    }

    // Fallback: naive replacements (for basic HTML)
    $markdown = $html;
    $markdown = preg_replace('/<h([1-6])>(.*?)<\/h\1>/i', "\n\n" . str_repeat('#', '$1') . ' $2' . "\n\n", $markdown);
    $markdown = preg_replace('/<p>(.*?)<\/p>/i', "\n\n$1\n\n", $markdown);
    $markdown = preg_replace('/<br\s*\/?>/i', "  \n", $markdown);
    $markdown = preg_replace('/<ul>(.*?)<\/ul>/is', "\n$1\n", $markdown);
    $markdown = preg_replace('/<li>(.*?)<\/li>/i', "- $1\n", $markdown);
    // Remove all other tags
    $markdown = strip_tags($markdown);
    // Cleanup excessive newlines
    $markdown = preg_replace("/\n{3,}/", "\n\n", $markdown);
    return trim($markdown);
}

// Helper: Render PHP file to string
function render_php_view($file) {
    ob_start();
    // $data = []; // Optionally pass data to views if needed in future
    include $file;
    return ob_get_clean();
}

foreach ($phpFiles as $phpFile) {
    $basename = basename($phpFile, '.php');
    echo "Processing: $basename.php\n";

    // Render HTML
    $html = render_php_view($phpFile);

    // Convert to Markdown
    $md = html_to_markdown($html);

    // Write output
    $mdPath = $docsDir . $basename . '.md';
    file_put_contents($mdPath, $md);
    echo " -> Wrote $mdPath\n";
}

echo "Done.\n";
