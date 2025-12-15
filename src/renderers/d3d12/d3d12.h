#ifndef __D3D12_H__
#define __D3D12_H__

#ifdef _WIN32

#include "../qcommon/q_shared.h"

#ifdef __cplusplus
#include <d3d12.h>
#include <dxgi1_6.h>
#include <wrl/client.h>
#include <d3dcompiler.h>
#include <d3d12raytracing.h>

#pragma comment(lib, "d3d12.lib")
#pragma comment(lib, "dxgi.lib")
#pragma comment(lib, "d3dcompiler.lib")

using Microsoft::WRL::ComPtr;
#else
// C-compatible forward declarations
typedef void* IDXGIFactory4;
typedef void* IDXGISwapChain3;
typedef void* ID3D12Device;
typedef void* ID3D12CommandQueue;
typedef void* ID3D12CommandAllocator;
typedef void* ID3D12GraphicsCommandList;
typedef void* ID3D12Fence;
typedef void* ID3D12DescriptorHeap;
typedef void* ID3D12Resource;
typedef void* ID3D12RootSignature;
typedef void* ID3D12PipelineState;
typedef void* ID3D12Debug;
typedef void* ID3D12InfoQueue;
#endif

// DirectX 12 context structure
#ifdef __cplusplus
typedef struct {
	qboolean initialized;
	
	// DXGI
	ComPtr<IDXGIFactory4> factory;
	ComPtr<IDXGISwapChain3> swapChain;
	
	// D3D12 Device
	ComPtr<ID3D12Device> device;
	ComPtr<ID3D12CommandQueue> commandQueue;
	ComPtr<ID3D12CommandAllocator> commandAllocator;
	ComPtr<ID3D12GraphicsCommandList> commandList;
	
	// Synchronization
	ComPtr<ID3D12Fence> fence;
	UINT64 fenceValue;
	HANDLE fenceEvent;
	
	// Descriptor heaps
	ComPtr<ID3D12DescriptorHeap> rtvHeap;
	ComPtr<ID3D12DescriptorHeap> dsvHeap;
	ComPtr<ID3D12DescriptorHeap> srvHeap;
	UINT rtvDescriptorSize;
	UINT dsvDescriptorSize;
	UINT srvDescriptorSize;
	
	// Render targets
	ComPtr<ID3D12Resource> renderTargets[3]; // Triple buffering
	ComPtr<ID3D12Resource> depthStencil;
	UINT frameIndex;
	UINT swapChainBufferCount;
	
	// Root signature and pipeline state
	ComPtr<ID3D12RootSignature> rootSignature;
	ComPtr<ID3D12PipelineState> pipelineState;
	
	// Viewport and scissor
	D3D12_VIEWPORT viewport;
	D3D12_RECT scissorRect;
	
	// Window handle
	HWND hwnd;
	UINT width;
	UINT height;
	
	// Feature support
	D3D_FEATURE_LEVEL featureLevel;
	qboolean supportsTier1;
	qboolean supportsTier2;
	qboolean supportsTier3;
	
	// Debug layer
	qboolean enableDebugLayer;
	ComPtr<ID3D12Debug> debugController;
	ComPtr<ID3D12InfoQueue> infoQueue;
	
	// Ray tracing (DXR)
	qboolean rayTracingSupported;
	ComPtr<ID3D12Device5> device5; // DXR requires device interface 5+
	ComPtr<ID3D12GraphicsCommandList4> commandList4; // DXR requires command list interface 4+
	ComPtr<ID3D12StateObject> rayTracingPipelineState;
	ComPtr<ID3D12RootSignature> rayTracingRootSignature;
	ComPtr<ID3D12Resource> shaderBindingTable;
	ComPtr<ID3D12Resource> topLevelAccelerationStructure;
	ComPtr<ID3D12Resource> bottomLevelAccelerationStructure;
	ComPtr<ID3D12Resource> rayTracingOutput;
	UINT shaderBindingTableSize;
	UINT shaderBindingTableStride;
	D3D12_RAYTRACING_ACCELERATION_STRUCTURE_BUILD_FLAGS buildFlags;
	D3D12_RAYTRACING_TIER rayTracingTier;
} d3d12Context_t;
#else
// C-compatible structure (pointers only)
typedef struct {
	qboolean initialized;
	void *factory;
	void *swapChain;
	void *device;
	void *commandQueue;
	void *commandAllocator;
	void *commandList;
	void *fence;
	UINT64 fenceValue;
	HANDLE fenceEvent;
	void *rtvHeap;
	void *dsvHeap;
	void *srvHeap;
	UINT rtvDescriptorSize;
	UINT dsvDescriptorSize;
	UINT srvDescriptorSize;
	void *renderTargets[3];
	void *depthStencil;
	UINT frameIndex;
	UINT swapChainBufferCount;
	void *rootSignature;
	void *pipelineState;
	// Viewport and scissor would need separate structs in C
	HWND hwnd;
	UINT width;
	UINT height;
	UINT featureLevel;
	qboolean supportsTier1;
	qboolean supportsTier2;
	qboolean supportsTier3;
	qboolean enableDebugLayer;
	void *debugController;
	void *infoQueue;
} d3d12Context_t;
#endif

extern d3d12Context_t d3d12;

// Function prototypes
qboolean D3D12_Init(void);
void D3D12_Shutdown(void);
qboolean D3D12_CreateDevice(void);
qboolean D3D12_CreateSwapChain(HWND hwnd, UINT width, UINT height);
qboolean D3D12_CreateRenderTargets(void);
qboolean D3D12_CreateRootSignature(void);
qboolean D3D12_CreatePipelineState(void);
void D3D12_WaitForGPU(void);
void D3D12_Present(void);
void D3D12_ResizeSwapChain(UINT width, UINT height);

// Helper functions
const char* D3D12_GetErrorString(HRESULT hr);
qboolean D3D12_CheckFeatureSupport(void);

// Ray tracing functions
qboolean D3D12_CheckRayTracingSupport(void);
qboolean D3D12_CreateRayTracingDevice(void);
qboolean D3D12_CreateRayTracingPipelineState(void);
qboolean D3D12_CreateAccelerationStructures(void);
qboolean D3D12_CreateShaderBindingTable(void);
qboolean D3D12_CreateRayTracingOutput(UINT width, UINT height);
qboolean D3D12_BuildBottomLevelAccelerationStructure(const D3D12_RAYTRACING_GEOMETRY_DESC* geometries, UINT geometryCount, ID3D12Resource** blas);
qboolean D3D12_BuildTopLevelAccelerationStructure(const D3D12_RAYTRACING_INSTANCE_DESC* instances, UINT instanceCount, ID3D12Resource** tlas);
qboolean D3D12_UpdateShaderBindingTable(void);
void D3D12_BuildAccelerationStructures(void);
void D3D12_DispatchRays(UINT width, UINT height);
void D3D12_UpdateRayTracingOutput(UINT width, UINT height);

#endif // _WIN32

#endif // __D3D12_H__

