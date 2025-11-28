<?php
/**
 * Main layout template for documentation pages
 */
?>
<!DOCTYPE html>
<html lang="en">
<head>
    <meta charset="UTF-8">
    <meta name="viewport" content="width=device-width, initial-scale=1.0, maximum-scale=1.0, user-scalable=no">
    <meta name="description" content="<?= $description ?? 'id Tech 3 Engine Documentation' ?>">
    <meta name="theme-color" content="#223e5b">
    <meta http-equiv="X-UA-Compatible" content="IE=edge">
    <title><?= $title ?? 'id Tech 3 Documentation' ?></title>
    <style>
        @font-face {
            font-family: 'FX300';
            src: url('/public/fonts/FX300.ttf') format('truetype');
            font-display: swap;
        }
        
        @font-face {
            font-family: '2197 Block';
            src: url('/public/fonts/2197 Block.ttf') format('truetype');
            font-display: swap;
        }
        
        /* CSS Reset and Base Styles */
        *, *::before, *::after {
            box-sizing: border-box;
        }
        
        html {
            scroll-behavior: smooth;
            height: 100%;
        }
        
        body {
            height: 100%;
            margin: 0;
            padding: 0;
            background-image: url('/public/images/background2.png');
            background-position: center;
            background-size: cover;
            background-attachment: fixed;
            font-family: 'Helvetica Neue', Helvetica, Arial, sans-serif;
            font-size: 16px;
            line-height: 1.7;
            color: #ffffff;
            overflow-x: hidden;
            min-height: 100vh;
        }
        
        /* Page Structure */
        .page-container {
            width: 100%;
            max-width: 1400px;
            margin: 0 auto;
            padding: 20px;
            min-height: 100vh;
            display: flex;
            flex-direction: column;
        }
        
        .doc-content {
            background: rgba(34, 62, 91, 0.25);
            backdrop-filter: blur(12px);
            -webkit-backdrop-filter: blur(12px);
            border: 1px solid rgba(255, 255, 255, 0.3);
            border-radius: 20px;
            padding: 40px;
            margin-bottom: 30px;
            flex: 1;
            box-shadow: 0 8px 32px rgba(0, 0, 0, 0.3);
            transition: all 0.3s ease;
        }
        
        .doc-content:hover {
            box-shadow: 0 12px 40px rgba(0, 0, 0, 0.4);
            transform: translateY(-2px);
        }
        
        /* Navigation Breadcrumbs */
        .breadcrumbs {
            background: rgba(0, 0, 0, 0.3);
            padding: 12px 20px;
            border-radius: 25px;
            margin-bottom: 30px;
            border: 1px solid rgba(255, 255, 255, 0.2);
            font-size: 14px;
            backdrop-filter: blur(8px);
        }
        
        .breadcrumbs a {
            color: #3498db;
            text-decoration: none;
            font-weight: 500;
            transition: all 0.3s ease;
            padding: 4px 8px;
            border-radius: 8px;
        }
        
        .breadcrumbs a:hover {
            color: #2980b9;
            background: rgba(52, 152, 219, 0.2);
            text-decoration: none;
        }
        
        /* Typography */
        h1, h2, h3, h4, h5, h6 {
            font-family: 'FX300', 'Arial Black', sans-serif;
            font-weight: bold;
            margin: 0 0 20px 0;
            color: #ffffff;
            text-shadow: 0 2px 4px rgba(0, 0, 0, 0.3);
            line-height: 1.3;
        }
        
        h1 {
            font-size: 2.5em;
            color: #3498db;
            border-bottom: 3px solid #3498db;
            padding-bottom: 15px;
            margin-bottom: 30px;
            text-align: center;
            background: linear-gradient(135deg, #3498db, #2980b9);
            -webkit-background-clip: text;
            -webkit-text-fill-color: transparent;
            background-clip: text;
        }
        
        h2 {
            font-size: 2em;
            color: #e74c3c;
            margin-top: 40px;
            margin-bottom: 20px;
            border-left: 4px solid #e74c3c;
            padding-left: 20px;
        }
        
        h3 {
            font-size: 1.5em;
            color: #f39c12;
            margin-top: 30px;
            margin-bottom: 15px;
        }
        
        h4 {
            font-size: 1.3em;
            color: #9b59b6;
            margin-top: 25px;
        }
        
        h5, h6 {
            font-size: 1.1em;
            color: #1abc9c;
            margin-top: 20px;
        }
        
        /* Paragraphs and Text */
        p {
            margin: 0 0 16px 0;
            text-align: justify;
            color: rgba(255, 255, 255, 0.9);
        }
        
        strong, b {
            color: #f39c12;
            font-weight: 600;
        }
        
        em, i {
            color: #e67e22;
            font-style: italic;
        }
        
        /* Links */
        a {
            color: #3498db;
            text-decoration: none;
            transition: all 0.3s ease;
            border-bottom: 1px solid transparent;
        }
        
        a:hover {
            color: #5dade2;
            border-bottom-color: #3498db;
            text-decoration: none;
        }
        
        /* Lists */
        ul, ol {
            margin: 20px 0;
            padding-left: 30px;
        }
        
        li {
            margin-bottom: 12px;
            color: rgba(255, 255, 255, 0.9);
            line-height: 1.6;
        }
        
        ul li {
            list-style: none;
            position: relative;
        }
        
        ul li::before {
            content: "▶";
            color: #3498db;
            font-weight: bold;
            position: absolute;
            left: -20px;
            top: 0;
        }
        
        ol li {
            list-style: decimal;
            color: rgba(255, 255, 255, 0.9);
        }
        
        /* Code and Pre */
        code {
            background: rgba(0, 0, 0, 0.4);
            color: #00ff00;
            padding: 4px 8px;
            border-radius: 6px;
            font-family: 'Fira Code', 'Consolas', 'Monaco', monospace;
            font-size: 0.9em;
            border: 1px solid rgba(0, 255, 0, 0.3);
        }
        
        pre {
            background: rgba(0, 0, 0, 0.6);
            color: #00ff41;
            padding: 20px;
            border-radius: 12px;
            overflow-x: auto;
            margin: 20px 0;
            border: 1px solid rgba(0, 255, 0, 0.3);
            box-shadow: 0 4px 20px rgba(0, 0, 0, 0.5);
            font-family: 'Fira Code', 'Consolas', 'Monaco', monospace;
            font-size: 0.9em;
            line-height: 1.5;
            position: relative;
        }
        
        pre::before {
            content: "CODE";
            position: absolute;
            top: -12px;
            left: 20px;
            background: rgba(0, 255, 0, 0.8);
            color: #000;
            padding: 4px 12px;
            border-radius: 8px;
            font-size: 0.7em;
            font-weight: bold;
            letter-spacing: 1px;
        }
        
        pre code {
            background: none;
            border: none;
            padding: 0;
            color: inherit;
        }
        
        /* Keyword Highlighting */
        .keyword {
            background: linear-gradient(135deg, rgba(52, 152, 219, 0.4), rgba(41, 128, 185, 0.4));
            color: #ffffff;
            padding: 4px 10px;
            border-radius: 8px;
            font-family: 'Fira Code', monospace;
            font-weight: 600;
            border: 1px solid rgba(52, 152, 219, 0.6);
            box-shadow: 0 2px 8px rgba(52, 152, 219, 0.3);
            display: inline-block;
            margin: 2px;
        }
        
        /* Example Boxes */
        .example {
            background: rgba(0, 0, 0, 0.4);
            border: 2px solid #3498db;
            border-radius: 12px;
            padding: 20px;
            margin: 25px 0;
            position: relative;
            box-shadow: 0 4px 20px rgba(52, 152, 219, 0.2);
        }
        
        .example::before {
            content: "EXAMPLE";
            position: absolute;
            top: -12px;
            left: 20px;
            background: #3498db;
            color: white;
            padding: 4px 12px;
            border-radius: 8px;
            font-size: 0.8em;
            font-weight: bold;
            letter-spacing: 1px;
        }
        
        /* Content Sections */
        .content-section {
            animation: fadeIn 0.6s ease-out;
        }
        
        /* Tables */
        table {
            width: 100%;
            border-collapse: collapse;
            margin: 20px 0;
            background: rgba(0, 0, 0, 0.3);
            border-radius: 12px;
            overflow: hidden;
        }
        
        th, td {
            padding: 15px;
            text-align: left;
            border-bottom: 1px solid rgba(255, 255, 255, 0.2);
        }
        
        th {
            background: rgba(52, 152, 219, 0.3);
            color: white;
            font-weight: bold;
            text-transform: uppercase;
            letter-spacing: 1px;
        }
        
        tr:hover {
            background: rgba(255, 255, 255, 0.1);
        }
        
        /* Blockquotes */
        blockquote {
            border-left: 4px solid #f39c12;
            background: rgba(243, 156, 18, 0.1);
            padding: 20px;
            margin: 20px 0;
            border-radius: 0 12px 12px 0;
            font-style: italic;
            color: rgba(255, 255, 255, 0.9);
        }
        
        /* Footer */
        footer {
            margin-top: 50px;
            padding-top: 30px;
            border-top: 2px solid rgba(255, 255, 255, 0.2);
            text-align: center;
            color: rgba(255, 255, 255, 0.7);
            font-size: 0.9em;
        }
        
        /* Animations */
        @keyframes fadeIn {
            from {
                opacity: 0;
                transform: translateY(20px);
            }
            to {
                opacity: 1;
                transform: translateY(0);
            }
        }
        
        @keyframes slideIn {
            from {
                transform: translateX(-20px);
                opacity: 0;
            }
            to {
                transform: translateX(0);
                opacity: 1;
            }
        }
        
        /* Responsive Design */
        @media (max-width: 1200px) {
            .page-container {
                max-width: 100%;
                padding: 15px;
            }
            
            .doc-content {
                padding: 30px;
            }
        }
        
        @media (max-width: 768px) {
            .doc-content {
                padding: 20px;
                border-radius: 15px;
            }
            
            h1 {
                font-size: 2em;
            }
            
            h2 {
                font-size: 1.6em;
            }
            
            h3 {
                font-size: 1.3em;
            }
            
            pre {
                padding: 15px;
                font-size: 0.8em;
            }
            
            .breadcrumbs {
                padding: 10px 15px;
                font-size: 12px;
            }
        }
        
        @media (max-width: 480px) {
            .page-container {
                padding: 10px;
            }
            
            .doc-content {
                padding: 15px;
            }
            
            h1 {
                font-size: 1.8em;
            }
            
            h2 {
                font-size: 1.4em;
                border-left-width: 3px;
                padding-left: 15px;
            }
        }
        
        /* Scrollbar Styling */
        ::-webkit-scrollbar {
            width: 12px;
        }
        
        ::-webkit-scrollbar-track {
            background: rgba(0, 0, 0, 0.3);
            border-radius: 6px;
        }
        
        ::-webkit-scrollbar-thumb {
            background: linear-gradient(45deg, #3498db, #2980b9);
            border-radius: 6px;
        }
        
        ::-webkit-scrollbar-thumb:hover {
            background: linear-gradient(45deg, #5dade2, #3498db);
        }
        
        /* Selection */
        ::selection {
            background: rgba(52, 152, 219, 0.4);
            color: white;
        }
        
        ::-moz-selection {
            background: rgba(52, 152, 219, 0.4);
            color: white;
        }
    </style>
</head>
<body>
    <div class="page-container">
        <div class="doc-content">
            <nav class="breadcrumbs">
                <a href="javascript:history.back()">← Back</a>
                <a href="/">Home</a>
                <?php if (isset($breadcrumbs)): ?>
                    <?php foreach ($breadcrumbs as $link => $name): ?>
                        &gt; <a href="<?= htmlspecialchars($link) ?>"><?= htmlspecialchars($name) ?></a>
                    <?php endforeach; ?>
                <?php endif; ?>
            </nav>

            <div class="content">
                <?= $content ?? '' ?>
            </div>

            <footer>
                <p>id Tech 3 Engine Documentation - <?= date('Y') ?></p>
                <p>Advanced Documentation System with Enhanced Styling</p>
            </footer>
        </div>
    </div>
</body>
</html> 