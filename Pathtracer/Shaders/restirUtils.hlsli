struct GBuffer {
	float4 pos;
	float4 norm;
	float4 color;
};

struct Reservoir {
	float y;     // Chosen sample
	float M;     // Number of samples seen so far
	float W;     // Weight
	float wSum;  // Sum of weights
};

Reservoir createReservoir(in float4 res)
{
	Reservoir r;
	r.y = res.x;
	r.M = res.y;
	r.W = res.z;
	r.wSum = res.w;
	return r;
}

// RIS

void updateReservoir(inout Reservoir res, in float xi, in float wi, inout uint randSeed) {
	res.wSum += wi;
	res.M++;
	if (nextRand(randSeed) < (wi / res.wSum)) {
		res.y = xi;
	}
}

float evaluateBSDF(float3 albedo, float3 lightIntensity, float cosTheta, float lightDist) {
	float3 f = albedo / M_PI;
	float3 Le = lightIntensity;
	float G = cosTheta / (lightDist * lightDist);
	return length(f * Le * G);
}

float evaluatePHat(float4 worldPos, float4 worldNorm, float4 difMatlColor, float light) {
	// Light data
	float dist;
	float3 lightIntensity;
	float3 lightDirection;

	// Calculate p_hat(r.y) for reservoir's light sample
	getLightData(light, worldPos.xyz, lightDirection, lightIntensity, dist);
	float cosTheta = saturate(dot(worldNorm.xyz, lightDirection));
	float p_hat = evaluateBSDF(difMatlColor.rgb, lightIntensity, cosTheta, dist);

	return p_hat;
}

float evaluateCos(uint2 pixelIndex, inout float3 lightDirection, inout float3 lightIntensity, inout float dist, float light) {
	float4 worldPos = gPos[pixelIndex];
	float4 worldNorm = gNorm[pixelIndex];
	
	// Calculate p_hat(r.y) for reservoir's light sample
	getLightData(light, worldPos.xyz, lightDirection, lightIntensity, dist);
	float cosTheta = saturate(dot(worldNorm.xyz, lightDirection));

	return cosTheta;
}

//void combineReservoirs(inout Reservoir merged, Reservoir curr, Reservoir prev, in uint randSeed) {
//	float cosTheta = 0.f;
//	float p_hat = 0.f;
//	
//	// Get p_hat(curr.y)
//	getLightData(reservoir.y, worldPos.xyz, lightDirection, lightIntensity, dist);
//	cosTheta = saturate(dot(worldNorm.xyz, lightDirection));
//	p_hat = evaluateBSDF(difMatlColor.rgb, lightIntensity, cosTheta, dist);
//	
//	updateReservoir(merged, curr.y, curr.W, randSeed);
//	updateReservoir(merged, prev.y, prev.W, randSeed);
//	merged.M = curr.M + prev.M;
//	return merged;
//}

// Halton sequence - https://en.wikipedia.org/wiki/Halton_sequence
float halton(inout uint i, uint b)
{
	i = (1664525u * i + 1013904223u);

	float f = 1.0;
	float r = 0.0;

	while (i > 0) {
		f = f / float(b);
		r = r + f * float(i % b);
		i = floor(i / b);
	}
	return r;
}