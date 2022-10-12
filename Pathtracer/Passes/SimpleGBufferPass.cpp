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

#include "SimpleGBufferPass.h"

namespace {
	const char* kGBufVertShader = "Shaders\\gBuffer.vs.hlsl";
	const char* kGBufFragShader = "Shaders\\gBuffer.ps.hlsl";
};

bool SimpleGBufferPass::initialize(RenderContext* pRenderContext, ResourceManager::SharedPtr pResManager)
{
	// Stash a copy of our resource manager, allowing us to access shared rendering resources
	mpResManager = pResManager;

	mpResManager->requestTextureResource("WorldPosition");
	mpResManager->requestTextureResource("WorldNormal");
	mpResManager->requestTextureResource("MaterialDiffuse");
	mpResManager->requestTextureResource("MaterialSpecRough");
	mpResManager->requestTextureResource("MaterialExtraParams");
	mpResManager->requestTextureResource("Z-Buffer",
										  ResourceFormat::D24UnormS8,
										  ResourceManager::kDepthBufferFlags);

	// Set the default scene
	mpResManager->setDefaultSceneName("Scenes/pink_room/pink_room.fscene");
	
	// Create default DirectX raster pipeline state
	mpGfxState = GraphicsState::create();

	// Create wrapper for scene rasterization pass
	mpRaster = RasterLaunch::createFromFiles(kGBufVertShader, kGBufFragShader);
	mpRaster->setScene(mpScene);

	return true;
}

void SimpleGBufferPass::initScene(RenderContext* pRenderContext, Scene::SharedPtr pScene)
{
	// Save copy of scene
	if (pScene) {
		mpScene = pScene;
	}

	// Update raster pass wrapper with scene
	if (mpRaster) {
		mpRaster->setScene(mpScene);
	}
}

void SimpleGBufferPass::execute(RenderContext* pRenderContext)
{
	// Create framebuffer to render into (cannot simply clear texture)
	Fbo::SharedPtr outputFbo = mpResManager->createManagedFbo(
		{ "WorldPosition", "WorldNormal", "MaterialDiffuse", "MaterialSpecRough", "MaterialExtraParams" }, // color buffers
		  "Z-Buffer" );																					   // depth buffer

	// If no valid frame buffer, return
	if (!outputFbo) return;

	// Clear G-Buffer (colors = black, depth = 1, stencil = 0)
	pRenderContext->clearFbo(outputFbo.get(), vec4(0, 0, 0, 0), 1.0f, 0);
	pRenderContext->clearUAV(outputFbo->getColorTexture(2)->getUAV().get(), vec4(vec3(0.5f, 0.5f, 1.0f), 1.0f));

	// Execute with DirectX context, DirectX graphics state, and framebuffer to store results
	mpRaster->execute(pRenderContext, mpGfxState, outputFbo);
}
