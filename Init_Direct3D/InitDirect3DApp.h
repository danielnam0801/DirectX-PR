#pragma once

#include "D3dApp.h"
#include <DirectXColors.h>
#include "../Common/MathHelper.h"
#include "../Common/GeometryGenerator.h"
using namespace DirectX;

//정점 정보
struct Vertex
{
	XMFLOAT3 Pos;
	XMFLOAT4 Color;
};

// 개별 오브젝트 상수 버퍼
struct ObjectConstants
{
	XMFLOAT4X4 World = MathHelper::Identity4x4();
};

// 공용 상수 버퍼
struct PassConstants
{
	XMFLOAT4X4 View = MathHelper::Identity4x4();
	XMFLOAT4X4 InvView = MathHelper::Identity4x4();
	XMFLOAT4X4 Proj = MathHelper::Identity4x4();
	XMFLOAT4X4 InvProj = MathHelper::Identity4x4();
	XMFLOAT4X4 ViewProj = MathHelper::Identity4x4();
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
	void UpdatePassCB(const GameTimer& gt);
	virtual void DrawBegin(const GameTimer& gt)override;
	virtual void Draw(const GameTimer& gt)override;
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

	//오브젝트 상수 버퍼
	ComPtr<ID3D12Resource> mObjectCB = nullptr;
	BYTE* mObjectMappedData = nullptr;
	UINT mObjectByteSize = 0;

	//공용 상수 버퍼
	ComPtr<ID3D12Resource> mPassCB = nullptr;
	BYTE* mPassMappedData = nullptr;
	UINT mPassByteSize = 0;

	// 기하 구조 맵
	std::unordered_map<std::string, std::unique_ptr<GeometryInfo>> mGeoMetries;

	//월드  / 시야 / 투영 행렬
	XMFLOAT4X4 mWorld = MathHelper::Identity4x4();
	XMFLOAT4X4 mView = MathHelper::Identity4x4();
	XMFLOAT4X4 mProj = MathHelper::Identity4x4();

	//구면 좌표 제어 값
	float mTheta = 1.5f * XM_PI;
	float mPhi = XM_PIDIV4;
	float mRadius = 5.0f;

	//마우스 좌표
	POINT mLastMovesePos = { 0,0 };
};