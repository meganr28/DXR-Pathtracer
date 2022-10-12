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

// This is the header including full-screen raster work
#include "../SharedUtils/RasterLaunch.h"

class SimpleGBufferPass : public ::RenderPass, inherit_shared_from_this<::RenderPass, SimpleGBufferPass>
{
public:
	using SharedPtr = std::shared_ptr<SimpleGBufferPass>;

	static SharedPtr create() { return SharedPtr(new SimpleGBufferPass()); }
	virtual ~SimpleGBufferPass() = default;

protected:
	SimpleGBufferPass() : ::RenderPass("Raster G-Buffer Pass", "G-Buffer Options") {}

	// RenderPass functionality
	bool initialize(RenderContext* pRenderContext, ResourceManager::SharedPtr pResManager) override;
	void initScene(RenderContext* pRenderContext, Scene::SharedPtr pScene) override;
	void execute(RenderContext* pRenderContext) override;

	// Override default RenderPass functionality (that control the rendering pipeline and its GUI)
	bool requiresScene() override { return true; }      // Adds 'load scene' option to GUI.
	bool usesRasterization() override { return true; }      // Removes a GUI control that is confusing for this simple demo

	// Internal state variables for this pass
	GraphicsState::SharedPtr    mpGfxState;          ///< Graphics pipeline state
	Scene::SharedPtr            mpScene;             ///< Falcor scene abstraction
	RasterLaunch::SharedPtr     mpRaster;            ///< Rasterization pass (for complex scene geometry)
};
