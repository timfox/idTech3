<?php
require_once __DIR__ . '/../helpers/ViewRenderer.php';

use App\Helpers\ViewRenderer;

header('Content-Type: application/json');

// Basic error logging for debugging
error_reporting(E_ALL);
ini_set('log_errors', 1);
ini_set('error_log', __DIR__ . '/../../error.log');

// Get the requested page path
$page = isset($_GET['page']) ? $_GET['page'] : '';
$page = ltrim($page, '/');

// Security: Prevent directory traversal
$page = str_replace(['..', '//'], '', $page);

// Remove file extension if present
$cleanPage = preg_replace('/\.(html|php)$/', '', $page);

/**
 * Render a view without layout for AJAX requests
 */
function renderViewContentOnly(string $view): string {
    $viewsPath = __DIR__ . '/../../views/';
    $viewFile = $viewsPath . $view . '.php';
    
    if (!file_exists($viewFile)) {
        throw new \Exception("View file not found: {$view} at path: {$viewFile}");
    }
    
    // Start output buffering
    ob_start();
    
    try {
        // Include the view file (without layout)
        include $viewFile;
    } catch (Exception $e) {
        ob_end_clean();
        throw new \Exception("Error rendering view {$view}: " . $e->getMessage());
    }
    
    // Get and return the view content
    return ob_get_clean();
}

try {
    // Log basic request info
    error_log("Request: page={$page}, cleanPage={$cleanPage}");
    
    // Build the correct path for PHP view template (handle subdirectories properly)
    $phpViewPath = __DIR__ . '/../../views/' . $cleanPage . '.php';
    
    if (file_exists($phpViewPath)) {
        // Render PHP view template content only (without layout)
        $content = renderViewContentOnly($cleanPage);
        
        echo json_encode([
            'success' => true,
            'content' => $content,
            'page' => $cleanPage,
            'type' => 'php_template'
        ]);
    } else {
        // Try to find standalone HTML file (handle subdirectories properly)
        $htmlFilePath = __DIR__ . '/../../views/' . $cleanPage . '.html';
        
        if (file_exists($htmlFilePath)) {
            // Read standalone HTML file
            $htmlContent = file_get_contents($htmlFilePath);
            
            // Extract title from HTML if present
            $title = 'id Tech 3 Documentation';
            if (preg_match('/<title>(.*?)<\/title>/i', $htmlContent, $matches)) {
                $title = $matches[1];
            }
            
            // Extract body content (remove html, head, body tags for embedding)
            if (preg_match('/<body[^>]*>(.*?)<\/body>/is', $htmlContent, $matches)) {
                $bodyContent = $matches[1];
            } else {
                $bodyContent = $htmlContent;
            }
            
            echo json_encode([
                'success' => true,
                'content' => $bodyContent,
                'title' => $title,
                'page' => $cleanPage,
                'type' => 'html_file'
            ]);
        } else {
            // Neither PHP nor HTML file found
            throw new \Exception("View file not found: {$cleanPage} (checked: {$phpViewPath} and {$htmlFilePath})");
        }
    }
    
} catch (Exception $e) {
    error_log("Content handler error: " . $e->getMessage());
    http_response_code(500);
    echo json_encode([
        'error' => 'Failed to load content',
        'message' => $e->getMessage(),
        'details' => [
            'requested_page' => $page,
            'clean_page' => $cleanPage,
            'php_path' => isset($phpViewPath) ? $phpViewPath : 'not checked',
            'html_path' => isset($htmlFilePath) ? $htmlFilePath : 'not checked'
        ]
    ]);
} 