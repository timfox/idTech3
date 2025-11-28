<?php
namespace App\Helpers;

class ViewRenderer {
    private static $layout = 'layouts/main';
    private static $viewsPath = __DIR__ . '/../../views/';

    public static function render(string $view, array $data = []): string {
        // Extract data to make variables available in view
        extract($data);

        // Start output buffering
        ob_start();

        // Include the view file
        $viewFile = self::$viewsPath . $view . '.php';
        if (!file_exists($viewFile)) {
            throw new \Exception("View file not found: {$view}");
        }
        include $viewFile;

        // Get the view content
        $content = ob_get_clean();

        // Start output buffering for layout
        ob_start();

        // Include the layout file
        $layoutFile = self::$viewsPath . self::$layout . '.php';
        if (!file_exists($layoutFile)) {
            throw new \Exception("Layout file not found: " . self::$layout);
        }
        include $layoutFile;

        // Return the complete rendered page
        return ob_get_clean();
    }
} 