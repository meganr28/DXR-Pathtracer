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

#include "JitteredGBufferPass.h"
#include <chrono>

namespace {
	// Use Raster G-Buffer shader
	const char* kGBufVertShader = "Shaders\\gBuffer.vs.hlsl";
	const char* kGBufFragShader = "Shaders\\gBuffer.ps.hlsl";

	// 8x MSAA pattern positions
	const float kMSAA[8][2] = { {1,-3}, {-1,3}, {5,1}, {-3,-5}, {-5,5}, {-7,-1}, {3,7}, {7,-7} };
};

bool JitteredGBufferPass::initialize(RenderContext* pRenderContext, ResourceManager::SharedPtr pResManager)
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

	// Setup random number generator
	auto currentTime     = std::chrono::high_resolution_clock::now();
	auto timeInMillisec  = std::chrono::time_point_cast<std::chrono::milliseconds>(currentTime);
	mRng = std::mt19937(uint32_t(timeInMillisec.time_since_epoch().count()));

	return true;
}

void JitteredGBufferPass::initScene(RenderContext* pRenderContext, Scene::SharedPtr pScene)
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

void JitteredGBufferPass::renderGui(Gui* pGui)
{
	int dirty = 0;

	// Checkbox to check if we are using jitter
	dirty |= (int)pGui->addCheckBox(mUseJitter ? "Camera jitter enabled" : "Camera jitter disabled", mUseJitter);

	// If using jitter, select which type
	if (mUseJitter)
	{
		dirty |= (int)pGui->addCheckBox(mUseRandom ? "Using randomized camera position" : "Using 8x MSAA pattern", mUseRandom);
	}

	if (dirty) setRefreshFlag();
}

void JitteredGBufferPass::execute(RenderContext* pRenderContext)
{
	// Create framebuffer to render into (cannot simply clear texture)
	Fbo::SharedPtr outputFbo = mpResManager->createManagedFbo(
		{ "WorldPosition", "WorldNormal", "MaterialDiffuse", "MaterialSpecRough", "MaterialExtraParams" }, // color buffers
		  "Z-Buffer" );																					   // depth buffer

	// If no valid frame buffer, return
	if (!outputFbo) return;

	// Update camera position with jitter
	if (mUseJitter && mpScene && mpScene->getActiveCamera())
	{
		mFrameCount++;

		float xOffset = mUseRandom ? mRngDist(mRng) - 0.5f : kMSAA[mFrameCount % 8][0] * 0.0625f;
		float yOffset = mUseRandom ? mRngDist(mRng) - 0.5f : kMSAA[mFrameCount % 8][1] * 0.0625f;

		// Update camera
		mpScene->getActiveCamera()->setJitter(xOffset / float(outputFbo->getWidth()), yOffset / float(outputFbo->getHeight()));
	}

	// Clear G-Buffer (colors = black, depth = 1, stencil = 0)
	pRenderContext->clearFbo(outputFbo.get(), vec4(0, 0, 0, 0), 1.0f, 0);
	pRenderContext->clearUAV(outputFbo->getColorTexture(2)->getUAV().get(), vec4(vec3(0.5f, 0.5f, 1.0f), 1.0f));

	// Execute with DirectX context, DirectX graphics state, and framebuffer to store results
	mpRaster->execute(pRenderContext, mpGfxState, outputFbo);
}
