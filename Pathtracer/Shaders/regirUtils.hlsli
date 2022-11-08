struct LightSample {
	uint lightID;
};

struct Reservoir {
	LightSample lsample;
	uint M;
	float totalWeight;
	float sampleTargetPdf;
};

float sampleLightsRIS(float3 p) {
	return 0.0;
}