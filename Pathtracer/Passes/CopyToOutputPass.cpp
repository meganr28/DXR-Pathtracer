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

#include "CopyToOutputPass.h"

bool CopyToOutputPass::initialize(RenderContext* pRenderContext, ResourceManager::SharedPtr pResManager)
{
	// Stash a copy of our resource manager, allowing us to access shared rendering resources
	//    We need an output buffer; tell our resource manager we expect the standard output channel
	mpResManager = pResManager;
	mpResManager->requestTextureResource(ResourceManager::kOutputChannel);

	// Create default DirectX raster pipeline state
	mDisplayableBuffers.push_back({ -1, "< None >" });
	return true;
}

void CopyToOutputPass::renderGui(Gui* pGui)
{
	// Add a widget to our GUI to allow us to change buffer
	pGui->addDropdown("  Displayed", mDisplayableBuffers, mSelectedBuffer);
}

void CopyToOutputPass::pipelineUpdated(ResourceManager::SharedPtr pResManager)
{
	if (!pResManager) return;
	mpResManager = pResManager;

	// Clear GUI's list of displayable textures
	mDisplayableBuffers.clear();

	int32_t outputChannel = mpResManager->getTextureIndex(ResourceManager::kOutputChannel);

	// Loop over all available resources
	for (uint32_t i = 0; i < mpResManager->getTextureCount(); i++)
	{
		// Do not let user display output resource
		if (i == outputChannel) continue;

		// Add resource's name to GUI
		mDisplayableBuffers.push_back({ int32_t(i), mpResManager->getTextureName(i) });

		// If invalid buffer selected, set valid one
		if (mSelectedBuffer == uint32_t(-1)) mSelectedBuffer = i;
	}

	// If no valid textures, set selected entry to "None"
	if (mDisplayableBuffers.size() <= 0)
	{
		mDisplayableBuffers.push_back({ -1, "< None >" });
		mSelectedBuffer = uint32_t(-1);
	}
}

void CopyToOutputPass::execute(RenderContext* pRenderContext)
{
	// Create framebuffer to render into (cannot simply clear texture)
	Texture::SharedPtr outTex = mpResManager->getTexture(ResourceManager::kOutputChannel);
	if (!outTex) return;

	// Get user-selected buffer
	Texture::SharedPtr inTex = mpResManager->getTexture(mSelectedBuffer);
	
	// If invalid texture, clear output to black 
	if (!inTex || mSelectedBuffer == uint32_t(-1)) 
	{
		pRenderContext->clearRtv(outTex->getRTV().get(), vec4(0.f, 0.f, 0.f, 1.f));
		return;
	}

	// Copy selected input buffer into output buffer
	pRenderContext->blit(inTex->getSRV(), outTex->getRTV());
}
