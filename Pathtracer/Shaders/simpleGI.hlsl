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
#include "simpleGIUtils.hlsli"
#include "shadowRay.hlsli"

// Include and import common Falcor utilities and data structures
import Raytracing;                   // Shared ray tracing specific functions & data
import ShaderCommon;                 // Shared shading data structures
import Shading;                      // Shading functions, etc     
import Lights;                       // Light structures for our current scene

cbuffer RayGenCB
{
	float gMinT;         // Avoid ray self-intersection
	uint  gFrameCount;   // Frame counter to act as random seed 
	bool  gDoIndirectGI;  // Should we shoot indirect rays?
	bool  gCosSampling; // Choose between cosine-weighted or uniform hemisphere sampling
	bool  gDirectShadows; // Should we shoot shadow rays?
}

// Input and output textures
Texture2D<float4>   gPos;           // G-buffer world-space position
Texture2D<float4>   gNorm;          // G-buffer world-space normal
Texture2D<float4>   gDiffuseMtl;    // G-buffer diffuse material (RGB) and opacity (A)
RWTexture2D<float4> gOutput;        // Output to store shaded result

// Environment map
Texture2D<float4>   gEnvMap;

struct IndirectRayPayload
{
	float3 color;
	uint randSeed;
};

[shader("miss")]
void IndirectMiss(inout IndirectRayPayload rayData)
{
	float2 dim;
	gEnvMap.GetDimensions(dim.x, dim.y);

	// Convert ray direction to (u, v) coordinate
	float2 uv = wsVectorToLatLong(WorldRayDirection());

	// Set environment map color as ray color
	rayData.color = gEnvMap[uint2(uv * dim)].rgb;
}

[shader("anyhit")]
void IndirectAnyHit(inout IndirectRayPayload rayData, BuiltInTriangleIntersectionAttributes attribs)
{
	// If we hit a transparent texel, ignore the hit; otherwise, accept
	if (alphaTestFails(attribs)) IgnoreHit();
}

[shader("closesthit")]
void IndirectClosestHit(inout IndirectRayPayload rayData, BuiltInTriangleIntersectionAttributes attribs)
{
	// Extract data from scene description 
	ShadingData shadeData = getHitShadingData(attribs);

	// Pick a single random light to sample
	int light = min(int(nextRand(rayData.randSeed) * gLightsCount), gLightsCount - 1);

	// Query scene to get information about current light
	float dist;
	float3 lightIntensity;
	float3 lightDirection;

	getLightData(light, shadeData.posW, lightDirection, lightIntensity, dist);

	// Lambertian dot product
	float cosTheta = saturate(dot(shadeData.N, lightDirection));

	// Shoot shadow ray
	float shadow = shadowRayVisibility(shadeData.posW, lightDirection, RayTMin(), dist);

	// Compute Lambertian shading color (divide by probability of light = 1.0 / N)
	rayData.color = gLightsCount * shadow * cosTheta * lightIntensity;
	rayData.color *= shadeData.diffuse / M_PI;
}

float3 shootIndirectRay(float3 rayOrigin, float3 rayDir, float minT, uint seed)
{
	// Setup indirect ray
	RayDesc rayThroughput;
	rayThroughput.Origin = rayOrigin;
	rayThroughput.Direction = rayDir;
	rayThroughput.TMin = minT;
	rayThroughput.TMax = 1e+38f;

	// Initialize payload
	IndirectRayPayload rayData;
	rayData.color = float3(0, 0, 0);
	rayData.randSeed = seed;

	// Trace ray (using hit group and miss shader #1)
	TraceRay(gRtScene,
		0,
		0XFF,
		1,
		hitProgramCount,
		1,
		rayThroughput,
		rayData);

	return rayData.color;
}

[shader("raygeneration")]
void SimpleGIRayGen()
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
		float shadow = 1.f;
		if (gDirectShadows) {
			shadow *= shadowRayVisibility(worldPos.xyz, lightDirection, gMinT, dist);
		}

		// Add direct lighting
		shadeColor += gLightsCount * shadow * cosTheta * lightIntensity * albedo / M_PI;

		if (gDoIndirectGI)
		{
			// Pick random direction for indirect ray
			float3 bounceDir = float3(0.0, 0.0, 0.0);
			if (gCosSampling) {
				bounceDir = getCosHemisphereSample(randSeed, worldNorm.xyz);
			}
			else {
				bounceDir = getUniformHemisphereSample(randSeed, worldNorm.xyz);
			}

			// Get cosTheta for selected direction
			float cosTheta = saturate(dot(worldNorm.xyz, bounceDir));

			// Shoot indirect ray
			float3 bounceColor = shootIndirectRay(worldPos.xyz, bounceDir, gMinT, randSeed);

			// Get probability of selecting this ray
			float sampleProb = gCosSampling ? (cosTheta / M_PI) : (1.f / (2.f * M_PI));

			// Add color
			shadeColor += (cosTheta * bounceColor * albedo / M_PI) / sampleProb;
		}
	}
	else
	{
		shadeColor = albedo;
	}
	
	gOutput[pixelIndex] = float4(shadeColor, 1.f);
}