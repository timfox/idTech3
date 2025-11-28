<?php

namespace App\Core;

class Router {
    private array $routes = [];
    private array $params = [];

    public function add(string $route, array $params = []): void {
        $route = preg_replace('/\//', '\\/', $route);
        $route = preg_replace('/\{([a-z]+)\}/', '(?P<\1>[a-z-]+)', $route);
        $route = '/^' . $route . '$/i';
        $this->routes[$route] = $params;
    }

    public function dispatch(string $url): void {
        if ($this->match($url)) {
            $controller = $this->params['controller'];
            $controller = $this->convertToStudlyCaps($controller);
            $controller = 'App\\Controllers\\' . $controller;
            if (class_exists($controller)) {
                $controller_object = new $controller($this->params);
                $action = $this->params['action'];
                $action = $this->convertToCamelCase($action);
                $controller_object->$action();
            } else {
                throw new \Exception('Controller class ' . $controller . ' not found');
            }
        } else {
            throw new \Exception('No route matched.', 404);
        }
    }

    public function match(string $url): bool {
        foreach ($this->routes as $route => $params) {
            if (preg_match($route, $url, $matches)) {
                foreach ($matches as $key => $match) {
                    if (is_string($key)) {
                        $params[$key] = $match;
                    }
                }
                $this->params = $params;
                return true;
            }
        }
        return false;
    }

    protected function convertToStudlyCaps(string $string): string {
        return str_replace(' ', '', ucwords(str_replace('-', ' ', $string)));
    }

    protected function convertToCamelCase(string $string): string {
        return lcfirst($this->convertToStudlyCaps($string));
    }
}
