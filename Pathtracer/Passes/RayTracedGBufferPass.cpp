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

#include "RayTracedGBufferPass.h"

namespace {
	const char* kFileRayTrace = "Shaders\\rtGBuffer.hlsl";

	// Function names for shader entry points
	const char* kEntryPointRayGen = "GBufferRayGen";
	const char* kEntryPointMiss0 = "PrimaryMiss";
	const char* kEntryPrimaryAnyHit = "PrimaryAnyHit";
	const char* kEntryPrimaryClosestHit = "PrimaryClosestHit";
};

bool RayTracedGBufferPass::initialize(RenderContext* pRenderContext, ResourceManager::SharedPtr pResManager)
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

	return true;
}

void RayTracedGBufferPass::initScene(RenderContext* pRenderContext, Scene::SharedPtr pScene)
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

void RayTracedGBufferPass::execute(RenderContext* pRenderContext)
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

	// Launch ray tracing
	mpRays->execute(pRenderContext, mpResManager->getScreenSize());
}