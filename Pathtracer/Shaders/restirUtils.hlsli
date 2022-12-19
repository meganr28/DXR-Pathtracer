struct Reservoir {
	float lightSample;  // Chosen sample
	float M;            // Number of samples seen so far
	float weight;       // Weight
	float totalWeight;  // Sum of weights
};

Reservoir createReservoir(in float4 res)
{
	Reservoir r;
	r.lightSample = res.x;
	r.M = res.y;
	r.weight = res.z;
	r.totalWeight = res.w;
	return r;
}

// RIS

void updateReservoir(inout Reservoir input, in float weight, in float light, inout uint randSeed) {
	input.totalWeight += weight;
	input.M++;
	if (nextRand(randSeed) < (weight / input.totalWeight)) {
		input.lightSample = light;
	}
}

Reservoir combineReservoirs(Reservoir curr, Reservoir prev, in uint randSeed) {
	Reservoir merged = { 0, 0, 0, 0 };
	updateReservoir(merged, curr.weight, curr.lightSample, randSeed);
	updateReservoir(merged, prev.weight, prev.lightSample, randSeed);
	merged.M = curr.M + prev.M;
	return merged;
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