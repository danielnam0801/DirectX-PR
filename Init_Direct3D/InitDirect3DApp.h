#pragma once

#include "D3dApp.h"
#include <DirectXColors.h>
#include "../Common/MathHelper.h"
#include "../Common/GeometryGenerator.h"
using namespace DirectX;

#define MAX_LIGHTS 16

//���� ����
struct Vertex
{
	XMFLOAT3 Pos;
	XMFLOAT3 Normal;
};

// ���� ������Ʈ ��� ����
struct ObjectConstants
{
	XMFLOAT4X4 World = MathHelper::Identity4x4();
};

// ���� ���� ��� ����
struct MaterialsConstants
{
	XMFLOAT4 DiffuseAlbedo = { 1.0f, 1.0f, 1.0f, 1.0f };
	XMFLOAT3 FresnelR0 = { 0.01f, 0.01f, 0.01f };
	float Roughness = 0.25f;
};

//������ ����
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

// ���� ��� ����
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

// ���ϵ��� ����
struct GeometryInfo
{
	std::string Name;

	//���� ���� ��
	D3D12_VERTEX_BUFFER_VIEW				VertexBufferView = {};
	ComPtr<ID3D12Resource>					VertexBuffer = nullptr;

	//�ε��� ���� ��
	D3D12_INDEX_BUFFER_VIEW					IndexBufferView = {};
	ComPtr<ID3D12Resource>					IndexBuffer = nullptr;

	// ���� ����
	int VertexCount = 0;

	//�ε��� ����
	int IndexCount = 0;
};

//���� ����
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

//�������� ������Ʈ ����ü
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
	//�Է� ��ġ
	std::vector<D3D12_INPUT_ELEMENT_DESC> mInputLayout;
	
	//��Ʈ �ñ״�ó
	ComPtr<ID3D12RootSignature>				mRootSignature = nullptr;

	//���������� ���� ��ü
	ComPtr<ID3D12PipelineState>				mPSO = nullptr;

	// ���� ���̴��� �ȼ� ���̴� ����
	ComPtr<ID3DBlob> mVSByteCode = nullptr;
	ComPtr<ID3DBlob> mPSByteCode = nullptr;

	// ���� ������Ʈ ��� ����
	ComPtr<ID3D12Resource> mObjectCB = nullptr;
	BYTE* mObjectMappedData = nullptr;
	UINT mObjectByteSize = 0;

	// ���� ������Ʈ ��� ����
	ComPtr<ID3D12Resource> mMaterialCB = nullptr;
	BYTE* mMaterialMappedData = nullptr;
	UINT mMaterialByteSize = 0;

	//���� ��� ����
	ComPtr<ID3D12Resource> mPassCB = nullptr;
	BYTE* mPassMappedData = nullptr;
	UINT mPassByteSize = 0;

	// ���� ���� ��
	std::unordered_map<std::string, std::unique_ptr<GeometryInfo>> mGeoMetries;

	// ���� ���� ��
	std::unordered_map<std::string, std::unique_ptr<MaterialInfo>> mMaterials;

	//�������� ������Ʈ ����Ʈ
	std::vector<std::unique_ptr<RenderItem>> mRenderItems;

	//����  / �þ� / ���� ���
	XMFLOAT4X4 mWorld = MathHelper::Identity4x4();
	XMFLOAT4X4 mView = MathHelper::Identity4x4();
	XMFLOAT4X4 mProj = MathHelper::Identity4x4();

	//�þ� ��ġ
	XMFLOAT3 mEyePos = { 0.0f, 0.0f, 0.0f };
		
	//���� ��ǥ ���� ��
	float mTheta = 1.5f * XM_PI;
	float mPhi = XM_PIDIV4;
	float mRadius = 5.0f;

	//���콺 ��ǥ
	POINT mLastMovesePos = { 0,0 };
};