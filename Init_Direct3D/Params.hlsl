#ifndef _PARAMS_HLSL_
#define _PARAMS_HLSL_

#define MAXLIGHTS 16

struct Material
{
	float4 DiffuseAlbedo;
	float3 FresnelR0;
	float Shininess;
};

struct Light
{
	int LightType; //0 Directional, 1 : Point, 2 : Spot
	float3 LightPadding;
	float3 Strength;
	float FalloffStart;
	float3 Direction;
	float FalloffEnd;
	float3 Position;
	float SpotPower;
};

cbuffer cbPerObject : register(b0)
{
	float4x4 gWorld;
	float4x4 gTexTransform;
};

cbuffer cbMaterial : register(b1)
{
	float4 gDiffuseAlbedo;
	float3 gFresnelR0;
	float gRoughness;
	int gTexture_On;
	float3 gTexturePadding;
}

cbuffer cbPass : register(b2)
{
	float4x4 gView;
	float4x4 gInvView;
	float4x4 gProj;
	float4x4 gInvProj;
	float4x4 gViewProj;

	float4 gAmbientLight;
	float3 gEyePosW;
	int gLightCount;
	Light gLights[MAXLIGHTS];

	float4 gFogColor;
	float gFogStart;
	float gFogRange;
	float2 fogpadding;
};

TextureCube gCubeMap : register(t0);

Texture2D gTexture_0 : register(t1);

SamplerState gSampler_0 : register(s0);


#endif