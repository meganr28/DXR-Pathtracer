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

#include "HostDeviceSharedMacros.h"

// Utilities for random numbers and alpha testing
#include "thinLensUtils.hlsli"

#define PI 3.14159265f

// Include and import common Falcor utilities and data structures
import Raytracing;                   // Shared ray tracing specific functions & data
import ShaderCommon;                 // Shared shading data structures
import Shading;                      // Shading functions, etc  

struct SimpleRayPayload
{
	bool dummyValue;
};

cbuffer RayGenCB
{
	float  gLensRadius;
	float  gFocalLen;
	uint   gFrameCount;
	float2 gPixelJitter;
}

// How do we generate the rays that we trace?
[shader("raygeneration")]
void GBufferRayGen()
{
	// Get our pixel's position on the screen
	uint2 pixelIndex = DispatchRaysIndex().xy;
	uint2 dim        = DispatchRaysDimensions().xy;

	// Convert our ray index into a ray direction in world space.  
	float2 pixelCenter = (pixelIndex + gPixelJitter) / dim;
	float2 ndc = float2(2, -2) * pixelCenter + float2(-1, 1);                    
	float3 rayDirection = ndc.x * gCamera.cameraU + ndc.y * gCamera.cameraV + gCamera.cameraW;

	// Find the focal point for this pixel
	rayDirection /= length(gCamera.cameraW); // normalize ray along w-axis
	float3 focalPoint = gCamera.posW + gFocalLen * rayDirection;

	// Initialize random number generator
	uint randSeed = initRand(pixelIndex.x + pixelIndex.y * dim.x, gFrameCount, 16);

	// Get random numbers in polar coordinates (random sample point on camera lens)
	float2 rand = float2(2.f * PI * nextRand(randSeed), gLensRadius * nextRand(randSeed));
	float2 uv = float2(cos(rand.x) * rand.y, sin(rand.x) * rand.y);

	// Compute random origin on lens
	float3 lensSample = gCamera.posW + uv.x * normalize(gCamera.cameraU) + uv.y * normalize(gCamera.cameraV);

	// Ray structure
	RayDesc ray;
	ray.Origin = lensSample;
	ray.Direction = normalize(focalPoint - lensSample);
	ray.TMin = 0.f;
	ray.TMax = 1e+38f;

	// Initialize payload
	SimpleRayPayload rayData = { false };

	// Trace ray
	TraceRay(gRtScene, 
		RAY_FLAG_CULL_BACK_FACING_TRIANGLES, 
		0XFF, 
		0, 
		hitProgramCount, 
		0, 
		ray, 
		rayData);
}


////////////////////////////////////////////////////////////////////////////////////////
// Same as Raytraced G-Buffer code
////////////////////////////////////////////////////////////////////////////////////////


// G-Buffer results
RWTexture2D<float4> gWsPos;
RWTexture2D<float4> gWsNorm;
RWTexture2D<float4> gMatDif;
RWTexture2D<float4> gMatSpec;
RWTexture2D<float4> gMatExtra;

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
	VertexOut  vsOut = getVertexAttributes(PrimitiveIndex(), attribs);             // Get geometrical data
	ShadingData shadeData = prepareShadingData(vsOut, gMaterial, gCamera.posW, 0); // Get shading data

	// Save out our G-Buffer values to the specified output textures
	gWsPos[pixelIndex] = float4(shadeData.posW, 1.f);
	gWsNorm[pixelIndex] = float4(shadeData.N, length(shadeData.posW - gCamera.posW));
	gMatDif[pixelIndex] = float4(shadeData.diffuse, shadeData.opacity);
	gMatSpec[pixelIndex] = float4(shadeData.specular, shadeData.linearRoughness);
	gMatExtra[pixelIndex] = float4(shadeData.IoR, shadeData.doubleSidedMaterial ? 1.f : 0.f, 0.f, 0.f);
}