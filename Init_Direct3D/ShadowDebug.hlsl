#ifndef _SHADOWDEBUG_HLSL_
#define _SHADOWDEBUG_HLSL_

#include "Params.hlsl"

struct VertexIn
{
	float3 PosL : POSITION;
	float2 Uv : TEXCOORD;
};

struct VertexOut
{
	float4 PosH : SV_Position;
	float2 Uv : TEXCOORD;
};

VertexOut VS(VertexIn vin)
{
	VertexOut vout = (VertexOut)0.0f;

	vout.PosH = float4(vin.PosL, 1.0f);
	vout.Uv = vin.Uv;

	return vout;
}

float4 PS(VertexOut pin) : SV_Target
{
	return float4(gShadowMap.Sample(gSampler_0, pin.Uv).rrr, 1.0f);
}

#endif