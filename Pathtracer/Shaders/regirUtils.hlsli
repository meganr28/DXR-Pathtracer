// Index conversion
bool outOfBounds(int3 gridCell) {
	return gridCell.x < 0 || gridCell.y < 0 || gridCell.z < 0 ||
		gridCell.x >= gridCellCount.x || gridCell.y >= gridCellCount.y || gridCell.z >= gridCellCount.z;
}

int coords3DToLinear(int3 coords, int3 dims) {
	return coords.x + dims.x * coords.y + dim.x * dim.y * coords.z;
}

float3 mapCellIndexToWorld(int gridCellIndex, int gridCellSize) {
	// TODO: change these numbers to macros
	const float3 gridCenter = float3(0.0, 0.0, 0.0);
	const int3 gridCellCount = int3(16, 16, 16);
	const float3 gridOrigin = gridCenter - float3(gridCellCount) * (gridCellSize * 0.5);

	int3 gridCellPos;
	gridCellPos.x = gridCellIndex;
	gridCellPos.y = gridCellPos.x / 16.0;
	gridCellPos.x %= 16.0;
	gridCellPos.z = gridCellPos.y / 16.0;
	gridCellPos.y %= 16.0;
	
	return (float3(gridCellPos) + 0.5) * gridCellSize + gridOrigin;
}

int3 mapWorldToGrid(float3 worldPos, int3 gridDims, int gridCellSize) {
	const float3 gridCenter = float3(0.0, 0.0, 0.0);
	const int3 gridCellCount = int3(16, 16, 16);
	const float3 gridOrigin = gridCenter - float3(gridCellCount) * (gridCellSize * 0.5);

	int3 gridCell = int3(floor((worldPos - gridOrigin) / gridCellSize));

	if (outOfBounds(gridCell)) return -1;

	return coords3DToLinear(gridCell, gridDims);
}

struct LightSample {
	uint lightID;
};

struct Reservoir {
	LightSample lsample;
	uint M;
	float totalWeight;
	float sampleTargetPdf;
};

// RIS

void updateReservoir(inout Reservoir result, in Reservoir input, in float weight, in uint randSeed) {
	result.totalWeight += input.weight;
	result.M++;
	if (nextRand(randSeed) < (weight / result.totalWeight)) {
		result.lsample = input.lsample;
		result.sampleTargetPdf = input.sampleTargetPdf;
	}
	return result;
}

Reservoir mergeReservoirs(Reservoir r1, Reservoir r2) {
	Reservoir merged;
	updateReservoir(merged, r1);
	updateReservoir(merged, r2);
	merged.M = r1.M + r2.M;
	return merged;
}

Reservoir sampleLightsRIS(float3 p, int lightsCount) {
	Reservoir r;
	for (int i = 0; i < min(lightCount, 8); i++) {
		Reservoir input;
		LightSample candidate = sampleFromSourcePool(sourcePdf);
		float3 lightVector = candidate.position - gridCellCenter;
		float lightDistanceSquared = max(gMinDistanceSquared, dot(lightVector, lightVector));
		float sourcePdf = 1 / 8;
		input.sampleTargetPdf = candidate.intensity / lightDistanceSquared;
		updateReservoir(r, input, weight, randSeed);
	}
	return r;
}

Reservoir sampleLightsUsingRIS(float3 p) {
	// TODO: use this to sample w.r.t. BRDF during 
	return 0.0;
}