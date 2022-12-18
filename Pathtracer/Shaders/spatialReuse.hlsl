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
	uint  gSpatialNeighbors;
	uint  gSpatialRadius;
	uint  gIter;
	uint  gTotalIter;

	bool  gEnableReSTIR;  
	bool  gDoSpatialReuse;
}

// Input and output textures
shared Texture2D<float4>   gPos;           // G-buffer world-space position
shared Texture2D<float4>   gNorm;          // G-buffer world-space normal
shared Texture2D<float4>   gDiffuseMtl;    // G-buffer diffuse material
shared Texture2D<float4>   gEmissive;
shared RWTexture2D<float4> gCurrReservoirs;        // Output to store shaded result
shared RWTexture2D<float4> gSpatialReservoirs;     // Output to store shaded result
shared RWTexture2D<float4> gSpatialReservoirsIn;     // Output to store shaded result
shared RWTexture2D<float4> gSpatialReservoirsOut;     // Output to store shaded result


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

	Reservoir reservoir = createReservoir(gCurrReservoirs[pixelIndex]);
	if (gIter != 0) {
		reservoir = createReservoir(gSpatialReservoirsOut[pixelIndex]);
	}

	Reservoir spatialReservoir = { 0, 0, 0, 0 };

	float3 shadeColor = float3(0.f, 0.f, 0.f);
	if (worldPos.w != 0)
	{
		// To hold information about current light
		float dist;
		float3 lightIntensity;
		float3 lightDirection;

		if (gEnableReSTIR && gDoSpatialReuse)
		{
			// Combine current reservoir
			getLightData(reservoir.lightSample, worldPos.xyz, lightDirection, lightIntensity, dist);
			float cosTheta = saturate(dot(worldNorm.xyz, lightDirection));
			float p_hat = length((difMatlColor.rgb / M_PI) * lightIntensity * cosTheta / (dist * dist));
			updateReservoir(spatialReservoir, p_hat * reservoir.weight * reservoir.M, reservoir.lightSample, randSeed);

			float lightCount = reservoir.M;
			for (int i = 0; i < gSpatialNeighbors; ++i)
			{
				float radius = gSpatialRadius * nextRand(randSeed);
				float angle = 2.0f * M_PI * nextRand(randSeed);

				// Calculate neighbor pixel
				float2 neighborIndex = pixelIndex;
				//uint2 neighborOffset = uint2(0, 0);

				neighborIndex.x += radius * cos(angle);
				neighborIndex.y += radius * sin(angle);

				// Calculate neighbor offset -> [0, 1] -> [0, 2 * NEIGHBOR_RADIUS] -> [-NEIGHBOR_RADIUS, NEIGHBOR_RADIUS]
				//neighborOffset.x = int(nextRand(randSeed) * 2 * SPATIAL_RADIUS) - SPATIAL_RADIUS;
				//neighborOffset.y = int(nextRand(randSeed) * 2 * SPATIAL_RADIUS) - SPATIAL_RADIUS;
				//neighborIndex += neighborOffset;

				// Clamp index
				//neighborIndex.x = max(0, min(neighborIndex.x + neighborOffset.x, dim.x - 1));
				//neighborIndex.y = max(0, min(neighborIndex.y + neighborOffset.y, dim.y - 1));
				uint2 u_neighborIndex = uint2(neighborIndex);
				u_neighborIndex.x = max(0, min(u_neighborIndex.x, dim.x - 1));
				u_neighborIndex.y = max(0, min(u_neighborIndex.y, dim.y - 1));
				Reservoir neighborReservoir = createReservoir(gCurrReservoirs[u_neighborIndex]);
				if (gIter != 0) {
					neighborReservoir = createReservoir(gSpatialReservoirsOut[u_neighborIndex]);
				}

				float4 neighborNorm = gNorm[u_neighborIndex];

				// Check that the angle between the normals are within 25 degrees
				if ((dot(worldNorm.xyz, neighborNorm.xyz)) < 0.906) continue;

				// Check if neighbor exceeds 10% of current pixel's depth
				if (neighborNorm.w > 1.1f * worldNorm.w || neighborNorm.w < 0.9f * worldNorm.w) continue;

				// Combine neighbor's reservoir
				getLightData(neighborReservoir.lightSample, worldPos.xyz, lightDirection, lightIntensity, dist);
				cosTheta = saturate(dot(worldNorm.xyz, lightDirection));
				p_hat = length((difMatlColor.rgb / M_PI) * lightIntensity * cosTheta / (dist * dist));
				updateReservoir(spatialReservoir, p_hat * neighborReservoir.weight * neighborReservoir.M, neighborReservoir.lightSample, randSeed);
			
				lightCount += neighborReservoir.M;
			}

			// Update M
			spatialReservoir.M = lightCount;

			// Update weight
			getLightData(spatialReservoir.lightSample, worldPos.xyz, lightDirection, lightIntensity, dist);
			cosTheta = saturate(dot(worldNorm.xyz, lightDirection));
			p_hat = length((difMatlColor.rgb / M_PI) * lightIntensity * cosTheta / (dist * dist));

			if (p_hat == 0.f) {
				spatialReservoir.weight = 0.f;
			}
			else {
				spatialReservoir.weight = (1.f / max(p_hat, 0.0001f)) * (spatialReservoir.totalWeight / max(spatialReservoir.M, 0.0001f));
			}

			//spatialReservoir = createReservoir(gCurrReservoirs[pixelIndex]);

			// Evaluate visibility for initial candidates
			float shadowed = shadowRayVisibility(worldPos.xyz, lightDirection, gMinT, dist);
			if (shadowed <= 0.001f) {
				spatialReservoir.weight = 0.f;
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

	gSpatialReservoirs[pixelIndex] = float4(shadeColor, 1.f);

	if (gEnableReSTIR)
	{
		if (gDoSpatialReuse)
		{
			if (gIter == gTotalIter - 1) {
				gSpatialReservoirs[pixelIndex] = float4(spatialReservoir.lightSample, spatialReservoir.M, spatialReservoir.weight, spatialReservoir.totalWeight);
			}
			else {
				gSpatialReservoirsOut[pixelIndex] = float4(spatialReservoir.lightSample, spatialReservoir.M, spatialReservoir.weight, spatialReservoir.totalWeight);
			}
		}
		else
		{
			gSpatialReservoirs[pixelIndex] = gCurrReservoirs[pixelIndex];
		}
	}
}