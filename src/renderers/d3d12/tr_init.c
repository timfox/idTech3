#ifdef _WIN32

#include "tr_local.h"
#include "../qcommon/qcommon.h"

d3d12Renderer_t tr;

/*
================
D3D12_Init
================
*/
void D3D12_Init(void) {
	Com_Memset(&tr, 0, sizeof(tr));
	
	if (!D3D12_Init()) {
		Com_Error(ERR_FATAL, "D3D12_Init failed");
	}
	
	tr.initialized = qtrue;
	Com_Printf("D3D12 renderer initialized\n");
}

/*
================
D3D12_Shutdown
================
*/
void D3D12_Shutdown(void) {
	if (!tr.initialized) {
		return;
	}
	
	D3D12_Shutdown();
	tr.initialized = qfalse;
	Com_Printf("D3D12 renderer shut down\n");
}

/*
================
D3D12_InitWindow
================
*/
qboolean D3D12_InitWindow(HWND hwnd) {
	if (!tr.initialized) {
		return qfalse;
	}
	
	tr.hwnd = hwnd;
	
	// Get window size
	RECT rect;
	GetClientRect(hwnd, &rect);
	tr.width = rect.right - rect.left;
	tr.height = rect.bottom - rect.top;
	
	// Create swap chain
	if (!D3D12_CreateSwapChain(hwnd, tr.width, tr.height)) {
		return qfalse;
	}
	
	// Create render targets
	if (!D3D12_CreateRenderTargets()) {
		return qfalse;
	}
	
	// Create root signature
	if (!D3D12_CreateRootSignature()) {
		return qfalse;
	}
	
	// Create pipeline state
	if (!D3D12_CreatePipelineState()) {
		return qfalse;
	}
	
	// Initialize ray tracing if supported
	if (d3d12.rayTracingSupported) {
		if (!D3D12_CreateRayTracingPipelineState()) {
			Com_Printf("D3D12: Warning - Ray tracing pipeline creation failed\n");
		}
		if (!D3D12_CreateAccelerationStructures()) {
			Com_Printf("D3D12: Warning - Acceleration structure creation failed\n");
		}
		if (!D3D12_CreateShaderBindingTable()) {
			Com_Printf("D3D12: Warning - Shader binding table creation failed\n");
		}
		if (!D3D12_CreateRayTracingOutput(tr.width, tr.height)) {
			Com_Printf("D3D12: Warning - Ray tracing output creation failed\n");
		}
	}
	
	tr.active = qtrue;
	return qtrue;
}

/*
================
D3D12_ShutdownWindow
================
*/
void D3D12_ShutdownWindow(void) {
	if (!tr.active) {
		return;
	}
	
	tr.active = qfalse;
}

/*
================
D3D12_BeginFrame
================
*/
void D3D12_BeginFrame(void) {
	if (!tr.active) {
		return;
	}
	
	// Reset command allocator
	d3d12.commandAllocator->Reset();
	
	// Reset command list
	d3d12.commandList->Reset(d3d12.commandAllocator.Get(), d3d12.pipelineState.Get());
	
	// Set render target
	D3D12_CPU_DESCRIPTOR_HANDLE rtvHandle = d3d12.rtvHeap->GetCPUDescriptorHandleForHeapStart();
	rtvHandle.ptr += d3d12.frameIndex * d3d12.rtvDescriptorSize;
	
	D3D12_CPU_DESCRIPTOR_HANDLE dsvHandle = d3d12.dsvHeap->GetCPUDescriptorHandleForHeapStart();
	
	d3d12.commandList->OMSetRenderTargets(1, &rtvHandle, FALSE, &dsvHandle);
	
	// Clear render target
	const float clearColor[] = { 0.0f, 0.0f, 0.0f, 1.0f };
	d3d12.commandList->ClearRenderTargetView(rtvHandle, clearColor, 0, nullptr);
	d3d12.commandList->ClearDepthStencilView(dsvHandle, D3D12_CLEAR_FLAG_DEPTH, 1.0f, 0, 0, nullptr);
	
	// Set viewport and scissor
	d3d12.commandList->RSSetViewports(1, &d3d12.viewport);
	d3d12.commandList->RSSetScissorRects(1, &d3d12.scissorRect);
	
	// Reset statistics
	tr.drawCalls = 0;
	tr.triangles = 0;
	tr.vertices = 0;
}

/*
================
D3D12_EndFrame
================
*/
void D3D12_EndFrame(void) {
	if (!tr.active) {
		return;
	}
	
	// Dispatch ray tracing if enabled
	if (d3d12.rayTracingSupported && d3d12.rayTracingPipelineState) {
		D3D12_UpdateRayTracingOutput(tr.width, tr.height);
		D3D12_DispatchRays(tr.width, tr.height);
	}
	
	// Close command list
	d3d12.commandList->Close();
	
	// Execute command list
	ID3D12CommandList* ppCommandLists[] = { d3d12.commandList.Get() };
	d3d12.commandQueue->ExecuteCommandLists(1, ppCommandLists);
}

/*
================
D3D12_Present
================
*/
void D3D12_Present(void) {
	if (!tr.active) {
		return;
	}
	
	D3D12_Present();
	D3D12_WaitForGPU();
}

/*
================
D3D12_Resize
================
*/
void D3D12_Resize(UINT width, UINT height) {
	if (!tr.active) {
		return;
	}
	
	if (tr.width == width && tr.height == height) {
		return;
	}
	
	D3D12_ResizeSwapChain(width, height);
	tr.width = width;
	tr.height = height;
	
	// Update ray tracing output size
	if (d3d12.rayTracingSupported) {
		D3D12_UpdateRayTracingOutput(width, height);
	}
}

#endif // _WIN32

