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

Reservoir combineReservoirs(Reservoir curr, Reservoir prev, in uint randSeed) {
	Reservoir merged = { 0, 0, 0, 0 };
	updateReservoir(merged, curr.y, curr.W, randSeed);
	updateReservoir(merged, prev.y, prev.W, randSeed);
	merged.M = curr.M + prev.M;
	return merged;
}

float evaluateBSDF(float3 albedo, float3 lightIntensity, float cosTheta, float lightDist) {
	float3 f = albedo / M_PI;
	float3 Le = lightIntensity;
	float G = cosTheta / (lightDist * lightDist);
	return length(f * Le * G);
}

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