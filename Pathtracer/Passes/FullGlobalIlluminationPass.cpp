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

#include "FullGlobalIlluminationPass.h"

namespace {
	const char* kFileRayTrace = "Shaders\\fullGI.hlsl";

	// Function names for shader entry points
	const char* kEntryPointRayGen = "FullGIRayGen";
	
	const char* kEntryPointMiss0 = "ShadowMiss";
	const char* kEntryShadowAnyHit = "ShadowAnyHit";
	const char* kEntryShadowClosestHit = "ShadowClosestHit";

	const char* kEntryPointMiss1 = "IndirectMiss";
	const char* kEntryIndirectAnyHit = "IndirectAnyHit";
	const char* kEntryIndirectClosestHit = "IndirectClosestHit";
};

FullGlobalIlluminationPass::FullGlobalIlluminationPass(const std::string& outBuf)
	: mOutChannel(outBuf), ::RenderPass("Full Global Illumination Pass", "Full Global Illumination Options")
{
}

bool FullGlobalIlluminationPass::initialize(RenderContext* pRenderContext, ResourceManager::SharedPtr pResManager)
{
	// Stash a copy of our resource manager, allowing us to access shared rendering resources
	mpResManager = pResManager;

	// Request texture resources for this pass (Note: We do not need a z-buffer since ray tracing does not generate one by default)
	mpResManager->requestTextureResources({ "WorldPosition", "WorldNormal", "MaterialDiffuse" });
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

void FullGlobalIlluminationPass::initScene(RenderContext* pRenderContext, Scene::SharedPtr pScene)
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

void FullGlobalIlluminationPass::renderGui(Gui* pGui)
{
	int dirty = 0;
	// User-controlled max depth
	dirty |= (int)pGui->addIntVar("Max Ray Depth", mRayDepth, 0, mMaxRayDepth);
	// Checkbox to determine if we are shooting indirect rays or not
	dirty |= (int)pGui->addCheckBox(mDoIndirectLighting ? "Direct illumination only" : "Enable indirect illumination", mDoIndirectLighting);
	// Checkbox to determine if we are shooting shadow rays or not
	dirty |= (int)pGui->addCheckBox(mDoDirectLighting ? "Enable indirect illumination" : "Direct illumination only", mDoDirectLighting);
	if (dirty) setRefreshFlag();
}

void FullGlobalIlluminationPass::execute(RenderContext* pRenderContext)
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

	globalVars["gOutput"]     = outTex;

	// Set environment map texture for indirect illumination
	globalVars["gEnvMap"] = mpResManager->getTexture(ResourceManager::kEnvironmentMap);

	// Launch ray tracing
	mpRays->execute(pRenderContext, mpResManager->getScreenSize());
}