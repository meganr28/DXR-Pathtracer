/**********************************************************************************************************************
# Copyright (c) 2018, NVIDIA CORPORATION. All rights reserved.
#
# Redistribution and use in source and binary forms, with or without modification, are permitted provided that the
# following conditions are met:
#  * Redistributions of code must retain the copyright notice, this list of conditions and the following disclaimer.
#  * Neither the name of NVIDIA CORPORATION nor the names of its contributors may be used to endorse or promote products
#    derived from this software without specific prior written permission.
#
# THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS ``AS IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
# LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.  IN NO EVENT
# SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
# CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA,
# OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
# LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF
# ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
**********************************************************************************************************************/

// Falcor host/device management and communication
#include "HostDeviceSharedMacros.h"

// Helper function to determine if the geometry uses alpha testing
#include "alphaTest.hlsli"

// Include and import common Falcor utilities and data structures
__import Raytracing;                   // Shared ray tracing specific functions & data
__import ShaderCommon;                 // Shared shading data structures
__import Shading;                      // Shading functions, etc.  

// Output textures where we store GBuffer results
RWTexture2D<float4> gWsPos;
RWTexture2D<float4> gWsNorm;
RWTexture2D<float4> gMatDif;
RWTexture2D<float4> gMatSpec;
RWTexture2D<float4> gMatExtra;
RWTexture2D<float4> gMatEmissive;

// Simple ray payload structure; not currently used by this shader
struct SimpleRayPayload
{
	bool dummyValue;
};


[shader("raygeneration")]
void GBufferRayGen()
{
	// Convert pixel (x, y) into world-space ray direction
	float2 pixelLocation = DispatchRaysIndex().xy + float2(0.5f, 0.5f);
	float2 pixelCenter = pixelLocation / DispatchRaysDimensions().xy;
	float2 ndc = float2(2, -2) * pixelCenter + float2(-1, 1);
	float3 rayDirection = (ndc.x * gCamera.cameraU +
						   ndc.y * gCamera.cameraV +
								   gCamera.cameraW);

	// Initialize the ray
	// Parameters are (origin, minDistForValidHit, direction, maxDistForValidHit)
	RayDesc ray = { gCamera.posW, 0.0f, rayDirection, 1e+38f };

	// Initialize ray payload (per-ray structure)
	// Allows you to stash temporary data during ray tracing
	SimpleRayPayload payload = { false };

	// Trace ray
	// Parameters are (acceleration structure, flags, instance mask, hit groups, # hit groups, miss shader, ray, payload)
	TraceRay(gRtScene, RAY_FLAG_NONE, 0XFF, 0, 1, 0, ray, payload);
}

// Constant buffer filled with data from CPU (background color)
cbuffer MissShaderCB
{
	float3  gBgColor;
};

[shader("miss")]
void PrimaryMiss(inout SimpleRayPayload)
{
	// If miss, store background color in diffuse texture
	gMatDif[DispatchRaysIndex().xy] = float4(gBgColor, 1.0f);
}

[shader("anyhit")]
void PrimaryAnyHit(inout SimpleRayPayload, BuiltInTriangleIntersectionAttributes attribs)
{
	// If we hit a transparent texel, ignore the hit; otherwise, accept
	if (alphaTestFails(attribs)) IgnoreHit();
}

[shader("closesthit")]
void PrimaryClosestHit(inout SimpleRayPayload, BuiltInTriangleIntersectionAttributes attribs)
{
	// Onscreen pixel this ray belongs to
	uint2 pixelIndex = DispatchRaysIndex().xy;

	// Use Falcor utility function to get shading data at intersection
	ShadingData shadeData = getShadingData(PrimitiveIndex(), attribs);

	// Save out our G-Buffer values to the specified output textures
	gWsPos[pixelIndex] = float4(shadeData.posW, 1.f);
	gWsNorm[pixelIndex] = float4(shadeData.N, length(shadeData.posW - gCamera.posW));
	gMatDif[pixelIndex] = float4(shadeData.diffuse, shadeData.opacity);
	gMatSpec[pixelIndex] = float4(shadeData.specular, shadeData.linearRoughness);
	gMatExtra[pixelIndex] = float4(shadeData.IoR, shadeData.doubleSidedMaterial ? 1.f : 0.f, 0.f, 0.f);
	gMatEmissive[pixelIndex] = float4(shadeData.emissive, 0.f);
}
