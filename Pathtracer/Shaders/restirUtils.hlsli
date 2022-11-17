struct Reservoir {
	float lightSample;  // Chosen sample
	float M;            // Number of samples seen so far
	float weight;       // Weight
	float totalWeight;  // Sum of weights
};

// RIS

void updateReservoir(inout Reservoir input, in float weight, in float light, in uint randSeed) {
	input.totalWeight += weight;
	input.M++;
	if (nextRand(randSeed) < (weight / input.totalWeight)) {
		input.lightSample = light;
	}
}

//Reservoir mergeReservoirs(Reservoir r1, Reservoir r2) {
//	Reservoir merged;
//	updateReservoir(merged, r1);
//	updateReservoir(merged, r2);
//	merged.M = r1.M + r2.M;
//	return merged;
//}

//Reservoir sampleLightsRIS(float3 p, int lightsCount) {
//	Reservoir r;
//	for (int i = 0; i < min(lightCount, 8); i++) {
//		Reservoir input;
//		LightSample candidate = sampleFromSourcePool(sourcePdf);
//		float3 lightVector = candidate.position - gridCellCenter;
//		float lightDistanceSquared = max(gMinDistanceSquared, dot(lightVector, lightVector));
//		float sourcePdf = 1 / 8;
//		input.sampleTargetPdf = candidate.intensity / lightDistanceSquared;
//		updateReservoir(r, input, weight, randSeed);
//	}
//	return r;
//}
//
//Reservoir sampleLightsUsingRIS(float3 p) {
//	// TODO: use this to sample w.r.t. BRDF during 
//	return 0.0;
//}