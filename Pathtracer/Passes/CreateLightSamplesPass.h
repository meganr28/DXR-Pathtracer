#pragma once

#include "../SharedUtils/RenderPass.h"
#include "../SharedUtils/RayLaunch.h"

class CreateLightSamplesPass : public ::RenderPass, inherit_shared_from_this<::RenderPass, CreateLightSamplesPass>
{
public:
	using SharedPtr = std::shared_ptr<CreateLightSamplesPass>;
	using SharedConstPtr = std::shared_ptr<const CreateLightSamplesPass>;

	static SharedPtr create(const std::string &outBuf, const RenderParams &params) { return SharedPtr(new CreateLightSamplesPass(outBuf, params)); }
	virtual ~CreateLightSamplesPass() = default;

protected:
	CreateLightSamplesPass(const std::string& outBuf, const RenderParams& params);

	// RenderPass functionality
	bool initialize(RenderContext* pRenderContext, ResourceManager::SharedPtr pResManager) override;
	void initScene(RenderContext* pRenderContext, Scene::SharedPtr pScene) override;
	void renderGui(Gui* pGui) override;
	void execute(RenderContext* pRenderContext) override;

	// Override default RenderPass functionality (that control the rendering pipeline and its GUI)
	bool requiresScene() override { return true; }       // Adds 'load scene' option to GUI.
	bool usesRayTracing() override { return true; }      // Removes a GUI control that is confusing for this simple demo
	bool usesEnvironmentMap() override { return true; }  // Use environment map to illuminate the scene
	bool hasCameraMoved();                               // Determine if there has been any camera motion.

	// Internal state variables for this pass
	RayLaunch::SharedPtr          mpRays;              ///< Wrapper around DXR pass
	RtScene::SharedPtr            mpScene;             ///< Falcor scene representation, with additions for ray tracing

	// Output buffer
	std::string                   mOutChannel;

	// User controls to switch on/off certain ray types
	bool                          mDoIndirectLighting = true;
	bool                          mDoDirectLighting = true;
	bool                          mEnableReSTIR = true;
	bool                          mDoVisibilityReuse = true;
	bool                          mDoTemporalReuse = true;
		
	int32_t                       mLightSamples = 32;  ///< Number of initial candidates (M)
	int32_t                       mRayDepth = 1;       ///< Current max. ray depth
	const int32_t                 mMaxRayDepth = 8;    ///< Max supported ray depth
	
	mat4                          mpLastCameraMatrix;
	
	// Counter to initialize thin lens random numbers each frame
	uint32_t                      mFrameCount = 0x1456u;                        ///< A frame counter to act as seed for random number generator 
};
