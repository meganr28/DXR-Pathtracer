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

#include "ThinLensGBufferPass.h"
#include <chrono>

namespace {
	const char* kFileRayTrace = "Shaders\\thinLensGBuffer.hlsl";

	// Function names for shader entry points
	const char* kEntryPointRayGen = "GBufferRayGen";
	const char* kEntryPointMiss0 = "PrimaryMiss";
	const char* kEntryPrimaryAnyHit = "PrimaryAnyHit";
	const char* kEntryPrimaryClosestHit = "PrimaryClosestHit";

	// 8x MSAA pattern positions
	const float kMSAA[8][2] = { {1,-3}, {-1,3}, {5,1}, {-3,-5}, {-5,5}, {-7,-1}, {3,7}, {7,-7} };
};

bool ThinLensGBufferPass::initialize(RenderContext* pRenderContext, ResourceManager::SharedPtr pResManager)
{
	// Stash a copy of our resource manager, allowing us to access shared rendering resources
	mpResManager = pResManager;

	// Request texture resources for this pass (Note: We do not need a z-buffer since ray tracing does not generate one by default)
	mpResManager->requestTextureResources({ "WorldPosition", "WorldNormal", "MaterialDiffuse",
										    "MaterialSpecRough", "MaterialExtraParams" });

	// Set the default scene
	mpResManager->setDefaultSceneName("Scenes/pink_room/pink_room.fscene");

	// Create wrapper around ray tracing pass
	mpRays = RayLaunch::create(kFileRayTrace, kEntryPointRayGen);
	mpRays->addMissShader(kFileRayTrace, kEntryPointMiss0);
	mpRays->addHitShader(kFileRayTrace, kEntryPrimaryClosestHit, kEntryPrimaryAnyHit);

	// Compile
	mpRays->compileRayProgram();
	if (mpScene) mpRays->setScene(mpScene);

	// Random number generator
	auto currentTime = std::chrono::high_resolution_clock::now();
	auto timeInMillisec = std::chrono::time_point_cast<std::chrono::milliseconds>(currentTime);
	mRng = std::mt19937(uint32_t(timeInMillisec.time_since_epoch().count()));

	// Resize GUI window
	setGuiSize(ivec2(250, 300));

	return true;
}

void ThinLensGBufferPass::initScene(RenderContext* pRenderContext, Scene::SharedPtr pScene)
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

void ThinLensGBufferPass::renderGui(Gui* pGui)
{
	int dirty = 0;

	// Checkbox to determine if we are using thin lens or pinhole camera model
	dirty |= (int)pGui->addCheckBox(mUseThinLens ? "Thin lens enabled" : "Pinhole enabled", mUseThinLens);

	if (mUseThinLens)
	{
		pGui->addText("     "); // Indent
		dirty |= (int)pGui->addFloatVar("F number", mFNumber, 1.0f, 128.0f, 0.01f, true);
		pGui->addText("     "); 
		dirty |= (int)pGui->addFloatVar("Focal distance", mFocalLength, 0.01f, FLT_MAX, 0.01f, true);
	}

	// Checkbox to check if we are using jitter
	dirty |= (int)pGui->addCheckBox(mUseJitter ? "Camera jitter enabled" : "Camera jitter disabled", mUseJitter);

	// If using jitter, select which type
	if (mUseJitter)
	{
		pGui->addText("     "); 
		dirty |= (int)pGui->addCheckBox(mUseRandomJitter ? "Using randomized camera position" : "Using 8x MSAA pattern", mUseRandomJitter);
	}

	if (dirty) setRefreshFlag();
}

void ThinLensGBufferPass::execute(RenderContext* pRenderContext)
{
	// Check that pass is ready to render
	if (!mpRays || !mpRays->readyToRender()) return;

	// Load G-buffer textures and clear them to black
	vec4 black = vec4(0, 0, 0, 0);
	Texture::SharedPtr wsPos    = mpResManager->getClearedTexture("WorldPosition",       black);
	Texture::SharedPtr wsNorm   = mpResManager->getClearedTexture("WorldNormal",         black);
	Texture::SharedPtr matDif   = mpResManager->getClearedTexture("MaterialDiffuse",     black);
	Texture::SharedPtr matSpec  = mpResManager->getClearedTexture("MaterialSpecRough",   black);
	Texture::SharedPtr matExtra = mpResManager->getClearedTexture("MaterialExtraParams", black);
	Texture::SharedPtr matEmit  = mpResManager->getClearedTexture("Emissive",            black);

	// Compute parameters based on GUI inputs
	mLensRadius = mFocalLength / (2.0f * mFNumber);

	// Pass background color to miss shader #0
	auto missVars = mpRays->getMissVars(0);
	missVars["MissShaderCB"]["gBgColor"] = vec3(0.5f, 0.5f, 1.0f);
	missVars["gMatDif"] = matDif; // if rays miss, store background color in diffuse texture

	// Pass down variables for hit group #0
	// Loop over each geometry instance in the scene
	for (auto pVars : mpRays->getHitVars(0))
	{
		pVars["gWsPos"] = wsPos;
		pVars["gWsNorm"] = wsNorm;
		pVars["gMatDif"] = matDif;
		pVars["gMatSpec"] = matSpec;
		pVars["gMatExtra"] = matExtra;
		pVars["gMatEmissive"] = matEmit;
	}

	// Pass camera parameters
	auto rayGenVars = mpRays->getRayGenVars();
	rayGenVars["RayGenCB"]["gFrameCount"] = mFrameCount++;
	rayGenVars["RayGenCB"]["gLensRadius"] = mUseThinLens ? mLensRadius : 0.0f;
	rayGenVars["RayGenCB"]["gFocalLen"]   = mFocalLength;

	// Compute jitter
	float xOffset = 0.0f; 
	float yOffset = 0.0f;
	if (mUseJitter)
	{
		float xOffset = mUseRandomJitter ? mRngDist(mRng) - 0.5f : kMSAA[mFrameCount % 8][0] * 0.0625f;
		float yOffset = mUseRandomJitter ? mRngDist(mRng) - 0.5f : kMSAA[mFrameCount % 8][1] * 0.0625f;
	}

	// Set ray parameters and scene camera
	rayGenVars["RayGenCB"]["gPixelJitter"] = vec2(xOffset + 0.5f, yOffset + 0.5f);
	mpScene->getActiveCamera()->setJitter(xOffset / float(wsPos->getWidth()), yOffset / float(wsPos->getHeight()));
	
	// Launch ray tracing
	mpRays->execute(pRenderContext, mpResManager->getScreenSize());
}