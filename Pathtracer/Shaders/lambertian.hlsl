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
#include "lambertianUtils.hlsli"

#define PI 3.14159265f

// Include and import common Falcor utilities and data structures
import Raytracing;                   // Shared ray tracing specific functions & data
import ShaderCommon;                 // Shared shading data structures
import Shading;                      // Shading functions, etc     
import Lights;                       // Light structures for our current scene

struct ShadowRayPayload
{
	float visFactor; // 1.0 = fully lit, 0.0 = fully shadow
};

cbuffer RayGenCB
{
	float gMinT;     // avoid ray self-intersection
}

// Input and output textures
Texture2D<float4>   gPos;           // G-buffer world-space position
Texture2D<float4>   gNorm;          // G-buffer world-space normal
Texture2D<float4>   gDiffuseMtl;    // G-buffer diffuse material (RGB) and opacity (A)
RWTexture2D<float4> gOutput;        // Output to store shaded result

float shadowRayVisibility( float3 origin, float3 direction, float minT, float maxT )
{
	// Setup shadow ray
	RayDesc ray;
	ray.Origin = origin;
	ray.Direction = direction;
	ray.TMin = minT;
	ray.TMax = maxT;

	ShadowRayPayload payload = { 0.0f };

	// Check for intersections (we care about any intersection, not just the closest intersection)
	TraceRay(gRtScene,
		RAY_FLAG_ACCEPT_FIRST_HIT_AND_END_SEARCH | RAY_FLAG_SKIP_CLOSEST_HIT_SHADER,
		0xFF, 0, hitProgramCount, 0, ray, payload);

	return payload.visFactor;
}

[shader("raygeneration")]
void LambertRayGen()
{
	// Get our pixel's position on the screen
	uint2 pixelIndex = DispatchRaysIndex().xy;
	uint2 dim = DispatchRaysDimensions().xy;

	// Read G-buffer data
	float4 worldPos = gPos[pixelIndex];
	float4 worldNorm = gNorm[pixelIndex];
	float4 difMatlColor = gDiffuseMtl[pixelIndex];
	
	float3 albedo = difMatlColor.rgb;
	float3 shadeColor = float3(0.f, 0.f, 0.f);
	if (worldPos.w != 0)
	{
		// Accumulate contributions from multiple lights
		for (int li = 0; li < gLightsCount; li++)
		{
			float dist;
			float3 lightIntensity;
			float3 lightDirection;

			getLightData(li, worldPos.xyz, lightDirection, lightIntensity, dist);

			// Lambertian dot product
			float cosTheta = saturate(dot(worldNorm.xyz, lightDirection));

			// Shoot shadow ray
			float shadow = shadowRayVisibility(worldPos.xyz, lightDirection, gMinT, dist);

			// Compute Lambertian shading color
			shadeColor += shadow * cosTheta * lightIntensity;
		}
		shadeColor *= albedo / PI;
	}
	else
	{
		shadeColor = albedo;
	}
	
	gOutput[pixelIndex] = float4(shadeColor, 1.f);
}

[shader("miss")]
void ShadowMiss(inout ShadowRayPayload rayData)
{
	// Light is visible
	rayData.visFactor = 1.f;
}

[shader("anyhit")]
void ShadowAnyHit(inout ShadowRayPayload rayData, BuiltInTriangleIntersectionAttributes attribs)
{
	if (alphaTestFails(attribs)) 
	{
		IgnoreHit();
	}
}

// Ignore this shader, leave it empty
[shader("closesthit")]
void ShadowClosestHit(inout ShadowRayPayload rayData, BuiltInTriangleIntersectionAttributes attribs)
{

}