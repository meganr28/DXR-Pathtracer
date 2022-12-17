/**********************************************************************************************************************
# Copyright (c) 2018, NVIDIA CORPORATION. All rights reserved.
#
# Redistribution and use in source and binary forms, with or without modification, are permitted provided that the
# following conditions are met:
#  * Redistributions of code must retain the copyright notice, this list of conditions and the following disclaimer.
#  * Neither the name of NVIDIA CORPORATION nor the names of its contributors may be used to endorse or promote products
#    derived from this software without specific prior written permission.
#
# THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS ``AS IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
# LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.  IN NO EVENT
# SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
# CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA,
# OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
# LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF
# ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
**********************************************************************************************************************/

#include "DenoisingPass.h"

namespace {
	const char* kFileRayTrace = "Shaders\\atrous.hlsl";

	// Function names for shader entry points
	const char* kEntryPointRayGen = "DenoisingRayGen";
};

DenoisingPass::DenoisingPass(const std::string& outBuf, const int iter, const int totalIter) :
	mOutChannel(outBuf),
	mIter(iter),
	mTotalIter(totalIter),
	::RenderPass("Denoising Pass", "Denoising Options") 
{
}

bool DenoisingPass::initialize(RenderContext* pRenderContext, ResourceManager::SharedPtr pResManager)
{
	// Stash a copy of our resource manager, allowing us to access shared rendering resources
	mpResManager = pResManager;

	// Request texture resources for this pass (Note: We do not need a z-buffer since ray tracing does not generate one by default)
	mpResManager->requestTextureResources({ "WorldPosition", "WorldNormal", "MaterialDiffuse", "ShadedOutput"
										    "CurrReservoirs", "DenoiseIn", "DenoiseOut", "DenoisedImage"});

	// Set the default scene
	mpResManager->setDefaultSceneName("Scenes/pink_room/pink_room.fscene");

	// Create wrapper around ray tracing pass
	mpRays = RayLaunch::create(kFileRayTrace, kEntryPointRayGen);

	// Compile
	mpRays->compileRayProgram();
	if (mpScene) mpRays->setScene(mpScene);

	return true;
}

void DenoisingPass::initScene(RenderContext* pRenderContext, Scene::SharedPtr pScene)
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

void DenoisingPass::renderGui(Gui* pGui)
{
	int dirty = 0;
	// User-controlled number of light samples (M)
	dirty |= (int)pGui->addIntVar("Filter Size", mFilterSize, 0, 512);
	dirty |= (int)pGui->addFloatVar("Color Weight", mColorPhi, 0.f, 200.f);
	dirty |= (int)pGui->addFloatVar("Normal Weight", mNormalPhi, 0.f, 10.f);
	dirty |= (int)pGui->addFloatVar("Position Weight", mPositionPhi, 0.f, 10.f);
	if (dirty) setRefreshFlag();
}

void DenoisingPass::execute(RenderContext* pRenderContext)
{
	// Get output buffer and clear it to black
	Texture::SharedPtr outTex = mpResManager->getClearedTexture(mOutChannel, vec4(0.f, 0.f, 0.f, 0.f));

	// Check that pass is ready to render
	if (!outTex || !mpRays || !mpRays->readyToRender()) return;

	// Pass background color to miss shader #0
	auto globalVars = mpRays->getGlobalVars();
	globalVars["GlobalCB"]["gFrameCount"] = mFrameCount++;
	globalVars["GlobalCB"]["gEnableDenoise"] = mpResManager->getDenoising();
	globalVars["GlobalCB"]["gFilterSize"] = mFilterSize; 
	globalVars["GlobalCB"]["gColorPhi"] = mColorPhi;
	globalVars["GlobalCB"]["gNormalPhi"] = mNormalPhi;
	globalVars["GlobalCB"]["gPositionPhi"] = mPositionPhi;
	globalVars["GlobalCB"]["gIter"] = mIter;
	globalVars["GlobalCB"]["gTotalIter"] = mTotalIter;

	// Pass G-Buffer textures to shader
	globalVars["gPos"] = mpResManager->getTexture("WorldPosition");
	globalVars["gNorm"] = mpResManager->getTexture("WorldNormal");
	globalVars["gDiffuseMtl"] = mpResManager->getTexture("MaterialDiffuse");
	globalVars["gEmissive"] = mpResManager->getTexture("Emissive");

	// Pass ReGIR grid structure for updating
	globalVars["gShadedOutput"] = mpResManager->getTexture("ShadedOutput");
	globalVars["gDenoiseIn"] = mpResManager->getTexture("DenoiseIn");
	globalVars["gDenoiseOut"] = mpResManager->getTexture("DenoiseOut");
	globalVars["gDenoisedImage"] = mpResManager->getTexture("DenoisedImage");

	globalVars["gOutput"]     = outTex;

	// Set environment map texture for indirect illumination
	globalVars["gEnvMap"] = mpResManager->getTexture(ResourceManager::kEnvironmentMap);

	// Launch ray tracing
	mpRays->execute(pRenderContext, mpResManager->getScreenSize());
}