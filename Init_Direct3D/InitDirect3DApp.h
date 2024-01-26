#pragma once

#include "D3dApp.h"
#include <DirectXColors.h>
#include "../Common/MathHelper.h"
#include "../Common/GeometryGenerator.h"
using namespace DirectX;

#define MAX_LIGHTS 16

//정점 정보
struct Vertex
{
	XMFLOAT3 Pos;
	XMFLOAT3 Normal;
};

// 개별 오브젝트 상수 버퍼
struct ObjectConstants
{
	XMFLOAT4X4 World = MathHelper::Identity4x4();
};

// 개별 재질 상수 버퍼
struct MaterialsConstants
{
	XMFLOAT4 DiffuseAlbedo = { 1.0f, 1.0f, 1.0f, 1.0f };
	XMFLOAT3 FresnelR0 = { 0.01f, 0.01f, 0.01f };
	float Roughness = 0.25f;
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
	XMFLOAT4 AmbientLight = { 0.0f, 0.0f, 0.0f, 1.0f };
	XMFLOAT3 EyePosW = { 0.0f, 0.0f, 0.0f };
	UINT LightCount;
	LightInfo Lights[MAX_LIGHTS];
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

//재질 정보
struct MaterialInfo
{
	std::string Name;

	int MatCBIndex = -1;
	int DiffuseSrvHeapIndex = -1;
	int Texture_On = 0;

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
	virtual void OnResize()override;
	virtual void Update(const GameTimer& gt)override;
	void UpdateCamera(const GameTimer& gt);
	void UpdateObjectCB(const GameTimer& gt);
	void UpdateMaterialCB(const GameTimer& gt);
	void UpdatePassCB(const GameTimer& gt);

	virtual void DrawBegin(const GameTimer& gt)override;
	virtual void Draw(const GameTimer& gt)override;
	void DrawRenderItems();
	virtual void DrawEnd(const GameTimer& gt)override;

	virtual void OnMouseDown(WPARAM btnState, int x, int y) override;
	virtual void OnMouseUp(WPARAM btnState, int x, int y) override;
	virtual void OnMouseMove(WPARAM btnState, int x, int y) override;
private:
	void BuildInputLayout();
	void BuildGeometry();
	void BuildBoxGeometry();
	void BuildGridGeometry();
	void BuildSphereGeometry();
	void BuildCylinderGeometry();
	void BuildSkullGeometry();
	void BuildMaterials();
	void BuildRenderItem();
	void BuildShader();
	void BuildConstantBuffer();
	void BuildRootSignature();
	void BuildPSO();

private:
	//입력 배치
	std::vector<D3D12_INPUT_ELEMENT_DESC> mInputLayout;
	
	//루트 시그니처
	ComPtr<ID3D12RootSignature>				mRootSignature = nullptr;

	//파이프라인 상태 객체
	ComPtr<ID3D12PipelineState>				mPSO = nullptr;

	// 정점 쉐이더와 픽셀 쉐이더 변수
	ComPtr<ID3DBlob> mVSByteCode = nullptr;
	ComPtr<ID3DBlob> mPSByteCode = nullptr;

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

	// 기하 구조 맵
	std::unordered_map<std::string, std::unique_ptr<GeometryInfo>> mGeoMetries;

	// 기하 구조 맵
	std::unordered_map<std::string, std::unique_ptr<MaterialInfo>> mMaterials;

	//렌더링할 오브젝트 리스트
	std::vector<std::unique_ptr<RenderItem>> mRenderItems;

	//월드  / 시야 / 투영 행렬
	XMFLOAT4X4 mWorld = MathHelper::Identity4x4();
	XMFLOAT4X4 mView = MathHelper::Identity4x4();
	XMFLOAT4X4 mProj = MathHelper::Identity4x4();

	//시야 위치
	XMFLOAT3 mEyePos = { 0.0f, 0.0f, 0.0f };
		
	//구면 좌표 제어 값
	float mTheta = 1.5f * XM_PI;
	float mPhi = XM_PIDIV4;
	float mRadius = 5.0f;

	//마우스 좌표
	POINT mLastMovesePos = { 0,0 };
};