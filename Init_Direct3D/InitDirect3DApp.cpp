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

    //초기화 명령들
    BuildInputLayout();
    BuildGeometry();
    BuildShader();
    BuildConstantBuffer();
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

void InitDirect3DApp::OnResize()
{
    D3DApp::OnResize();

    //창의 크기가 바뀌었을 때 , 종횡비 갱신-> 투영 행렬
    XMMATRIX proj = XMMatrixPerspectiveFovLH(0.25f * MathHelper::Pi, AspectRatio(), 1.0f, 1000.0f);
    XMStoreFloat4x4(&mProj, proj);
}

void InitDirect3DApp::Update(const GameTimer& gt)
{
    // 구면 좌표를 직교 좌표
    UpdateCamera(gt);
    UpdateObjectCB(gt);
    UpdatePassCB(gt);
}

void InitDirect3DApp::UpdateCamera(const GameTimer& gt)
{
    float x = mRadius * sinf(mPhi) * cosf(mTheta);
    float z = mRadius * sinf(mPhi) * sinf(mTheta);
    float y = mRadius * cosf(mPhi);

    // 시야 행렬
    XMVECTOR pos = XMVectorSet(x, y, z, 1.0f);
    XMVECTOR target = XMVectorZero();
    XMVECTOR up = XMVectorSet(0.0f, 1.0f, 0.0f, 0.0f);

    XMMATRIX view = XMMatrixLookAtLH(pos, target, up);
    XMStoreFloat4x4(&mView, view);
}

void InitDirect3DApp::UpdateObjectCB(const GameTimer& gt)
{
    // 개별 오브젝트 상수 버퍼 갱신
}

void InitDirect3DApp::UpdatePassCB(const GameTimer& gt)
{
    PassConstants mainPass;
    XMMATRIX view = XMLoadFloat4x4(&mView);
    XMMATRIX proj = XMLoadFloat4x4(&mProj);

    XMMATRIX invView = XMMatrixInverse(&XMMatrixDeterminant(view), view);
    XMMATRIX invProj = XMMatrixInverse(&XMMatrixDeterminant(proj), proj);
    XMMATRIX viewProj = XMMatrixMultiply(view, proj);

    XMStoreFloat4x4(&mainPass.View, XMMatrixTranspose(view));
    XMStoreFloat4x4(&mainPass.InvView, XMMatrixTranspose(invView));
    XMStoreFloat4x4(&mainPass.Proj, XMMatrixTranspose(proj));
    XMStoreFloat4x4(&mainPass.InvProj, XMMatrixTranspose(invProj));
    XMStoreFloat4x4(&mainPass.ViewProj, XMMatrixTranspose(viewProj));
   
    memcpy(&mPassMappedData[0], &mainPass, sizeof(PassConstants));
}

void InitDirect3DApp::DrawBegin(const GameTimer& gt)
{
    ThrowIfFailed(mCommandListAlloc->Reset());
    ThrowIfFailed(mCommandList->Reset(mCommandListAlloc.Get(), nullptr));

    mCommandList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(
        CurrentBackBuffer(), D3D12_RESOURCE_STATE_PRESENT, D3D12_RESOURCE_STATE_RENDER_TARGET));
    
    mCommandList->RSSetViewports(1, &mScreenViewport);
    mCommandList->RSSetScissorRects(1, &mScissorRect);

    mCommandList->ClearRenderTargetView(CurrentBackBufferView(), Colors::LightSteelBlue, 0, nullptr);
    mCommandList->ClearDepthStencilView(DepthStencilView(),
        D3D12_CLEAR_FLAG_DEPTH | D3D12_CLEAR_FLAG_STENCIL, 1.0f, 0, 0, nullptr);

    mCommandList->OMSetRenderTargets(1, &CurrentBackBufferView(), true, &DepthStencilView());
}

void InitDirect3DApp::Draw(const GameTimer& gt)
{
    // 렌더링 파이프라인 설정
    mCommandList->SetPipelineState(mPSO.Get());

    // 루트 시그니처 바인딩
    mCommandList->SetGraphicsRootSignature(mRootSignature.Get());

    // 개별 오브젝트 렌더링
    // 상수 버퍼 바인딩
    D3D12_GPU_VIRTUAL_ADDRESS objCBAddress = mObjectCB->GetGPUVirtualAddress();
    mCommandList->SetGraphicsRootConstantBufferView(0, objCBAddress);

    // 삼각형 정점 정보와 인덱스 정보 셋팅
    mCommandList->IASetVertexBuffers(0, 1, &VertexBufferView);
    mCommandList->IASetIndexBuffer(&IndexBufferView);
    mCommandList->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

    // 삼각형 그리기
    mCommandList->DrawIndexedInstanced(IndexCount, 1, 0, 0, 0);
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
        mTheta += dx;
        mPhi += dy;
        mPhi = MathHelper::Clamp(mPhi, 0.1f, MathHelper::Pi - 0.1f);
    }
    else if ((btnState & MK_RBUTTON) != 0)
    {
        float dx = 0.2f * static_cast<float>(x - mLastMovesePos.x);
        float dy = 0.2f * static_cast<float>(y - mLastMovesePos.y);

        mRadius += dx - dy;

        mRadius = MathHelper::Clamp(mRadius, 3.0f, 150.f);
    }

    mLastMovesePos.x = x;
    mLastMovesePos.y = y;
}

void InitDirect3DApp::BuildInputLayout()
{
    mInputLayout =
    {
        {"POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0},
        {"COLOR", 0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, 12, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0}
    };
}

void InitDirect3DApp::BuildGeometry()
{
    BuildBoxGeometry();
}

void InitDirect3DApp::BuildBoxGeometry()
{
    GeometryGenerator geoGen;
    GeometryGenerator::MeshData box = geoGen.CreateBox(1.5f, 0.5f, 1.5f, 3);

    std::vector<Vertex> vertices(box.Vertices.size());

    for (UINT i = 0; i < box.Vertices.size(); ++i)
    {
        vertices[i].Pos = box.Vertices[i].Position;
        vertices[i].Color = XMFLOAT4(box.Vertices[i].Normal.x, box.Vertices[i].Normal.y, box.Vertices
            [i].Normal.z, 1.0f);
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
}

void InitDirect3DApp::BuildSphereGeometry()
{
}

void InitDirect3DApp::BuildCylinderGeometry()
{
}

void InitDirect3DApp::BuildSkullGeometry()
{
}

void InitDirect3DApp::BuildShader()
{
    mVSByteCode = d3dUtil::CompileShader(L"Color.hlsl", nullptr, "VS", "vs_5_0");
    mPSByteCode = d3dUtil::CompileShader(L"Color.hlsl", nullptr, "PS", "ps_5_0");
}

void InitDirect3DApp::BuildConstantBuffer()
{
    // 개별 오브젝트 상수 버퍼
    UINT size = sizeof(ObjectConstants);
    mObjectByteSize = (size + 255) & ~255;

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

    // 공용 상수 버퍼
    size = sizeof(PassConstants);
    mPassByteSize = (size + 255) & ~255;
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
}

void InitDirect3DApp::BuildRootSignature()
{
    CD3DX12_ROOT_PARAMETER param[2];
    param[0].InitAsConstantBufferView(0); // 0번 -> b0 : 개별 오브젝트 CBV
    param[1].InitAsConstantBufferView(1); // 1번 -> b1 : 공용 CBV

    D3D12_ROOT_SIGNATURE_DESC sigDesc = CD3DX12_ROOT_SIGNATURE_DESC(_countof(param), param);
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
    psoDesc.InputLayout = { mInputLayout.data(), (UINT)mInputLayout.size() };
    psoDesc.pRootSignature = mRootSignature.Get();
    psoDesc.VS =
    {
        reinterpret_cast<BYTE*>(mVSByteCode->GetBufferPointer()),
        mVSByteCode->GetBufferSize()
    };
    psoDesc.PS =
    {
        reinterpret_cast<BYTE*>(mPSByteCode->GetBufferPointer()),
        mPSByteCode->GetBufferSize()
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

    ThrowIfFailed(md3dDevice->CreateGraphicsPipelineState(&psoDesc, IID_PPV_ARGS(&mPSO)));


}
