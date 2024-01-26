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
};

cbuffer cbMaterial : register(b1)
{
	float4 gDiffuseAlbedo;
	float3 gFresnelR0;
	float gRoughness;
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
};


#endif