#pragma once

#include "../SharedUtils/RenderPass.h"
#include "../SharedUtils/RayLaunch.h"

class BuildCellReservoirsPass : public ::RenderPass, inherit_shared_from_this<::RenderPass, BuildCellReservoirsPass>
{
public:
	using SharedPtr = std::shared_ptr<BuildCellReservoirsPass>;
	using SharedConstPtr = std::shared_ptr<const BuildCellReservoirsPass>;

	static SharedPtr create(const std::string& outBuf) { return SharedPtr(new BuildCellReservoirsPass(outBuf)); }
	virtual ~BuildCellReservoirsPass() = default;

protected:
	BuildCellReservoirsPass(const std::string& outBuf);

	// RenderPass functionality
	bool initialize(RenderContext* pRenderContext, ResourceManager::SharedPtr pResManager) override;
	void initScene(RenderContext* pRenderContext, Scene::SharedPtr pScene) override;
	void renderGui(Gui* pGui) override;
	void execute(RenderContext* pRenderContext) override;

	// Override default RenderPass functionality (that control the rendering pipeline and its GUI)
	bool requiresScene() override { return true; }       // Adds 'load scene' option to GUI.
	bool usesRayTracing() override { return true; }      // Removes a GUI control that is confusing for this simple demo
	bool usesEnvironmentMap() override { return true; }  // Use environment map to illuminate the scene
	bool usesCompute() override { return true;  }        // Uses a compute pass to construct the world-space grid

	// Internal state variables for this pass
	RayLaunch::SharedPtr          mpRays;              ///< Wrapper around DXR pass
	RtScene::SharedPtr            mpScene;             ///< Falcor scene representation, with additions for ray tracing
	
	int32_t                       mRayDepth = 1;       ///< Current max. ray depth

	//ComputeVars::SharedPtr        mpComputeVars;       ///< Falcor compute vars representation
	//ComputeState::SharedPtr       mpComputeState;      ///< Falcor compute state representation

	struct
	{
		ComputeState::SharedPtr pComputeState;
		ComputeProgram::SharedPtr pComputeProgram;
		ComputeVars::SharedPtr pComputeVars;
	} mInitReservoirsPass;

	// Compute output channel
	std::string                   mOutChannel;
};
