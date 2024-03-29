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
#include "HostDeviceData.h"
#include "diffuseOneShadowUtils.hlsli"
#include "shadowRay.hlsli"

#define PI 3.14159265f

// Include and import common Falcor utilities and data structures
import Raytracing;                   // Shared ray tracing specific functions & data
import ShaderCommon;                 // Shared shading data structures
import Shading;                      // Shading functions, etc     
import Lights;                       // Light structures for our current scene

cbuffer RayGenCB
{
	float gMinT;       // Avoid ray self-intersection
	uint  gFrameCount; // Frame counter to act as random seed 
}

// Input and output textures
Texture2D<float4>   gPos;           // G-buffer world-space position
Texture2D<float4>   gNorm;          // G-buffer world-space normal
Texture2D<float4>   gDiffuseMtl;    // G-buffer diffuse material (RGB) and opacity (A)
RWTexture2D<float4> gOutput;        // Output to store shaded result

[shader("raygeneration")]
void DiffuseShadowRayGen()
{
	// Get our pixel's position on the screen
	uint2 pixelIndex = DispatchRaysIndex().xy;
	uint2 dim = DispatchRaysDimensions().xy;

	// Read G-buffer data
	float4 worldPos = gPos[pixelIndex];
	float4 worldNorm = gNorm[pixelIndex];
	float4 difMatlColor = gDiffuseMtl[pixelIndex];
	
	float3 albedo = difMatlColor.rgb;

	// Initialize random number generator
	uint randSeed = initRand(pixelIndex.x + dim.x * pixelIndex.y, gFrameCount, 16);

	float3 shadeColor = float3(0.f, 0.f, 0.f);
	if (worldPos.w != 0)
	{
		// Pick a single random light to sample
		int light = min(int(nextRand(randSeed) * gLightsCount), gLightsCount - 1);

		// Query scene to get information about current light
		float dist;
		float3 lightIntensity;
		float3 lightDirection;

		getLightData(light, worldPos.xyz, lightDirection, lightIntensity, dist);

		// Lambertian dot product
		float cosTheta = saturate(dot(worldNorm.xyz, lightDirection));

		// Shoot shadow ray
		float shadow = shadowRayVisibility(worldPos.xyz, lightDirection, gMinT, dist);

		// Compute Lambertian shading color (divide by probability of light = 1.0 / N)
		shadeColor = gLightsCount * shadow * cosTheta * lightIntensity;
		shadeColor *= albedo / PI;
	}
	else
	{
		shadeColor = albedo;
	}
	
	gOutput[pixelIndex] = float4(shadeColor, 1.f);
}