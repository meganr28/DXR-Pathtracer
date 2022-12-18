#include "SpatialReusePass.h"

namespace {
	const char* kFileRayTrace = "Shaders\\spatialReuse.hlsl";

	// Function names for shader entry points
	const char* kEntryPointRayGen = "SpatialReuseRayGen";
	
	const char* kEntryPointMiss0 = "ShadowMiss";
	const char* kEntryShadowAnyHit = "ShadowAnyHit";
	const char* kEntryShadowClosestHit = "ShadowClosestHit";
};

SpatialReusePass::SpatialReusePass(const std::string& outBuf, const RenderParams& params, const int iter, const int totalIter) :
	mOutChannel(outBuf), 
	mEnableReSTIR(params.mEnableReSTIR),
	mIter(iter),
	mTotalIter(totalIter),
	::RenderPass("Spatial Reuse Pass", "Spatial Reuse Options")
{
}

bool SpatialReusePass::initialize(RenderContext* pRenderContext, ResourceManager::SharedPtr pResManager)
{
	// Stash a copy of our resource manager, allowing us to access shared rendering resources
	mpResManager = pResManager;

	// Request texture resources for this pass (Note: We do not need a z-buffer since ray tracing does not generate one by default)
	mpResManager->requestTextureResources({ "WorldPosition", "WorldNormal", "MaterialDiffuse", "CurrReservoirs"
		, "SpatialReservoirsIn", "SpatialReservoirsOut", "SpatialReservoirs"});
	mpResManager->requestTextureResource(mOutChannel);
	mpResManager->requestTextureResource(ResourceManager::kEnvironmentMap);

	// Set the default scene
	mpResManager->setDefaultSceneName("Scenes/pink_room/pink_room.fscene");

	// Create wrapper around ray tracing pass
	mpRays = RayLaunch::create(kFileRayTrace, kEntryPointRayGen);

	// Ray type 0 (shadow rays)
	mpRays->addMissShader(kFileRayTrace, kEntryPointMiss0);
	mpRays->addHitShader(kFileRayTrace, kEntryShadowClosestHit, kEntryShadowAnyHit);
	
	// Compile
	mpRays->compileRayProgram();
	mpRays->setMaxRecursionDepth(uint32_t(mMaxRayDepth));
	if (mpScene) mpRays->setScene(mpScene);

	return true;
}

void SpatialReusePass::initScene(RenderContext* pRenderContext, Scene::SharedPtr pScene)
{
	// Save copy of scene
	if (pScene) {
		mpScene = std::dynamic_pointer_cast<RtScene>(pScene);
	}

	// Pass scene to ray tracer
	if (mpRays) {
		mpRays->setScene(mpScene);
	}
}

void SpatialReusePass::renderGui(Gui* pGui)
{
	int dirty = 0;
	// Enable/disable different passes
	dirty |= (int)pGui->addIntVar("Spatial Neighbors", mSpatialNeighbors, 0, 100);
	dirty |= (int)pGui->addIntVar("Spatial Radius", mSpatialRadius, 0, 100);
	if (dirty) setRefreshFlag();
}

void SpatialReusePass::execute(RenderContext* pRenderContext)
{
	// Get output buffer and clear it to black
	Texture::SharedPtr outTex = mpResManager->getClearedTexture(mOutChannel, vec4(0.f, 0.f, 0.f, 0.f));

	// Check that pass is ready to render
	if (!outTex || !mpRays || !mpRays->readyToRender()) return;

	// Pass background color to miss shader #0
	auto globalVars = mpRays->getGlobalVars();
	globalVars["GlobalCB"]["gMinT"] = mpResManager->getMinTDist();
	globalVars["GlobalCB"]["gFrameCount"] = mFrameCount++;
	globalVars["GlobalCB"]["gMaxDepth"] = mRayDepth;
	globalVars["GlobalCB"]["gEmitMult"] = 1.0f;
	globalVars["GlobalCB"]["gSpatialNeighbors"] = mSpatialNeighbors;
	globalVars["GlobalCB"]["gSpatialRadius"] = mSpatialRadius;
	globalVars["GlobalCB"]["gEnableReSTIR"] = mpResManager->getWeightedRIS();
	globalVars["GlobalCB"]["gDoSpatialReuse"] = mpResManager->getSpatial();
	globalVars["GlobalCB"]["gIter"] = mIter;
	globalVars["GlobalCB"]["gTotalIter"] = mTotalIter;
	
	// Pass G-Buffer textures to shader
	globalVars["gPos"]        = mpResManager->getTexture("WorldPosition");
	globalVars["gNorm"]       = mpResManager->getTexture("WorldNormal");
	globalVars["gDiffuseMtl"] = mpResManager->getTexture("MaterialDiffuse");
	globalVars["gEmissive"]   = mpResManager->getTexture("Emissive");

	// Pass ReGIR grid structure for updating
	globalVars["gCurrReservoirs"]  = mpResManager->getTexture("CurrReservoirs");
	globalVars["gSpatialReservoirsIn"]  = mpResManager->getTexture("SpatialReservoirsIn");
	globalVars["gSpatialReservoirsOut"] = mpResManager->getTexture("SpatialReservoirsOut");
	globalVars["gSpatialReservoirs"]    = mpResManager->getTexture("SpatialReservoirs");

	//globalVars["gOutput"]     = outTex;

	// Set environment map texture for indirect illumination
	globalVars["gEnvMap"] = mpResManager->getTexture(ResourceManager::kEnvironmentMap);

	// Launch ray tracing
	mpRays->execute(pRenderContext, mpResManager->getScreenSize());
}