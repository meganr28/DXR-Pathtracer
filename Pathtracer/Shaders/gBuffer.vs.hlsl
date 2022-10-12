// Access Falcor's utility functions (using Slang compiler)
#include "VertexAttrib.h"
__import ShaderCommon;
__import DefaultVS;

VertexOut main(VertexIn vIn)
{
	return defaultVS(vIn);
}