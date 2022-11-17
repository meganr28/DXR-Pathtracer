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

CreateLightSamplesPass::CreateLightSamplesPass(const std::string& outBuf)
	: mOutChannel(outBuf), ::RenderPass("Create Light Samples Pass", "Create Light Samples Options")
{
}

bool CreateLightSamplesPass::initialize(RenderContext* pRenderContext, ResourceManager::SharedPtr pResManager)
{
	// Stash a copy of our resource manager, allowing us to access shared rendering resources
	mpResManager = pResManager;

	// Request texture resources for this pass (Note: We do not need a z-buffer since ray tracing does not generate one by default)
	mpResManager->requestTextureResources({ "WorldPosition", "WorldNormal", "MaterialDiffuse", "LightGrid"});
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

void CreateLightSamplesPass::initScene(RenderContext* pRenderContext, Scene::SharedPtr pScene)
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

void CreateLightSamplesPass::renderGui(Gui* pGui)
{
	int dirty = 0;
	// User-controlled number of light samples (M)
	dirty |= (int)pGui->addIntVar("M", mLightSamples, 0, 32); // TODO: change this to scene lights count
	if (dirty) setRefreshFlag();
}

void CreateLightSamplesPass::execute(RenderContext* pRenderContext)
{
	// Get output buffer and clear it to black
	Texture::SharedPtr outTex = mpResManager->getClearedTexture(mOutChannel, vec4(0.f, 0.f, 0.f, 0.f));

	// Check that pass is ready to render
	if (!outTex || !mpRays || !mpRays->readyToRender()) return;

	// Pass background color to miss shader #0
	auto globalVars = mpRays->getGlobalVars();
	globalVars["GlobalCB"]["gMinT"] = mpResManager->getMinTDist();
	globalVars["GlobalCB"]["gFrameCount"] = mFrameCount++;
	globalVars["GlobalCB"]["gDoIndirectLighting"] = mDoIndirectLighting;
	globalVars["GlobalCB"]["gDoDirectLighting"] = mDoDirectLighting;
	globalVars["GlobalCB"]["gMaxDepth"] = mRayDepth;
	globalVars["GlobalCB"]["gEmitMult"] = 1.0f;
	
	// Pass G-Buffer textures to shader
	globalVars["gPos"]        = mpResManager->getTexture("WorldPosition");
	globalVars["gNorm"]       = mpResManager->getTexture("WorldNormal");
	globalVars["gDiffuseMtl"] = mpResManager->getTexture("MaterialDiffuse");
	globalVars["gEmissive"]   = mpResManager->getTexture("Emissive");

	// Pass ReGIR grid structure for updating
	globalVars["gLightGrid"]  = mpResManager->getTexture("LightGrid");

	globalVars["gOutput"]     = outTex;

	// Set environment map texture for indirect illumination
	globalVars["gEnvMap"] = mpResManager->getTexture(ResourceManager::kEnvironmentMap);

	// Launch ray tracing
	mpRays->execute(pRenderContext, mpResManager->getScreenSize());
}