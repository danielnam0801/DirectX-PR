#ifndef _COLOR_HLSL_
#define _COLOR_HLSL_

#include "Params.hlsl"
#include "LightingUtil.hlsl"

struct VertexIn
{
	float3 PosL : POSITION;
	float3 NormalL : NORMAL;
	float2 Uv : TEXCOORD;
};

struct VertexOut
{
	float4 PosH : SV_POSITION;
	float3 PosW : POSITION;
	float3 NormalW : NORMAL;
	float2 Uv : TEXCOORD;
};

VertexOut VS(VertexIn vin)
{
	VertexOut vout;

	float4 posW = mul(float4(vin.PosL, 1.0f), gWorld);
	vout.PosW = posW.xyz;
	vout.PosH = mul(posW, gViewProj);
	
	vout.NormalW = mul(vin.NormalL, (float3x3)gWorld);
	
	float4 Uv = mul(float4(vin.Uv, 0.0f, 1.0f), gTexTransform);
	vout.Uv = Uv.xy;
	
	return vout;
}

float4 PS(VertexOut pin) : SV_Target
{
	float4 diffuseAlbedo = gDiffuseAlbedo;

	if (gTexture_On)
	{
		diffuseAlbedo = gTexture_0.Sample(gSampler_0, pin.Uv) * gDiffuseAlbedo;
	}

#ifdef ALPHA_TEST
	clip(diffuseAlbedo.a - 0.1f);
#endif

	pin.NormalW = normalize(pin.NormalW);
	float3 toEyeW = normalize(gEyePosW - pin.PosW);

	float4 ambient = gAmbientLight * diffuseAlbedo;

	const float shininess = 1.0f - gRoughness;
	Material mat = { diffuseAlbedo, gFresnelR0, shininess };

	float4 directLight = ComputeLighting(gLights, gLightCount, mat, pin.PosW, pin.NormalW, toEyeW);

	float4 litColor = ambient + directLight;

	float3 r = reflect(-toEyeW, pin.NormalW);
	float4 reflectColor = gCubeMap.Sample(gSampler_0, r);
	float3 fresnelFactor = SchlickFresnel(gFresnelR0, pin.NormalW, r);
	litColor.rgb += shininess * fresnelFactor * reflectColor.rgb;

#ifdef FOG
	float distToEye = length(gEyePosW - pin.PosW);

	float fogAmount = saturate((distToEye - gFogStart) / gFogRange);
	litColor = lerp(litColor, gFogColor, fogAmount);
#endif

	litColor.a = diffuseAlbedo.a;

	return litColor;
}
#endif
