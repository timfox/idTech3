#ifdef _WIN32

#include "../qcommon/q_shared.h"
#include "../qcommon/qcommon.h"
#include "d3d12.h"
#include <stdio.h>

// D3DX12 helper header (if available, otherwise define inline helpers)
#ifndef CD3DX12_HEAP_PROPERTIES
#define CD3DX12_HEAP_PROPERTIES D3D12_HEAP_PROPERTIES
inline D3D12_HEAP_PROPERTIES CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE type, D3D12_CPU_PAGE_PROPERTY cpuPageProperty = D3D12_CPU_PAGE_PROPERTY_UNKNOWN, D3D12_MEMORY_POOL memoryPoolPreference = D3D12_MEMORY_POOL_UNKNOWN, UINT creationNodeMask = 1, UINT nodeMask = 1) {
	D3D12_HEAP_PROPERTIES props = {};
	props.Type = type;
	props.CPUPageProperty = cpuPageProperty;
	props.MemoryPoolPreference = memoryPoolPreference;
	props.CreationNodeMask = creationNodeMask;
	props.VisibleNodeMask = nodeMask;
	return props;
}
#endif

#ifndef CD3DX12_CPU_DESCRIPTOR_HANDLE
#define CD3DX12_CPU_DESCRIPTOR_HANDLE D3D12_CPU_DESCRIPTOR_HANDLE
inline D3D12_CPU_DESCRIPTOR_HANDLE CD3DX12_CPU_DESCRIPTOR_HANDLE(D3D12_CPU_DESCRIPTOR_HANDLE handle, INT offsetScaledByIncrementSize) {
	D3D12_CPU_DESCRIPTOR_HANDLE result = handle;
	result.ptr += offsetScaledByIncrementSize;
	return result;
}
inline D3D12_CPU_DESCRIPTOR_HANDLE CD3DX12_CPU_DESCRIPTOR_HANDLE(D3D12_CPU_DESCRIPTOR_HANDLE handle, INT offsetInDescriptors, UINT descriptorIncrementSize) {
	D3D12_CPU_DESCRIPTOR_HANDLE result = handle;
	result.ptr += offsetInDescriptors * descriptorIncrementSize;
	return result;
}
#endif

d3d12Context_t d3d12;

/*
================
D3D12_GetErrorString
================
*/
const char* D3D12_GetErrorString(HRESULT hr) {
	switch (hr) {
		case DXGI_ERROR_DEVICE_REMOVED:
			return "Device removed";
		case DXGI_ERROR_DEVICE_HUNG:
			return "Device hung";
		case DXGI_ERROR_DEVICE_RESET:
			return "Device reset";
		case DXGI_ERROR_DRIVER_INTERNAL_ERROR:
			return "Driver internal error";
		case DXGI_ERROR_INVALID_CALL:
			return "Invalid call";
		case E_INVALIDARG:
			return "Invalid argument";
		case E_OUTOFMEMORY:
			return "Out of memory";
		case E_NOTIMPL:
			return "Not implemented";
		default:
			return "Unknown error";
	}
}

/*
================
D3D12_CheckFeatureSupport
================
*/
qboolean D3D12_CheckFeatureSupport(void) {
	D3D12_FEATURE_DATA_D3D12_OPTIONS options = {};
	HRESULT hr = d3d12.device->CheckFeatureSupport(D3D12_FEATURE_D3D12_OPTIONS, &options, sizeof(options));
	
	if (FAILED(hr)) {
		return qfalse;
	}
	
	// Check resource binding tier
	switch (options.ResourceBindingTier) {
		case D3D12_RESOURCE_BINDING_TIER_1:
			d3d12.supportsTier1 = qtrue;
			break;
		case D3D12_RESOURCE_BINDING_TIER_2:
			d3d12.supportsTier1 = qtrue;
			d3d12.supportsTier2 = qtrue;
			break;
		case D3D12_RESOURCE_BINDING_TIER_3:
			d3d12.supportsTier1 = qtrue;
			d3d12.supportsTier2 = qtrue;
			d3d12.supportsTier3 = qtrue;
			break;
	}
	
	// Check ray tracing support
	D3D12_CheckRayTracingSupport();
	
	return qtrue;
}

/*
================
D3D12_CheckRayTracingSupport
================
*/
qboolean D3D12_CheckRayTracingSupport(void) {
	if (!d3d12.device) {
		return qfalse;
	}
	
	// Query ray tracing tier
	D3D12_FEATURE_DATA_D3D12_OPTIONS5 options5 = {};
	HRESULT hr = d3d12.device->CheckFeatureSupport(D3D12_FEATURE_D3D12_OPTIONS5, &options5, sizeof(options5));
	
	if (FAILED(hr)) {
		d3d12.rayTracingSupported = qfalse;
		return qfalse;
	}
	
	// Check if ray tracing tier 1.0 or higher is supported
	if (options5.RaytracingTier >= D3D12_RAYTRACING_TIER_1_0) {
		d3d12.rayTracingSupported = qtrue;
		d3d12.rayTracingTier = options5.RaytracingTier;
		Com_Printf("D3D12: Ray tracing supported (Tier %d.%d)\n", 
			(options5.RaytracingTier >> 4) & 0xF, options5.RaytracingTier & 0xF);
		
		// Query device for DXR interface
		hr = d3d12.device.As(&d3d12.device5);
		if (SUCCEEDED(hr)) {
			Com_Printf("D3D12: DXR device interface acquired\n");
		} else {
			Com_Printf("D3D12: Warning - Failed to acquire DXR device interface\n");
			d3d12.rayTracingSupported = qfalse;
		}
	} else {
		d3d12.rayTracingSupported = qfalse;
		Com_Printf("D3D12: Ray tracing not supported (Tier %d.%d)\n",
			(options5.RaytracingTier >> 4) & 0xF, options5.RaytracingTier & 0xF);
	}
	
	return d3d12.rayTracingSupported;
}

/*
================
D3D12_CreateRayTracingDevice
================
*/
qboolean D3D12_CreateRayTracingDevice(void) {
	if (!d3d12.rayTracingSupported || !d3d12.device5) {
		return qfalse;
	}
	
	// Query command list for DXR interface
	HRESULT hr = d3d12.commandList.As(&d3d12.commandList4);
	if (FAILED(hr)) {
		Com_Printf("D3D12: Failed to acquire DXR command list interface: %s\n", D3D12_GetErrorString(hr));
		return qfalse;
	}
	
	Com_Printf("D3D12: DXR command list interface acquired\n");
	return qtrue;
}

/*
================
D3D12_CreateDevice
================
*/
qboolean D3D12_CreateDevice(void) {
	HRESULT hr;
	
	// Enable debug layer in debug builds
#ifdef _DEBUG
	d3d12.enableDebugLayer = qtrue;
	if (SUCCEEDED(D3D12GetDebugInterface(IID_PPV_ARGS(&d3d12.debugController)))) {
		d3d12.debugController->EnableDebugLayer();
		Com_Printf("D3D12: Debug layer enabled\n");
	}
#endif
	
	// Create DXGI Factory
	hr = CreateDXGIFactory2(d3d12.enableDebugLayer ? DXGI_CREATE_FACTORY_DEBUG : 0, IID_PPV_ARGS(&d3d12.factory));
	if (FAILED(hr)) {
		Com_Printf("D3D12: Failed to create DXGI factory: %s\n", D3D12_GetErrorString(hr));
		return qfalse;
	}
	
	// Try to create device with highest feature level
	D3D_FEATURE_LEVEL featureLevels[] = {
		D3D_FEATURE_LEVEL_12_2,
		D3D_FEATURE_LEVEL_12_1,
		D3D_FEATURE_LEVEL_12_0,
		D3D_FEATURE_LEVEL_11_1,
		D3D_FEATURE_LEVEL_11_0
	};
	
	ComPtr<IDXGIAdapter1> adapter;
	ComPtr<IDXGIAdapter1> bestAdapter;
	SIZE_T maxDedicatedVideoMemory = 0;
	
	// Find best adapter
	for (UINT i = 0; d3d12.factory->EnumAdapters1(i, &adapter) != DXGI_ERROR_NOT_FOUND; ++i) {
		DXGI_ADAPTER_DESC1 desc;
		adapter->GetDesc1(&desc);
		
		// Skip software adapters
		if (desc.Flags & DXGI_ADAPTER_FLAG_SOFTWARE) {
			continue;
		}
		
		// Check if adapter supports D3D12
		if (SUCCEEDED(D3D12CreateDevice(adapter.Get(), D3D_FEATURE_LEVEL_11_0, _uuidof(ID3D12Device), nullptr))) {
			if (desc.DedicatedVideoMemory > maxDedicatedVideoMemory) {
				maxDedicatedVideoMemory = desc.DedicatedVideoMemory;
				bestAdapter = adapter;
			}
		}
	}
	
	if (!bestAdapter) {
		Com_Printf("D3D12: No suitable adapter found\n");
		return qfalse;
	}
	
	// Create device with best adapter
	for (UINT i = 0; i < ARRAY_LEN(featureLevels); ++i) {
		if (SUCCEEDED(D3D12CreateDevice(bestAdapter.Get(), featureLevels[i], IID_PPV_ARGS(&d3d12.device)))) {
			d3d12.featureLevel = featureLevels[i];
			Com_Printf("D3D12: Created device with feature level %d.%d\n", 
				(d3d12.featureLevel >> 12) & 0xF, (d3d12.featureLevel >> 8) & 0xF);
			break;
		}
	}
	
	if (!d3d12.device) {
		Com_Printf("D3D12: Failed to create device\n");
		return qfalse;
	}
	
	// Check feature support
	if (!D3D12_CheckFeatureSupport()) {
		Com_Printf("D3D12: Failed to check feature support\n");
		return qfalse;
	}
	
	// Create command queue
	D3D12_COMMAND_QUEUE_DESC queueDesc = {};
	queueDesc.Flags = D3D12_COMMAND_QUEUE_FLAG_NONE;
	queueDesc.Type = D3D12_COMMAND_LIST_TYPE_DIRECT;
	
	hr = d3d12.device->CreateCommandQueue(&queueDesc, IID_PPV_ARGS(&d3d12.commandQueue));
	if (FAILED(hr)) {
		Com_Printf("D3D12: Failed to create command queue: %s\n", D3D12_GetErrorString(hr));
		return qfalse;
	}
	
	// Create command allocator
	hr = d3d12.device->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT, IID_PPV_ARGS(&d3d12.commandAllocator));
	if (FAILED(hr)) {
		Com_Printf("D3D12: Failed to create command allocator: %s\n", D3D12_GetErrorString(hr));
		return qfalse;
	}
	
	// Create command list
	hr = d3d12.device->CreateCommandList(0, D3D12_COMMAND_LIST_TYPE_DIRECT, d3d12.commandAllocator.Get(), 
		nullptr, IID_PPV_ARGS(&d3d12.commandList));
	if (FAILED(hr)) {
		Com_Printf("D3D12: Failed to create command list: %s\n", D3D12_GetErrorString(hr));
		return qfalse;
	}
	
	// Command list starts in recording state, close it
	d3d12.commandList->Close();
	
	// Create synchronization objects
	hr = d3d12.device->CreateFence(0, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&d3d12.fence));
	if (FAILED(hr)) {
		Com_Printf("D3D12: Failed to create fence: %s\n", D3D12_GetErrorString(hr));
		return qfalse;
	}
	
	d3d12.fenceValue = 0;
	d3d12.fenceEvent = CreateEvent(nullptr, FALSE, FALSE, nullptr);
	if (d3d12.fenceEvent == nullptr) {
		Com_Printf("D3D12: Failed to create fence event\n");
		return qfalse;
	}
	
	return qtrue;
}

/*
================
D3D12_CreateSwapChain
================
*/
qboolean D3D12_CreateSwapChain(HWND hwnd, UINT width, UINT height) {
	HRESULT hr;
	
	d3d12.hwnd = hwnd;
	d3d12.width = width;
	d3d12.height = height;
	d3d12.swapChainBufferCount = 3; // Triple buffering
	
	// Describe swap chain
	DXGI_SWAP_CHAIN_DESC1 swapChainDesc = {};
	swapChainDesc.Width = width;
	swapChainDesc.Height = height;
	swapChainDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
	swapChainDesc.SampleDesc.Count = 1;
	swapChainDesc.SampleDesc.Quality = 0;
	swapChainDesc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
	swapChainDesc.BufferCount = d3d12.swapChainBufferCount;
	swapChainDesc.SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD;
	swapChainDesc.Flags = DXGI_SWAP_CHAIN_FLAG_ALLOW_MODE_SWITCH;
	
	ComPtr<IDXGISwapChain1> swapChain1;
	hr = d3d12.factory->CreateSwapChainForHwnd(
		d3d12.commandQueue.Get(),
		hwnd,
		&swapChainDesc,
		nullptr,
		nullptr,
		&swapChain1
	);
	
	if (FAILED(hr)) {
		Com_Printf("D3D12: Failed to create swap chain: %s\n", D3D12_GetErrorString(hr));
		return qfalse;
	}
	
	hr = swapChain1.As(&d3d12.swapChain);
	if (FAILED(hr)) {
		Com_Printf("D3D12: Failed to query swap chain interface: %s\n", D3D12_GetErrorString(hr));
		return qfalse;
	}
	
	d3d12.frameIndex = d3d12.swapChain->GetCurrentBackBufferIndex();
	
	return qtrue;
}

/*
================
D3D12_CreateRenderTargets
================
*/
qboolean D3D12_CreateRenderTargets(void) {
	HRESULT hr;
	
	// Create RTV descriptor heap
	D3D12_DESCRIPTOR_HEAP_DESC rtvHeapDesc = {};
	rtvHeapDesc.NumDescriptors = d3d12.swapChainBufferCount;
	rtvHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_RTV;
	rtvHeapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE;
	
	hr = d3d12.device->CreateDescriptorHeap(&rtvHeapDesc, IID_PPV_ARGS(&d3d12.rtvHeap));
	if (FAILED(hr)) {
		Com_Printf("D3D12: Failed to create RTV heap: %s\n", D3D12_GetErrorString(hr));
		return qfalse;
	}
	
	d3d12.rtvDescriptorSize = d3d12.device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV);
	
	// Create render target views for each back buffer
	D3D12_CPU_DESCRIPTOR_HANDLE rtvHandle = d3d12.rtvHeap->GetCPUDescriptorHandleForHeapStart();
	
	for (UINT i = 0; i < d3d12.swapChainBufferCount; ++i) {
		hr = d3d12.swapChain->GetBuffer(i, IID_PPV_ARGS(&d3d12.renderTargets[i]));
		if (FAILED(hr)) {
			Com_Printf("D3D12: Failed to get swap chain buffer %d: %s\n", i, D3D12_GetErrorString(hr));
			return qfalse;
		}
		
		d3d12.device->CreateRenderTargetView(d3d12.renderTargets[i].Get(), nullptr, rtvHandle);
		rtvHandle.ptr += d3d12.rtvDescriptorSize;
	}
	
	// Create depth stencil
	D3D12_RESOURCE_DESC depthStencilDesc = {};
	depthStencilDesc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
	depthStencilDesc.Width = d3d12.width;
	depthStencilDesc.Height = d3d12.height;
	depthStencilDesc.DepthOrArraySize = 1;
	depthStencilDesc.MipLevels = 1;
	depthStencilDesc.Format = DXGI_FORMAT_D32_FLOAT;
	depthStencilDesc.SampleDesc.Count = 1;
	depthStencilDesc.SampleDesc.Quality = 0;
	depthStencilDesc.Flags = D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL;
	
	D3D12_CLEAR_VALUE depthOptimizedClearValue = {};
	depthOptimizedClearValue.Format = DXGI_FORMAT_D32_FLOAT;
	depthOptimizedClearValue.DepthStencil.Depth = 1.0f;
	depthOptimizedClearValue.DepthStencil.Stencil = 0;
	
	D3D12_HEAP_PROPERTIES heapProps = {};
	heapProps.Type = D3D12_HEAP_TYPE_DEFAULT;
	heapProps.CPUPageProperty = D3D12_CPU_PAGE_PROPERTY_UNKNOWN;
	heapProps.MemoryPoolPreference = D3D12_MEMORY_POOL_UNKNOWN;
	heapProps.CreationNodeMask = 1;
	heapProps.VisibleNodeMask = 1;
	
	hr = d3d12.device->CreateCommittedResource(
		&heapProps,
		D3D12_HEAP_FLAG_NONE,
		&depthStencilDesc,
		D3D12_RESOURCE_STATE_DEPTH_WRITE,
		&depthOptimizedClearValue,
		IID_PPV_ARGS(&d3d12.depthStencil)
	);
	
	if (FAILED(hr)) {
		Com_Printf("D3D12: Failed to create depth stencil: %s\n", D3D12_GetErrorString(hr));
		return qfalse;
	}
	
	// Create DSV descriptor heap
	D3D12_DESCRIPTOR_HEAP_DESC dsvHeapDesc = {};
	dsvHeapDesc.NumDescriptors = 1;
	dsvHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_DSV;
	dsvHeapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE;
	
	hr = d3d12.device->CreateDescriptorHeap(&dsvHeapDesc, IID_PPV_ARGS(&d3d12.dsvHeap));
	if (FAILED(hr)) {
		Com_Printf("D3D12: Failed to create DSV heap: %s\n", D3D12_GetErrorString(hr));
		return qfalse;
	}
	
	d3d12.dsvDescriptorSize = d3d12.device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_DSV);
	d3d12.device->CreateDepthStencilView(d3d12.depthStencil.Get(), nullptr, d3d12.dsvHeap->GetCPUDescriptorHandleForHeapStart());
	
	// Set viewport and scissor
	d3d12.viewport.TopLeftX = 0;
	d3d12.viewport.TopLeftY = 0;
	d3d12.viewport.Width = (float)d3d12.width;
	d3d12.viewport.Height = (float)d3d12.height;
	d3d12.viewport.MinDepth = 0.0f;
	d3d12.viewport.MaxDepth = 1.0f;
	
	d3d12.scissorRect.left = 0;
	d3d12.scissorRect.top = 0;
	d3d12.scissorRect.right = (LONG)d3d12.width;
	d3d12.scissorRect.bottom = (LONG)d3d12.height;
	
	return qtrue;
}

/*
================
D3D12_CreateRootSignature
================
*/
qboolean D3D12_CreateRootSignature(void) {
	// Simple root signature for basic rendering
	D3D12_ROOT_SIGNATURE_DESC rootSignatureDesc = {};
	rootSignatureDesc.Flags = D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT;
	
	ComPtr<ID3DBlob> signature;
	ComPtr<ID3DBlob> error;
	
	HRESULT hr = D3D12SerializeRootSignature(&rootSignatureDesc, D3D_ROOT_SIGNATURE_VERSION_1, &signature, &error);
	if (FAILED(hr)) {
		Com_Printf("D3D12: Failed to serialize root signature: %s\n", D3D12_GetErrorString(hr));
		return qfalse;
	}
	
	hr = d3d12.device->CreateRootSignature(0, signature->GetBufferPointer(), signature->GetBufferSize(), 
		IID_PPV_ARGS(&d3d12.rootSignature));
	if (FAILED(hr)) {
		Com_Printf("D3D12: Failed to create root signature: %s\n", D3D12_GetErrorString(hr));
		return qfalse;
	}
	
	return qtrue;
}

/*
================
D3D12_CreatePipelineState
================
*/
qboolean D3D12_CreatePipelineState(void) {
	// This is a placeholder - actual pipeline state creation will be done when shaders are implemented
	// For now, return success
	return qtrue;
}

/*
================
D3D12_WaitForGPU
================
*/
void D3D12_WaitForGPU(void) {
	// Signal fence
	const UINT64 fence = d3d12.fenceValue;
	HRESULT hr = d3d12.commandQueue->Signal(d3d12.fence.Get(), fence);
	if (FAILED(hr)) {
		return;
	}
	
	d3d12.fenceValue++;
	
	// Wait until fence is reached
	if (d3d12.fence->GetCompletedValue() < fence) {
		hr = d3d12.fence->SetEventOnCompletion(fence, d3d12.fenceEvent);
		if (SUCCEEDED(hr)) {
			WaitForSingleObject(d3d12.fenceEvent, INFINITE);
		}
	}
}

/*
================
D3D12_Present
================
*/
void D3D12_Present(void) {
	HRESULT hr = d3d12.swapChain->Present(1, 0); // VSync enabled
	
	if (FAILED(hr)) {
		Com_Printf("D3D12: Present failed: %s\n", D3D12_GetErrorString(hr));
		return;
	}
	
	d3d12.frameIndex = d3d12.swapChain->GetCurrentBackBufferIndex();
}

/*
================
D3D12_ResizeSwapChain
================
*/
void D3D12_ResizeSwapChain(UINT width, UINT height) {
	if (d3d12.width == width && d3d12.height == height) {
		return;
	}
	
	D3D12_WaitForGPU();
	
	// Release render targets
	for (UINT i = 0; i < d3d12.swapChainBufferCount; ++i) {
		d3d12.renderTargets[i].Reset();
	}
	d3d12.depthStencil.Reset();
	
	// Resize swap chain
	HRESULT hr = d3d12.swapChain->ResizeBuffers(d3d12.swapChainBufferCount, width, height, 
		DXGI_FORMAT_R8G8B8A8_UNORM, DXGI_SWAP_CHAIN_FLAG_ALLOW_MODE_SWITCH);
	
	if (FAILED(hr)) {
		Com_Printf("D3D12: Failed to resize swap chain: %s\n", D3D12_GetErrorString(hr));
		return;
	}
	
	d3d12.width = width;
	d3d12.height = height;
	d3d12.frameIndex = d3d12.swapChain->GetCurrentBackBufferIndex();
	
	// Recreate render targets
	D3D12_CreateRenderTargets();
}

/*
================
D3D12_Init
================
*/
qboolean D3D12_Init(void) {
	Com_Memset(&d3d12, 0, sizeof(d3d12));
	
	if (!D3D12_CreateDevice()) {
		return qfalse;
	}
	
	// Initialize ray tracing if supported
	if (d3d12.rayTracingSupported) {
		if (!D3D12_CreateRayTracingDevice()) {
			Com_Printf("D3D12: Warning - Ray tracing device creation failed, continuing without DXR\n");
			d3d12.rayTracingSupported = qfalse;
		}
	}
	
	d3d12.initialized = qtrue;
	Com_Printf("D3D12: Initialized successfully\n");
	
	return qtrue;
}

/*
================
D3D12_Shutdown
================
*/
void D3D12_Shutdown(void) {
	if (!d3d12.initialized) {
		return;
	}
	
	D3D12_WaitForGPU();
	
	// Release resources
	for (UINT i = 0; i < d3d12.swapChainBufferCount; ++i) {
		d3d12.renderTargets[i].Reset();
	}
	d3d12.depthStencil.Reset();
	d3d12.rtvHeap.Reset();
	d3d12.dsvHeap.Reset();
	d3d12.srvHeap.Reset();
	d3d12.rootSignature.Reset();
	d3d12.pipelineState.Reset();
	d3d12.commandList.Reset();
	d3d12.commandAllocator.Reset();
	d3d12.commandQueue.Reset();
	d3d12.swapChain.Reset();
	d3d12.device.Reset();
	d3d12.factory.Reset();
	
	if (d3d12.fenceEvent) {
		CloseHandle(d3d12.fenceEvent);
		d3d12.fenceEvent = nullptr;
	}
	
	d3d12.fence.Reset();
	d3d12.debugController.Reset();
	d3d12.infoQueue.Reset();
	
	d3d12.initialized = qfalse;
	Com_Printf("D3D12: Shutdown complete\n");
}

#endif // _WIN32

