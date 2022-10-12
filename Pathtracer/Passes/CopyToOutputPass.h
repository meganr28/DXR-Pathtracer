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

// Displays a full-screen sine pattern that changes over time

#pragma once

// This is the header including the base RenderPass class
#include "../SharedUtils/RenderPass.h"

class CopyToOutputPass : public ::RenderPass, inherit_shared_from_this<::RenderPass, CopyToOutputPass>
{
public:
	using SharedPtr = std::shared_ptr<CopyToOutputPass>;

	static SharedPtr create() { return SharedPtr(new CopyToOutputPass()); }
	virtual ~CopyToOutputPass() = default;

protected:
	CopyToOutputPass() : ::RenderPass("Copy-to-Output Pass", "Copy-to-Output Options") {}

	// RenderPass functionality
	bool initialize(RenderContext* pRenderContext, ResourceManager::SharedPtr pResManager) override;
	void renderGui(Gui* pGui) override;
	void pipelineUpdated(ResourceManager::SharedPtr pResManager) override;
	void execute(RenderContext* pRenderContext) override;

	// Override default RenderPass functionality (that control the rendering pipeline and its GUI)
	bool appliesPostprocess() override { return true; }  // This pass uses rasterization.      // Removes a GUI control that is confusing for this simple demo

	// Internal state variables for this pass
	Gui::DropdownList mDisplayableBuffers;
	uint32_t          mSelectedBuffer = 0xFFFFFFFFu;
};
