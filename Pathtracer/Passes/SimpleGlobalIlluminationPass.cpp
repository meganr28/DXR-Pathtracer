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

#include "SimpleGlobalIlluminationPass.h"

namespace {
	const char* kFileRayTrace = "Shaders\\simpleGI.hlsl";

	// Function names for shader entry points
	const char* kEntryPointRayGen = "SimpleGIRayGen";
	
	const char* kEntryPointMiss0 = "ShadowMiss";
	const char* kEntryShadowAnyHit = "ShadowAnyHit";
	const char* kEntryShadowClosestHit = "ShadowClosestHit";

	const char* kEntryPointMiss1 = "IndirectMiss";
	const char* kEntryIndirectAnyHit = "IndirectAnyHit";
	const char* kEntryIndirectClosestHit = "IndirectClosestHit";
};

SimpleGlobalIlluminationPass::SimpleGlobalIlluminationPass(const std::string& outBuf)
	: mOutChannel(outBuf), ::RenderPass("Simple Diffuse Global Illumination Pass", "Simple Diffuse Global Illumination Options")
{
}

bool SimpleGlobalIlluminationPass::initialize(RenderContext* pRenderContext, ResourceManager::SharedPtr pResManager)
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
	if (mpScene) mpRays->setScene(mpScene);

	return true;
}

void SimpleGlobalIlluminationPass::initScene(RenderContext* pRenderContext, Scene::SharedPtr pScene)
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

void SimpleGlobalIlluminationPass::renderGui(Gui* pGui)
{
	int dirty = 0;
	// Checkbox to determine if we are shooting indirect rays or not
	dirty |= (int)pGui->addCheckBox(mDoIndirectGI ? "Direct illumination only" : "Enable indirect illumination", mDoIndirectGI);
	// Checkbox to determine if we are using cosine-weighted hemisphere sampling or uniform sampling
	dirty |= (int)pGui->addCheckBox(mDoCosSampling ? "Use uniform sampling" : "Use cosine-weighted sampling", mDoCosSampling);
	// Checkbox to determine if we are shooting shadow rays or not
	dirty |= (int)pGui->addCheckBox(mDoDirectShadows ? "Disable shadow rays" : "Shoot shadow rays", mDoDirectShadows);
	if (dirty) setRefreshFlag();
}

void SimpleGlobalIlluminationPass::execute(RenderContext* pRenderContext)
{
	// Get output buffer and clear it to black
	Texture::SharedPtr outTex = mpResManager->getClearedTexture(mOutChannel, vec4(0.f, 0.f, 0.f, 0.f));

	// Check that pass is ready to render
	if (!outTex || !mpRays || !mpRays->readyToRender()) return;

	// Pass background color to miss shader #0
	auto rayGenVars = mpRays->getRayGenVars();
	rayGenVars["RayGenCB"]["gMinT"] = mpResManager->getMinTDist();
	rayGenVars["RayGenCB"]["gFrameCount"] = mFrameCount++;
	rayGenVars["RayGenCB"]["gDoIndirectGI"] = mDoIndirectGI;
	rayGenVars["RayGenCB"]["gCosSampling"] = mDoCosSampling;
	rayGenVars["RayGenCB"]["gDirectShadows"] = mDoDirectShadows;

	// Pass G-Buffer textures to shader
	rayGenVars["gPos"]        = mpResManager->getTexture("WorldPosition");
	rayGenVars["gNorm"]       = mpResManager->getTexture("WorldNormal");
	rayGenVars["gDiffuseMtl"] = mpResManager->getTexture("MaterialDiffuse");
	rayGenVars["gOutput"]     = outTex;

	// Set environment map texture for indirect illumination
	auto missVars = mpRays->getMissVars(1);
	missVars["gEnvMap"] = mpResManager->getTexture(ResourceManager::kEnvironmentMap);

	// Launch ray tracing
	mpRays->execute(pRenderContext, mpResManager->getScreenSize());
}