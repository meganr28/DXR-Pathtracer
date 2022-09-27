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

#include "SinusoidRasterPass.h"

namespace {
	// Where is our shader located?
	const char* kSinusoidShader = "Shaders\\sinusoid.ps.hlsl";
};

bool SinusoidRasterPass::initialize(RenderContext* pRenderContext, ResourceManager::SharedPtr pResManager)
{
	// Stash a copy of our resource manager, allowing us to access shared rendering resources
	//    We need an output buffer; tell our resource manager we expect the standard output channel
	mpResManager = pResManager;
	mpResManager->requestTextureResource(ResourceManager::kOutputChannel);

	// Create default DirectX raster pipeline state
	mpGfxState = GraphicsState::create();

	// Create full-screen pass shader
	mpSinusoidPass = FullscreenLaunch::create(kSinusoidShader);

	return true;
}

void SinusoidRasterPass::renderGui(Gui* pGui)
{
	// Add a widget to our GUI to allow us to change function's scale value in increments of 0.00001
	pGui->addFloatVar("  Sin mulitiplier", mScaleValue, 0.0f, 1.0f, 0.00001f, false);
}

void SinusoidRasterPass::execute(RenderContext* pRenderContext)
{
	// Create framebuffer to render into (cannot simply clear texture)
	Fbo::SharedPtr outputFbo = mpResManager->createManagedFbo({ ResourceManager::kOutputChannel });
	
	// If no valid frame buffer, return
	if (!outputFbo) return;

	// Set shader parameters (PerFrameCB = constant buffer, g_ = shader variable)
	auto shaderVars = mpSinusoidPass->getVars();
	shaderVars["PerFrameCB"]["gFrameCount"] = mFrameCount++;
	shaderVars["PerFrameCB"]["gMultValue"]  = mScaleValue;

	// Execute shader
	mpGfxState->setFbo(outputFbo);
	mpSinusoidPass->execute(pRenderContext, mpGfxState);
}
