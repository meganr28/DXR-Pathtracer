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
#include "../SharedUtils/SimpleVars.h"
#include "../SharedUtils/RayLaunch.h"

class DenoisingPass : public ::RenderPass, inherit_shared_from_this<::RenderPass, DenoisingPass>
{
public:
	using SharedPtr = std::shared_ptr<DenoisingPass>;
	using SharedConstPtr = std::shared_ptr<const DenoisingPass>;

	static SharedPtr create(const std::string& outBuf, const int iter, const int totalIter) { return SharedPtr(new DenoisingPass(outBuf, iter, totalIter)); }
	virtual ~DenoisingPass() = default;

protected:
	DenoisingPass(const std::string& outBuf, const int iter, const int totalIter);

	// RenderPass functionality
	bool initialize(RenderContext* pRenderContext, ResourceManager::SharedPtr pResManager) override;
	void initScene(RenderContext* pRenderContext, Scene::SharedPtr pScene) override;
	void renderGui(Gui* pGui) override;
	void execute(RenderContext* pRenderContext) override;

	// Override default RenderPass functionality (that control the rendering pipeline and its GUI)
	bool requiresScene() override { return true; }      // Adds 'load scene' option to GUI.
	bool usesRayTracing() override { return true; }      // Removes a GUI control that is confusing for this simple demo
	bool usesEnvironmentMap() override { return true; }  // Use environment map to illuminate the scene

	// Internal state variables for this pass
	RayLaunch::SharedPtr          mpRays;              ///< Wrapper around DXR pass
	RtScene::SharedPtr            mpScene;             ///< Falcor scene representation, with additions for ray tracing

	// Output buffer
	std::string                   mOutChannel;

	bool                          mDoDenoise = true;
	int32_t                       mIter = 0;
	int32_t                       mTotalIter = 0;
	int32_t                       mFilterSize = 80;       ///< A-Trous filter size
	float                         mColorPhi = 0.1f;      ///< Color weight
	float                         mNormalPhi = 0.2f;      ///< Normal weight
	float                         mPositionPhi = 0.1f;    ///< Normal weight

	// Counter to initialize thin lens random numbers each frame
	uint32_t                      mFrameCount = 0x1456u;                        ///< A frame counter to act as seed for random number generator 
};
