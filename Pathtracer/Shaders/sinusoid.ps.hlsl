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

// A constant buffer of shader parameters populated from our C++ code
cbuffer PerFrameCB
{
    uint gFrameCount;
	float gMultValue;
}

// Our main pixel shader.  It writes a float4 to the output color buffer (SV_Target0) 
float4 main(float2 texC : TEXCOORD, float4 pos : SV_Position) : SV_Target0
{
	// Compute a per-pixel sinusoidal value
	float sinusoid = 0.5 * (1.0f + sin(0.001f * gMultValue * (dot(pos.xy, pos.xy) + gFrameCount / gMultValue) ));
	//float4 sinusoid = float4(0.5 * (pos.xy + float2(1.0, 1.0)), 0.5 * (sin(gFrameCount * 3.14159 * 0.01) + 1.0), 1.0);

	// Save our color out to our framebuffer
    return float4(sinusoid, sinusoid, sinusoid, 0.0);
	//return sinusoid;
}
