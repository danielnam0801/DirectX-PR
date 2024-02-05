#pragma once

#include "D3dApp.h"
#include "ShadowMap.h"
#include "LoadM3d.h"
#include "SkinnedData.h"
#include <DirectXColors.h>
#include "../Common/Camera.h"
#include "../Common/MathHelper.h"
#include "../Common/GeometryGenerator.h"
#include "../Common/DDSTextureLoader.h"

using namespace DirectX;

#define MAX_LIGHTS 16

//렌더링 레이어
enum class RenderLayer : int
{
	Opaque = 0,
	SkinnedOpaque,
	Transparent,
	AlphaTested,
	Debug,
	SKybox,
	Count
};
//정점 정보
struct Vertex
{
	XMFLOAT3 Pos;
	XMFLOAT3 Normal;
	XMFLOAT2 Uv;
	XMFLOAT3 Tangent;
};

// 개별 오브젝트 상수 버퍼
struct ObjectConstants
{
	XMFLOAT4X4 World = MathHelper::Identity4x4();
	XMFLOAT4X4 TexTransform = MathHelper::Identity4x4();
};

// 개별 재질 상수 버퍼
struct MatConstants
{
	XMFLOAT4 DiffuseAlbedo = { 1.0f, 1.0f, 1.0f, 1.0f };
	XMFLOAT3 FresnelR0 = { 0.01f, 0.01f, 0.01f };
	float Roughness = 0.25f;
	UINT Texture_On = 0;
	UINT Normal_On = 0;
	XMFLOAT2 padding = { 0.0f, 0.0f };
};

// 본 행렬 상수 버퍼
struct SkinnedConstants
{
	XMFLOAT4X4 BoneTransforms[96];
};

//스키닝 애니메이션
struct SkinnedModelInstance
{
	SkinnedData* SkinnedInfo = nullptr;
	std::vector<XMFLOAT4X4> FinalTransforms;
	std::string ClipName;
	float TimePos = 0.0f;

	void UpdateSkinnedAnimation(float dt)
	{
		TimePos += dt;

		if (TimePos > SkinnedInfo->GetClipEndTime(ClipName))
			TimePos = 0.0f;

		//FinalTransform
		SkinnedInfo->GetFinalTransforms(ClipName, TimePos, FinalTransforms);
	}
};

//라이팅 정보
struct LightInfo
{
	UINT LightType = 0;
	XMFLOAT3 padding = { 0.0f, 0.0f, 0.0f };
	XMFLOAT3 Strength = { 0.5f,  0.5f, 0.5f };
	float FalloffStart = 1.0f;						//point / spot
	XMFLOAT3 Direction = { 0.0f, -1.0f, 0.0f };		//direction / spot
	float FalloffEnd = 10.0f;						//point / spot
	XMFLOAT3 Position = { 0.0f, 0.0f, 0.0f };		//point / spot
	float SpotPower = 64.0f;						//spot
};

// 공용 상수 버퍼
struct PassConstants
{
	XMFLOAT4X4 View = MathHelper::Identity4x4();
	XMFLOAT4X4 InvView = MathHelper::Identity4x4();
	XMFLOAT4X4 Proj = MathHelper::Identity4x4();
	XMFLOAT4X4 InvProj = MathHelper::Identity4x4();
	XMFLOAT4X4 ViewProj = MathHelper::Identity4x4();
	XMFLOAT4X4 InvViewProj = MathHelper::Identity4x4();
	XMFLOAT4X4 ShadowTransform = MathHelper::Identity4x4();

	XMFLOAT4 AmbientLight = { 0.0f, 0.0f, 0.0f, 1.0f };
	XMFLOAT3 EyePosW = { 0.0f, 0.0f, 0.0f };
	UINT LightCount;
	LightInfo Lights[MAX_LIGHTS];

	XMFLOAT4 FogColor = { 0.7f, 0.7f, 0.7f, 1.0f };
	float FogStart = 5.0f;
	float FogRange = 150.f;
	XMFLOAT2 padding;

};

// 기하도형 정보
struct GeometryInfo
{
	std::string Name;

	//정점 버퍼 뷰
	D3D12_VERTEX_BUFFER_VIEW				VertexBufferView = {};
	ComPtr<ID3D12Resource>					VertexBuffer = nullptr;

	//인덱스 버퍼 뷰
	D3D12_INDEX_BUFFER_VIEW					IndexBufferView = {};
	ComPtr<ID3D12Resource>					IndexBuffer = nullptr;

	// 정점 갯수
	int VertexCount = 0;

	//인덱스 갯수
	int IndexCount = 0;

	UINT StartIndexLocation = 0;
	int BaseVertexLocation = 0;
};

// 텍스처 정보
struct TextureInfo
{
	std::string Name;
	std::wstring Filename;

	int DiffuseSrvHeapIndex = -1;

	ComPtr<ID3D12Resource> Resource = nullptr;
	ComPtr<ID3D12Resource> UploadHeap = nullptr;
};


//재질 정보
struct MaterialInfo
{
	std::string Name;

	int MatCBIndex = -1;
	int DiffuseSrvHeapIndex = -1;
	int NormalSrvHeapIndex = -1;

	XMFLOAT4 DiffuseAlbedo = { 1.0f, 1.0f, 1.0f, 1.0f };
	XMFLOAT3 FresnelR0 = { 0.01f, 0.01f, 0.01f };
	float Roughness = 0.25f;

};

//렌더링할 오브젝트 구조체
struct RenderItem
{
	RenderItem() = default;

	UINT ObjCBIndex = -1;

	XMFLOAT4X4 World = MathHelper::Identity4x4();
	XMFLOAT4X4 TexTransform = MathHelper::Identity4x4();
	D3D12_PRIMITIVE_TOPOLOGY PrimitiveType = D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST;

	GeometryInfo* Geo = nullptr;
	MaterialInfo* Mat = nullptr;

	int IndexCount = 0;

	// 스키닝 애니메이션
	UINT SkinnedCBIndex = -1;

	SkinnedModelInstance* SkinnedModelInst = nullptr;
};
