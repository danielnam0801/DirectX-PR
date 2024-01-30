#pragma once

#include "D3dApp.h"
#include "ShadowMap.h"
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
};

class InitDirect3DApp : public D3DApp
{
public:
	InitDirect3DApp(HINSTANCE hInstance);
	~InitDirect3DApp();

	virtual bool Initialize()override;

private:
	virtual void CreateDsvDescriptorHeaps()override;
	virtual void OnResize()override;
	virtual void Update(const GameTimer& gt)override;
	void UpdateLight(const GameTimer& gt);
	void UpdateCamera(const GameTimer& gt);
	void UpdateObjectCB(const GameTimer& gt);
	void UpdateMaterialCB(const GameTimer& gt);
	void UpdatePassCB(const GameTimer& gt);
	void UpdateShadowCB(const GameTimer& gt);

	virtual void DrawBegin(const GameTimer& gt)override;
	virtual void Draw(const GameTimer& gt)override;
	void DrawSceneToShadowMap();
	void DrawRenderItems(const std::vector<RenderItem*>& ritems);
	virtual void DrawEnd(const GameTimer& gt)override;

	virtual void OnMouseDown(WPARAM btnState, int x, int y) override;
	virtual void OnMouseUp(WPARAM btnState, int x, int y) override;
	virtual void OnMouseMove(WPARAM btnState, int x, int y) override;
private:
	void BuildInputLayout();
	void BuildGeometry();
	void BuildBoxGeometry();
	void BuildGridGeometry();
	void BuildQuadGeometry();
	void BuildSphereGeometry();
	void BuildCylinderGeometry();
	void BuildSkullGeometry();
	void BuildTextures();
	void BuildMaterials();
	void BuildRenderItem();
	void BuildShader();
	void BuildConstantBuffer();
	void BuildDescriptorHeaps();
	void BuildRootSignature();
	void BuildPSO();

private:
	//입력 배치
	std::vector<D3D12_INPUT_ELEMENT_DESC> mInputLayout;
	
	//루트 시그니처
	ComPtr<ID3D12RootSignature>				mRootSignature = nullptr;

	//쉐이더 맵
	std::unordered_map<std::string, ComPtr<ID3DBlob>> mShaders;
	
	//렌더링 파이프라인 상태 맵
	std::unordered_map<std::string, ComPtr<ID3D12PipelineState>> mPSOs;

	// 개별 오브젝트 상수 버퍼
	ComPtr<ID3D12Resource> mObjectCB = nullptr;
	BYTE* mObjectMappedData = nullptr;
	UINT mObjectByteSize = 0;

	// 재질 오브젝트 상수 버퍼
	ComPtr<ID3D12Resource> mMaterialCB = nullptr;
	BYTE* mMaterialMappedData = nullptr;
	UINT mMaterialByteSize = 0;

	//공용 상수 버퍼
	ComPtr<ID3D12Resource> mPassCB = nullptr;
	BYTE* mPassMappedData = nullptr;
	UINT mPassByteSize = 0;

	// 스카이박스 텍스처 서술자 힙 인덱스
	UINT mSkyTesHeapIndex = -1;
	
	// 그림자 맵 서술자 힙 인덱스
	UINT mShadowMapHeapIndex = -1;

	//서술자 힙
	ComPtr<ID3D12DescriptorHeap> mSrvDescriptorHeap = nullptr;

	// 기하 구조 맵
	std::unordered_map<std::string, std::unique_ptr<GeometryInfo>> mGeoMetries;

	// 재질 맵
	std::unordered_map<std::string, std::unique_ptr<MaterialInfo>> mMaterials;

	//텍스처 맵
	std::unordered_map<std::string, std::unique_ptr<TextureInfo>> mTextures;

	//렌더링할 오브젝트 리스트
	std::vector<std::unique_ptr<RenderItem>> mRenderItems;

	// 렌더링할 오브젝트 나누기 : PSO
	std::vector<RenderItem*> mRenderItemLayer[(int)RenderLayer::Count];
	
	//그림자 맵
	std::unique_ptr<ShadowMap> mShadowMap;
	//카메라 클래스
	Camera mCamera;

	float mCameraSpeed = 10.0f;

	//경계 구
	BoundingSphere mSceneBounds;
	
	// 라이트 행렬
	float mLightRotationAngle = 0.0f;
	float mLightRotationSpeed = 0.1f;
	XMFLOAT3 mBaseLightDirection = XMFLOAT3(0.57735f, -0.57735f, 0.57735f);
	XMFLOAT3 mRotatedLightDirection;

	float mLightNearZ;
	float mLightFarZ = 0.0f;
	XMFLOAT3 mLightPosW;
	XMFLOAT4X4 mLightView = MathHelper::Identity4x4();
	XMFLOAT4X4 mLightProj = MathHelper::Identity4x4();
	XMFLOAT4X4 mShadowTransform = MathHelper::Identity4x4();


	//마우스 좌표
	POINT mLastMovesePos = { 0,0 };
};