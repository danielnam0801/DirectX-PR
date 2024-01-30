#ifndef _SHADOWS_HLSL_
#define _SHADOWS_HLSL_

#include "Params.hlsl"

struct VertexIn
{
	float3 PosL : POSITION;
};

struct VertexOut
{
	float4 PosH : SV_Position;
};

VertexOut VS(VertexIn vin)
{
	VertexOut vout = (VertexOut)0.0f;

	float4 posW = mul(float4(vin.PosL, 1.0f), gWorld);

	vout.PosH = mul(posW, gViewProj);

	return vout;
}

void PS(VertexOut vin)
{
	
}

#endif