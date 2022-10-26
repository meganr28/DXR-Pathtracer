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

#include "SimpleToneMappingPass.h"

SimpleToneMappingPass::SimpleToneMappingPass(const std::string& inBuf, const std::string& outBuf)
	: mInChannel(inBuf), mOutChannel(outBuf), ::RenderPass("Simple Tone Mapping Pass", "Simple Tone Mapping Options")
{

}

bool SimpleToneMappingPass::initialize(RenderContext* pRenderContext, ResourceManager::SharedPtr pResManager)
{
	// Stash a copy of our resource manager, allowing us to access shared rendering resources
	if (!pResManager) return false;
	mpResManager = pResManager;

	// Request texture resources for this pass (Note: We do not need a z-buffer since ray tracing does not generate one by default)
	mpResManager->requestTextureResources({ mInChannel, mOutChannel });

	// Set the default scene
	mpResManager->setDefaultSceneName("Scenes/pink_room/pink_room.fscene");

	// Initialize tone mapper with clamp operator (i.e. performs NO tone mapping)
	mpToneMapper = ToneMapping::create(ToneMapping::Operator::Clamp);

	// State object for tone mapping
	mpGfxState = GraphicsState::create();

	return true;
}

void SimpleToneMappingPass::renderGui(Gui* pGui)
{
	// Use Falcor tone mapper's UI
	mpToneMapper->renderUI(pGui, nullptr);
}

void SimpleToneMappingPass::execute(RenderContext* pRenderContext)
{
	if (!mpResManager) return;

	// Create FBOs for input and output textures
	Texture::SharedPtr srcTex = mpResManager->getTexture(mInChannel);
	Fbo::SharedPtr dstFbo = mpResManager->createManagedFbo({ mOutChannel });

	// Execute tone mapping pass (use push/pop to ensure that changes to tone mapper don't affect rest of program)
	pRenderContext->pushGraphicsState(mpGfxState);
	mpToneMapper->execute(pRenderContext, srcTex, dstFbo);
	pRenderContext->popGraphicsState();
}