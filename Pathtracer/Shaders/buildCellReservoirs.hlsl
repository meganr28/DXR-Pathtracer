#include "HostDeviceSharedMacros.h"
#include "HostDeviceData.h"
#include "regirUtils.hlsli"
#include "simpleGIUtils.hlsli"

// Include and import common Falcor utilities and data structures
import Raytracing;                   // Shared ray tracing specific functions & data
import ShaderCommon;                 // Shared shading data structures
import Shading;                      // Shading functions, etc     
import Lights;                       // Light structures for our current scene

// Input and output textures
shared Texture2D<float4>   gLightGridPrev; // Track grid history for temporal reuse (TODO: make extendable to GRID_HISTORY_LENGTH)
shared RWTexture2D<float4> gLightGrid;     // ReGIR grid structure to hold light reservoirs (numCells x numReservoirsPerCell)
shared RWTexture2D<float4> gOutput;        // Output to store shaded result

[numthreads(512, 1, 1)]
void main(uint lightSlot : SV_DispatchThreadID)
{
	// One thread per light slot (reservoir) in grid cell
	// Find center of grid cell in which sample belongs
	int gridCellLinearIndex = lightSlot / gGridLightsPerCell;
	float3 gridCellCenter = mapCellIndexToWorld(gridCellLinearIndex);

	// Select light sample to be stored in light slot
	Reservoir lightSlotReservoir = sampleLightRIS(gridCellCenter);
	lightSlotReservoir.totalWeight /= lightSlotReservoir.M;
	lightSlotReservoir.M = 1;

	// TODO: Temporal reuse

	// Store in grid
	gLightGrid[gridCellLinearIndex][lightSlot] = lightSlotReservoir;
}