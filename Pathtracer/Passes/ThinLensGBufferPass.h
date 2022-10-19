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
#include "../SharedUtils/RayLaunch.h"
#include <random>

class ThinLensGBufferPass : public ::RenderPass, inherit_shared_from_this<::RenderPass, ThinLensGBufferPass>
{
public:
	using SharedPtr = std::shared_ptr<ThinLensGBufferPass>;

	static SharedPtr create() { return SharedPtr(new ThinLensGBufferPass()); }
	virtual ~ThinLensGBufferPass() = default;

protected:
	ThinLensGBufferPass() : ::RenderPass("Thin Lens G-Buffer Pass", "Thin Lens G-Buffer Options") {}

	// RenderPass functionality
	bool initialize(RenderContext* pRenderContext, ResourceManager::SharedPtr pResManager) override;
	void initScene(RenderContext* pRenderContext, Scene::SharedPtr pScene) override;
	void renderGui(Gui* pGui) override;
	void execute(RenderContext* pRenderContext) override;

	// Override default RenderPass functionality (that control the rendering pipeline and its GUI)
	bool requiresScene() override { return true; }      // Adds 'load scene' option to GUI.
	bool usesRayTracing() override { return true; }      // Removes a GUI control that is confusing for this simple demo

	// Internal state variables for this pass
	RayLaunch::SharedPtr          mpRays;              ///< Wrapper around DXR pass
	RtScene::SharedPtr            mpScene;             ///< Falcor scene representation, with additions for ray tracing

	// Thin lens parameters
	bool                        mUseThinLens     = false;  ///< Use thin lens approximation
	float                       mFNumber         = 32.0f;      ///< F-number
	float                       mFocalLength     = 1.0f;   ///< Camera's focal length
	float                       mLensRadius;               ///< Camera's aperture

	// Anti-aliasing
	bool                        mUseJitter       = true;   ///< Do we jitter the camera?
	bool						mUseRandomJitter = false;  ///< Use random samples or 8x MSAA pattern

	// Random number generator for random samples
	std::uniform_real_distribution<float> mRngDist;  ///< Distribution b/w [0, 1]
	std::mt19937 mRng;

	// Counter to initialize thin lens random numbers each frame
	uint32_t mFrameCount = 0xdfedfeeb;      
};
