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

#include "SimpleAccumulationPass.h"

namespace {
	const char* kAccumShader = "Shaders\\accumulation.ps.hlsl";
};

SimpleAccumulationPass::SharedPtr SimpleAccumulationPass::create(const std::string& accumulationBuffer)
{
	return SharedPtr(new SimpleAccumulationPass(accumulationBuffer));
}

SimpleAccumulationPass::SimpleAccumulationPass(const std::string& accumulationBuffer)
	: ::RenderPass("Accumulation Pass", "Accumulation Options")
{
	mAccumChannel = accumulationBuffer;
}

bool SimpleAccumulationPass::initialize(RenderContext* pRenderContext, ResourceManager::SharedPtr pResManager)
{
	// Stash a copy of our resource manager, allowing us to access shared rendering resources
	mpResManager = pResManager;

	// Request texture resources for this pass (Note: We do not need a z-buffer since ray tracing does not generate one by default)
	mpResManager->requestTextureResource(mAccumChannel);

	// Set the default scene
	mpResManager->setDefaultSceneName("Scenes/pink_room/pink_room.fscene");

	// Create wrapper around ray tracing pass
	mpGfxState = GraphicsState::create();
	mpAccumShader = FullscreenLaunch::create(kAccumShader);
	return true;
}

void SimpleAccumulationPass::initScene(RenderContext* pRenderContext, Scene::SharedPtr pScene)
{
	// Reset accumulation when loading new scene
	mAccumCount = 0;

	// Save copy of scene
	if (pScene) {
		mpScene = pScene;
	}

	// Get copy of scene's camera matrix
	if (!mpScene && mpScene->getActiveCamera())
	{
		mpLastCameraMatrix = mpScene->getActiveCamera()->getViewMatrix();
	}
}

void SimpleAccumulationPass::resize(uint32_t width, uint32_t height)
{
	mpLastFrame = Texture::create2D(width, height, ResourceFormat::RGBA32Float, 1, 1, nullptr, ResourceManager::kDefaultFlags);
	mpInternalFbo = ResourceManager::createFbo(width, height, ResourceFormat::RGBA32Float);
	mpGfxState->setFbo(mpInternalFbo);
	mAccumCount = 0;
}

void SimpleAccumulationPass::renderGui(Gui* pGui)
{
	// Print name of accumulation buffer
	pGui->addText((std::string("Accumulating buffer:  ") + mAccumChannel).c_str());

	// Enable/disable temporal accumulation
	std::string accumText = mDoAccumulation ? "Disable temporal accumulation" : "Enable temporal accumulation";
	if (pGui->addCheckBox(accumText.c_str(), mDoAccumulation))
	{
		mAccumCount = 0;
		setRefreshFlag();
	}

	// Display count of accumulated frame
	pGui->addText("");
	pGui->addText((std::string("Frame Count: ") + std::to_string(mAccumCount)).c_str());
}

bool SimpleAccumulationPass::hasCameraMoved()
{
	if (!mpScene || !mpScene->getActiveCamera())
	{
		return false;
	}
	bool hasCameraChanged = (mpLastCameraMatrix != mpScene->getActiveCamera()->getViewMatrix());
	return hasCameraChanged;
}

void SimpleAccumulationPass::execute(RenderContext* pRenderContext)
{
	// Texture to accumulate
	Texture::SharedPtr inTex = mpResManager->getTexture(mAccumChannel);

	// Check that pass is ready to render
	if (!inTex || !mDoAccumulation) return;

	if (hasCameraMoved())
	{
		mAccumCount = 0;
		mpLastCameraMatrix = mpScene->getActiveCamera()->getViewMatrix();
	}

	// Set ray tracing shader variables for ray generation shader
	auto accumVars = mpAccumShader->getVars();
	accumVars["PerFrameCB"]["gAccumCount"] = mAccumCount++;
	accumVars["gLastFrame"] = mpLastFrame;
	accumVars["gCurFrame"] = inTex;

	// Execute shader
	mpAccumShader->execute(pRenderContext, mpGfxState);

	// Accumulated result, copy back to in/out buffer
	pRenderContext->blit(mpInternalFbo->getColorTexture(0)->getSRV(), inTex->getRTV());

	// Keep copy of accumulation to use next frame
	pRenderContext->blit(mpInternalFbo->getColorTexture(0)->getSRV(), mpLastFrame->getRTV());
}

void SimpleAccumulationPass::stateRefreshed()
{
	// Restart accumulation process
	mAccumCount = 0;
}