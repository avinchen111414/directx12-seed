#include "BoxApp.h"

#include <array>
#include <DirectXColors.h>
using namespace DirectX;

bool BoxApp::Initialize(const D3DAppInfo& appInfo)
{
    if (!D3DApp::Initialize(appInfo))
        return false;

    // Reset the command list to prep for initialization commands.
    ThrowIfHRFailed(mCommandList->Reset(mDirectCmdListAlloc.Get(), nullptr));

    BuildDescriptorHeaps();
    BuildConstantBuffers();
    BuildRootSignature();
    BuildShaders();
    BuildInputLayout();
    BuildBoxGeometry();
    BuildPSO();

    // Execute the initialization commands.
    ThrowIfHRFailed(mCommandList->Close());
    ID3D12CommandList* cmdsList [] = { mCommandList.Get() };
    mCommandQueue->ExecuteCommandLists(_countof(cmdsList), cmdsList);

    // Wait until initialization is complete.
    FlushCommandQueue();

    return true;
}

void BoxApp::OnResize()
{
    D3DApp::OnResize();

    // The window resized, so update the aspect ratio rend recompute the projection matrix.
    const XMMATRIX projMat = XMMatrixPerspectiveFovLH(0.25f * MathHelper::Pi, AspectRatio(), 1.0f, 1000.0f);
    XMStoreFloat4x4(&mProj, projMat);
}

void BoxApp::OnMouseMove(const xwin::MouseMoveData& mouseMoveData)
{
    D3DApp::OnMouseMove(mouseMoveData);

    if (EnumHasAnyFlags(mMouseBtnState, MouseBtnState::LBUTTON))
    {
        // Make each pixel corresponding to a quarter of a degree. 
        float dx = XMConvertToRadians(0.25f * static_cast<float>(mouseMoveData.deltax));
        float dy = XMConvertToRadians(0.25f * static_cast<float>(mouseMoveData.deltay));

        // Update angles based on input to orbit camera around box.
        mTheta += dx;
        mPhi += dy;

        // Restrict the angle mPhi.
        mPhi = MathHelper::Clamp(mPhi, 0.1f, MathHelper::Pi - 0.1f);
    }

    if (EnumHasAnyFlags(mMouseBtnState, MouseBtnState::RBUTTON))
    {
        // Make each pixel corresponding to 0.005 unit in the scene.
        float dx = 0.0005f * static_cast<float>(mouseMoveData.deltax);
        float dy = 0.0005f * static_cast<float>(mouseMoveData.deltay);

        // Update the camera radius based on input.
        mRadius += dx - dy;

        // Restrict the radius.
        mRadius = MathHelper::Clamp(mRadius, 3.0f, 15.0f);
    }
    
}

void BoxApp::OnMouseInput()
{
    D3DApp::OnMouseInput();
}

void BoxApp::Update(const GameTimer& gt)
{
    // Convert spherical to cartesian coordinates.
    float x = mRadius * sinf(mPhi) * cosf(mTheta);
    float z = mRadius * sinf(mPhi) * sinf(mTheta);
    float y = mRadius * cosf(mPhi);

    // Build the view matrix.
    XMVECTOR pos = XMVectorSet(x, y, z, 1.0f);
    XMVECTOR target = XMVectorZero();
    XMVECTOR up = XMVectorSet(0.0f, 1.0f, 0.0f, 0.0f);

    XMMATRIX view = XMMatrixLookAtLH(pos, target, up);
    XMStoreFloat4x4(&mView, view);

    XMMATRIX world = XMLoadFloat4x4(&mWorld);
    XMMATRIX proj = XMLoadFloat4x4(&mProj);
    XMMATRIX worldViewProj = world * view * proj;

    ObjectConstants objConstants;
    // Why transpose the mvp matrix. Column Major?
    XMStoreFloat4x4(&objConstants.WorldViewProj, XMMatrixTranspose(worldViewProj));
    mObjectCB->CopyData(0, objConstants);
}

void BoxApp::Draw(const GameTimer& gt)
{
    // Reuse the memory associated with command recording.
    // We can only reset when the associated command lists have finished execution on the GPU.
    ThrowIfHRFailed(mDirectCmdListAlloc->Reset());

	// A command list can be reset after it has been added to the command queue via ExecuteCommandList.
    // Reusing the command list reuses memory.
    mCommandList->Reset(mDirectCmdListAlloc.Get(), mPSO.Get());

    mCommandList->RSSetViewports(1, &mScreenViewport);
    mCommandList->RSSetScissorRects(1, &mScissorRect);

    // Indicate a state transition on the resource usage.
    CD3DX12_RESOURCE_BARRIER BackBufferTransition [] =
        {
            CD3DX12_RESOURCE_BARRIER::Transition(CurrentBackBuffer(), D3D12_RESOURCE_STATE_PRESENT, D3D12_RESOURCE_STATE_RENDER_TARGET)
        };
    mCommandList->ResourceBarrier(_countof(BackBufferTransition), BackBufferTransition);

    // Clear the back buffer and depth buffer.
    mCommandList->ClearRenderTargetView(CurrentBackBufferView(), Colors::LightSteelBlue, 0, nullptr);
    mCommandList->ClearDepthStencilView(DepthStencilView(), D3D12_CLEAR_FLAG_DEPTH | D3D12_CLEAR_FLAG_STENCIL, 1.0f, 0, 0, nullptr);

    // Specify the buffers we are going to render to.
    const D3D12_CPU_DESCRIPTOR_HANDLE currentBackBufferView = CurrentBackBufferView();
    const D3D12_CPU_DESCRIPTOR_HANDLE depthStencilView = DepthStencilView();
    mCommandList->OMSetRenderTargets(1, &currentBackBufferView, true, &depthStencilView);

    ID3D12DescriptorHeap* descriptorHeaps[] = { mCbvHeap.Get() };
    mCommandList->SetDescriptorHeaps(_countof(descriptorHeaps), descriptorHeaps);

    mCommandList->SetGraphicsRootSignature(mRootSignature.Get());

    const D3D12_VERTEX_BUFFER_VIEW& boxGeoVB = mBoxGeo->VertexBufferView(); 
    mCommandList->IASetVertexBuffers(0, 1, &boxGeoVB);
    const D3D12_INDEX_BUFFER_VIEW& boxGeoIB = mBoxGeo->IndexBufferView();
    mCommandList->IASetIndexBuffer(&boxGeoIB);
    mCommandList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

    const D3D12_GPU_DESCRIPTOR_HANDLE boxGeoCbv = mCbvHeap->GetGPUDescriptorHandleForHeapStart();
    mCommandList->SetGraphicsRootDescriptorTable(0, boxGeoCbv);

    mCommandList->DrawIndexedInstanced(mBoxGeo->DrawArgs["box"].IndexCount, 1, 0, 0, 0);

    // Indicate a state transition on the resource usage.
    CD3DX12_RESOURCE_BARRIER transitionRenderTargetToPresent [] =
        {
            CD3DX12_RESOURCE_BARRIER::Transition(CurrentBackBuffer(), D3D12_RESOURCE_STATE_RENDER_TARGET, D3D12_RESOURCE_STATE_PRESENT)
        };
    mCommandList->ResourceBarrier(_countof(transitionRenderTargetToPresent), transitionRenderTargetToPresent);

    // Done recording commands.
    ThrowIfHRFailed(mCommandList->Close());

    // Add the command list the queue for execution.
    ID3D12CommandList* cmdsLists [] = { mCommandList.Get() };
    mCommandQueue->ExecuteCommandLists(_countof(cmdsLists), cmdsLists);

    // swap the back and front buffers.
    ThrowIfHRFailed(mSwapChain->Present(0, 0));
    mCurrBackBuffer = (mCurrBackBuffer + 1) % SwapChainBufferCount;

    // Wait until fence commands are complete.
    // This waiting is inefficient and is done for simplicity.
    // Later we will show how to organize our rendering code so we do not have to wait per frame.
    FlushCommandQueue();
}

void BoxApp::BuildShaders()
{
    mvsByteCode = D3DUtils::CompileShader("Shaders\\color.hlsl", nullptr, "VS", "vs_5_0");
    mpsByteCode = D3DUtils::CompileShader("Shaders\\color.hlsl", nullptr, "PS", "ps_5_0");
}

void BoxApp::BuildInputLayout()
{
    D3D12_INPUT_ELEMENT_DESC position =
    {
        .SemanticName = "POSITION",
        .SemanticIndex = 0,
        .Format = DXGI_FORMAT_R32G32B32_FLOAT,
        .InputSlot = 0,
        .AlignedByteOffset = offsetof(Vertex, Pos),
        .InputSlotClass = D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA,
        .InstanceDataStepRate = 0,
    };
    
    D3D12_INPUT_ELEMENT_DESC color =
    {
        .SemanticName = "COLOR",
        .SemanticIndex = 0,
        .Format = DXGI_FORMAT_R32G32B32A32_FLOAT,
        .InputSlot = 0,
        .AlignedByteOffset = offsetof(Vertex, Color),
        .InputSlotClass = D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA,
        .InstanceDataStepRate = 0,
    };
    
    mInputLayout = { position, color };
}

void BoxApp::BuildPSO()
{
    D3D12_GRAPHICS_PIPELINE_STATE_DESC psoDesc;
    ZeroMemory(&psoDesc, sizeof(D3D12_GRAPHICS_PIPELINE_STATE_DESC));
    psoDesc.InputLayout =
        {
            .pInputElementDescs = mInputLayout.data(),
            .NumElements = static_cast<uint32_t>(mInputLayout.size()),
        };
    psoDesc.pRootSignature = mRootSignature.Get();
    psoDesc.VS =
        {
            .pShaderBytecode = mvsByteCode->GetBufferPointer(),
            .BytecodeLength = mvsByteCode->GetBufferSize(),
        };
    psoDesc.PS =
        {
            .pShaderBytecode = mpsByteCode->GetBufferPointer(),
            .BytecodeLength = mpsByteCode->GetBufferSize(),
        };
    psoDesc.RasterizerState = CD3DX12_RASTERIZER_DESC(D3D12_DEFAULT);
    psoDesc.BlendState = CD3DX12_BLEND_DESC(D3D12_DEFAULT);
    psoDesc.DepthStencilState = CD3DX12_DEPTH_STENCIL_DESC(D3D12_DEFAULT);
    psoDesc.SampleMask = UINT_MAX;
    psoDesc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
    psoDesc.NumRenderTargets = 1;
    psoDesc.RTVFormats[0] = mBackBufferFormat;
    psoDesc.DSVFormat = mDepthStencilFormat;
    psoDesc.SampleDesc.Count = m4xMsaaState ? 4 : 1;
    psoDesc.SampleDesc.Quality = m4xMsaaState ? (m4xMsaaQuality - 1) : 0;
    ThrowIfHRFailed(mD3dDevice->CreateGraphicsPipelineState(&psoDesc, IID_PPV_ARGS(mPSO.GetAddressOf())));
}

void BoxApp::BuildBoxGeometry()
{
    std::array<Vertex, 8> vertices =
    {
        Vertex({ XMFLOAT3(-1.0f, -1.0f, -1.0f), XMFLOAT4(Colors::White) }),
		Vertex({ XMFLOAT3(-1.0f, +1.0f, -1.0f), XMFLOAT4(Colors::Black) }),
		Vertex({ XMFLOAT3(+1.0f, +1.0f, -1.0f), XMFLOAT4(Colors::Red) }),
		Vertex({ XMFLOAT3(+1.0f, -1.0f, -1.0f), XMFLOAT4(Colors::Green) }),
		Vertex({ XMFLOAT3(-1.0f, -1.0f, +1.0f), XMFLOAT4(Colors::Blue) }),
		Vertex({ XMFLOAT3(-1.0f, +1.0f, +1.0f), XMFLOAT4(Colors::Yellow) }),
		Vertex({ XMFLOAT3(+1.0f, +1.0f, +1.0f), XMFLOAT4(Colors::Cyan) }),
		Vertex({ XMFLOAT3(+1.0f, -1.0f, +1.0f), XMFLOAT4(Colors::Magenta) })
    };

	std::array<uint16_t, 36> indices =
	{
		// front face
		0, 1, 2,
		0, 2, 3,

		// back face
		4, 6, 5,
		4, 7, 6,

		// left face
		4, 5, 1,
		4, 1, 0,

		// right face
		3, 2, 6,
		3, 6, 7,

		// top face
		1, 5, 6,
		1, 6, 2,

		// bottom face
		4, 0, 3,
		4, 3, 7
	};

    const uint32_t vbByteSize = static_cast<uint32_t>(vertices.size() * sizeof(Vertex));
    const uint32_t ibByteSize = static_cast<uint32_t>(indices.size() * sizeof(uint16_t));

    mBoxGeo = std::make_unique<MeshGeometry>();
    mBoxGeo->Name = "BoxGeo";

    ThrowIfHRFailed(D3DCreateBlob(vbByteSize, &mBoxGeo->VertexBufferCPU));
    CopyMemory(mBoxGeo->VertexBufferCPU->GetBufferPointer(), vertices.data(), vbByteSize);
    ThrowIfHRFailed(D3DCreateBlob(ibByteSize, &mBoxGeo->IndexBufferCPU));
    CopyMemory(mBoxGeo->IndexBufferCPU->GetBufferPointer(), indices.data(), ibByteSize);

    mBoxGeo->VertexBufferGPU = D3DUtils::CreateDefaultBuffer(mD3dDevice.Get(), mCommandList.Get(), vertices.data(),
        vbByteSize, mBoxGeo->VertexBufferUploader);
    mBoxGeo->IndexBufferGPU = D3DUtils::CreateDefaultBuffer(mD3dDevice.Get(), mCommandList.Get(), indices.data(),
        ibByteSize, mBoxGeo->IndexBufferUploader);

    mBoxGeo->VertexByteStride = sizeof(Vertex);
    mBoxGeo->VertexBufferByteSize = vbByteSize;
    mBoxGeo->IndexFormat = DXGI_FORMAT_R16_UINT;
    mBoxGeo->IndexBufferByteSize = ibByteSize;

    SubmeshGeometry submesh
    {
        .IndexCount = static_cast<uint32_t>(indices.size()),
        .StartIndexLocation = 0,
        .BaseVertexLocation = 0,
    };

    mBoxGeo->DrawArgs["box"] = submesh;
}

void BoxApp::BuildDescriptorHeaps()
{
    D3D12_DESCRIPTOR_HEAP_DESC cbvHeapDesc
    {
        .Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV,
        .NumDescriptors = 1,
        .Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE,
        .NodeMask = 0
    };
    ThrowIfHRFailed(mD3dDevice->CreateDescriptorHeap(&cbvHeapDesc, IID_PPV_ARGS(&mCbvHeap)));    
}

void BoxApp::BuildConstantBuffers()
{
    mObjectCB = std::make_unique<UploadBuffer<ObjectConstants>>(mD3dDevice.Get(), 1, true);

    // GPU address of the constant buffer resource.
    D3D12_GPU_VIRTUAL_ADDRESS cbAddress = mObjectCB->Resource()->GetGPUVirtualAddress();
    
    // Offset to the ith object constant buffer int the constant buffer resource.
    size_t ObjCBByteSize = D3DUtils::CalcConstantBufferByteSize(sizeof(ObjectConstants));

    int32_t boxCBufIndex = 0;
    cbAddress += boxCBufIndex * ObjCBByteSize;
    D3D12_CONSTANT_BUFFER_VIEW_DESC cbvDesc
    {
        .BufferLocation = cbAddress,
        .SizeInBytes = static_cast<UINT>(ObjCBByteSize),
    };

    mD3dDevice->CreateConstantBufferView(&cbvDesc, mCbvHeap->GetCPUDescriptorHandleForHeapStart());
}

void BoxApp::BuildRootSignature()
{
	// Shader programs typically require resources as input (constant buffers,
	// textures, samplers).  The root signature defines the resources the shader
	// programs expect.  If we think of the shader programs as a function, and
	// the input resources as function parameters, then the root signature can be
	// thought of as defining the function signature.  

	// Root parameter can be a table, root descriptor or root constants.
    CD3DX12_ROOT_PARAMETER slotRootParameters[1];

    // Create a single descriptor table of CBVs.
    CD3DX12_DESCRIPTOR_RANGE cbvTable;
    cbvTable.Init(D3D12_DESCRIPTOR_RANGE_TYPE_CBV, 1, 0);
    slotRootParameters[0].InitAsDescriptorTable(1, &cbvTable);

    // A root signature is an array of root parameters.
    CD3DX12_ROOT_SIGNATURE_DESC rootSigDesc(1, slotRootParameters, 0, nullptr,
        // Pipeline needs an IA stage.
        D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT);

    // Create a root signature with a single slot which points to a descriptor range consisting of a single constant buffer.
    ComPtr<ID3DBlob> serializedRootSig = nullptr;
    ComPtr<ID3DBlob> errorBlot = nullptr;

    HRESULT hr = D3D12SerializeRootSignature(&rootSigDesc, D3D_ROOT_SIGNATURE_VERSION_1, serializedRootSig.GetAddressOf(),
        errorBlot.GetAddressOf());

    if (errorBlot != nullptr)
    {
        OutputDebugString((char*)errorBlot->GetBufferPointer());
    }
    ThrowIfHRFailed(hr);

    mD3dDevice->CreateRootSignature(0, serializedRootSig->GetBufferPointer(), serializedRootSig->GetBufferSize(),
        IID_PPV_ARGS(&mRootSignature));
}
