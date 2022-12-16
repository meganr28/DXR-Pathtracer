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

#include "Falcor.h"
#include "../SharedUtils/RenderingPipeline.h"
#include "Passes/LightProbeGBufferPass.h"
#include "Passes/RayTracedGBufferPass.h"
#include "Passes/BuildCellReservoirsPass.h"
#include "Passes/SampleLightGridPass.h"
#include "Passes/CreateLightSamplesPass.h"
#include "Passes/SpatialReusePass.h"
#include "Passes/ShadeWithReservoirsPass.h"
#include "Passes/SimpleAccumulationPass.h"
#include "Passes/SimpleToneMappingPass.h"
#include "Passes/FullGlobalIlluminationPass.h"
#include "Passes/DenoisingPass.h"

int WINAPI WinMain(_In_ HINSTANCE hInstance, _In_opt_ HINSTANCE hPrevInstance, _In_ LPSTR lpCmdLine, _In_ int nShowCmd)
{
	// Create our rendering pipeline
	RenderingPipeline *pipeline = new RenderingPipeline();

	RenderParams params;
	params.mEnableReSTIR = pipeline->mDoWeightedRIS;
	params.mTemporalReuse = pipeline->mDoTemporalReuse;
	params.mSpatialReuse = pipeline->mDoSpatialReuse;

	int spatialPasses = 1;

	// Add passes into our pipeline
	pipeline->setPass(0, RayTracedGBufferPass::create()); // TODO: change to ray traced g-buffer pass
	//pipeline->setPass(1, BuildCellReservoirsPass::create("HDRColorOutput")); // build grid reservoirs and temporal reuse
	//pipeline->setPass(2, SampleLightGridPass::create("HDRColorOutput")); // use grid to perform shading
	pipeline->setPass(1, CreateLightSamplesPass::create("HDRColorOutput", params)); // collect light samples and temporal reuse

	pipeline->setPass(2, SpatialReusePass::create("HDRColorOutput", params)); // spatial reuse

	for (int i = 0; i < spatialPasses; i++) {
		pipeline->setPass(3 + i, ShadeWithReservoirsPass::create("HDRColorOutput", params)); // use reservoirs to perform shading
	}
	//pipeline->setPass(3 + spatialPasses, DenoisingPass::create("HDRColorOutput"));
	pipeline->setPass(3 + spatialPasses, SimpleToneMappingPass::create("HDRColorOutput", ResourceManager::kOutputChannel));

	// Define a set of config / window parameters for our program
    SampleConfig config;
    config.windowDesc.title = "DirectX Raytracing Path Tracer";
    config.windowDesc.resizableWindow = true;

	// Start our program!
	RenderingPipeline::run(pipeline, config);
}
