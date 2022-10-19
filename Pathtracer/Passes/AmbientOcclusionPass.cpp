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

#include "AmbientOcclusionPass.h"

namespace {
	const char* kFileAORayTrace = "Shaders\\aoTracing.hlsl";

	// Function names for shader entry points
	const char* kEntryPointRayGen = "AORayGen";
	const char* kEntryPointMiss0 = "AOMiss";
	const char* kEntryAOAnyHit = "AOAnyHit";
};

bool AmbientOcclusionPass::initialize(RenderContext* pRenderContext, ResourceManager::SharedPtr pResManager)
{
	// Stash a copy of our resource manager, allowing us to access shared rendering resources
	mpResManager = pResManager;

	// Request texture resources for this pass (Note: We do not need a z-buffer since ray tracing does not generate one by default)
	mpResManager->requestTextureResources({ "WorldPosition", "WorldNormal", "MaterialDiffuse",
										    "MaterialSpecRough", "MaterialExtraParams" });

	// Set the default scene
	mpResManager->setDefaultSceneName("Scenes/pink_room/pink_room.fscene");

	// Create wrapper around ray tracing pass
	mpRays = RayLaunch::create(kFileAORayTrace, kEntryPointRayGen);
	mpRays->addMissShader(kFileAORayTrace, kEntryPointMiss0);
	mpRays->addHitShader(kFileAORayTrace, "", kEntryAOAnyHit);

	// Compile
	mpRays->compileRayProgram();
	if (mpScene) mpRays->setScene(mpScene);

	return true;
}

void AmbientOcclusionPass::initScene(RenderContext* pRenderContext, Scene::SharedPtr pScene)
{
	// Save copy of scene
	if (pScene) {
		mpScene = std::dynamic_pointer_cast<RtScene>(pScene);
	}

	// Pass scene to ray tracer
	if (!mpScene) return;
	if (mpRays) {
		mpRays->setScene(mpScene);
	}

	// Set default AO radius when new scene is loaded
	mAORadius = glm::max(0.5f, mpScene->getRadius() * 0.05f);
}

void AmbientOcclusionPass::renderGui(Gui* pGui)
{
	// Add a GUI option to allow the user to change the AO radius
	int dirty = 0;
	dirty |= (int)pGui->addFloatVar("AO radius", mAORadius, 1e-4f, 1e38f, mAORadius * 0.01f);
	dirty |= (int)pGui->addIntVar("Number AO Rays", mNumRaysPerPixel, 1, 64);

	// If changed, let other passes know we changed a rendering parameter
	if (dirty) setRefreshFlag();
}

void AmbientOcclusionPass::execute(RenderContext* pRenderContext)
{
	// Clear output buffer to black
	Texture::SharedPtr pOutTex = mpResManager->getClearedTexture(ResourceManager::kOutputChannel, vec4(0.f, 0.f, 0.f, 0.f));

	// Check that pass is ready to render
	if (!pOutTex || !mpRays || !mpRays->readyToRender()) return;

	// Set ray tracing shader variables for ray generation shader
	auto rayGenVars = mpRays->getRayGenVars();
	rayGenVars["RayGenCB"]["gFrameCount"] = mFrameCount++;
	rayGenVars["RayGenCB"]["gAORadius"] = mAORadius;
	rayGenVars["RayGenCB"]["gMinT"] = mpResManager->getMinTDist();   // bias added to ray direciton
	rayGenVars["RayGenCB"]["gNumRays"] = uint32_t(mNumRaysPerPixel);
	rayGenVars["gPos"] = mpResManager->getTexture("WorldPosition");
	rayGenVars["gNorm"] = mpResManager->getTexture("WorldNormal");
	rayGenVars["gOutput"] = pOutTex;

	// Shoot AO rays
	mpRays->execute(pRenderContext, mpResManager->getScreenSize());
}