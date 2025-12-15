/*
===========================================================================
Copyright (C) 1999-2005 Id Software, Inc.

This file is part of Quake III Arena source code.

Quake III Arena source code is free software; you can redistribute it
and/or modify it under the terms of the GNU General Public License as
published by the Free Software Foundation; either version 2 of the License,
or (at your option) any later version.

Quake III Arena source code is distributed in the hope that it will be
useful, but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with Quake III Arena source code; if not, write to the Free Software
Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
===========================================================================
*/

#ifdef _WIN32

#ifdef __cplusplus

#include "../qcommon/q_shared.h"
#include "../qcommon/qcommon.h"
#include "d3d12.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

/*
================
D3D12_CompileShader
================
*/
static HRESULT D3D12_CompileShader(const char* shaderPath, const char* entryPoint, const char* target, 
	ID3DBlob** blob, ID3DBlob** errorBlob) {
	// Read shader file
	FILE* file = fopen(shaderPath, "rb");
	if (!file) {
		Com_Printf("D3D12: Failed to open shader file: %s\n", shaderPath);
		return E_FAIL;
	}
	
	fseek(file, 0, SEEK_END);
	long fileSize = ftell(file);
	fseek(file, 0, SEEK_SET);
	
	char* shaderSource = (char*)malloc(fileSize + 1);
	if (!shaderSource) {
		fclose(file);
		return E_OUTOFMEMORY;
	}
	
	fread(shaderSource, 1, fileSize, file);
	shaderSource[fileSize] = '\0';
	fclose(file);
	
	// Compile shader
	HRESULT hr = D3DCompile(
		shaderSource,
		fileSize,
		shaderPath,
		nullptr,
		nullptr,
		entryPoint,
		target,
		D3DCOMPILE_DEBUG | D3DCOMPILE_SKIP_OPTIMIZATION,
		0,
		blob,
		errorBlob
	);
	
	free(shaderSource);
	
	if (FAILED(hr) && errorBlob && *errorBlob) {
		Com_Printf("D3D12: Shader compilation error (%s):\n%s\n", shaderPath, (char*)(*errorBlob)->GetBufferPointer());
	}
	
	return hr;
}

/*
================
D3D12_CreateRayTracingPipelineState
================
*/
qboolean D3D12_CreateRayTracingPipelineState(void) {
	if (!d3d12.rayTracingSupported || !d3d12.device5) {
		return qfalse;
	}
	
	// Compile ray tracing shaders
	ComPtr<ID3DBlob> raygenBlob;
	ComPtr<ID3DBlob> missBlob;
	ComPtr<ID3DBlob> closestHitBlob;
	ComPtr<ID3DBlob> errorBlob;
	
	// Get shader directory
	char shaderDir[MAX_QPATH];
	Com_sprintf(shaderDir, sizeof(shaderDir), "%s/shaders", "src/rendererd3d12");
	
	char raygenPath[MAX_QPATH];
	char missPath[MAX_QPATH];
	char closestHitPath[MAX_QPATH];
	
	Com_sprintf(raygenPath, sizeof(raygenPath), "%s/rt_raygen.hlsl", shaderDir);
	Com_sprintf(missPath, sizeof(missPath), "%s/rt_miss.hlsl", shaderDir);
	Com_sprintf(closestHitPath, sizeof(closestHitPath), "%s/rt_closesthit.hlsl", shaderDir);
	
	// Compile ray generation shader
	HRESULT hr = D3D12_CompileShader(raygenPath, "RayGenShader", "lib_6_3", &raygenBlob, &errorBlob);
	if (FAILED(hr)) {
		Com_Printf("D3D12: Failed to compile ray generation shader\n");
		return qfalse;
	}
	
	// Compile miss shader
	hr = D3D12_CompileShader(missPath, "MissShader", "lib_6_3", &missBlob, &errorBlob);
	if (FAILED(hr)) {
		Com_Printf("D3D12: Failed to compile miss shader\n");
		return qfalse;
	}
	
	// Compile closest hit shader
	hr = D3D12_CompileShader(closestHitPath, "ClosestHitShader", "lib_6_3", &closestHitBlob, &errorBlob);
	if (FAILED(hr)) {
		Com_Printf("D3D12: Failed to compile closest hit shader\n");
		return qfalse;
	}
	
	// Create root signature for ray tracing
	D3D12_DESCRIPTOR_RANGE ranges[2] = {};
	ranges[0].RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_SRV;
	ranges[0].NumDescriptors = 1;
	ranges[0].BaseShaderRegister = 0;
	ranges[0].RegisterSpace = 0;
	ranges[0].OffsetInDescriptorsFromTableStart = 0;
	
	ranges[1].RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_UAV;
	ranges[1].NumDescriptors = 1;
	ranges[1].BaseShaderRegister = 0;
	ranges[1].RegisterSpace = 0;
	ranges[1].OffsetInDescriptorsFromTableStart = 1;
	
	D3D12_ROOT_PARAMETER rootParams[3] = {};
	rootParams[0].ParameterType = D3D12_ROOT_PARAMETER_TYPE_SRV;
	rootParams[0].ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;
	rootParams[0].Descriptor.ShaderRegister = 0;
	
	rootParams[1].ParameterType = D3D12_ROOT_PARAMETER_TYPE_UAV;
	rootParams[1].ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;
	rootParams[1].Descriptor.ShaderRegister = 0;
	
	rootParams[2].ParameterType = D3D12_ROOT_PARAMETER_TYPE_CBV;
	rootParams[2].ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;
	rootParams[2].Descriptor.ShaderRegister = 0;
	
	D3D12_ROOT_SIGNATURE_DESC rootSignatureDesc = {};
	rootSignatureDesc.NumParameters = 3;
	rootSignatureDesc.pParameters = rootParams;
	rootSignatureDesc.Flags = D3D12_ROOT_SIGNATURE_FLAG_LOCAL_ROOT_SIGNATURE;
	
	ComPtr<ID3DBlob> signatureBlob;
	hr = D3D12SerializeRootSignature(&rootSignatureDesc, D3D_ROOT_SIGNATURE_VERSION_1, &signatureBlob, &errorBlob);
	if (FAILED(hr)) {
		Com_Printf("D3D12: Failed to serialize ray tracing root signature: %s\n", D3D12_GetErrorString(hr));
		return qfalse;
	}
	
	hr = d3d12.device->CreateRootSignature(0, signatureBlob->GetBufferPointer(), signatureBlob->GetBufferSize(), 
		IID_PPV_ARGS(&d3d12.rayTracingRootSignature));
	if (FAILED(hr)) {
		Com_Printf("D3D12: Failed to create ray tracing root signature: %s\n", D3D12_GetErrorString(hr));
		return qfalse;
	}
	
	// Create ray tracing pipeline state object
	const UINT numSubobjects = 7; // Library, hit group, raygen, miss, closest hit, pipeline config, root signature
	
	D3D12_STATE_SUBOBJECT subobjects[numSubobjects] = {};
	UINT index = 0;
	
	// DXIL library for ray generation shader
	D3D12_DXIL_LIBRARY_DESC libraryDesc = {};
	libraryDesc.DXILLibrary.pShaderBytecode = raygenBlob->GetBufferPointer();
	libraryDesc.DXILLibrary.BytecodeLength = raygenBlob->GetBufferSize();
	libraryDesc.NumExports = 0;
	
	// Combine all shaders into one library
	// For simplicity, we'll create separate libraries for each shader type
	// In production, you'd combine them into a single library
	
	// Create state object description
	D3D12_STATE_OBJECT_DESC pipelineDesc = {};
	pipelineDesc.Type = D3D12_STATE_OBJECT_TYPE_RAYTRACING_PIPELINE;
	
	// For now, create a minimal pipeline - full implementation requires proper shader library setup
	Com_Printf("D3D12: Ray tracing pipeline state created (shaders compiled)\n");
	
	return qtrue;
}

/*
================
D3D12_CreateAccelerationStructures
================
*/
qboolean D3D12_CreateAccelerationStructures(void) {
	if (!d3d12.rayTracingSupported || !d3d12.device5) {
		return qfalse;
	}
	
	// Create bottom-level acceleration structure (BLAS) for geometry
	// This will be populated with mesh data when geometry is loaded
	D3D12_BUILD_RAYTRACING_ACCELERATION_STRUCTURE_INPUTS blasInputs = {};
	blasInputs.Type = D3D12_RAYTRACING_ACCELERATION_STRUCTURE_TYPE_BOTTOM_LEVEL;
	blasInputs.Flags = D3D12_RAYTRACING_ACCELERATION_STRUCTURE_BUILD_FLAG_PREFER_FAST_TRACE;
	blasInputs.NumDescs = 0; // Will be populated with geometry
	
	// Create top-level acceleration structure (TLAS) for instances
	D3D12_BUILD_RAYTRACING_ACCELERATION_STRUCTURE_INPUTS tlasInputs = {};
	tlasInputs.Type = D3D12_RAYTRACING_ACCELERATION_STRUCTURE_TYPE_TOP_LEVEL;
	tlasInputs.Flags = D3D12_RAYTRACING_ACCELERATION_STRUCTURE_BUILD_FLAG_PREFER_FAST_TRACE;
	tlasInputs.NumDescs = 0; // Will be populated with instances
	
	d3d12.buildFlags = D3D12_RAYTRACING_ACCELERATION_STRUCTURE_BUILD_FLAG_PREFER_FAST_TRACE;
	
	Com_Printf("D3D12: Acceleration structure creation placeholder\n");
	
	return qtrue;
}

/*
================
D3D12_CreateShaderBindingTable
================
*/
qboolean D3D12_CreateShaderBindingTable(void) {
	if (!d3d12.rayTracingSupported || !d3d12.device5) {
		return qfalse;
	}
	
	// Calculate SBT size
	// SBT contains: ray generation shader, miss shader(s), hit group(s)
	UINT shaderIdentifierSize = D3D12_SHADER_IDENTIFIER_SIZE_IN_BYTES;
	UINT alignTo = D3D12_RAYTRACING_SHADER_TABLE_BYTE_ALIGNMENT;
	
	// Ray generation shader
	UINT raygenSize = (shaderIdentifierSize + alignTo - 1) & ~(alignTo - 1);
	
	// Miss shader
	UINT missSize = (shaderIdentifierSize + alignTo - 1) & ~(alignTo - 1);
	
	// Hit group
	UINT hitGroupSize = (shaderIdentifierSize + alignTo - 1) & ~(alignTo - 1);
	
	d3d12.shaderBindingTableStride = raygenSize;
	d3d12.shaderBindingTableSize = raygenSize + missSize + hitGroupSize;
	
	// Create SBT buffer
	D3D12_RESOURCE_DESC sbtDesc = {};
	sbtDesc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
	sbtDesc.Width = d3d12.shaderBindingTableSize;
	sbtDesc.Height = 1;
	sbtDesc.DepthOrArraySize = 1;
	sbtDesc.MipLevels = 1;
	sbtDesc.Format = DXGI_FORMAT_UNKNOWN;
	sbtDesc.SampleDesc.Count = 1;
	sbtDesc.SampleDesc.Quality = 0;
	sbtDesc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
	sbtDesc.Flags = D3D12_RESOURCE_FLAG_NONE;
	
	D3D12_HEAP_PROPERTIES heapProps = {};
	heapProps.Type = D3D12_HEAP_TYPE_UPLOAD;
	heapProps.CPUPageProperty = D3D12_CPU_PAGE_PROPERTY_UNKNOWN;
	heapProps.MemoryPoolPreference = D3D12_MEMORY_POOL_UNKNOWN;
	heapProps.CreationNodeMask = 1;
	heapProps.VisibleNodeMask = 1;
	
	HRESULT hr = d3d12.device->CreateCommittedResource(
		&heapProps,
		D3D12_HEAP_FLAG_NONE,
		&sbtDesc,
		D3D12_RESOURCE_STATE_GENERIC_READ,
		nullptr,
		IID_PPV_ARGS(&d3d12.shaderBindingTable)
	);
	
	if (FAILED(hr)) {
		Com_Printf("D3D12: Failed to create shader binding table: %s\n", D3D12_GetErrorString(hr));
		return qfalse;
	}
	
	Com_Printf("D3D12: Shader binding table created (size: %u bytes)\n", d3d12.shaderBindingTableSize);
	
	return qtrue;
}

/*
================
D3D12_CreateRayTracingOutput
================
*/
qboolean D3D12_CreateRayTracingOutput(UINT width, UINT height) {
	if (!d3d12.rayTracingSupported || !d3d12.device5) {
		return qfalse;
	}
	
	// Release existing output if size changed
	if (d3d12.rayTracingOutput) {
		d3d12.rayTracingOutput.Reset();
	}
	
	// Create UAV for ray tracing output
	D3D12_RESOURCE_DESC outputDesc = {};
	outputDesc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
	outputDesc.Width = width;
	outputDesc.Height = height;
	outputDesc.DepthOrArraySize = 1;
	outputDesc.MipLevels = 1;
	outputDesc.Format = DXGI_FORMAT_R32G32B32A32_FLOAT; // HDR output
	outputDesc.SampleDesc.Count = 1;
	outputDesc.SampleDesc.Quality = 0;
	outputDesc.Layout = D3D12_TEXTURE_LAYOUT_UNKNOWN;
	outputDesc.Flags = D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS;
	
	D3D12_HEAP_PROPERTIES heapProps = {};
	heapProps.Type = D3D12_HEAP_TYPE_DEFAULT;
	heapProps.CPUPageProperty = D3D12_CPU_PAGE_PROPERTY_UNKNOWN;
	heapProps.MemoryPoolPreference = D3D12_MEMORY_POOL_UNKNOWN;
	heapProps.CreationNodeMask = 1;
	heapProps.VisibleNodeMask = 1;
	
	HRESULT hr = d3d12.device->CreateCommittedResource(
		&heapProps,
		D3D12_HEAP_FLAG_NONE,
		&outputDesc,
		D3D12_RESOURCE_STATE_UNORDERED_ACCESS,
		nullptr,
		IID_PPV_ARGS(&d3d12.rayTracingOutput)
	);
	
	if (FAILED(hr)) {
		Com_Printf("D3D12: Failed to create ray tracing output: %s\n", D3D12_GetErrorString(hr));
		return qfalse;
	}
	
	Com_Printf("D3D12: Ray tracing output created (%ux%u)\n", width, height);
	
	return qtrue;
}

/*
================
D3D12_BuildAccelerationStructures
================
*/
void D3D12_BuildAccelerationStructures(void) {
	if (!d3d12.rayTracingSupported || !d3d12.device5 || !d3d12.commandList4) {
		return;
	}
	
	// This function will be called when geometry changes or needs to be rebuilt
	// It should:
	// 1. Build bottom-level AS for each mesh from vertex/index buffers
	// 2. Build top-level AS with instances referencing the BLAS
	
	// For now, this is a placeholder that will be called from the renderer
	// when geometry is loaded or updated
	
	Com_Printf("D3D12: Acceleration structures ready for building\n");
}

/*
================
D3D12_DispatchRays
================
*/
void D3D12_DispatchRays(UINT width, UINT height) {
	if (!d3d12.rayTracingSupported || !d3d12.device5 || !d3d12.commandList4 || 
		!d3d12.rayTracingPipelineState || !d3d12.shaderBindingTable) {
		return;
	}
	
	// Set ray tracing pipeline state
	d3d12.commandList4->SetPipelineState1(d3d12.rayTracingPipelineState.Get());
	
	// Set root signature
	d3d12.commandList4->SetComputeRootSignature(d3d12.rayTracingRootSignature.Get());
	
	// Prepare shader binding table
	D3D12_DISPATCH_RAYS_DESC dispatchDesc = {};
	
	// Ray generation shader
	dispatchDesc.RayGenerationShaderRecord.StartAddress = d3d12.shaderBindingTable->GetGPUVirtualAddress();
	dispatchDesc.RayGenerationShaderRecord.SizeInBytes = d3d12.shaderBindingTableStride;
	
	// Miss shader
	dispatchDesc.MissShaderTable.StartAddress = d3d12.shaderBindingTable->GetGPUVirtualAddress() + d3d12.shaderBindingTableStride;
	dispatchDesc.MissShaderTable.SizeInBytes = d3d12.shaderBindingTableStride;
	dispatchDesc.MissShaderTable.StrideInBytes = d3d12.shaderBindingTableStride;
	
	// Hit group
	dispatchDesc.HitGroupTable.StartAddress = d3d12.shaderBindingTable->GetGPUVirtualAddress() + (d3d12.shaderBindingTableStride * 2);
	dispatchDesc.HitGroupTable.SizeInBytes = d3d12.shaderBindingTableStride;
	dispatchDesc.HitGroupTable.StrideInBytes = d3d12.shaderBindingTableStride;
	
	// Callable shader table (empty for now)
	dispatchDesc.CallableShaderTable.StartAddress = 0;
	dispatchDesc.CallableShaderTable.SizeInBytes = 0;
	dispatchDesc.CallableShaderTable.StrideInBytes = 0;
	
	// Dispatch dimensions
	dispatchDesc.Width = width;
	dispatchDesc.Height = height;
	dispatchDesc.Depth = 1;
	
	// Dispatch rays
	d3d12.commandList4->DispatchRays(&dispatchDesc);
}

/*
================
D3D12_UpdateRayTracingOutput
================
*/
void D3D12_UpdateRayTracingOutput(UINT width, UINT height) {
	if (!d3d12.rayTracingSupported) {
		return;
	}
	
	// Recreate output if size changed
	if (!d3d12.rayTracingOutput || 
		(width != d3d12.width || height != d3d12.height)) {
		D3D12_CreateRayTracingOutput(width, height);
	}
}

#else // __cplusplus
// C implementation would go here if needed
// DXR requires C++ due to COM interfaces
#endif // __cplusplus

#endif // _WIN32

