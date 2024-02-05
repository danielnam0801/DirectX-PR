#pragma once

#include "D3dHeader.h"

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
	void UpdateSkinnedCB(const GameTimer& gt);

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
	void BuildSkinnedModel();
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
	
	// Skinned Model 입력 배치
	std::vector<D3D12_INPUT_ELEMENT_DESC> mSkinnedInputLayout;

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

	// 스키닝 애니메이션 상수 버퍼
	ComPtr<ID3D12Resource> mSkinnedCB = nullptr;
	BYTE* mSkinnedMappedData = nullptr;
	UINT mSkinnedByteSize = 0;

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
	
	//Skinned Model 관련 변수
	std::string mSkinnedModelFilename = "..\\Models\\soldier.m3d";
	SkinnedData mSkinnedInfo;
	std::vector<M3DLoader::Subset> mSkinnedSubsets;
	std::vector<M3DLoader::M3dMaterial> mSkinnedMats;

	std::unique_ptr<SkinnedModelInstance> mSkinnedModelInst;

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