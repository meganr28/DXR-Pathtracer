#include "CreateLightSamplesPass.h"

namespace {
	const char* kFileRayTrace = "Shaders\\createLightSamples.hlsl";

	// Function names for shader entry points
	const char* kEntryPointRayGen = "CreateLightSamplesRayGen";
	
	const char* kEntryPointMiss0 = "ShadowMiss";
	const char* kEntryShadowAnyHit = "ShadowAnyHit";
	const char* kEntryShadowClosestHit = "ShadowClosestHit";

	const char* kEntryPointMiss1 = "IndirectMiss";
	const char* kEntryIndirectAnyHit = "IndirectAnyHit";
	const char* kEntryIndirectClosestHit = "IndirectClosestHit";
};

CreateLightSamplesPass::CreateLightSamplesPass(const std::string& outBuf, const RenderParams& params) : 
	mOutChannel(outBuf), 
	mEnableReSTIR(params.mEnableReSTIR), 
	mDoTemporalReuse(params.mTemporalReuse),
	::RenderPass("Create Light Samples Pass", "Create Light Samples Options")
{
}

bool CreateLightSamplesPass::initialize(RenderContext* pRenderContext, ResourceManager::SharedPtr pResManager)
{
	// Stash a copy of our resource manager, allowing us to access shared rendering resources
	mpResManager = pResManager;

	// Request texture resources for this pass (Note: We do not need a z-buffer since ray tracing does not generate one by default)
	mpResManager->requestTextureResources({ "WorldPosition", "WorldNormal", "MaterialDiffuse", "CurrReservoirs", "PrevReservoirs"});
	mpResManager->requestTextureResource(mOutChannel);
	mpResManager->requestTextureResource(ResourceManager::kEnvironmentMap);

	// Set the default scene
	mpResManager->setDefaultSceneName("Scenes/pink_room/pink_room.fscene");

	// Create wrapper around ray tracing pass
	mpRays = RayLaunch::create(kFileRayTrace, kEntryPointRayGen);

	// Ray type 0 (shadow rays)
	mpRays->addMissShader(kFileRayTrace, kEntryPointMiss0);
	mpRays->addHitShader(kFileRayTrace, kEntryShadowClosestHit, kEntryShadowAnyHit);

	// Ray type 1 (indirect GI rays)
	mpRays->addMissShader(kFileRayTrace, kEntryPointMiss1);
	mpRays->addHitShader(kFileRayTrace, kEntryIndirectClosestHit, kEntryIndirectAnyHit);
	
	// Compile
	mpRays->compileRayProgram();
	mpRays->setMaxRecursionDepth(uint32_t(mMaxRayDepth));
	if (mpScene) mpRays->setScene(mpScene);

	return true;
}

bool CreateLightSamplesPass::hasCameraMoved()
{
	if (!mpScene || !mpScene->getActiveCamera())
	{
		return false;
	}
	bool hasCameraChanged = (mpLastCameraMatrix != mpScene->getActiveCamera()->getViewProjMatrix());
	return hasCameraChanged;
}

void CreateLightSamplesPass::initScene(RenderContext* pRenderContext, Scene::SharedPtr pScene)
{
	// Save copy of scene
	if (pScene) {
		mpScene = std::dynamic_pointer_cast<RtScene>(pScene);
	}

	// Get copy of scene's camera matrix
	if (mpScene && mpScene->getActiveCamera())
	{
		mpLastCameraMatrix = mpScene->getActiveCamera()->getViewProjMatrix();
	}

	// Pass scene to ray tracer
	if (mpRays) {
		mpRays->setScene(mpScene);
	}
}

void CreateLightSamplesPass::renderGui(Gui* pGui)
{
	int dirty = 0;
	// User-controlled number of light samples (M)
	dirty |= (int)pGui->addIntVar("M", mLightSamples, 0, 32); // TODO: change this to scene lights count
	// Enable/disable different passes
	dirty |= (int)pGui->addCheckBox(mEnableReSTIR ? "Show Direct Lighting" : "Show ReSTIR", mEnableReSTIR);
	dirty |= (int)pGui->addCheckBox(mDoVisibilityReuse ? "Disable Visibility Reuse" : "Enable Visibility Reuse", mDoVisibilityReuse);
	dirty |= (int)pGui->addCheckBox(mDoTemporalReuse ? "Disable Temporal Reuse" : "Enable Temporal Reuse", mDoTemporalReuse);
	if (dirty) setRefreshFlag();
}

void CreateLightSamplesPass::execute(RenderContext* pRenderContext)
{
	// Get output buffer and clear it to black
	Texture::SharedPtr outTex = mpResManager->getClearedTexture(mOutChannel, vec4(0.f, 0.f, 0.f, 0.f));

	// Check that pass is ready to render
	if (!outTex || !mpRays || !mpRays->readyToRender()) return;

	if (hasCameraMoved())
	{
		mpLastCameraMatrix = mpScene->getActiveCamera()->getViewProjMatrix();
	}

	// Pass background color to miss shader #0
	auto globalVars = mpRays->getGlobalVars();
	globalVars["GlobalCB"]["gMinT"] = mpResManager->getMinTDist();
	globalVars["GlobalCB"]["gFrameCount"] = mFrameCount++;
	globalVars["GlobalCB"]["gMaxDepth"] = mRayDepth;
	globalVars["GlobalCB"]["gLightSamples"] = mLightSamples;
	globalVars["GlobalCB"]["gEmitMult"] = 1.0f;
	globalVars["GlobalCB"]["gLastCameraMatrix"] = mpLastCameraMatrix;

	globalVars["GlobalCB"]["gDoIndirectLighting"] = mDoIndirectLighting;
	globalVars["GlobalCB"]["gDoDirectLighting"] = mDoDirectLighting;
	globalVars["GlobalCB"]["gEnableWeightedRIS"] = mpResManager->getWeightedRIS();
	globalVars["GlobalCB"]["gDoVisiblityReuse"] = mDoVisibilityReuse;
	globalVars["GlobalCB"]["gDoTemporalReuse"] = mpResManager->getTemporal();
	
	// Pass G-Buffer textures to shader
	globalVars["gPos"]        = mpResManager->getTexture("WorldPosition");
	globalVars["gNorm"]       = mpResManager->getTexture("WorldNormal");
	globalVars["gDiffuseMtl"] = mpResManager->getTexture("MaterialDiffuse");
	globalVars["gEmissive"]   = mpResManager->getTexture("Emissive");

	// Pass ReGIR grid structure for updating
	globalVars["gCurrReservoirs"]  = mpResManager->getTexture("CurrReservoirs");
	globalVars["gPrevReservoirs"]  = mpResManager->getTexture("PrevReservoirs");

	//globalVars["gOutput"]     = outTex;

	// Set environment map texture for indirect illumination
	globalVars["gEnvMap"] = mpResManager->getTexture(ResourceManager::kEnvironmentMap);

	// Launch ray tracing
	mpRays->execute(pRenderContext, mpResManager->getScreenSize());
}