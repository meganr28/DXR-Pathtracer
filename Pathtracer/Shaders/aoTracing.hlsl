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
#include "hlslUtils.hlsli"

// Include and import common Falcor utilities and data structures
__import Raytracing;                   // Shared ray tracing specific functions & data
__import ShaderCommon;                 // Shared shading data structures
__import Shading;                      // Shading functions, etc.  

// Payload for AO rays
struct AORayPayload
{
	bool aoValue; // 0 if we hit a surface, 1 if we miss all surfaces
};

// Constant buffer filled with data from CPU (background color)
cbuffer RayGenCB
{
	float gAORadius;
	uint  gFrameCount;
	float gMinT;
	uint  gNumRays;
};

// Input and output textures set by the CPU
Texture2D<float4> gPos;
Texture2D<float4> gNorm;
RWTexture2D<float4> gOutput;

float shootAmbientOcclusionRay(float3 origin, float3 direction, float minT, float maxT)
{
	// Setup AO payload
	AORayPayload rayPayload = { 0.f };   // Value returned if we hit a surface
	RayDesc rayAO;                      
	rayAO.Origin = origin;               
	rayAO.Direction = direction;
	rayAO.TMin = minT;
	rayAO.TMax = maxT;

	// Trace ray; ray stops after its first confirmed hitS
	TraceRay(gRtScene,
		RAY_FLAG_ACCEPT_FIRST_HIT_AND_END_SEARCH | RAY_FLAG_SKIP_CLOSEST_HIT_SHADER,
		0XFF, 0, hitProgramCount, 0, rayAO, rayPayload);

	// Copy AO value from payload
	return rayPayload.aoValue;
}

[shader("raygeneration")]
void AORayGen()
{
	// Thread's ray on screen
	float2 pixelIndex = DispatchRaysIndex().xy;
	float2 dim = DispatchRaysDimensions().xy;

	// Initialize random seed per-pixel based on screen position and frame count
	uint randSeed = initRand(pixelIndex.x + pixelIndex.y * dim.x, gFrameCount, 16);

	// Get position and normal from rasterized GBuffer
	float4 worldPos = gPos[pixelIndex];
	float4 worldNorm = gNorm[pixelIndex];

	// Default ambient occlusioin value if we hit background
	float ambientOcclusion = float(gNumRays);

	// If not a background pixel, shoot an AO ray
	if (worldPos.w != 0.f)
	{
		// Accumulate AO from zero
		ambientOcclusion = 0.f;

		for (int i = 0; i < gNumRays; i++)
		{
			// Sample cosine-weighted hemisphere centered around surface normal
			float3 worldDir = getCosHemisphereSample(randSeed, worldNorm.xyz);

			// Shoot AO ray and update output value
			ambientOcclusion += shootAmbientOcclusionRay(worldPos.xyz, worldDir, gMinT, gAORadius);
		}
	}

	// Save AO color
	float aoColor = ambientOcclusion / float(gNumRays);
	gOutput[pixelIndex] = float4(aoColor, aoColor, aoColor, 1.f);
}

[shader("miss")]
void AOMiss(inout AORayPayload rayData)
{
	// If miss, AO value is 1
	rayData.aoValue = 1.0f;
}

[shader("anyhit")]
void AOAnyHit(inout AORayPayload rayData, BuiltInTriangleIntersectionAttributes attribs)
{
	// If we hit a transparent texel, ignore the hit; otherwise, accept
	if (alphaTestFails(attribs)) IgnoreHit();
}
