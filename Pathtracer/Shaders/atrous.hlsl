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
#define SPATIAL_NEIGHBORS  20
#define SPATIAL_RADIUS     5

// Include and import common Falcor utilities and data structures
import Raytracing;                   // Shared ray tracing specific functions & data
import ShaderCommon;                 // Shared shading data structures
import Shading;                      // Shading functions, etc     
import Lights;                       // Light structures for our current scene

shared cbuffer GlobalCB
{
	uint  gFrameCount;    // Frame counter to act as random seed 
	bool  gEnableDenoise;
	uint  gFilterSize;
	float gColorPhi;
	float gNormalPhi;
	float gPositionPhi;
	uint  gIter;
	uint  gTotalIter;
}

// Input and output textures
shared Texture2D<float4>   gPos;           // G-buffer world-space position
shared Texture2D<float4>   gNorm;          // G-buffer world-space normal
shared Texture2D<float4>   gDiffuseMtl;    // G-buffer diffuse material
shared Texture2D<float4>   gEmissive;
shared RWTexture2D<float4> gShadedOutput;
shared RWTexture2D<float4> gOutput;                // Output to store shaded result
shared RWTexture2D<float4> gDenoiseIn;             // Output to store shaded result
shared RWTexture2D<float4> gDenoiseOut;            // Output to store shaded result
shared RWTexture2D<float4> gDenoisedImage;         // Output to store shaded result

// Environment map
shared Texture2D<float4>   gEnvMap;

static const int2 offsets[25] = { int2(-2, -2), int2(-2, -1), int2(-2, 0), int2(-2, 1), int2(-2, 2),
								  int2(-1, -2), int2(-1, -1), int2(-1, 0), int2(-1, 1), int2(-1, 2),
								  int2(0, -2), int2(0, -1), int2(0, 0), int2(0, 1), int2(0, 2),
								  int2(1, -2), int2(1, -1), int2(1, 0), int2(1, 1), int2(1, 2),
								  int2(2, -2), int2(2, -1), int2(2, 0), int2(2, 1), int2(2, 2) };

static const float kernel[25] = { 1.f / 256.f, 1.f / 64.f, 3.f / 128.f, 1.f / 64.f, 1.f / 256.f,
								  1.f / 64.f, 1.f / 16.f, 3.f / 32.f, 1.f / 16.f, 1 / 64.f,
								  3.f / 128.f, 3.f / 32.f, 9.f / 64.f, 3.f / 32.f, 3.f / 128.f,
								  1.f / 64.f, 1.f / 16.f, 3.f / 32.f, 1.f / 16.f, 1 / 64.f,
								  1.f / 256.f, 1.f / 64.f, 3.f / 128.f, 1.f / 64.f, 1.f / 256.f };

[shader("raygeneration")]
void DenoisingRayGen()
{
	// Get our pixel's position on the screen
	uint2 pixelIndex = DispatchRaysIndex().xy;
	uint2 dim = DispatchRaysDimensions().xy;

	// Read G-buffer data
	float4 worldPos = gPos[pixelIndex];
	float4 worldNorm = gNorm[pixelIndex];

	// Edge weights
	int stepsize = 1 << gIter;

	// Initialize random number generator
	uint randSeed = initRand(pixelIndex.x + dim.x * pixelIndex.y, gFrameCount, 16);

	if (gIter == 0) {
		gDenoiseIn[pixelIndex] = gShadedOutput[pixelIndex];
	}
	else {
		gDenoiseIn[pixelIndex] = gDenoiseOut[pixelIndex];
	}

	float3 sum = float3(0.f, 0.f, 0.f);
	for (int i = 0; i < 25; i++) {
		int2 offset = offsets[i] * stepsize;
		uint2 uv = pixelIndex + offset;

		// Clamp indices to image width and height
		uv.x = clamp(uv.x, 0, dim.x - 1);
		uv.y = clamp(uv.y, 0, dim.y - 1);

		// Apply kernel
		float3 col = gDenoiseIn[uv].xyz;
		sum += col * kernel[i];
	}

	if (gIter == gTotalIter - 1) {
		gOutput[pixelIndex] = float4(sum, 1.f);
	}
	else {
		gDenoiseOut[pixelIndex] = float4(sum, 1.f);
	}
}