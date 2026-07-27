-- ModelDoc - idTech3 Model Documentation System
-- Scene + GameObjects + Components architecture

local function modeldoc_banner()
  print("[ModelDoc] Lua modeldoc.lua loaded (script_reload scripts/lua/modeldoc.lua)")
end

-- ============================================================================
-- Core Architecture: Scene, GameObject, Component
-- ============================================================================

-- Base Component class
Component = {
    name = "Component",
    enabled = true,
    
    init = function(self, gameObject)
        self.gameObject = gameObject
    end,
    
    start = function(self)
        -- Override in subclasses
    end,
    
    update = function(self, deltaTime)
        -- Override in subclasses
    end,
    
    destroy = function(self)
        -- Cleanup
    end
}

-- Base GameObject class
GameObject = {
    name = "GameObject",
    active = true,
    components = {},
    
    create = function(name)
        local go = {
            name = name,
            active = true,
            components = {}
        }
        setmetatable(go, {__index = GameObject})
        return go
    end,
    
    addComponent = function(self, componentType, ...)
        local component = componentType:new(self, ...)
        table.insert(self.components, component)
        component:init(self)
        return component
    end,
    
    getComponent = function(self, componentType)
        for _, component in ipairs(self.components) do
            if component.__type == componentType then
                return component
            end
        end
        return nil
    end,
    
    start = function(self)
        for _, component in ipairs(self.components) do
            if component.start then
                component:start()
            end
        end
    end,
    
    update = function(self, deltaTime)
        if not self.active then return end
        for _, component in ipairs(self.components) do
            if component.update then
                component:update(deltaTime)
            end
        end
    end,
    
    destroy = function(self)
        for _, component in ipairs(self.components) do
            if component.destroy then
                component:destroy()
            end
        end
        self.components = {}
    end
}

-- Base Scene class
Scene = {
    name = "Scene",
    gameObjects = {},
    active = true,
    
    create = function(name)
        local scene = {
            name = name,
            gameObjects = {},
            active = true
        }
        setmetatable(scene, {__index = Scene})
        return scene
    end,
    
    addGameObject = function(self, gameObject)
        table.insert(self.gameObjects, gameObject)
        gameObject:start()
    end,
    
    removeGameObject = function(self, gameObject)
        for i, go in ipairs(self.gameObjects) do
            if go == gameObject then
                table.remove(self.gameObjects, i)
                gameObject:destroy()
                break
            end
        end
    end,
    
    update = function(self, deltaTime)
        if not self.active then return end
        for _, go in ipairs(self.gameObjects) do
            if go.update then
                go:update(deltaTime)
            end
        end
    end,
    
    destroy = function(self)
        for _, go in ipairs(self.gameObjects) do
            go:destroy()
        end
        self.gameObjects = {}
    end
}

-- ============================================================================
-- ModelDoc Components
-- ============================================================================

-- ModelPreviewComponent: Handles model rendering and preview
ModelPreviewComponent = {
    __type = "ModelPreviewComponent",
    modelPath = "",
    shaderPath = "",
    materialParams = {},
    
    new = function(self, gameObject, modelPath, shaderPath)
        local comp = {
            modelPath = modelPath or "",
            shaderPath = shaderPath or "",
            materialParams = {
                baseColor = {1, 1, 1, 1},
                metallic = 0.5,
                roughness = 0.5,
                emissive = {0, 0, 0}
            }
        }
        setmetatable(comp, {__index = ModelPreviewComponent})
        return comp
    end,
    
    init = function(self, gameObject)
        if self.modelPath ~= "" then
            self.modelHandle = Engine.Model.load(self.modelPath)
        end
    end,
    
    update = function(self, deltaTime)
        -- Update model preview properties
        if self.modelHandle and Engine.Model then
            Engine.Model.setMaterialParams(self.modelHandle, self.materialParams)
        end
    end
}

-- ModelBrowserComponent: Handles model browsing and selection
ModelBrowserComponent = {
    __type = "ModelBrowserComponent",
    searchPath = "models/",
    filteredModels = {},
    selectedModel = nil,
    
    new = function(self, gameObject, searchPath)
        local comp = {
            searchPath = searchPath or "models/",
            filteredModels = {},
            selectedModel = nil
        }
        setmetatable(comp, {__index = ModelBrowserComponent})
        return comp
    end,
    
    start = function(self)
        self:refreshModels()
    end,
    
    refreshModels = function(self)
        -- Scan for models in search path
        self.filteredModels = Engine.FileSystem.findFiles(self.searchPath, {"*.md3", "*.gltf", "*.glb", "*.iqm", "*.md5mesh", "*.usda", "*.usd"})
    end,
    
    selectModel = function(self, modelPath)
        self.selectedModel = modelPath
        -- Notify other components about selection change
        if Engine.Event then
            Engine.Event.dispatch("model_selected", {path = modelPath})
        end
    end
}

-- ValidationComponent: Handles model validation
ValidationComponent = {
    __type = "ValidationComponent",
    validationResults = {},
    
    new = function(self, gameObject)
        local comp = {
            validationResults = {}
        }
        setmetatable(comp, {__index = ValidationComponent})
        return comp
    end,
    
    validateModel = function(self, modelPath)
        local results = {
            meshIntegrity = true,
            textureResolution = true,
            materialCompleteness = true,
            animationValidity = true,
            collisionBounds = true,
            shaderCompatibility = true
        }
        
        -- Perform actual validation checks
        if Engine.Model then
            local modelInfo = Engine.Model.getInfo(modelPath)
            if modelInfo then
                results.meshIntegrity = modelInfo.meshCount > 0
                results.textureResolution = modelInfo.textureCount > 0
                results.materialCompleteness = modelInfo.materialCount > 0
                results.animationValidity = modelInfo.animationCount >= 0
                results.collisionBounds = modelInfo.hasCollision or false
                results.shaderCompatibility = modelInfo.shaderCompatible or false
            end
        end
        
        self.validationResults = results
        return results
    end
}

-- ExportComponent: Handles documentation export
ExportComponent = {
    __type = "ExportComponent",
    exportFormats = {"pdf", "html", "json", "markdown"},
    
    new = function(self, gameObject)
        local comp = {
            exportFormats = {"pdf", "html", "json", "markdown"}
        }
        setmetatable(comp, {__index = ExportComponent})
        return comp
    end,
    
    exportModel = function(self, modelPath, outputPath, format)
        if not Engine.Export then
            print("[ModelDoc] Export not available (Engine.Export missing)")
            return false
        end
        
        return Engine.Export.model(modelPath, outputPath, format)
    end
}

-- ============================================================================
-- ModelDoc System
-- ============================================================================

ModelDoc = {
    currentScene = nil,
    deltaTime = 0,
    
    -- Initialize ModelDoc system
    init = function()
        print("[ModelDoc] Initializing ModelDoc system")
        
        -- Create main scene
        ModelDoc.currentScene = Scene.create("ModelDocScene")
        
        -- Create core game objects
        ModelDoc.createCoreGameObjects()
        
        -- Register console commands
        ModelDoc.registerConsoleCommands()
        
        print("[ModelDoc] ModelDoc system initialized")
    end,
    
    -- Create core game objects with components
    createCoreGameObjects = function()
        -- Model Browser GO
        local browserGO = GameObject.create("ModelBrowser")
        browserGO:addComponent(ModelBrowserComponent, "models/")
        
        -- Model Preview GO
        local previewGO = GameObject.create("ModelPreview")
        previewGO:addComponent(ModelPreviewComponent, "", "")
        
        -- Validation GO
        local validationGO = GameObject.create("Validation")
        validationGO:addComponent(ValidationComponent)
        
        -- Export GO
        local exportGO = GameObject.create("Export")
        exportGO:addComponent(ExportComponent)
        
        -- Add all to scene
        ModelDoc.currentScene:addGameObject(browserGO)
        ModelDoc.currentScene:addGameObject(previewGO)
        ModelDoc.currentScene:addGameObject(validationGO)
        ModelDoc.currentScene:addGameObject(exportGO)
    end,
    
    -- Register console commands
    registerConsoleCommands = function()
        -- Console: modeldoc_reload
        function modeldoc_reload()
            script_reload("scripts/lua/modeldoc.lua")
        end
        
        -- Console: modeldoc_open
        function modeldoc_open()
            if Engine.UI then
                Engine.UI.openWindow("modeldoc")
            end
        end
        
        -- Console: modeldoc_validate <model_path>
        function modeldoc_validate(modelPath)
            if not ModelDoc.currentScene then
                print("[ModelDoc] ModelDoc not initialized")
                return
            end
            
            local validationComponent = nil
            for _, go in ipairs(ModelDoc.currentScene.gameObjects) do
                local comp = go:getComponent(ValidationComponent)
                if comp then
                    validationComponent = comp
                    break
                end
            end
            
            if validationComponent then
                local results = validationComponent:validateModel(modelPath)
                print("[ModelDoc] Validation results for " .. modelPath)
                for key, value in pairs(results) do
                    print(string.format("  %s: %s", key, value and "PASS" or "FAIL"))
                end
            else
                print("[ModelDoc] Validation component not found")
            end
        end
        
        -- Console: modeldoc_export <model_path> <output_path> <format>
        function modeldoc_export(modelPath, outputPath, format)
            if not ModelDoc.currentScene then
                print("[ModelDoc] ModelDoc not initialized")
                return
            end
            
            local exportComponent = nil
            for _, go in ipairs(ModelDoc.currentScene.gameObjects) do
                local comp = go:getComponent(ExportComponent)
                if comp then
                    exportComponent = comp
                    break
                end
            end
            
            if exportComponent then
                local success = exportComponent:exportModel(modelPath, outputPath, format or "pdf")
                if success then
                    print(string.format("[ModelDoc] Exported %s to %s.%s", modelPath, outputPath, format or "pdf"))
                else
                    print(string.format("[ModelDoc] Export failed for %s", modelPath))
                end
            else
                print("[ModelDoc] Export component not found")
            end
        end
        
        -- Console: modeldoc_browser_refresh
        function modeldoc_browser_refresh()
            if not ModelDoc.currentScene then
                print("[ModelDoc] ModelDoc not initialized")
                return
            end
            
            local browserComponent = nil
            for _, go in ipairs(ModelDoc.currentScene.gameObjects) do
                local comp = go:getComponent(ModelBrowserComponent)
                if comp then
                    browserComponent = comp
                    break
                end
            end
            
            if browserComponent then
                browserComponent:refreshModels()
                print(string.format("[ModelDoc] Refreshed model browser. Found %d models", #browserComponent.filteredModels))
            else
                print("[ModelDoc] Browser component not found")
            end
        end
        
        -- Console: modeldoc_preview <model_path>
        function modeldoc_preview(modelPath)
            if not ModelDoc.currentScene then
                print("[ModelDoc] ModelDoc not initialized")
                return
            end
            
            local previewComponent = nil
            for _, go in ipairs(ModelDoc.currentScene.gameObjects) do
                local comp = go:getComponent(ModelPreviewComponent)
                if comp then
                    previewComponent = comp
                    break
                end
            end
            
            if previewComponent then
                previewComponent.modelPath = modelPath
                previewComponent.shaderPath = "models/" .. modelPath .. ".shader"
                print(string.format("[ModelDoc] Previewing %s", modelPath))
            else
                print("[ModelDoc] Preview component not found")
            end
        end
    end,
    
    -- Update loop
    update = function(deltaTime)
        ModelDoc.deltaTime = deltaTime
        if ModelDoc.currentScene then
            ModelDoc.currentScene:update(deltaTime)
        end
    end,
    
    -- Cleanup
    shutdown = function()
        if ModelDoc.currentScene then
            ModelDoc.currentScene:destroy()
            ModelDoc.currentScene = nil
        end
        print("[ModelDoc] ModelDoc system shutdown")
    end
}

-- ============================================================================
-- Lua Run Functions
-- ============================================================================

-- Console: lua_run modeldoc_init()
function modeldoc_init()
    ModelDoc.init()
end

-- Console: lua_run modeldoc_update()
function modeldoc_update()
    ModelDoc.update(0.016) -- 60 FPS
end

-- Console: lua_run modeldoc_shutdown()
function modeldoc_shutdown()
    ModelDoc.shutdown()
end

-- ============================================================================
-- Initialization
-- ============================================================================

modeldoc_banner()

-- Auto-initialize when script is loaded
modeldoc_init()