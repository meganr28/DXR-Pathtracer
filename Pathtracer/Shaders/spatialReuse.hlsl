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
#include "restirUtils.hlsli"
#include "simpleGIUtils.hlsli"
#include "shadowRay.hlsli"

#define PI                 3.14159265f
#define SPATIAL_NEIGHBORS  5

// Include and import common Falcor utilities and data structures
import Raytracing;                   // Shared ray tracing specific functions & data
import ShaderCommon;                 // Shared shading data structures
import Shading;                      // Shading functions, etc     
import Lights;                       // Light structures for our current scene

shared cbuffer GlobalCB
{
	float gMinT;          // Avoid ray self-intersection
	uint  gFrameCount;    // Frame counter to act as random seed 
	uint  gMaxDepth;      // Max recursion depth
	float gEmitMult;      // Multiply emissive channel by this channel

	bool  gEnableReSTIR;  
}

// Input and output textures
shared Texture2D<float4>   gPos;           // G-buffer world-space position
shared Texture2D<float4>   gNorm;          // G-buffer world-space normal
shared Texture2D<float4>   gDiffuseMtl;    // G-buffer diffuse material
shared Texture2D<float4>   gEmissive;
shared RWTexture2D<float4> gCurrReservoirs;        // Output to store shaded result
shared RWTexture2D<float4> gSpatialReservoirs;     // Output to store shaded result

// Environment map
shared Texture2D<float4>   gEnvMap;

float3 lambertianDirect(inout uint rndSeed, float3 hit, float3 norm, float3 diffuseColor)
{
	// Pick a single random light to sample
	int light = min(int(nextRand(rndSeed) * gLightsCount), gLightsCount - 1);

	// Query scene to get information about current light
	float dist;
	float3 lightIntensity;
	float3 lightDirection;

	getLightData(light, hit, lightDirection, lightIntensity, dist);

	// Lambertian dot product
	float cosTheta = saturate(dot(norm, lightDirection));

	// Shoot shadow ray
	float shadow = shadowRayVisibility(hit, lightDirection, gMinT, dist);

	// Compute Lambertian shading color (divide by probability of light = 1.0 / N)
	float3 color = gLightsCount * shadow * cosTheta * lightIntensity;
	color *= diffuseColor / M_PI;
	color /= dist * dist;

	return color;
}

[shader("raygeneration")]
void SpatialReuseRayGen()
{
	// Get our pixel's position on the screen
	uint2 pixelIndex = DispatchRaysIndex().xy;
	uint2 dim = DispatchRaysDimensions().xy;

	// Read G-buffer data
	float4 worldPos = gPos[pixelIndex];
	float4 worldNorm = gNorm[pixelIndex];
	float4 difMatlColor = gDiffuseMtl[pixelIndex];
	float4 emissiveData = gEmissive[pixelIndex];

	float3 albedo = difMatlColor.rgb;

	// Initialize random number generator
	uint randSeed = initRand(pixelIndex.x + dim.x * pixelIndex.y, gFrameCount, 16);

	Reservoir spatial_reservoir = { 0, 0, 0, 0 };
	float3 shadeColor = float3(0.f, 0.f, 0.f);
	if (worldPos.w != 0)
	{
		// To hold information about current light
		float dist;
		float3 lightIntensity;
		float3 lightDirection;

		if (gEnableReSTIR)
		{
			spatial_reservoir = createReservoir(gCurrReservoirs[pixelIndex]);
		}
		else
		{
			shadeColor += lambertianDirect(randSeed, worldPos.xyz, worldNorm.xyz, difMatlColor.rgb);
		}
	}
	else
	{
		shadeColor = albedo;
	}

	gSpatialReservoirs[pixelIndex] = float4(shadeColor, 1.f);

	if (gEnableReSTIR)
	{
		gSpatialReservoirs[pixelIndex] = float4(spatial_reservoir.lightSample, spatial_reservoir.M, spatial_reservoir.weight, spatial_reservoir.totalWeight);
	}
}