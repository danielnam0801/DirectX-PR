#include "InitDirect3DApp.h"

int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE prevInstance,
    PSTR cmdLine, int showCmd)
{
    // Enable run-time memory check for debug builds.
#if defined(DEBUG) | defined(_DEBUG)
    _CrtSetDbgFlag(_CRTDBG_ALLOC_MEM_DF | _CRTDBG_LEAK_CHECK_DF);
#endif

    try
    {
        InitDirect3DApp theApp(hInstance);
        if (!theApp.Initialize())
            return 0;

        return theApp.Run();
    }
    catch (DxException& e)
    {
        MessageBox(nullptr, e.ToString().c_str(), L"HR Failed", MB_OK);
        return 0;
    }
}

InitDirect3DApp::InitDirect3DApp(HINSTANCE hInstance)
    : D3DApp(hInstance)
{
}

InitDirect3DApp::~InitDirect3DApp()
{
}

bool InitDirect3DApp::Initialize()
{
    if (!D3DApp::Initialize())
        return false;

    //초기화 명령들을 준비하기 위해 명령 목록 재설정
    ThrowIfFailed(mCommandList->Reset(mCommandListAlloc.Get(), nullptr));

    // 그림자 맵 구축
    mShadowMap = std::make_unique<ShadowMap>(md3dDevice.Get(), 2048, 2048);

    // 경계구 셋팅
    mSceneBounds.Center = XMFLOAT3(0.0f, 0.0f, 0.0f);
    mSceneBounds.Radius = sqrtf(10.0f * 10.0f + 15.0f * 15.0f);

    // 카메라 셋팅
    mCamera.SetPosition(0.0f, 2.0f, -15.0f);

    //초기화 명령들
    BuildInputLayout();
    BuildGeometry();
    BuildSkinnedModel();
    BuildTextures();
    BuildMaterials();
    BuildRenderItem();
    BuildShader();
    BuildConstantBuffer();
    BuildDescriptorHeaps();
    BuildRootSignature();
    BuildPSO();
    
    //초기화 명령들 실행
    ThrowIfFailed(mCommandList->Close());

    ID3D12CommandList* cmdsLists[] = { mCommandList.Get() };
    mCommandQueue->ExecuteCommandLists(_countof(cmdsLists), cmdsLists);

    //초기화 완료까지 기다리기
    FlushCommandQueue();

    return true;
}

void InitDirect3DApp::CreateDsvDescriptorHeaps()
{
    D3D12_DESCRIPTOR_HEAP_DESC dsvHeapDesc;
    dsvHeapDesc.NumDescriptors = 2;
    dsvHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_DSV;
    dsvHeapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE;
    dsvHeapDesc.NodeMask = 0;
    ThrowIfFailed(md3dDevice->CreateDescriptorHeap(&dsvHeapDesc, IID_PPV_ARGS(mDsvHeap.GetAddressOf())));

    D3D12_RESOURCE_DESC depthStencilDesc;
    depthStencilDesc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
    depthStencilDesc.Alignment = 0;
    depthStencilDesc.Width = mClientWidth;
    depthStencilDesc.Height = mClientHeight;
    depthStencilDesc.DepthOrArraySize = 1;
    depthStencilDesc.MipLevels = 1;
    depthStencilDesc.Format = DXGI_FORMAT_R24G8_TYPELESS;
    depthStencilDesc.SampleDesc.Count = m4xMsaaState ? 4 : 1;
    depthStencilDesc.SampleDesc.Quality = m4xMsaaState ? (m4xMsaaQuality - 1) : 0;
    depthStencilDesc.Layout = D3D12_TEXTURE_LAYOUT_UNKNOWN;
    depthStencilDesc.Flags = D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL;

    D3D12_CLEAR_VALUE optClear;
    optClear.Format = mDepthStencilFormat;
    optClear.DepthStencil.Depth = 1.f;
    optClear.DepthStencil.Stencil = 0;

    ThrowIfFailed(md3dDevice->CreateCommittedResource(
        &CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT),
        D3D12_HEAP_FLAG_NONE,
        &depthStencilDesc,
        D3D12_RESOURCE_STATE_COMMON,
        &optClear,
        IID_PPV_ARGS(mDepthStencilBuffer.GetAddressOf())));

    D3D12_DEPTH_STENCIL_VIEW_DESC dsvDesc;
    dsvDesc.Flags = D3D12_DSV_FLAG_NONE;
    dsvDesc.ViewDimension = D3D12_DSV_DIMENSION_TEXTURE2D;
    dsvDesc.Format = mDepthStencilFormat;
    dsvDesc.Texture2D.MipSlice = 0;

    mDepthStencilView = mDsvHeap->GetCPUDescriptorHandleForHeapStart();
    md3dDevice->CreateDepthStencilView(mDepthStencilBuffer.Get(), &dsvDesc, mDepthStencilView);
}

void InitDirect3DApp::OnResize()
{
    D3DApp::OnResize();

    //창의 크기가 바뀌었을 때 , 종횡비 갱신-> 투영 행렬
    mCamera.SetLens(0.25f * MathHelper::Pi, AspectRatio(), 1.0f, 1000.0f);
}

void InitDirect3DApp::Update(const GameTimer& gt)
{
    // 구면 좌표를 직교 좌표
    UpdateLight(gt);
    UpdateCamera(gt);
    UpdateObjectCB(gt);
    UpdateMaterialCB(gt);
    UpdatePassCB(gt);
    UpdateShadowCB(gt);
    UpdateSkinnedCB(gt);
}

void InitDirect3DApp::UpdateLight(const GameTimer& gt)
{
    mLightRotationAngle += mLightRotationSpeed * gt.DeltaTime();

    XMMATRIX R = XMMatrixRotationY(mLightRotationAngle);
    XMVECTOR lightDir = XMLoadFloat3(&mBaseLightDirection);
    lightDir = XMVector3TransformNormal(lightDir, R);
    XMStoreFloat3(&mRotatedLightDirection, lightDir);

    XMVECTOR lightPos = -2.0f * lightDir * mSceneBounds.Radius;
    XMVECTOR targetPos = XMLoadFloat3(&mSceneBounds.Center);
    XMVECTOR lightUp = XMVectorSet(0.0f, 1.0f, 0.0f, 0.0f);
    XMMATRIX lightView = XMMatrixLookAtLH(lightPos, targetPos, lightUp);

    XMFLOAT3 sphereCenterLS;
    XMStoreFloat3(&sphereCenterLS, XMVector3TransformCoord(targetPos, lightView));

    float l = sphereCenterLS.x - mSceneBounds.Radius;
    float r = sphereCenterLS.x + mSceneBounds.Radius;
    float b = sphereCenterLS.y - mSceneBounds.Radius;
    float t = sphereCenterLS.y + mSceneBounds.Radius;
    float n = sphereCenterLS.z - mSceneBounds.Radius;
    float f = sphereCenterLS.z + mSceneBounds.Radius;

    mLightNearZ = n;
    mLightFarZ = f;
    XMMATRIX lightProj = XMMatrixOrthographicOffCenterLH(l, r, b, t, n, f);
    
    //NDC space [-1, + 1]^2 to Texture space [0,1]^2
    XMMATRIX T(
        0.5f, 0.0f, 0.0f, 0.0f,
        0.0f, -0.5f, 0.0f, 0.0f,
        0.0f, 0.0f, 1.0f, 0.0f,
        0.5f, 0.5f, 0.0f, 1.0f
    );

    XMMATRIX S = lightView * lightProj * T;
    XMStoreFloat3(&mLightPosW, lightPos);
    XMStoreFloat4x4(&mLightView, lightView);
    XMStoreFloat4x4(&mLightProj, lightProj);
    XMStoreFloat4x4(&mShadowTransform, S);


}

void InitDirect3DApp::UpdateCamera(const GameTimer& gt)
{
    const float dt = gt.DeltaTime();

    if (GetAsyncKeyState('W') & 0x8000)
        mCamera.Walk(mCameraSpeed * dt);

    if (GetAsyncKeyState('S') & 0x8000)
        mCamera.Walk(-mCameraSpeed * dt);

    if (GetAsyncKeyState('D') & 0x8000)
        mCamera.Strafe(mCameraSpeed * dt);

    if (GetAsyncKeyState('A') & 0x8000)
        mCamera.Strafe(-mCameraSpeed * dt);

    mCamera.UpdateViewMatrix();
}

void InitDirect3DApp::UpdateObjectCB(const GameTimer& gt)
{
    // 개별 오브젝트 상수 버퍼 갱신
    for (auto& item : mRenderItems)
    {
        XMMATRIX world = XMLoadFloat4x4(&item->World);
        XMMATRIX texTransform = XMLoadFloat4x4(&item->TexTransform);

        ObjectConstants objConstants;
        XMStoreFloat4x4(&objConstants.World, XMMatrixTranspose(world));
        XMStoreFloat4x4(&objConstants.TexTransform, XMMatrixTranspose(texTransform));
        
        UINT elementIndex = item->ObjCBIndex;
        UINT elementByteSize = (sizeof(ObjectConstants) + 255) & ~255;
        memcpy(&mObjectMappedData[elementIndex * elementByteSize], &objConstants, sizeof(ObjectConstants));
    }
}

void InitDirect3DApp::UpdateMaterialCB(const GameTimer& gt)
{
    for (auto& item : mMaterials)
    {
        MaterialInfo* mat = item.second.get();

        MatConstants matConstants;
        matConstants.DiffuseAlbedo = mat->DiffuseAlbedo;
        matConstants.FresnelR0 = mat->FresnelR0;
        matConstants.Roughness = mat->Roughness;
        matConstants.Texture_On = (mat->DiffuseSrvHeapIndex == -1) ? 0 : 1;
        matConstants.Normal_On = (mat->NormalSrvHeapIndex == -1) ? 0 : 1;

        UINT elementIndex = mat->MatCBIndex;
        UINT elementByteSize = (sizeof(MatConstants) + 255) & ~255;
        memcpy(&mMaterialMappedData[elementIndex * elementByteSize], &matConstants, sizeof(matConstants));
    }
}

void InitDirect3DApp::UpdatePassCB(const GameTimer& gt)
{
    PassConstants mainPass;
    XMMATRIX view = mCamera.GetView();
    XMMATRIX proj = mCamera.GetProj();

    XMMATRIX invView = XMMatrixInverse(&XMMatrixDeterminant(view), view);
    XMMATRIX invProj = XMMatrixInverse(&XMMatrixDeterminant(proj), proj);
    XMMATRIX viewProj = XMMatrixMultiply(view, proj);
    XMMATRIX invViewProj = XMMatrixInverse(&XMMatrixDeterminant(viewProj), viewProj);
    XMMATRIX shadowTransform = XMLoadFloat4x4(&mShadowTransform);

    XMStoreFloat4x4(&mainPass.View, XMMatrixTranspose(view));
    XMStoreFloat4x4(&mainPass.InvView, XMMatrixTranspose(invView));
    XMStoreFloat4x4(&mainPass.Proj, XMMatrixTranspose(proj));
    XMStoreFloat4x4(&mainPass.InvProj, XMMatrixTranspose(invProj));
    XMStoreFloat4x4(&mainPass.ViewProj, XMMatrixTranspose(viewProj));
    XMStoreFloat4x4(&mainPass.InvViewProj, XMMatrixTranspose(invViewProj));
    XMStoreFloat4x4(&mainPass.ShadowTransform, XMMatrixTranspose(shadowTransform));
   
    mainPass.AmbientLight = { 0.25f, 0.25f, 0.35f, 1.0f };
    mainPass.EyePosW = mCamera.GetPosition3f();
    mainPass.LightCount = 11;

    mainPass.Lights[0].LightType = 0;
    mainPass.Lights[0].Direction = mRotatedLightDirection;
    mainPass.Lights[0].Strength = { 0.6f, 0.6f, 0.6f };
    
    for (int i = 0; i < 5; ++i)
    {
        mainPass.Lights[1 + i].LightType = 1;
        mainPass.Lights[1 + i].Strength = {0.6f,0.6f,0.6f};
        mainPass.Lights[1 + i].Position = XMFLOAT3(-5.0f, 3.5f, -10.0f + i * 5.0f);
        mainPass.Lights[1 + i].FalloffStart = 2;
        mainPass.Lights[1 + i].FalloffEnd = 5;
    }

    for (int i = 0; i < 5; ++i)
    {
        mainPass.Lights[6 + i].LightType = 1;
        mainPass.Lights[6 + i].Strength = { 0.6f,0.6f,0.6f };
        mainPass.Lights[6 + i].Position = XMFLOAT3(+5.0f, 3.5f, -10.0f + i * 5.0f);
        mainPass.Lights[6 + i].FalloffStart = 2;
        mainPass.Lights[6 + i].FalloffEnd = 5;
    }

    memcpy(&mPassMappedData[0], &mainPass, sizeof(PassConstants));
}

void InitDirect3DApp::UpdateShadowCB(const GameTimer& gt)
{
    PassConstants mainPass;
    XMMATRIX view = XMLoadFloat4x4(&mLightView);
    XMMATRIX proj = XMLoadFloat4x4(&mLightProj);

    XMMATRIX invView = XMMatrixInverse(&XMMatrixDeterminant(view), view);
    XMMATRIX invProj = XMMatrixInverse(&XMMatrixDeterminant(proj), proj);
    XMMATRIX viewProj = XMMatrixMultiply(view, proj);
    XMMATRIX invViewProj = XMMatrixInverse(&XMMatrixDeterminant(viewProj), viewProj);

    XMStoreFloat4x4(&mainPass.View, XMMatrixTranspose(view));
    XMStoreFloat4x4(&mainPass.InvView, XMMatrixTranspose(invView));
    XMStoreFloat4x4(&mainPass.Proj, XMMatrixTranspose(proj));
    XMStoreFloat4x4(&mainPass.InvProj, XMMatrixTranspose(invProj));
    XMStoreFloat4x4(&mainPass.ViewProj, XMMatrixTranspose(viewProj));
    XMStoreFloat4x4(&mainPass.InvViewProj, XMMatrixTranspose(invViewProj));
    mainPass.EyePosW = mLightPosW;

    UINT passCBByteSize = (sizeof(PassConstants) + 255) & ~255;
    memcpy(&mPassMappedData[passCBByteSize], &mainPass, sizeof(PassConstants));

}

void InitDirect3DApp::UpdateSkinnedCB(const GameTimer& gt)
{
    mSkinnedModelInst->UpdateSkinnedAnimation(gt.DeltaTime());

    SkinnedConstants skinnedConstants;
    std::copy(
        std::begin(mSkinnedModelInst->FinalTransforms),
        std::end(mSkinnedModelInst->FinalTransforms),
        &skinnedConstants.BoneTransforms[0]
    );

    memcpy(&mSkinnedMappedData[0], &skinnedConstants, sizeof(SkinnedConstants));
}

void InitDirect3DApp::DrawBegin(const GameTimer& gt)
{
    ThrowIfFailed(mCommandListAlloc->Reset());
    ThrowIfFailed(mCommandList->Reset(mCommandListAlloc.Get(), nullptr));
}

void InitDirect3DApp::Draw(const GameTimer& gt)
{
    // 루트 시그니처 바인딩
    mCommandList->SetGraphicsRootSignature(mRootSignature.Get());

    // 서술자 렌더링 파이프라인 바인딩
    ID3D12DescriptorHeap* descriptorHeap[] = { mSrvDescriptorHeap.Get() };
    mCommandList->SetDescriptorHeaps(_countof(descriptorHeap), descriptorHeap);

    //1pass : 그림자 맵 그리기
    DrawSceneToShadowMap();

    //2pass : 오브젝트 렌더링에 그림자맵 더하기
    mCommandList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(
        CurrentBackBuffer(), D3D12_RESOURCE_STATE_PRESENT, D3D12_RESOURCE_STATE_RENDER_TARGET));

    mCommandList->RSSetViewports(1, &mScreenViewport);
    mCommandList->RSSetScissorRects(1, &mScissorRect);

    mCommandList->ClearRenderTargetView(CurrentBackBufferView(), Colors::LightSteelBlue, 0, nullptr);
    mCommandList->ClearDepthStencilView(DepthStencilView(),
        D3D12_CLEAR_FLAG_DEPTH | D3D12_CLEAR_FLAG_STENCIL, 1.0f, 0, 0, nullptr);

    mCommandList->OMSetRenderTargets(1, &CurrentBackBufferView(), true, &DepthStencilView());

    // 공용 상수 버퍼 바인딩
    D3D12_GPU_VIRTUAL_ADDRESS passCBAddress = mPassCB->GetGPUVirtualAddress();
    mCommandList->SetGraphicsRootConstantBufferView(2, passCBAddress);

    // 스카이박스 텍스처 바인딩
    CD3DX12_GPU_DESCRIPTOR_HANDLE skyTexDescripor(mSrvDescriptorHeap->GetGPUDescriptorHandleForHeapStart());
    skyTexDescripor.Offset(mSkyTesHeapIndex, mCbvSrvUavDescriptorSize);
    mCommandList->SetGraphicsRootDescriptorTable(3, skyTexDescripor);

    // 그림자 맵 텍스처 바인딩
    CD3DX12_GPU_DESCRIPTOR_HANDLE shadowTexDescripor(mSrvDescriptorHeap->GetGPUDescriptorHandleForHeapStart());
    shadowTexDescripor.Offset(mShadowMapHeapIndex, mCbvSrvUavDescriptorSize);
    mCommandList->SetGraphicsRootDescriptorTable(6, shadowTexDescripor);

    // 렌더링 파이프라인 설정
    mCommandList->SetPipelineState(mPSOs["opaque"].Get());
    DrawRenderItems(mRenderItemLayer[(int)RenderLayer::Opaque]);

    mCommandList->SetPipelineState(mPSOs["skinnedOpaque"].Get());
    DrawRenderItems(mRenderItemLayer[(int)RenderLayer::SkinnedOpaque]);

    mCommandList->SetPipelineState(mPSOs["alphaTested"].Get());
    DrawRenderItems(mRenderItemLayer[(int)RenderLayer::AlphaTested]);

    mCommandList->SetPipelineState(mPSOs["transparent"].Get());
    DrawRenderItems(mRenderItemLayer[(int)RenderLayer::Transparent]);
    
    mCommandList->SetPipelineState(mPSOs["debug"].Get());
    DrawRenderItems(mRenderItemLayer[(int)RenderLayer::Debug]);
    
    mCommandList->SetPipelineState(mPSOs["skybox"].Get());
    DrawRenderItems(mRenderItemLayer[(int)RenderLayer::SKybox]);

}

void InitDirect3DApp::DrawSceneToShadowMap()
{
    mCommandList->RSSetViewports(1, &mShadowMap->Viewport());
    mCommandList->RSSetScissorRects(1, &mShadowMap->ScissorRect());

    mCommandList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(mShadowMap->Resource(),
        D3D12_RESOURCE_STATE_GENERIC_READ, D3D12_RESOURCE_STATE_DEPTH_WRITE));

    mCommandList->ClearDepthStencilView(mShadowMap->Dsv(), D3D12_CLEAR_FLAG_DEPTH | D3D12_CLEAR_FLAG_STENCIL,
        1.0f, 0, 0, nullptr);

    mCommandList->OMSetRenderTargets(0, nullptr, false, &mShadowMap->Dsv());

    // 그림자 맵 공용 상수 버퍼 설정
    UINT passCBByyteSize = (sizeof(PassConstants) + 255) & ~255;
    D3D12_GPU_VIRTUAL_ADDRESS passCBAddress = mPassCB->GetGPUVirtualAddress() + passCBByyteSize;
    mCommandList->SetGraphicsRootConstantBufferView(2, passCBAddress);

    mCommandList->SetPipelineState(mPSOs["shadow"].Get());
    DrawRenderItems(mRenderItemLayer[(int)RenderLayer::Opaque]);

    mCommandList->SetPipelineState(mPSOs["skinnedShadow"].Get());
    DrawRenderItems(mRenderItemLayer[(int)RenderLayer::SkinnedOpaque]);

    mCommandList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(mShadowMap->Resource(),
        D3D12_RESOURCE_STATE_DEPTH_WRITE, D3D12_RESOURCE_STATE_GENERIC_READ));

}

void InitDirect3DApp::DrawRenderItems(const std::vector<RenderItem*>& ritems)
{
    UINT objCBByteSize = (sizeof(ObjectConstants) + 255) & ~255;
    UINT matCBByteSize = (sizeof(MatConstants) + 255) & ~255;
    UINT skinnedCBByteSize = (sizeof(SkinnedConstants) + 255) & ~255;

    for (size_t i = 0; i < ritems.size(); ++i)
    {
        auto item = ritems[i];

        //개별 오브젝트 상수 버퍼 뷰 설정
        D3D12_GPU_VIRTUAL_ADDRESS objCBAddress = mObjectCB->GetGPUVirtualAddress();
        objCBAddress += item->ObjCBIndex * objCBByteSize;
        
        mCommandList->SetGraphicsRootConstantBufferView(0, objCBAddress);

        //개별 재질 상수 버퍼 뷰 설정
        D3D12_GPU_VIRTUAL_ADDRESS matCBAddress = mMaterialCB->GetGPUVirtualAddress();
        matCBAddress += item->Mat->MatCBIndex * matCBByteSize;

        mCommandList->SetGraphicsRootConstantBufferView(1, matCBAddress);

        // 텍스처 버퍼 서술자 뷰 설정
        CD3DX12_GPU_DESCRIPTOR_HANDLE tex(mSrvDescriptorHeap->GetGPUDescriptorHandleForHeapStart());
        tex.Offset(item->Mat->DiffuseSrvHeapIndex, mCbvSrvUavDescriptorSize);

        if (item->Mat->DiffuseSrvHeapIndex != -1)
        {
            mCommandList->SetGraphicsRootDescriptorTable(4, tex);
        }

        // normal텍스처 버퍼 서술자 뷰 설정
        CD3DX12_GPU_DESCRIPTOR_HANDLE normal(mSrvDescriptorHeap->GetGPUDescriptorHandleForHeapStart());
        normal.Offset(item->Mat->NormalSrvHeapIndex, mCbvSrvUavDescriptorSize);

        if (item->Mat->NormalSrvHeapIndex != -1)
        {
            mCommandList->SetGraphicsRootDescriptorTable(5, normal);
        }

        if (item->SkinnedCBIndex != -1)
        {
            D3D12_GPU_VIRTUAL_ADDRESS skinnedCBAddress = mSkinnedCB->GetGPUVirtualAddress();
            skinnedCBAddress += item->SkinnedCBIndex * skinnedCBByteSize;
            mCommandList->SetGraphicsRootConstantBufferView(7, skinnedCBAddress);
        }

        mCommandList->IASetVertexBuffers(0, 1, &item->Geo->VertexBufferView);
        mCommandList->IASetIndexBuffer(&item->Geo->IndexBufferView);
        mCommandList->IASetPrimitiveTopology(item->PrimitiveType);

        mCommandList->DrawIndexedInstanced(
            item->Geo->IndexCount,
            1,
            item->Geo->StartIndexLocation, 
            item->Geo->BaseVertexLocation, 
            0);

    }

}

void InitDirect3DApp::DrawEnd(const GameTimer& gt)
{
    mCommandList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(CurrentBackBuffer(),
        D3D12_RESOURCE_STATE_RENDER_TARGET, D3D12_RESOURCE_STATE_PRESENT));

    ThrowIfFailed(mCommandList->Close());

    ID3D12CommandList* cmdsLists[] = { mCommandList.Get() };
    mCommandQueue->ExecuteCommandLists(_countof(cmdsLists), cmdsLists);
    
    ThrowIfFailed(mSwapChain->Present(0, 0));
    mCurrentBackBuffer = (mCurrentBackBuffer + 1) % SwapChainBufferCount;

    FlushCommandQueue();
}

void InitDirect3DApp::OnMouseDown(WPARAM btnState, int x, int y)
{
    mLastMovesePos.x = x;
    mLastMovesePos.y = y;
    
    SetCapture(mhMainWnd);
}

void InitDirect3DApp::OnMouseUp(WPARAM btnState, int x, int y)
{
    ReleaseCapture();
}

void InitDirect3DApp::OnMouseMove(WPARAM btnState, int x, int y)
{
    if ((btnState & MK_LBUTTON) != 0)
    {
        float dx = XMConvertToRadians(0.25f * static_cast<float>(x - mLastMovesePos.x));
        float dy = XMConvertToRadians(0.25f * static_cast<float>(y - mLastMovesePos.y));
        
        mCamera.Pitch(dy);
        mCamera.RotateY(dx);
    }

    mLastMovesePos.x = x;
    mLastMovesePos.y = y;
}

void InitDirect3DApp::BuildInputLayout()
{
    mInputLayout =
    {
        {"POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0},
        {"NORMAL", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 12, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0},
        {"TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT, 0, 24, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0},
        {"TANGENT", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 32, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0}
    };

    mSkinnedInputLayout = 
    {
       {"POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0},
       {"NORMAL", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 12, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0},
       {"TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT, 0, 24, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0},
       {"TANGENT", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 32, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0},
       {"WEIGHTS", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 44, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0},
       {"BONEINDICES", 0, DXGI_FORMAT_R8G8B8A8_UINT, 0, 56, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0},
    };
}

void InitDirect3DApp::BuildGeometry()
{
    BuildBoxGeometry();
    BuildGridGeometry();
    BuildQuadGeometry();
    BuildSphereGeometry();
    BuildCylinderGeometry();
    BuildSkullGeometry();
}

void InitDirect3DApp::BuildBoxGeometry()
{
    GeometryGenerator geoGen;
    GeometryGenerator::MeshData box = geoGen.CreateBox(1.5f, 0.5f, 1.5f, 3);

    std::vector<Vertex> vertices(box.Vertices.size());

    for (UINT i = 0; i < box.Vertices.size(); ++i)
    {
        vertices[i].Pos = box.Vertices[i].Position;
        vertices[i].Normal = box.Vertices[i].Normal;
        vertices[i].Uv = box.Vertices[i].TexC;
        vertices[i].Tangent = box.Vertices[i].TangentU;
    }

    std::vector<std::uint16_t> indices;
    indices.insert(indices.end(), std::begin(box.GetIndices16()), std::end(box.GetIndices16()));

    auto geo = std::make_unique<GeometryInfo>();
    geo->Name = "Box";
    geo->VertexCount = (UINT)vertices.size();
    const UINT vbByteSize = geo->VertexCount * sizeof(Vertex);

    D3D12_HEAP_PROPERTIES heapProperty = CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD);
    D3D12_RESOURCE_DESC desc = CD3DX12_RESOURCE_DESC::Buffer(vbByteSize);

    md3dDevice->CreateCommittedResource(
        &heapProperty,
        D3D12_HEAP_FLAG_NONE,
        &desc,
        D3D12_RESOURCE_STATE_GENERIC_READ,
        nullptr,
        IID_PPV_ARGS(&geo->VertexBuffer));
    
    void* vertexDataBuffer = nullptr;
    CD3DX12_RANGE vertexRange(0, 0);
    geo->VertexBuffer->Map(0, &vertexRange, &vertexDataBuffer);
    memcpy(vertexDataBuffer, vertices.data(), vbByteSize);
    geo->VertexBuffer->Unmap(0, nullptr);

    geo->VertexBufferView.BufferLocation = geo->VertexBuffer->GetGPUVirtualAddress();
    geo->VertexBufferView.StrideInBytes = sizeof(Vertex);
    geo->VertexBufferView.SizeInBytes = vbByteSize;

    //인덱스 버퍼 만들기
    geo->IndexCount = (UINT)indices.size();
    const UINT ibByteSize = geo->IndexCount * sizeof(std::uint16_t);

    heapProperty = CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD);
    desc = CD3DX12_RESOURCE_DESC::Buffer(ibByteSize);

    md3dDevice->CreateCommittedResource(
        &heapProperty,
        D3D12_HEAP_FLAG_NONE,
        &desc,
        D3D12_RESOURCE_STATE_GENERIC_READ,
        nullptr,
        IID_PPV_ARGS(&geo->IndexBuffer));

    void* indexDataBuffer = nullptr;
    CD3DX12_RANGE indexRange(0, 0);
    geo->IndexBuffer->Map(0, &indexRange, &indexDataBuffer);
    memcpy(indexDataBuffer, indices.data(), ibByteSize);
    geo->IndexBuffer->Unmap(0, nullptr);
    
    geo->IndexBufferView.BufferLocation = geo->IndexBuffer->GetGPUVirtualAddress();
    geo->IndexBufferView.Format = DXGI_FORMAT_R16_UINT;
    geo->IndexBufferView.SizeInBytes = ibByteSize;

    mGeoMetries[geo->Name] = std::move(geo);

}

void InitDirect3DApp::BuildGridGeometry()
{
    GeometryGenerator geoGen;
    GeometryGenerator::MeshData grid = geoGen.CreateGrid(20.0f, 30.0f, 60, 40);

    std::vector<Vertex> vertices(grid.Vertices.size());

    for (UINT i = 0; i < grid.Vertices.size(); ++i)
    {
        vertices[i].Pos = grid.Vertices[i].Position;
        vertices[i].Normal = grid.Vertices[i].Normal;
        vertices[i].Uv = grid.Vertices[i].TexC;
        vertices[i].Tangent = grid.Vertices[i].TangentU;
    }

    std::vector<std::uint16_t> indices;
    indices.insert(indices.end(), std::begin(grid.GetIndices16()), std::end(grid.GetIndices16()));

    auto geo = std::make_unique<GeometryInfo>();
    geo->Name = "Grid";
    geo->VertexCount = (UINT)vertices.size();
    const UINT vbByteSize = geo->VertexCount * sizeof(Vertex);

    D3D12_HEAP_PROPERTIES heapProperty = CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD);
    D3D12_RESOURCE_DESC desc = CD3DX12_RESOURCE_DESC::Buffer(vbByteSize);

    md3dDevice->CreateCommittedResource(
        &heapProperty,
        D3D12_HEAP_FLAG_NONE,
        &desc,
        D3D12_RESOURCE_STATE_GENERIC_READ,
        nullptr,
        IID_PPV_ARGS(&geo->VertexBuffer));

    void* vertexDataBuffer = nullptr;
    CD3DX12_RANGE vertexRange(0, 0);
    geo->VertexBuffer->Map(0, &vertexRange, &vertexDataBuffer);
    memcpy(vertexDataBuffer, vertices.data(), vbByteSize);
    geo->VertexBuffer->Unmap(0, nullptr);

    geo->VertexBufferView.BufferLocation = geo->VertexBuffer->GetGPUVirtualAddress();
    geo->VertexBufferView.StrideInBytes = sizeof(Vertex);
    geo->VertexBufferView.SizeInBytes = vbByteSize;

    //인덱스 버퍼 만들기
    geo->IndexCount = (UINT)indices.size();
    const UINT ibByteSize = geo->IndexCount * sizeof(std::uint16_t);

    heapProperty = CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD);
    desc = CD3DX12_RESOURCE_DESC::Buffer(ibByteSize);

    md3dDevice->CreateCommittedResource(
        &heapProperty,
        D3D12_HEAP_FLAG_NONE,
        &desc,
        D3D12_RESOURCE_STATE_GENERIC_READ,
        nullptr,
        IID_PPV_ARGS(&geo->IndexBuffer));

    void* indexDataBuffer = nullptr;
    CD3DX12_RANGE indexRange(0, 0);
    geo->IndexBuffer->Map(0, &indexRange, &indexDataBuffer);
    memcpy(indexDataBuffer, indices.data(), ibByteSize);
    geo->IndexBuffer->Unmap(0, nullptr);

    geo->IndexBufferView.BufferLocation = geo->IndexBuffer->GetGPUVirtualAddress();
    geo->IndexBufferView.Format = DXGI_FORMAT_R16_UINT;
    geo->IndexBufferView.SizeInBytes = ibByteSize;

    mGeoMetries[geo->Name] = std::move(geo);

}

void InitDirect3DApp::BuildQuadGeometry()
{
    GeometryGenerator geoGen;
    GeometryGenerator::MeshData quad = geoGen.CreateQuad(0.0f, 0.0f, 1.0f, 1.0f, 0.0f);

    std::vector<Vertex> vertices(quad.Vertices.size());

    for (UINT i = 0; i < quad.Vertices.size(); ++i)
    {
        vertices[i].Pos = quad.Vertices[i].Position;
        vertices[i].Normal = quad.Vertices[i].Normal;
        vertices[i].Uv = quad.Vertices[i].TexC;
        vertices[i].Tangent = quad.Vertices[i].TangentU;
    }

    std::vector<std::uint16_t> indices;
    indices.insert(indices.end(), std::begin(quad.GetIndices16()), std::end(quad.GetIndices16()));

    auto geo = std::make_unique<GeometryInfo>();
    geo->Name = "Quad";
    geo->VertexCount = (UINT)vertices.size();
    const UINT vbByteSize = geo->VertexCount * sizeof(Vertex);

    D3D12_HEAP_PROPERTIES heapProperty = CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD);
    D3D12_RESOURCE_DESC desc = CD3DX12_RESOURCE_DESC::Buffer(vbByteSize);

    md3dDevice->CreateCommittedResource(
        &heapProperty,
        D3D12_HEAP_FLAG_NONE,
        &desc,
        D3D12_RESOURCE_STATE_GENERIC_READ,
        nullptr,
        IID_PPV_ARGS(&geo->VertexBuffer));

    void* vertexDataBuffer = nullptr;
    CD3DX12_RANGE vertexRange(0, 0);
    geo->VertexBuffer->Map(0, &vertexRange, &vertexDataBuffer);
    memcpy(vertexDataBuffer, vertices.data(), vbByteSize);
    geo->VertexBuffer->Unmap(0, nullptr);

    geo->VertexBufferView.BufferLocation = geo->VertexBuffer->GetGPUVirtualAddress();
    geo->VertexBufferView.StrideInBytes = sizeof(Vertex);
    geo->VertexBufferView.SizeInBytes = vbByteSize;

    //인덱스 버퍼 만들기
    geo->IndexCount = (UINT)indices.size();
    const UINT ibByteSize = geo->IndexCount * sizeof(std::uint16_t);

    heapProperty = CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD);
    desc = CD3DX12_RESOURCE_DESC::Buffer(ibByteSize);

    md3dDevice->CreateCommittedResource(
        &heapProperty,
        D3D12_HEAP_FLAG_NONE,
        &desc,
        D3D12_RESOURCE_STATE_GENERIC_READ,
        nullptr,
        IID_PPV_ARGS(&geo->IndexBuffer));

    void* indexDataBuffer = nullptr;
    CD3DX12_RANGE indexRange(0, 0);
    geo->IndexBuffer->Map(0, &indexRange, &indexDataBuffer);
    memcpy(indexDataBuffer, indices.data(), ibByteSize);
    geo->IndexBuffer->Unmap(0, nullptr);

    geo->IndexBufferView.BufferLocation = geo->IndexBuffer->GetGPUVirtualAddress();
    geo->IndexBufferView.Format = DXGI_FORMAT_R16_UINT;
    geo->IndexBufferView.SizeInBytes = ibByteSize;

    mGeoMetries[geo->Name] = std::move(geo);
}

void InitDirect3DApp::BuildSphereGeometry()
{
    GeometryGenerator geoGen;
    GeometryGenerator::MeshData sphere = geoGen.CreateSphere(0.5f, 20, 20);

    std::vector<Vertex> vertices(sphere.Vertices.size());

    for (UINT i = 0; i < sphere.Vertices.size(); ++i)
    {
        vertices[i].Pos = sphere.Vertices[i].Position;
        vertices[i].Normal = sphere.Vertices[i].Normal;
        vertices[i].Uv = sphere.Vertices[i].TexC;
        vertices[i].Tangent = sphere.Vertices[i].TangentU;
    }

    std::vector<std::uint16_t> indices;
    indices.insert(indices.end(), std::begin(sphere.GetIndices16()), std::end(sphere.GetIndices16()));

    auto geo = std::make_unique<GeometryInfo>();
    geo->Name = "Sphere";
    geo->VertexCount = (UINT)vertices.size();
    const UINT vbByteSize = geo->VertexCount * sizeof(Vertex);

    D3D12_HEAP_PROPERTIES heapProperty = CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD);
    D3D12_RESOURCE_DESC desc = CD3DX12_RESOURCE_DESC::Buffer(vbByteSize);

    md3dDevice->CreateCommittedResource(
        &heapProperty,
        D3D12_HEAP_FLAG_NONE,
        &desc,
        D3D12_RESOURCE_STATE_GENERIC_READ,
        nullptr,
        IID_PPV_ARGS(&geo->VertexBuffer));

    void* vertexDataBuffer = nullptr;
    CD3DX12_RANGE vertexRange(0, 0);
    geo->VertexBuffer->Map(0, &vertexRange, &vertexDataBuffer);
    memcpy(vertexDataBuffer, vertices.data(), vbByteSize);
    geo->VertexBuffer->Unmap(0, nullptr);

    geo->VertexBufferView.BufferLocation = geo->VertexBuffer->GetGPUVirtualAddress();
    geo->VertexBufferView.StrideInBytes = sizeof(Vertex);
    geo->VertexBufferView.SizeInBytes = vbByteSize;

    //인덱스 버퍼 만들기
    geo->IndexCount = (UINT)indices.size();
    const UINT ibByteSize = geo->IndexCount * sizeof(std::uint16_t);

    heapProperty = CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD);
    desc = CD3DX12_RESOURCE_DESC::Buffer(ibByteSize);

    md3dDevice->CreateCommittedResource(
        &heapProperty,
        D3D12_HEAP_FLAG_NONE,
        &desc,
        D3D12_RESOURCE_STATE_GENERIC_READ,
        nullptr,
        IID_PPV_ARGS(&geo->IndexBuffer));

    void* indexDataBuffer = nullptr;
    CD3DX12_RANGE indexRange(0, 0);
    geo->IndexBuffer->Map(0, &indexRange, &indexDataBuffer);
    memcpy(indexDataBuffer, indices.data(), ibByteSize);
    geo->IndexBuffer->Unmap(0, nullptr);

    geo->IndexBufferView.BufferLocation = geo->IndexBuffer->GetGPUVirtualAddress();
    geo->IndexBufferView.Format = DXGI_FORMAT_R16_UINT;
    geo->IndexBufferView.SizeInBytes = ibByteSize;

    mGeoMetries[geo->Name] = std::move(geo);
}

void InitDirect3DApp::BuildCylinderGeometry()
{
    GeometryGenerator geoGen;
    GeometryGenerator::MeshData cylinder = geoGen.CreateCylinder(0.5f, 0.3f, 3.0f, 20, 20);

    std::vector<Vertex> vertices(cylinder.Vertices.size());

    for (UINT i = 0; i < cylinder.Vertices.size(); ++i)
    {
        vertices[i].Pos = cylinder.Vertices[i].Position;
        vertices[i].Normal = cylinder.Vertices[i].Normal;
        vertices[i].Uv = cylinder.Vertices[i].TexC;
        vertices[i].Tangent = cylinder.Vertices[i].TangentU;
    }

    std::vector<std::uint16_t> indices;
    indices.insert(indices.end(), std::begin(cylinder.GetIndices16()), std::end(cylinder.GetIndices16()));

    auto geo = std::make_unique<GeometryInfo>();
    geo->Name = "Cylinder";
    geo->VertexCount = (UINT)vertices.size();
    const UINT vbByteSize = geo->VertexCount * sizeof(Vertex);

    D3D12_HEAP_PROPERTIES heapProperty = CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD);
    D3D12_RESOURCE_DESC desc = CD3DX12_RESOURCE_DESC::Buffer(vbByteSize);

    md3dDevice->CreateCommittedResource(
        &heapProperty,
        D3D12_HEAP_FLAG_NONE,
        &desc,
        D3D12_RESOURCE_STATE_GENERIC_READ,
        nullptr,
        IID_PPV_ARGS(&geo->VertexBuffer));

    void* vertexDataBuffer = nullptr;
    CD3DX12_RANGE vertexRange(0, 0);
    geo->VertexBuffer->Map(0, &vertexRange, &vertexDataBuffer);
    memcpy(vertexDataBuffer, vertices.data(), vbByteSize);
    geo->VertexBuffer->Unmap(0, nullptr);

    geo->VertexBufferView.BufferLocation = geo->VertexBuffer->GetGPUVirtualAddress();
    geo->VertexBufferView.StrideInBytes = sizeof(Vertex);
    geo->VertexBufferView.SizeInBytes = vbByteSize;

    //인덱스 버퍼 만들기
    geo->IndexCount = (UINT)indices.size();
    const UINT ibByteSize = geo->IndexCount * sizeof(std::uint16_t);

    heapProperty = CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD);
    desc = CD3DX12_RESOURCE_DESC::Buffer(ibByteSize);

    md3dDevice->CreateCommittedResource(
        &heapProperty,
        D3D12_HEAP_FLAG_NONE,
        &desc,
        D3D12_RESOURCE_STATE_GENERIC_READ,
        nullptr,
        IID_PPV_ARGS(&geo->IndexBuffer));

    void* indexDataBuffer = nullptr;
    CD3DX12_RANGE indexRange(0, 0);
    geo->IndexBuffer->Map(0, &indexRange, &indexDataBuffer);
    memcpy(indexDataBuffer, indices.data(), ibByteSize);
    geo->IndexBuffer->Unmap(0, nullptr);

    geo->IndexBufferView.BufferLocation = geo->IndexBuffer->GetGPUVirtualAddress();
    geo->IndexBufferView.Format = DXGI_FORMAT_R16_UINT;
    geo->IndexBufferView.SizeInBytes = ibByteSize;

    mGeoMetries[geo->Name] = std::move(geo);
}

void InitDirect3DApp::BuildSkullGeometry()
{
    std::ifstream fin("../Models/skull.txt");
    if (!fin)
    {
        MessageBox(0, L"../Models/skull.txt not found.", 0, 0);
        return;
    }

    UINT vCount = 0;
    UINT tCount = 0;
    std::string ignore;
    
    fin >> ignore >> vCount;
    fin >> ignore >> tCount;
    fin >> ignore >> ignore >> ignore >> ignore;

    std::vector<Vertex> vertices(vCount);
    for (UINT i = 0; i < vCount; ++i)
    {
        fin >> vertices[i].Pos.x >> vertices[i].Pos.y >> vertices[i].Pos.z;
        fin >> vertices[i].Normal.x >> vertices[i].Normal.y >> vertices[i].Normal.z;
    }

    fin >> ignore;
    fin >> ignore;
    fin >> ignore;
    
    std::vector<std::int32_t> indices(3 * tCount);
    for (UINT i = 0; i < tCount; ++i)
    {
        fin >> indices[i * 3 + 0] >> indices[i * 3 + 1] >> indices[i * 3 + 2];
    }

    auto geo = std::make_unique<GeometryInfo>();
    geo->Name = "Skull";
    geo->VertexCount = (UINT)vertices.size();
    const UINT vbByteSize = geo->VertexCount * sizeof(Vertex);

    D3D12_HEAP_PROPERTIES heapProperty = CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD);
    D3D12_RESOURCE_DESC desc = CD3DX12_RESOURCE_DESC::Buffer(vbByteSize);

    md3dDevice->CreateCommittedResource(
        &heapProperty,
        D3D12_HEAP_FLAG_NONE,
        &desc,
        D3D12_RESOURCE_STATE_GENERIC_READ,
        nullptr,
        IID_PPV_ARGS(&geo->VertexBuffer));

    void* vertexDataBuffer = nullptr;
    CD3DX12_RANGE vertexRange(0, 0);
    geo->VertexBuffer->Map(0, &vertexRange, &vertexDataBuffer);
    memcpy(vertexDataBuffer, vertices.data(), vbByteSize);
    geo->VertexBuffer->Unmap(0, nullptr);

    geo->VertexBufferView.BufferLocation = geo->VertexBuffer->GetGPUVirtualAddress();
    geo->VertexBufferView.StrideInBytes = sizeof(Vertex);
    geo->VertexBufferView.SizeInBytes = vbByteSize;

    //인덱스 버퍼 만들기
    geo->IndexCount = (UINT)indices.size();
    const UINT ibByteSize = geo->IndexCount * sizeof(std::int32_t);

    heapProperty = CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD);
    desc = CD3DX12_RESOURCE_DESC::Buffer(ibByteSize);

    md3dDevice->CreateCommittedResource(
        &heapProperty,
        D3D12_HEAP_FLAG_NONE,
        &desc,
        D3D12_RESOURCE_STATE_GENERIC_READ,
        nullptr,
        IID_PPV_ARGS(&geo->IndexBuffer));

    void* indexDataBuffer = nullptr;
    CD3DX12_RANGE indexRange(0, 0);
    geo->IndexBuffer->Map(0, &indexRange, &indexDataBuffer);
    memcpy(indexDataBuffer, indices.data(), ibByteSize);
    geo->IndexBuffer->Unmap(0, nullptr);

    geo->IndexBufferView.BufferLocation = geo->IndexBuffer->GetGPUVirtualAddress();
    geo->IndexBufferView.Format = DXGI_FORMAT_R32_UINT;
    geo->IndexBufferView.SizeInBytes = ibByteSize;

    mGeoMetries[geo->Name] = std::move(geo);

    fin.close();
}

void InitDirect3DApp::BuildSkinnedModel()
{
    std::vector<M3DLoader::SkinnedVertex> vertices;
    std::vector<std::uint16_t> indices;

    M3DLoader m3dLoader;
    m3dLoader.LoadM3d(mSkinnedModelFilename, vertices, indices, mSkinnedSubsets, mSkinnedMats, mSkinnedInfo);
        
    mSkinnedModelInst = std::make_unique<SkinnedModelInstance>();
    mSkinnedModelInst->SkinnedInfo = &mSkinnedInfo;;
    mSkinnedModelInst->FinalTransforms.resize(mSkinnedInfo.BoneCount());
    mSkinnedModelInst->ClipName = "Take1";
    mSkinnedModelInst->TimePos = 0.0f;
 
    const UINT vbByteSize = (UINT)vertices.size() * sizeof(M3DLoader::SkinnedVertex);
    const UINT ibByteSize = (UINT)indices.size() * sizeof(std::uint16_t);

    for (UINT i = 0; i < (UINT)mSkinnedSubsets.size(); ++i)
    {
        auto geo = std::make_unique<GeometryInfo>();

        //정점 버퍼 만들기
        geo->VertexCount = (UINT)vertices.size();
        const UINT vbByteSize = geo->VertexCount * sizeof(M3DLoader::SkinnedVertex);

        D3D12_HEAP_PROPERTIES heapProperty = CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD);
        D3D12_RESOURCE_DESC desc = CD3DX12_RESOURCE_DESC::Buffer(vbByteSize);

        md3dDevice->CreateCommittedResource(
            &heapProperty,
            D3D12_HEAP_FLAG_NONE,
            &desc,
            D3D12_RESOURCE_STATE_GENERIC_READ,
            nullptr,
            IID_PPV_ARGS(&geo->VertexBuffer));

        void* vertexDataBuffer = nullptr;
        CD3DX12_RANGE vertexRange(0, 0);
        geo->VertexBuffer->Map(0, &vertexRange, &vertexDataBuffer);
        memcpy(vertexDataBuffer, vertices.data(), vbByteSize);
        geo->VertexBuffer->Unmap(0, nullptr);

        geo->VertexBufferView.BufferLocation = geo->VertexBuffer->GetGPUVirtualAddress();
        geo->VertexBufferView.StrideInBytes = sizeof(M3DLoader::SkinnedVertex);
        geo->VertexBufferView.SizeInBytes = vbByteSize;

        //인덱스 버퍼 만들기
        geo->IndexCount = (UINT)indices.size();
        const UINT ibByteSize = geo->IndexCount * sizeof(std::uint16_t);

        heapProperty = CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD);
        desc = CD3DX12_RESOURCE_DESC::Buffer(ibByteSize);

        md3dDevice->CreateCommittedResource(
            &heapProperty,
            D3D12_HEAP_FLAG_NONE,
            &desc,
            D3D12_RESOURCE_STATE_GENERIC_READ,
            nullptr,
            IID_PPV_ARGS(&geo->IndexBuffer));

        void* indexDataBuffer = nullptr;
        CD3DX12_RANGE indexRange(0, 0);
        geo->IndexBuffer->Map(0, &indexRange, &indexDataBuffer);
        memcpy(indexDataBuffer, indices.data(), ibByteSize);
        geo->IndexBuffer->Unmap(0, nullptr);

        geo->IndexBufferView.BufferLocation = geo->IndexBuffer->GetGPUVirtualAddress();
        geo->IndexBufferView.Format = DXGI_FORMAT_R16_UINT;
        geo->IndexBufferView.SizeInBytes = ibByteSize;

        geo->IndexCount = (UINT)mSkinnedSubsets[i].FaceCount * 3;
        geo->StartIndexLocation = mSkinnedSubsets[i].FaceStart * 3;
        geo->BaseVertexLocation = 0;
        geo->Name = "sm_" + std::to_string(i);

        mGeoMetries[geo->Name] = std::move(geo);
    }
}

void InitDirect3DApp::BuildTextures()
{
    UINT indexCount = 0;

    auto bricksTex = std::make_unique<TextureInfo>();
    bricksTex->Name = "bricks";
    bricksTex->Filename = L"../Textures/bricks.dds";
    bricksTex->DiffuseSrvHeapIndex = indexCount++;
    ThrowIfFailed(CreateDDSTextureFromFile12(md3dDevice.Get(),
        mCommandList.Get(),
        bricksTex->Filename.c_str(),
        bricksTex->Resource,
        bricksTex->UploadHeap));
    mTextures[bricksTex->Name] = std::move(bricksTex);

    auto bricksNormalTex = std::make_unique<TextureInfo>();
    bricksNormalTex->Name = "bricksNormal";
    bricksNormalTex->Filename = L"../Textures/bricks_nmap.dds";
    bricksNormalTex->DiffuseSrvHeapIndex = indexCount++;
    ThrowIfFailed(CreateDDSTextureFromFile12(md3dDevice.Get(),
        mCommandList.Get(),
        bricksNormalTex->Filename.c_str(),
        bricksNormalTex->Resource,
        bricksNormalTex->UploadHeap));
    mTextures[bricksNormalTex->Name] = std::move(bricksNormalTex);

    auto stoneTex = std::make_unique<TextureInfo>();
    stoneTex->Name = "stone";
    stoneTex->Filename = L"../Textures/stone.dds";
    stoneTex->DiffuseSrvHeapIndex = indexCount++;
    ThrowIfFailed(CreateDDSTextureFromFile12(md3dDevice.Get(),
        mCommandList.Get(),
        stoneTex->Filename.c_str(),
        stoneTex->Resource,
        stoneTex->UploadHeap));
    mTextures[stoneTex->Name] = std::move(stoneTex);

    auto tileTex = std::make_unique<TextureInfo>();
    tileTex->Name = "tile";
    tileTex->Filename = L"../Textures/tile.dds";
    tileTex->DiffuseSrvHeapIndex = indexCount++;
    ThrowIfFailed(CreateDDSTextureFromFile12(md3dDevice.Get(),
        mCommandList.Get(),
        tileTex->Filename.c_str(),
        tileTex->Resource,
        tileTex->UploadHeap));
    mTextures[tileTex->Name] = std::move(tileTex);

    auto tileNormalTex = std::make_unique<TextureInfo>();
    tileNormalTex->Name = "tileNormal";
    tileNormalTex->Filename = L"../Textures/tile_nmap.dds";
    tileNormalTex->DiffuseSrvHeapIndex = indexCount++;
    ThrowIfFailed(CreateDDSTextureFromFile12(md3dDevice.Get(),
        mCommandList.Get(),
        tileNormalTex->Filename.c_str(),
        tileNormalTex->Resource,
        tileNormalTex->UploadHeap));
    mTextures[tileNormalTex->Name] = std::move(tileNormalTex);

    auto fenceTex = std::make_unique<TextureInfo>();
    fenceTex->Name = "wirefence";
    fenceTex->Filename = L"../Textures/WireFence.dds";
    fenceTex->DiffuseSrvHeapIndex = indexCount++;
    ThrowIfFailed(CreateDDSTextureFromFile12(md3dDevice.Get(),
        mCommandList.Get(),
        fenceTex->Filename.c_str(),
        fenceTex->Resource,
        fenceTex->UploadHeap));
    mTextures[fenceTex->Name] = std::move(fenceTex);
    
    auto skyboxTex = std::make_unique<TextureInfo>();
    skyboxTex->Name = "skybox";
    skyboxTex->Filename = L"../Textures/grasscube1024.dds";
    skyboxTex->DiffuseSrvHeapIndex = indexCount++;
    ThrowIfFailed(CreateDDSTextureFromFile12(md3dDevice.Get(),
        mCommandList.Get(),
        skyboxTex->Filename.c_str(),
        skyboxTex->Resource,
        skyboxTex->UploadHeap));
    mTextures[skyboxTex->Name] = std::move(skyboxTex);

    // SKinned Model 텍스처 로드
    for (UINT i = 0; i < mSkinnedMats.size(); ++i)
    {
        std::string diffuseName = mSkinnedMats[i].DiffuseMapName;
        std::string normalName = mSkinnedMats[i].NormalMapName;

        std::wstring diffuseFilename = L"../Textures/" + AnsiToWString(diffuseName);
        std::wstring normalFilename = L"../Textures/" + AnsiToWString(normalName);

        
        if (mTextures.find(diffuseName) == mTextures.end())
        {
            auto texDiff = std::make_unique<TextureInfo>();
            texDiff->Name = diffuseName;
            texDiff->Filename = diffuseFilename;
            texDiff->DiffuseSrvHeapIndex = indexCount++;
            ThrowIfFailed(CreateDDSTextureFromFile12(md3dDevice.Get(),
                mCommandList.Get(),
                texDiff->Filename.c_str(),
                texDiff->Resource,
                texDiff->UploadHeap));
            mTextures[texDiff->Name] = std::move(texDiff);
        }

        //노말 텍스처 중복 방지
        if (mTextures.find(normalName) == mTextures.end())
        {
            auto texDiff = std::make_unique<TextureInfo>();
            texDiff->Name = normalName;
            texDiff->Filename = normalFilename;
            texDiff->DiffuseSrvHeapIndex = indexCount++;
            ThrowIfFailed(CreateDDSTextureFromFile12(md3dDevice.Get(),
                mCommandList.Get(),
                texDiff->Filename.c_str(),
                texDiff->Resource,
                texDiff->UploadHeap));
            mTextures[texDiff->Name] = std::move(texDiff);
        }
    }
}

void InitDirect3DApp::BuildMaterials()
{
    UINT indexCount = 0;

    auto green = std::make_unique<MaterialInfo>();
    green->Name = "Green";
    green->MatCBIndex = indexCount++;
    green->DiffuseAlbedo = XMFLOAT4(Colors::ForestGreen);
    green->FresnelR0 = XMFLOAT3(0.02f, 0.02f, 0.02f);
    green->Roughness = 0.1f;
    mMaterials[green->Name] = std::move(green);

    auto blue = std::make_unique<MaterialInfo>();
    blue->Name = "Blue";
    blue->MatCBIndex = indexCount++;
    blue->DiffuseAlbedo = XMFLOAT4(Colors::LightSteelBlue);
    blue->FresnelR0 = XMFLOAT3(0.05f, 0.05f, 0.05f);
    blue->Roughness = 0.3f;
    mMaterials[blue->Name] = std::move(blue);


    auto gray = std::make_unique<MaterialInfo>();
    gray->Name = "Gray";
    gray->MatCBIndex = indexCount++;
    gray->DiffuseAlbedo = XMFLOAT4(Colors::LightGray);
    gray->FresnelR0 = XMFLOAT3(0.02f, 0.02f, 0.02f);
    gray->Roughness = 0.2f;
    mMaterials[gray->Name] = std::move(gray);


    auto skull = std::make_unique<MaterialInfo>();
    skull->Name = "Skull";
    skull->MatCBIndex = indexCount++;
    skull->DiffuseAlbedo = XMFLOAT4(1.0f, 1.0f, 1.0f, 1.f);
    skull->FresnelR0 = XMFLOAT3(0.05f, 0.05f, 0.05f);
    skull->Roughness = 0.3f;
    mMaterials[skull->Name] = std::move(skull);

    auto bricks = std::make_unique<MaterialInfo>();
    bricks->Name = "Bricks";
    bricks->MatCBIndex = indexCount++;
    bricks->DiffuseSrvHeapIndex = mTextures["bricks"]->DiffuseSrvHeapIndex;
    bricks->NormalSrvHeapIndex = mTextures["bricksNormal"]->DiffuseSrvHeapIndex;
    bricks->DiffuseAlbedo = XMFLOAT4(Colors::White);
    bricks->FresnelR0 = XMFLOAT3(0.02f, 0.02f, 0.02f);
    bricks->Roughness = 0.1f;
    mMaterials[bricks->Name] = std::move(bricks);

    auto stone = std::make_unique<MaterialInfo>();
    stone->Name = "Stone";
    stone->MatCBIndex = indexCount++;
    stone->DiffuseSrvHeapIndex = mTextures["stone"]->DiffuseSrvHeapIndex;
    stone->DiffuseAlbedo = XMFLOAT4(Colors::White);
    stone->FresnelR0 = XMFLOAT3(0.05f, 0.05f, 0.05f);
    stone->Roughness = 0.3f;
    mMaterials[stone->Name] = std::move(stone);

    auto tile = std::make_unique<MaterialInfo>();
    tile->Name = "Tile";
    tile->MatCBIndex = indexCount++;
    tile->DiffuseSrvHeapIndex = mTextures["tile"]->DiffuseSrvHeapIndex;
    tile->NormalSrvHeapIndex = mTextures["tileNormal"]->DiffuseSrvHeapIndex;
    tile->DiffuseAlbedo = XMFLOAT4(Colors::White);
    tile->FresnelR0 = XMFLOAT3(0.02f, 0.02f, 0.02f);
    tile->Roughness = 0.2f;
    mMaterials[tile->Name] = std::move(tile);

    auto wireFence = std::make_unique<MaterialInfo>();
    wireFence->Name = "WireFence";
    wireFence->MatCBIndex = indexCount++;
    wireFence->DiffuseSrvHeapIndex = mTextures["wirefence"]->DiffuseSrvHeapIndex;
    wireFence->DiffuseAlbedo = XMFLOAT4(Colors::White);
    wireFence->FresnelR0 = XMFLOAT3(0.1f, 0.1f, 0.1f);
    wireFence->Roughness = 0.25f;
    mMaterials[wireFence->Name] = std::move(wireFence);

    auto skybox = std::make_unique<MaterialInfo>();
    skybox->Name = "Skybox";
    skybox->MatCBIndex = indexCount++;
    skybox->DiffuseSrvHeapIndex = mTextures["skybox"]->DiffuseSrvHeapIndex;
    skybox->DiffuseAlbedo = XMFLOAT4(Colors::White);
    skybox->FresnelR0 = XMFLOAT3(0.1f, 0.1f, 0.1f);
    skybox->Roughness = 1.0f;
    mMaterials[skybox->Name] = std::move(skybox);

    auto mirror = std::make_unique<MaterialInfo>();
    mirror->Name = "Mirror";
    mirror->MatCBIndex = indexCount++;
    mirror->DiffuseAlbedo = XMFLOAT4(0.0f, 0.0f, 0.0f, 1.0f);
    mirror->FresnelR0 = XMFLOAT3(0.98f, 0.97f, 0.95f);
    mirror->Roughness = 0.1f;
    mMaterials[mirror->Name] = std::move(mirror);
    
    // Skinned Model Materials
    for (UINT i = 0; i < mSkinnedMats.size(); ++i)
    {
        std::string diffuseName = mSkinnedMats[i].DiffuseMapName;
        std::string normalName = mSkinnedMats[i].NormalMapName;

        auto mat = std::make_unique<MaterialInfo>();
        mat->Name = mSkinnedMats[i].Name;
        mat->MatCBIndex = indexCount++;
        mat->DiffuseSrvHeapIndex = mTextures[diffuseName]->DiffuseSrvHeapIndex;
        mat->NormalSrvHeapIndex = mTextures[normalName]->DiffuseSrvHeapIndex;
        mat->DiffuseAlbedo = mSkinnedMats[i].DiffuseAlbedo;
        mat->FresnelR0 = mSkinnedMats[i].FresnelR0;
        mat->Roughness = mSkinnedMats[i].Roughness;
        mMaterials[mat->Name] = std::move(mat);
    }
}

void InitDirect3DApp::BuildRenderItem()
{
    UINT objCBIndex = 0;

    // 스카이박스
    auto skyItem = std::make_unique<RenderItem>();
    skyItem->ObjCBIndex = objCBIndex++;
    skyItem->World = MathHelper::Identity4x4();
    XMStoreFloat4x4(&skyItem->TexTransform, XMMatrixScaling(5000.0f, 5000.0f, 5000.0f));
    skyItem->Geo = mGeoMetries["Box"].get();
    skyItem->Mat = mMaterials["Skybox"].get();
    skyItem->PrimitiveType = D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST;
    skyItem->IndexCount = skyItem->Geo->IndexCount;
    mRenderItemLayer[(int)RenderLayer::SKybox].push_back(skyItem.get());
    mRenderItems.push_back(std::move(skyItem));

    //바닥
    auto gridItem = std::make_unique<RenderItem>();
    gridItem->ObjCBIndex = objCBIndex++;
    gridItem->World = MathHelper::Identity4x4();
    XMStoreFloat4x4(&gridItem->TexTransform, XMMatrixScaling(8.0f, 8.0f, 8.0f));
    gridItem->Geo = mGeoMetries["Grid"].get();
    gridItem->Mat = mMaterials["Tile"].get();
    gridItem->PrimitiveType = D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST;
    gridItem->IndexCount = gridItem->Geo->IndexCount;
    mRenderItemLayer[(int)RenderLayer::Opaque].push_back(gridItem.get());
    mRenderItems.push_back(std::move(gridItem));
    
    //쿼드
    auto quadItem = std::make_unique<RenderItem>();
    quadItem->ObjCBIndex = objCBIndex++;
    quadItem->World = MathHelper::Identity4x4();
    quadItem->TexTransform = MathHelper::Identity4x4();
    quadItem->Geo = mGeoMetries["Quad"].get();
    quadItem->Mat = mMaterials["Tile"].get();
    quadItem->PrimitiveType = D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST;
    quadItem->IndexCount = quadItem->Geo->IndexCount;
    mRenderItemLayer[(int)RenderLayer::Debug].push_back(quadItem.get());
    mRenderItems.push_back(std::move(quadItem));

    auto boxItem = std::make_unique<RenderItem>();
    boxItem->ObjCBIndex = objCBIndex++;
    XMStoreFloat4x4(&boxItem->World, XMMatrixScaling(2.0f, 2.0f, 2.0f) * XMMatrixTranslation(0.0f, 0.5f, 0.0f));
    XMStoreFloat4x4(&boxItem->TexTransform, XMMatrixScaling(1.0f, 1.0f, 1.0f));
    boxItem->Geo = mGeoMetries["Box"].get();
    boxItem->Mat = mMaterials["WireFence"].get();
    boxItem->PrimitiveType = D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST;
    boxItem->IndexCount = boxItem->Geo->IndexCount;
    mRenderItemLayer[(int)RenderLayer::AlphaTested].push_back(boxItem.get());
    mRenderItems.push_back(std::move(boxItem));

    //해골
    auto skullItem = std::make_unique<RenderItem>();
    skullItem->ObjCBIndex = objCBIndex++;
    XMStoreFloat4x4(&skullItem->World, XMMatrixScaling(0.5f, 0.5f, 0.5f) * XMMatrixTranslation(0.0f, 1.f, 0.0f));
    XMStoreFloat4x4(&skullItem->TexTransform, XMMatrixScaling(1.f, 1.f, 1.f));
    skullItem->Geo = mGeoMetries["Skull"].get();
    skullItem->Mat = mMaterials["Skull"].get();
    skullItem->PrimitiveType = D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST;
    skullItem->IndexCount = skullItem->Geo->IndexCount;
    mRenderItemLayer[(int)RenderLayer::Opaque].push_back(skullItem.get());
    mRenderItems.push_back(std::move(skullItem));

    for (int i = 0; i < 5; ++i)
    {
        auto leftCylItem = std::make_unique<RenderItem>();
        auto rightCylItem = std::make_unique<RenderItem>();
        auto leftsphereItem = std::make_unique<RenderItem>();
        auto rightsphereItem = std::make_unique<RenderItem>();

        XMMATRIX leftCylWorld = XMMatrixTranslation(-5.0f, 1.5f, -10.0f + i * 5.0f);
        XMMATRIX rightCylWorld = XMMatrixTranslation(+5.0f, 1.5f, -10.0f + i * 5.0f);

        XMMATRIX leftsphereWorld = XMMatrixTranslation(-5.0f, 3.5f, -10.0f + i * 5.0f);
        XMMATRIX rightsphereWorld = XMMatrixTranslation(+5.0f, 3.5f, -10.0f + i * 5.0f);

        //왼쪽 실린더
        XMStoreFloat4x4(&leftCylItem->World, leftCylWorld);
        XMStoreFloat4x4(&leftCylItem->TexTransform, XMMatrixScaling(1.f, 1.f, 1.f));
        leftCylItem->ObjCBIndex = objCBIndex++;
        leftCylItem->Geo = mGeoMetries["Cylinder"].get();
        leftCylItem->Mat = mMaterials["Bricks"].get();
        leftCylItem->PrimitiveType = D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST;
        leftCylItem->IndexCount = leftCylItem->Geo->IndexCount;
        mRenderItemLayer[(int)RenderLayer::Opaque].push_back(leftCylItem.get());
        mRenderItems.push_back(std::move(leftCylItem));

        //오른쪽 실린더
        XMStoreFloat4x4(&rightCylItem->World, rightCylWorld);
        XMStoreFloat4x4(&rightCylItem->TexTransform, XMMatrixScaling(1.f, 1.f, 1.f));
        rightCylItem->ObjCBIndex = objCBIndex++;
        rightCylItem->Geo = mGeoMetries["Cylinder"].get();
        rightCylItem->Mat = mMaterials["Bricks"].get();
        rightCylItem->PrimitiveType = D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST;
        rightCylItem->IndexCount = rightCylItem->Geo->IndexCount;
        mRenderItemLayer[(int)RenderLayer::Opaque].push_back(rightCylItem.get());
        mRenderItems.push_back(std::move(rightCylItem));

        //왼쪽 스피어
        XMStoreFloat4x4(&leftsphereItem->World, leftsphereWorld);
        XMStoreFloat4x4(&leftsphereItem->TexTransform, XMMatrixScaling(1.f, 1.f, 1.f));
        leftsphereItem->ObjCBIndex = objCBIndex++;
        leftsphereItem->Geo = mGeoMetries["Sphere"].get();
        leftsphereItem->Mat = mMaterials["Mirror"].get();
        leftsphereItem->PrimitiveType = D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST;
        leftsphereItem->IndexCount = leftsphereItem->Geo->IndexCount;
        mRenderItemLayer[(int)RenderLayer::Opaque].push_back(leftsphereItem.get());
        mRenderItems.push_back(std::move(leftsphereItem));

        //오른쪽 스피어
        XMStoreFloat4x4(&rightsphereItem->World, rightsphereWorld);
        XMStoreFloat4x4(&rightsphereItem->TexTransform, XMMatrixScaling(1.f, 1.f, 1.f));
        rightsphereItem->ObjCBIndex = objCBIndex++;
        rightsphereItem->Geo = mGeoMetries["Sphere"].get();
        rightsphereItem->Mat = mMaterials["Mirror"].get();
        rightsphereItem->PrimitiveType = D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST;
        rightsphereItem->IndexCount = rightsphereItem->Geo->IndexCount;
        mRenderItemLayer[(int)RenderLayer::Opaque].push_back(rightsphereItem.get());
        mRenderItems.push_back(std::move(rightsphereItem));
    }

    // Skinned Model Object
    for (UINT i = 0; i < mSkinnedMats.size(); ++i)
    {
        std::string submeshName = "sm_" + std::to_string(i);
        auto ritem = std::make_unique<RenderItem>();

        XMMATRIX modelScale = XMMatrixScaling(0.05f, 0.05f, -0.05f);
        XMMATRIX modelRot = XMMatrixRotationY(MathHelper::Pi);
        XMMATRIX modelOffset = XMMatrixTranslation(0.0f, 0.0f, -5.0f);
        XMStoreFloat4x4(&ritem->World, modelScale * modelRot * modelOffset);
        ritem->TexTransform = MathHelper::Identity4x4();
        ritem->ObjCBIndex = objCBIndex++;
        ritem->Mat = mMaterials[mSkinnedMats[i].Name].get();
        ritem->Geo = mGeoMetries[submeshName].get();
        ritem->PrimitiveType = D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST;
        ritem->IndexCount = ritem->Geo->IndexCount;

        ritem->SkinnedCBIndex = 0;
        ritem->SkinnedModelInst = mSkinnedModelInst.get();

        mRenderItemLayer[(int)RenderLayer::SkinnedOpaque].push_back(ritem.get());
        mRenderItems.push_back(std::move(ritem));


    }


}

void InitDirect3DApp::BuildShader()
{
    const D3D_SHADER_MACRO defines[] =
    {
        "FOG", "1",
        NULL, NULL
    };

    const D3D_SHADER_MACRO alphaTestDefines[] =
    {
        "FOG", "1",
        "ALPHA_TEST", "1",
        NULL, NULL
    };

    const D3D_SHADER_MACRO skinnedDefines[] =
    {
        "FOG", "1",
        "ALPHA_TEST", "1",
        "SKINNED", "1",
        NULL, NULL
    };

    mShaders["standardVS"] = d3dUtil::CompileShader(L"Color.hlsl", nullptr, "VS", "vs_5_0");
    mShaders["skinnedVS"] = d3dUtil::CompileShader(L"Color.hlsl", skinnedDefines, "VS", "vs_5_0");
    mShaders["opaquePS"] = d3dUtil::CompileShader(L"Color.hlsl", defines, "PS", "ps_5_0");
    mShaders["alphaTestedPS"] = d3dUtil::CompileShader(L"Color.hlsl", alphaTestDefines, "PS", "ps_5_0");
    
    mShaders["skyVS"] = d3dUtil::CompileShader(L"Skybox.hlsl", nullptr, "VS", "vs_5_1");
    mShaders["skyPS"] = d3dUtil::CompileShader(L"Skybox.hlsl", nullptr, "PS", "ps_5_1");
    
    mShaders["shadowVS"] = d3dUtil::CompileShader(L"Shadows.hlsl", nullptr, "VS", "vs_5_0");
    mShaders["skinnedShadowVS"] = d3dUtil::CompileShader(L"Shadows.hlsl", skinnedDefines, "VS", "vs_5_0");
    mShaders["shadowPS"] = d3dUtil::CompileShader(L"Shadows.hlsl", nullptr, "PS", "ps_5_0");

    mShaders["debugVS"] = d3dUtil::CompileShader(L"ShadowDebug.hlsl", nullptr, "VS", "vs_5_0");
    mShaders["debugPS"] = d3dUtil::CompileShader(L"ShadowDebug.hlsl", nullptr, "PS", "ps_5_0");

}

void InitDirect3DApp::BuildConstantBuffer()
{
    // 개별 오브젝트 상수 버퍼
    UINT size = sizeof(ObjectConstants);
    mObjectByteSize = ((size + 255) & ~255) * mRenderItems.size();

    D3D12_HEAP_PROPERTIES heapProperty = CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD);
    D3D12_RESOURCE_DESC desc = CD3DX12_RESOURCE_DESC::Buffer(mObjectByteSize);

    md3dDevice->CreateCommittedResource(
        &heapProperty,
        D3D12_HEAP_FLAG_NONE,
        &desc,
        D3D12_RESOURCE_STATE_GENERIC_READ,
        nullptr,
        IID_PPV_ARGS(&mObjectCB));

    mObjectCB->Map(0, nullptr, reinterpret_cast<void**>(&mObjectMappedData));

    // 개별 재질 상수 버퍼
    size = sizeof(MatConstants);
    mMaterialByteSize = ((size + 255) & ~255) * mMaterials.size();

    heapProperty = CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD);
    desc = CD3DX12_RESOURCE_DESC::Buffer(mMaterialByteSize);

    md3dDevice->CreateCommittedResource(
        &heapProperty,
        D3D12_HEAP_FLAG_NONE,
        &desc,
        D3D12_RESOURCE_STATE_GENERIC_READ,
        nullptr,
        IID_PPV_ARGS(&mMaterialCB));

    mMaterialCB->Map(0, nullptr, reinterpret_cast<void**>(&mMaterialMappedData));

    // 공용 상수 버퍼
    size = sizeof(PassConstants);
    mPassByteSize = ((size + 255) & ~255) * 2;
    heapProperty = CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD);
    desc = CD3DX12_RESOURCE_DESC::Buffer(mPassByteSize);

    md3dDevice->CreateCommittedResource(
        &heapProperty,
        D3D12_HEAP_FLAG_NONE,
        &desc,
        D3D12_RESOURCE_STATE_GENERIC_READ,
        nullptr,
        IID_PPV_ARGS(&mPassCB));

    mPassCB->Map(0, nullptr, reinterpret_cast<void**>(&mPassMappedData));
    
    // 스키닝 오브젝트 상수 버퍼
    size = sizeof(SkinnedConstants);
    mSkinnedByteSize = ((size + 255) & ~255);
    heapProperty = CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD);
    desc = CD3DX12_RESOURCE_DESC::Buffer(mSkinnedByteSize);

    md3dDevice->CreateCommittedResource(
        &heapProperty,
        D3D12_HEAP_FLAG_NONE,
        &desc,
        D3D12_RESOURCE_STATE_GENERIC_READ,
        nullptr,
        IID_PPV_ARGS(&mSkinnedCB));

    mSkinnedCB->Map(0, nullptr, reinterpret_cast<void**>(&mSkinnedMappedData));
}

void InitDirect3DApp::BuildDescriptorHeaps()
{
    // SRV Heap 만들기
    D3D12_DESCRIPTOR_HEAP_DESC srcHeapDesc = {};
    srcHeapDesc.NumDescriptors = mTextures.size() + 1; // +1 : 그림자맵의 수
    srcHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
    srcHeapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;
    ThrowIfFailed(md3dDevice->CreateDescriptorHeap(&srcHeapDesc, IID_PPV_ARGS(&mSrvDescriptorHeap)));

    // SRV Heap 채우기
    for (auto& item : mTextures)
    {
        TextureInfo* tex = item.second.get();

        CD3DX12_CPU_DESCRIPTOR_HANDLE hDescriptor(mSrvDescriptorHeap->GetCPUDescriptorHandleForHeapStart());
        hDescriptor.Offset(tex->DiffuseSrvHeapIndex, mCbvSrvUavDescriptorSize);

        auto texResource = tex->Resource;
        D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
        
        if (tex->Name == "skybox")
        {
            srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
            srvDesc.Format = texResource->GetDesc().Format;
            srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURECUBE;
            srvDesc.TextureCube.MostDetailedMip = 0;
            srvDesc.TextureCube.MipLevels = texResource->GetDesc().MipLevels;
            srvDesc.TextureCube.ResourceMinLODClamp = 0.0f;

            mSkyTesHeapIndex = tex->DiffuseSrvHeapIndex;
        }
        else
        {
            srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
            srvDesc.Format = texResource->GetDesc().Format;
            srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
            srvDesc.Texture2D.MostDetailedMip = 0;
            srvDesc.Texture2D.MipLevels = texResource->GetDesc().MipLevels;
            srvDesc.Texture2D.ResourceMinLODClamp = 0.0f;
        }

      
        md3dDevice->CreateShaderResourceView(texResource.Get(), &srvDesc, hDescriptor);

    }

    mShadowMapHeapIndex = (UINT)mTextures.size();
    
    auto srvCpuStart = mSrvDescriptorHeap->GetCPUDescriptorHandleForHeapStart();
    auto srvGpuStart = mSrvDescriptorHeap->GetGPUDescriptorHandleForHeapStart();
    auto dsvCpuStart = mDsvHeap->GetCPUDescriptorHandleForHeapStart();

    mShadowMap->BuildDescriptors(
        CD3DX12_CPU_DESCRIPTOR_HANDLE(srvCpuStart, mShadowMapHeapIndex, mCbvSrvUavDescriptorSize),
        CD3DX12_GPU_DESCRIPTOR_HANDLE(srvGpuStart, mShadowMapHeapIndex, mCbvSrvUavDescriptorSize),
        CD3DX12_CPU_DESCRIPTOR_HANDLE(dsvCpuStart, 1, mDsvDescriptorSize));
}

void InitDirect3DApp::BuildRootSignature()
{
    D3D12_DESCRIPTOR_RANGE skyboxTable[] =
    {
        CD3DX12_DESCRIPTOR_RANGE(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 1, 0), // t0
    };

    CD3DX12_DESCRIPTOR_RANGE texTable[] =
    {
        CD3DX12_DESCRIPTOR_RANGE(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 1,1), //t1
    };

    CD3DX12_DESCRIPTOR_RANGE normalTable[] =
    {
        CD3DX12_DESCRIPTOR_RANGE(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 1,2), //t2
    };

    CD3DX12_DESCRIPTOR_RANGE shadowTable[] =
    {
        CD3DX12_DESCRIPTOR_RANGE(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 1,3), //t3
    };
    CD3DX12_ROOT_PARAMETER param[8];
    param[0].InitAsConstantBufferView(0); // 0번 -> b0 : 개별 오브젝트 CBV
    param[1].InitAsConstantBufferView(1); // 1번 -> b1 : 개별 재질 CBV
    param[2].InitAsConstantBufferView(2); // 2번 -> b2 : 공용 CBV
    param[3].InitAsDescriptorTable(_countof(skyboxTable), skyboxTable);     //3번 -> t0 : 스카이박스 텍스처
    param[4].InitAsDescriptorTable(_countof(texTable), texTable);           //4번 -> t1 : object 텍스처
    param[5].InitAsDescriptorTable(_countof(normalTable), normalTable);     //5번 -> t2 : normal 텍스처
    param[6].InitAsDescriptorTable(_countof(shadowTable), shadowTable);     //6번 -> t3 : 그림자맵 텍스처
    param[7].InitAsConstantBufferView(3);     //3번 -> b3 :  스키닝 애니메이션 CBV


    //s0 : 기본 텍스처 샘플러
    //s1 : 그림자 맵 텍스처 샘플러
    const CD3DX12_STATIC_SAMPLER_DESC pointerWrap(
        0, //shaderRegister //s0 : 기본 텍스처 샘플러
        D3D12_FILTER_MIN_MAG_MIP_POINT, //filter
        D3D12_TEXTURE_ADDRESS_MODE_WRAP, //U
        D3D12_TEXTURE_ADDRESS_MODE_WRAP, //V
        D3D12_TEXTURE_ADDRESS_MODE_WRAP); //W

    const CD3DX12_STATIC_SAMPLER_DESC shadow(
        1, //ShaderRegister // s1 : 그림자 맵 텍스처 샘플러
        D3D12_FILTER_COMPARISON_MIN_MAG_LINEAR_MIP_POINT,
        D3D12_TEXTURE_ADDRESS_MODE_BORDER, //U
        D3D12_TEXTURE_ADDRESS_MODE_BORDER, //V
        D3D12_TEXTURE_ADDRESS_MODE_BORDER, //W
        0.0f,
        16,
        D3D12_COMPARISON_FUNC_LESS_EQUAL,
        D3D12_STATIC_BORDER_COLOR_OPAQUE_BLACK);

    std::array<const CD3DX12_STATIC_SAMPLER_DESC, 2> staticSamplers = { pointerWrap, shadow };

    D3D12_ROOT_SIGNATURE_DESC sigDesc = CD3DX12_ROOT_SIGNATURE_DESC(_countof(param), param,
        (UINT)staticSamplers.size(), staticSamplers.data());
    sigDesc.Flags = D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT;

    ComPtr<ID3DBlob> blobSignature;
    ComPtr<ID3DBlob> blobError;

    ::D3D12SerializeRootSignature(&sigDesc, D3D_ROOT_SIGNATURE_VERSION_1, &blobSignature, &blobError);

    md3dDevice->CreateRootSignature(0, blobSignature->GetBufferPointer(), blobSignature->GetBufferSize(),
        IID_PPV_ARGS(&mRootSignature));
}

void InitDirect3DApp::BuildPSO()
{
    D3D12_GRAPHICS_PIPELINE_STATE_DESC psoDesc;
    ZeroMemory(&psoDesc, sizeof(D3D12_GRAPHICS_PIPELINE_STATE_DESC));
    
    // opaque objects
    psoDesc.InputLayout = { mInputLayout.data(), (UINT)mInputLayout.size() };
    psoDesc.pRootSignature = mRootSignature.Get();
    psoDesc.VS =
    {
        reinterpret_cast<BYTE*>(mShaders["standardVS"]->GetBufferPointer()),
        mShaders["standardVS"]->GetBufferSize()
    };
    psoDesc.PS =
    {
        reinterpret_cast<BYTE*>(mShaders["opaquePS"]->GetBufferPointer()),
        mShaders["opaquePS"]->GetBufferSize()
    };
    psoDesc.RasterizerState = CD3DX12_RASTERIZER_DESC(D3D12_DEFAULT);
    psoDesc.BlendState = CD3DX12_BLEND_DESC(D3D12_DEFAULT);
    psoDesc.DepthStencilState = CD3DX12_DEPTH_STENCIL_DESC(D3D12_DEFAULT);
    psoDesc.SampleMask = UINT_MAX;
    psoDesc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
    psoDesc.NumRenderTargets = 1;
    psoDesc.RTVFormats[0] = mbackBufferFormat;
    psoDesc.SampleDesc.Count = m4xMsaaState ? 4 : 1;
    psoDesc.SampleDesc.Quality = m4xMsaaState ? (m4xMsaaQuality - 1) : 0;
    psoDesc.DSVFormat = mDepthStencilFormat;

    ThrowIfFailed(md3dDevice->CreateGraphicsPipelineState(&psoDesc, IID_PPV_ARGS(&mPSOs["opaque"])));

    //alpha tested objects
    D3D12_GRAPHICS_PIPELINE_STATE_DESC alphaTestedPsoDesc = psoDesc;
    alphaTestedPsoDesc.PS =
    {
        reinterpret_cast<BYTE*>(mShaders["alphaTestedPS"]->GetBufferPointer()),
        mShaders["alphaTestedPS"]->GetBufferSize()
    };
    alphaTestedPsoDesc.RasterizerState.CullMode = D3D12_CULL_MODE_NONE;
    
    ThrowIfFailed(md3dDevice->CreateGraphicsPipelineState(&alphaTestedPsoDesc, IID_PPV_ARGS(&mPSOs["alphaTested"])));
    
    //transparent obbjects
    D3D12_GRAPHICS_PIPELINE_STATE_DESC transparentPSODesc = psoDesc;
    D3D12_RENDER_TARGET_BLEND_DESC transparentBlendDesc;
    transparentBlendDesc.BlendEnable = true;
    transparentBlendDesc.LogicOpEnable = false;
    transparentBlendDesc.SrcBlend = D3D12_BLEND_SRC_ALPHA;
    transparentBlendDesc.DestBlend = D3D12_BLEND_INV_SRC_ALPHA;
    transparentBlendDesc.BlendOp = D3D12_BLEND_OP_ADD;
    transparentBlendDesc.SrcBlendAlpha = D3D12_BLEND_ONE;
    transparentBlendDesc.DestBlendAlpha = D3D12_BLEND_ZERO;
    transparentBlendDesc.BlendOpAlpha = D3D12_BLEND_OP_ADD;
    transparentBlendDesc.LogicOp = D3D12_LOGIC_OP_NOOP;
    transparentBlendDesc.RenderTargetWriteMask = D3D12_COLOR_WRITE_ENABLE_ALL;

    transparentPSODesc.BlendState.RenderTarget[0] = transparentBlendDesc;
    ThrowIfFailed(md3dDevice->CreateGraphicsPipelineState(&transparentPSODesc, IID_PPV_ARGS(&mPSOs["transparent"])));

    //skybox
    D3D12_GRAPHICS_PIPELINE_STATE_DESC skyPsoDesc = psoDesc;
    skyPsoDesc.VS =
    {
        reinterpret_cast<BYTE*>(mShaders["skyVS"]->GetBufferPointer()),
        mShaders["skyVS"]->GetBufferSize()
    };
    skyPsoDesc.PS =
    {
        reinterpret_cast<BYTE*>(mShaders["skyPS"]->GetBufferPointer()),
        mShaders["skyPS"]->GetBufferSize()
    };

    skyPsoDesc.RasterizerState.CullMode = D3D12_CULL_MODE_NONE;
    skyPsoDesc.DepthStencilState.DepthFunc = D3D12_COMPARISON_FUNC_LESS_EQUAL;
    
    ThrowIfFailed(md3dDevice->CreateGraphicsPipelineState(&skyPsoDesc, IID_PPV_ARGS(&mPSOs["skybox"])));

    //shadowmap
    D3D12_GRAPHICS_PIPELINE_STATE_DESC shadowPsoDesc = psoDesc;
    shadowPsoDesc.VS =
    {
        reinterpret_cast<BYTE*>(mShaders["shadowVS"]->GetBufferPointer()),
        mShaders["shadowVS"]->GetBufferSize()
    };
    shadowPsoDesc.PS =
    {
        reinterpret_cast<BYTE*>(mShaders["shadowPS"]->GetBufferPointer()),
        mShaders["shadowPS"]->GetBufferSize()
    };
    
    shadowPsoDesc.RasterizerState.DepthBias = 100000;
    shadowPsoDesc.RasterizerState.DepthBiasClamp = 0.0f;
    shadowPsoDesc.RasterizerState.SlopeScaledDepthBias = 1.0f;
    shadowPsoDesc.NumRenderTargets = 0;
    shadowPsoDesc.RTVFormats[0] = DXGI_FORMAT_UNKNOWN;
    ThrowIfFailed(md3dDevice->CreateGraphicsPipelineState(&shadowPsoDesc, IID_PPV_ARGS(&mPSOs["shadow"])));

    //shadow debug
    D3D12_GRAPHICS_PIPELINE_STATE_DESC debugPsoDesc = psoDesc;
    debugPsoDesc.VS =
    {
        reinterpret_cast<BYTE*>(mShaders["debugVS"]->GetBufferPointer()),
        mShaders["debugVS"]->GetBufferSize()
    };
    debugPsoDesc.PS =
    {
        reinterpret_cast<BYTE*>(mShaders["debugPS"]->GetBufferPointer()),
        mShaders["debugPS"]->GetBufferSize()
    };

    ThrowIfFailed(md3dDevice->CreateGraphicsPipelineState(&debugPsoDesc, IID_PPV_ARGS(&mPSOs["debug"])));

    //skinned model PSO
    D3D12_GRAPHICS_PIPELINE_STATE_DESC skinnedPsoDesc = psoDesc;
    skinnedPsoDesc.InputLayout = { mSkinnedInputLayout.data(), (UINT)mSkinnedInputLayout.size() };
    skinnedPsoDesc.VS =
    {
        reinterpret_cast<BYTE*>(mShaders["skinnedVS"]->GetBufferPointer()),
        mShaders["skinnedVS"]->GetBufferSize()
    };
    ThrowIfFailed(md3dDevice->CreateGraphicsPipelineState(&skinnedPsoDesc, IID_PPV_ARGS(&mPSOs["skinnedOpaque"])));
    

    //skinned shadow PSO
    D3D12_GRAPHICS_PIPELINE_STATE_DESC skinnedShadowPsoDesc = shadowPsoDesc;
    skinnedShadowPsoDesc.InputLayout = { mSkinnedInputLayout.data(), (UINT)mSkinnedInputLayout.size() };
    skinnedShadowPsoDesc.VS =
    {
        reinterpret_cast<BYTE*>(mShaders["skinnedShadowVS"]->GetBufferPointer()),
        mShaders["skinnedShadowVS"]->GetBufferSize()
    };
 
    ThrowIfFailed(md3dDevice->CreateGraphicsPipelineState(&skinnedShadowPsoDesc, IID_PPV_ARGS(&mPSOs["skinnedShadow"])));
}
