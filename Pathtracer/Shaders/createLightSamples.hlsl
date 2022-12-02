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

#define PI 3.14159265f

// Include and import common Falcor utilities and data structures
import Raytracing;                   // Shared ray tracing specific functions & data
import ShaderCommon;                 // Shared shading data structures
import Shading;                      // Shading functions, etc     
import Lights;                       // Light structures for our current scene

shared cbuffer GlobalCB
{
	float4x4 gLastCameraMatrix;
	float gMinT;          // Avoid ray self-intersection
	uint  gFrameCount;    // Frame counter to act as random seed 
	uint  gMaxDepth;      // Max recursion depth
	uint  gLightSamples;  // M candidate samples
	float gEmitMult;      // Multiply emissive channel by this channel

	bool  gDoIndirectLighting;  // Should we shoot indirect rays?
	bool  gDoDirectLighting; // Should we shoot shadow rays?
	bool  gEnableReSTIR;
	bool  gDoVisibilityReuse;
	bool  gDoTemporalReuse;
}

// Input and output textures
shared Texture2D<float4>   gPos;           // G-buffer world-space position
shared Texture2D<float4>   gNorm;          // G-buffer world-space normal
shared Texture2D<float4>   gDiffuseMtl;    // G-buffer diffuse material
shared Texture2D<float4>   gEmissive;
shared RWTexture2D<float4> gCurrReservoirs;        // Output to store shaded result
shared RWTexture2D<float4> gPrevReservoirs;        // Output to store shaded result

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
	color /= dist * dist;

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
void CreateLightSamplesRayGen()
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

	Reservoir reservoir = { 0, 0, 0, 0 };
	float3 shadeColor = float3(0.f, 0.f, 0.f);
	if (worldPos.w != 0)
	{
		// To hold information about current light
		float dist;
		float3 lightIntensity;
		float3 lightDirection;

		if (gEnableReSTIR)
		{
			float cosTheta = 0.f;
			float p_hat = 0.f;

			// Get previous reservoir
			Reservoir prev_reservoir = { 0, 0, 0, 0 };

			if (gFrameCount > 0)
			{
				float4 prevScreenPos = mul(worldPos, gLastCameraMatrix);
				prevScreenPos / prevScreenPos.w;

				uint2 prevIndex = pixelIndex;
				/*prevIndex.x = ((prevScreenPos.x + 1.f) * 0.5f) * (float)dim.x;
				prevIndex.y = ((1.f - prevScreenPos.y) * 0.5f) * (float)dim.y;*/

				if (prevIndex.x >= 0 && prevIndex.x < dim.x && prevIndex.y >= 0 && prevIndex.y < dim.y) {
					prev_reservoir = createReservoir(gPrevReservoirs[prevIndex]);
				}
			}

			// Generate initial candidate light samples
			for (int i = 0; i < min(gLightsCount, 32); i++) {
				// Randomly pick a light to sample
				int light = min(int(nextRand(randSeed) * gLightsCount), gLightsCount - 1);
				getLightData(light, worldPos.xyz, lightDirection, lightIntensity, dist);

				// Calcuate light weight based on BRDF and PDF
				float p = 1.f / float(gLightsCount);
				cosTheta = saturate(dot(worldNorm.xyz, lightDirection));

				float3 f = difMatlColor.rgb / M_PI;
				float3 Le = lightIntensity;
				float G = cosTheta / (dist * dist);
				float3 brdf = f * Le * G;
				p_hat = length(brdf) / p;
				updateReservoir(reservoir, p_hat, float(light), randSeed);
			}

			// Calculate p_hat for reservoir's light
			getLightData(reservoir.lightSample, worldPos.xyz, lightDirection, lightIntensity, dist);
			cosTheta = saturate(dot(worldNorm.xyz, lightDirection));
			p_hat = length((difMatlColor.rgb / M_PI) * lightIntensity * cosTheta / (dist * dist));

			if (p_hat == 0.f) {
				reservoir.weight = 0.f;
			}
			else {
				reservoir.weight = (1.f / p_hat) * (reservoir.totalWeight / reservoir.M);
			}

			// Evaluate visibility for initial candidates
			float shadowed = shadowRayVisibility(worldPos.xyz, lightDirection, gMinT, dist);
			if (shadowed <= 0.001f) {
				reservoir.weight = 0.f;
			}

			// TODO: make combining reservoirs into its own function
			// tempReservoir = combineReservoirs(reservoir, prev_reservoir, randSeed);
			if (gDoTemporalReuse)
			{
				// Temporal reuse
				Reservoir tempReservoir = { 0, 0, 0, 0 };

				// Add current reservoir
				updateReservoir(tempReservoir, p_hat * reservoir.weight * reservoir.M, reservoir.lightSample, randSeed);

				// Add previous reservoir
				getLightData(prev_reservoir.lightSample, worldPos.xyz, lightDirection, lightIntensity, dist);
				cosTheta = saturate(dot(worldNorm.xyz, lightDirection));
				p_hat = length((difMatlColor.rgb / M_PI) * lightIntensity * cosTheta / (dist * dist));
				prev_reservoir.M = min(20.f * reservoir.M, prev_reservoir.M);
				updateReservoir(tempReservoir, p_hat * prev_reservoir.weight * prev_reservoir.M, prev_reservoir.lightSample, randSeed);

				// Update M
				tempReservoir.M = reservoir.M + prev_reservoir.M;

				// Set weight
				getLightData(tempReservoir.lightSample, worldPos.xyz, lightDirection, lightIntensity, dist);
				cosTheta = saturate(dot(worldNorm.xyz, lightDirection));
				p_hat = length((difMatlColor.rgb / M_PI) * lightIntensity * cosTheta / (dist * dist));

				if (p_hat == 0.f) {
					tempReservoir.weight = 0.f;
				}
				else {
					tempReservoir.weight = (1.f / p_hat) * (tempReservoir.totalWeight / tempReservoir.M);
				}
				reservoir = tempReservoir;
			}				
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

	gCurrReservoirs[pixelIndex] = float4(shadeColor, 1.f);

	if (gEnableReSTIR)
	{
		gCurrReservoirs[pixelIndex] = float4(reservoir.lightSample, reservoir.M, reservoir.weight, reservoir.totalWeight);
	}
}