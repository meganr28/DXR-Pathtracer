#include "BuildCellReservoirsPass.h"

namespace {
	const char* kFileInitReservoirs = "Shaders\\buildCellReservoirs.hlsl";
	const uint32_t kThreadGroupSize = 256;
};

BuildCellReservoirsPass::BuildCellReservoirsPass(const std::string& outBuf)
	: mOutChannel(outBuf), ::RenderPass("Build Cell Reservoirs Pass", "Build Cell Reservoirs Options")
{
}

bool BuildCellReservoirsPass::initialize(RenderContext* pRenderContext, ResourceManager::SharedPtr pResManager)
{
	// Stash a copy of our resource manager, allowing us to access shared rendering resources
	mpResManager = pResManager;

	// Request texture resources for this pass (Note: We do not need a z-buffer since ray tracing does not generate one by default)
	mpResManager->requestTextureResources({ "WorldPosition", "WorldNormal", "MaterialDiffuse", "LightGrid"});
	mpResManager->requestTextureResource(mOutChannel);
	mpResManager->requestTextureResource(ResourceManager::kEnvironmentMap);

	// Set the default scene
	mpResManager->setDefaultSceneName("Scenes/pink_room/pink_room.fscene");

	// Create compute shader from filename
	mInitReservoirsPass.pComputeProgram = ComputeProgram::createFromFile(kFileInitReservoirs, "main");
	
	if (mInitReservoirsPass.pComputeProgram) {
		mInitReservoirsPass.pComputeVars = ComputeVars::create(mInitReservoirsPass.pComputeProgram->getReflector());
		//mInitReservoirsPass.pComputeVars->setStructuredBuffer("gLightGrid", mpAliveList);
		
		// Create state
		mInitReservoirsPass.pComputeState = ComputeState::create();
		mInitReservoirsPass.pComputeState->setProgram(mInitReservoirsPass.pComputeProgram);
	}

	return true;
}

void BuildCellReservoirsPass::initScene(RenderContext* pRenderContext, Scene::SharedPtr pScene)
{
	// Save copy of scene
	if (pScene) {
		mpScene = std::dynamic_pointer_cast<RtScene>(pScene);
	}
}

void BuildCellReservoirsPass::renderGui(Gui* pGui)
{
	int dirty = 0;
	// User-controlled max depth
	dirty |= (int)pGui->addIntVar("Max Ray Depth", mRayDepth, 0, 8);
	// Checkbox to determine if we are shooting indirect rays or not
	//dirty |= (int)pGui->addCheckBox(mDoIndirectLighting ? "Enable Direct Illumination" : "Enable Indirect Illumination", mDoIndirectLighting);
	if (dirty) setRefreshFlag();
}

void BuildCellReservoirsPass::execute(RenderContext* pRenderContext)
{
	// Get output buffer and clear it to black
	Texture::SharedPtr outTex = mpResManager->getClearedTexture(mOutChannel, vec4(0.f, 0.f, 0.f, 0.f));

	// Check that pass is ready to render
	if (!outTex) return;

	// Launch compute shader
	pRenderContext->pushComputeState(mInitReservoirsPass.pComputeState);
	pRenderContext->pushComputeVars(mInitReservoirsPass.pComputeVars);

	pRenderContext->dispatch(1, 1, 1); // TODO: change this to 16 x 16 x 16

	pRenderContext->popComputeVars();
	pRenderContext->popComputeState();
}