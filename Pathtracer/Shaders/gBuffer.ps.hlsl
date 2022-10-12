__import Shading;    // To get ShadingData structure/helper functions
__import DefaultVS;  // To get VertexOut declaration

struct GBuffer
{
	float4 wsPos    : SV_Target0;
	float4 wsNorm   : SV_Target1;
	float4 matDif   : SV_Target2;
	float4 matSpec  : SV_Target3;
	float4 matExtra : SV_Target4;
};

GBuffer main(VertexOut vsOut, uint primId : SV_PrimitiveID, float4 pos: SV_Position)
{
	// Falcor built-in to extract geometry and material data for shding
	ShadingData hitPt = prepareShadingData(vsOut, gMaterial, gCamera.posW);

	// Store this data in G-Buffer
	GBuffer gBufOut;
	gBufOut.wsPos    = float4(hitPt.posW, 1.f);
	gBufOut.wsNorm   = float4(hitPt.N, length(hitPt.posW - gCamera.posW));
	gBufOut.matDif   = float4(hitPt.diffuse, hitPt.opacity);
	gBufOut.matSpec  = float4(hitPt.specular, hitPt.linearRoughness);
	gBufOut.matExtra = float4(hitPt.IoR, hitPt.doubleSidedMaterial ? 1.f : 0.f, 0.f, 0.f);

	return gBufOut;
}