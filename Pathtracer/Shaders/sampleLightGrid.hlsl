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

shared cbuffer GlobalCB
{
	float gMinT;          // Avoid ray self-intersection
	uint  gFrameCount;    // Frame counter to act as random seed 
	bool  gDoIndirectLighting;  // Should we shoot indirect rays?
	bool  gDoDirectLighting; // Should we shoot shadow rays?
	uint  gMaxDepth;      // Max recursion depth
	float gEmitMult;      // Multiply emissive channel by this channel
}

// Input and output textures
shared Texture2D<float4>   gPos;           // G-buffer world-space position
shared Texture2D<float4>   gNorm;          // G-buffer world-space normal
shared Texture2D<float4>   gDiffuseMtl;    // G-buffer diffuse material
shared Texture2D<float4>   gEmissive;
shared Texture2D<float4>   gLightGrid;
shared RWTexture2D<float4> gOutput;        // Output to store shaded result

// Environment map
shared Texture2D<float4>   gEnvMap;

struct IndirectRayPayload
{
	float3 color;
	uint randSeed;
	uint rayDepth;
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

float3 shootIndirectRay(float3 rayOrigin, float3 rayDir, float minT, uint seed, uint curDepth)
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
	rayData.rayDepth = curDepth + 1;

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

	return color;
}

float3 lambertianIndirect(inout uint rndSeed, float3 hit, float3 norm, float3 diffuseColor, uint depth)
{
	// Use cosine-weighted hemisphere sampling to choose random direction
	float3 wi = getCosHemisphereSample(rndSeed, norm);
	float3 bounceColor = shootIndirectRay(hit, wi, gMinT, rndSeed, depth);

	// Attenuate color by indirect color
	return bounceColor * diffuseColor;
}

[shader("closesthit")]
void IndirectClosestHit(inout IndirectRayPayload rayData, BuiltInTriangleIntersectionAttributes attribs)
{
	// Extract data from scene description 
	ShadingData shadeData = getHitShadingData(attribs);

	// Add emissive color
	//rayData.color = gEmitMult * shadeData.emissive.rgb;

	// Direct illumination
	if (gDoDirectLighting)
	{
		rayData.color += lambertianDirect(rayData.randSeed, shadeData.posW, shadeData.N, shadeData.diffuse);
	}

	// Indirect illumination
	if (rayData.rayDepth < gMaxDepth)
	{
		rayData.color += lambertianIndirect(rayData.randSeed, shadeData.posW, shadeData.N, shadeData.diffuse, rayData.rayDepth);
	}
}

[shader("raygeneration")]
void SampleLightGridRayGen()
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

	float3 shadeColor = albedo;
	if (worldPos.w != 0)
	{
		/*// Jitter intersection point within size of grid cell
		float3 gridLoadPosition = worldPos + float3(nextRand(randSeed), nextRand(randSeed), nextRand(randSeed)) * 
			2.0f - 1.0f) * gHalfGridCellSize;

		// Find grid cell to sample from
		int3 gridCellsCoords = mapWorldToGrid(gridLoadPosition);
		uint gridCellIndex = coords3DToLinear(gridCellCoords, gLightGridDims);
		uint gridCellStartIndex = gridCellIndex * gGridLightsPerCell;

		Reservoir risReservoir = sampleLightUsingRIS(shadedPoint);
		float lightSampleWeight = (risReservoir.totalWeight / M) / risReservoir.sampleTargetPdf;
		LightSample light = loadLight(risReservoir.lightSample);
		shadeColor = light.intensity * Brdf(light, worldPos) * lightSampleWeight;*/
	}
	
	gOutput[pixelIndex] = float4(shadeColor, 1.f);
}