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

#pragma once

#include "../SharedUtils/RenderPass.h"
#include "../SharedUtils/FullscreenLaunch.h"

class SimpleAccumulationPass : public ::RenderPass, inherit_shared_from_this<::RenderPass, SimpleAccumulationPass>
{
public:
	using SharedPtr = std::shared_ptr<SimpleAccumulationPass>;

	static SharedPtr create(const std::string &accumulationBuffer);
	virtual ~SimpleAccumulationPass() = default;

protected:
	SimpleAccumulationPass(const std::string& accumulationBuffer);

	// RenderPass functionality
	bool initialize(RenderContext* pRenderContext, ResourceManager::SharedPtr pResManager) override;
	void initScene(RenderContext* pRenderContext, Scene::SharedPtr pScene) override;
	void renderGui(Gui* pGui) override;
	void execute(RenderContext* pRenderContext) override;
	void resize(uint32_t width, uint32_t height) override;
	void stateRefreshed() override;

	// Override default RenderPass functionality (that control the rendering pipeline and its GUI)
	bool appliesPostprocess() override { return true; }      // Adds 'load scene' option to GUI.
	bool hasCameraMoved();  // Determine if there has been any camera motion.

	// Texture we're accumulating in
	std::string                   mAccumChannel;
	
	// State variables
	FullscreenLaunch::SharedPtr   mpAccumShader;
	GraphicsState::SharedPtr      mpGfxState;

	// Internal parameters
	Texture::SharedPtr            mpLastFrame;            ///< Radius for ambient occlusion rays (only examine nearby geometry determined by this radius)          
	uint32_t                      mAccumCount = 0;        ///< Frame count to use as seed for random number generator
	bool                          mDoAccumulation = true; ///< Is accumulation enabled
	Scene::SharedPtr              mpScene;                ///< Number of ambient occlusion rays to shoot per pixel
	mat4                          mpLastCameraMatrix;     ///< The last camera matrix 
	Fbo::SharedPtr                mpInternalFbo;          ///< Temp framebuffer
};
