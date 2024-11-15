#include "D3DApp.h"
#include "D3DUtils.h"
#include "d3dx12.h"

int32_t D3DApp::Run()
{
    mTimer.Reset();

    bool bIsRunning = true;
    while (bIsRunning)
    {
        bool bShouldRender = true;

        // Update window event queue.
        mEventQueue.update();

        // Iterate through that queue.
        while (!mEventQueue.empty())
        {
            // Update events.
            const xwin::Event& event = mEventQueue.front();

            if (event.type == xwin::EventType::Resize)
            {
                const xwin::ResizeData& data = event.data.resize;
                mClientWidth = data.width;
                mClientHeight = data.height;
                OnResize();
                bShouldRender = false;
            }
            else if (event.type == xwin::EventType::Close)
            {
                mWindow.close();
                bShouldRender = false;
                bIsRunning = false;
            }

            mEventQueue.pop();
        }

        // Update visuals.
        if (bShouldRender)
        {
            mTimer.Tick();
            Update(mTimer);
            Draw(mTimer);
        }
    }

    return 0;
}

bool D3DApp::Initialize(const D3DAppInfo& appInfo)
{
    if (!InitWindow(appInfo.windowSize))
        return false;
    
    if (!InitDirect3D())
        return false;

    // Do the initial resize code.
    OnResize();
    
    return true;
}

bool D3DApp::InitWindow(const glm::ivec2& windowSize)
{
    xwin::WindowDesc windowDesc
    {
        .width = (uint32_t)windowSize.x,
        .height = (uint32_t)windowSize.y,
        .visible = true,
        .title = "D3DApp",
        .name = "MainWindow",
    };
    mWindow.create(windowDesc, mEventQueue);
    mWindowSize = windowSize;

    // NOTE: getWindowSize: window size, including toolbar, frames.
    // getCurrentDisplaySize: current display monitor size.
    mClientWidth = windowDesc.width;
    mClientHeight = windowDesc.height;
    
    return true;
}

bool D3DApp::InitDirect3D()
{
#if defined(DEBUG) || defined(_DEBUG)
    // Activate D3D12 debug layer.
    {
        ComPtr<ID3D12Debug> debugController;
        ThrowIfHRFailed(D3D12GetDebugInterface(IID_PPV_ARGS(&debugController)));
        debugController->EnableDebugLayer();
    }
#endif

    ThrowIfHRFailed(CreateDXGIFactory1(IID_PPV_ARGS(&mDxgiFactory)));

    // Adapter means some kinda physical graphics devices.
    ComPtr<IDXGIAdapter1> adapter;
    // We prefer a hardware graphics adapter.
    for (uint32_t adapterIndex = 0; DXGI_ERROR_NOT_FOUND != mDxgiFactory->EnumAdapters1(adapterIndex, &adapter); ++adapterIndex)
    {
        DXGI_ADAPTER_DESC1 desc;
        adapter->GetDesc1(&desc);

        if (desc.Flags & DXGI_ADAPTER_FLAG_SOFTWARE)
        {
            continue;
        }

        ComPtr<ID3D12Device> d3dDevice;
        if (SUCCEEDED(D3D12CreateDevice(adapter.Get(), D3D_FEATURE_LEVEL_11_0, IID_PPV_ARGS(&d3dDevice))))
        {
            std::wstring text = L"Using Adapter: ";
            text += desc.Description;
            text += L"\n";
            OutputDebugStringW(text.c_str());
            
            mD3dAdapter = adapter;
            mD3dDevice = d3dDevice;
            break;
        }
    }

    // Fallback to WARP adapter.
    if (mD3dDevice.Get() == nullptr)
    {
        assert(mD3dAdapter.Get() == nullptr);
        OutputDebugString("Fallback to WARP Adapter.");

        ThrowIfHRFailed(mDxgiFactory->EnumWarpAdapter(IID_PPV_ARGS(&mD3dAdapter)));
        ThrowIfHRFailed(D3D12CreateDevice(mD3dAdapter.Get(), D3D_FEATURE_LEVEL_11_0, IID_PPV_ARGS(&mD3dDevice)));
    }

    ThrowIfHRFailed(mD3dDevice->CreateFence(0, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&mFence)));

    // Cache three kinda descriptor element size.
    mRtvDescriptorSize = mD3dDevice->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV);
    mDsvDescriptorSize = mD3dDevice->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_DSV);
    mCbvDescriptorSize = mD3dDevice->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);

    // Check 4X MSAA quality support for our back buffer format.
    // All Direct3D 11 capable devices support 4X MSAA for all render 
    // target formats, so we only need to check quality support.
	D3D12_FEATURE_DATA_MULTISAMPLE_QUALITY_LEVELS msQualityLevels;
	msQualityLevels.Format = mBackBufferFormat;
	msQualityLevels.SampleCount = 4;
	msQualityLevels.Flags = D3D12_MULTISAMPLE_QUALITY_LEVELS_FLAG_NONE;
	msQualityLevels.NumQualityLevels = 0;
	ThrowIfHRFailed(mD3dDevice->CheckFeatureSupport(
		D3D12_FEATURE_MULTISAMPLE_QUALITY_LEVELS,
		&msQualityLevels,
		sizeof(msQualityLevels)));

    m4xMsaaQuality = msQualityLevels.NumQualityLevels;
	assert(m4xMsaaQuality > 0 && "Unexpected MSAA quality level.");

    CreateCommandObjects();
    CreateSwapChain();
    CreateRtvAndDsvDescriptorHeaps();

    return true;
}

void D3DApp::CreateCommandObjects()
{
    D3D12_COMMAND_QUEUE_DESC queueDesc
    {
        .Type = D3D12_COMMAND_LIST_TYPE_DIRECT,
        .Flags = D3D12_COMMAND_QUEUE_FLAG_NONE,
    };
    ThrowIfHRFailed(mD3dDevice->CreateCommandQueue(&queueDesc, IID_PPV_ARGS(&mCommandQueue)));
    
    ThrowIfHRFailed(mD3dDevice->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT, IID_PPV_ARGS(mDirectCmdListAlloc.GetAddressOf())));
    // pInitialState: The initial graphics pipeline state that applied to the command list.
    ThrowIfHRFailed(mD3dDevice->CreateCommandList(0, D3D12_COMMAND_LIST_TYPE_DIRECT, mDirectCmdListAlloc.Get(), nullptr, IID_PPV_ARGS(mCommandList.GetAddressOf())));

	// Start off in a closed state.  This is because the first time we refer 
	// to the command list we will Reset it, and it needs to be closed before
	// calling Reset.
    mCommandList->Close();
}

void D3DApp::CreateSwapChain()
{
    // Release the previous swapchain we will be recreating.
    mSwapChain.Reset();

    DXGI_SWAP_CHAIN_DESC desc
    {
        .BufferDesc =
        {
            .Width = mClientWidth,
            .Height = mClientHeight,
            .RefreshRate =
            {
                .Numerator = 60,
                .Denominator = 1,
            },
            .Format = mBackBufferFormat,
            .ScanlineOrdering = DXGI_MODE_SCANLINE_ORDER_UNSPECIFIED,
            .Scaling = DXGI_MODE_SCALING_UNSPECIFIED,
        },
        .SampleDesc =
        {
            .Count = m4xMsaaState ? 4u : 1u,
            .Quality = m4xMsaaState ? (m4xMsaaQuality - 1) : 0,
        },
        .BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT,
        .BufferCount = SwapChainBufferCount,
        .OutputWindow = mWindow.getHwnd(),
        .Windowed = true,
        .SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD,
        .Flags = DXGI_SWAP_CHAIN_FLAG_ALLOW_MODE_SWITCH,
    };

    // Swapchain need command queue to wait/signal the fence that mark for the start/finish of presentation.
    // Also swapchain need some correct timing for waiting resource layout transition to present image.
    ThrowIfHRFailed(mDxgiFactory->CreateSwapChain(mCommandQueue.Get(), &desc, mSwapChain.GetAddressOf()));
}

void D3DApp::CreateRtvAndDsvDescriptorHeaps()
{
    D3D12_DESCRIPTOR_HEAP_DESC rtvHeapDesc
    {
        .Type = D3D12_DESCRIPTOR_HEAP_TYPE_RTV,
        .NumDescriptors = SwapChainBufferCount,
        .Flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE,
        .NodeMask = 0,
    };
    ThrowIfHRFailed(mD3dDevice->CreateDescriptorHeap(&rtvHeapDesc, IID_PPV_ARGS(mRtvHeap.GetAddressOf())));
    
    D3D12_DESCRIPTOR_HEAP_DESC dsvHeapDesc
    {
        .Type = D3D12_DESCRIPTOR_HEAP_TYPE_DSV,
        // NOTE: Why only one descriptor in DSV descriptor heap?
        // Since we do not present the DSV content to swapchain, only one DSV would be written by GPU command queue.
        // But RTV would be referenced by both swapchain and command queue, multiple RTVs should be prepared for frames in-flight.  
        .NumDescriptors = 1,
        .Flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE,
        .NodeMask = 0,
    };
    ThrowIfHRFailed(mD3dDevice->CreateDescriptorHeap(&dsvHeapDesc, IID_PPV_ARGS(mDsvHeap.GetAddressOf())));
    
}

D3D12_CPU_DESCRIPTOR_HANDLE D3DApp::CurrentBackBufferView() const
{
    return CD3DX12_CPU_DESCRIPTOR_HANDLE(mRtvHeap->GetCPUDescriptorHandleForHeapStart(), mCurrBackBuffer, mRtvDescriptorSize);
}

D3D12_CPU_DESCRIPTOR_HANDLE D3DApp::DepthStencilView() const
{
    return mDsvHeap->GetCPUDescriptorHandleForHeapStart();
}

void D3DApp::OnResize()
{
    assert(mD3dDevice && mSwapChain && mDirectCmdListAlloc);

    // Flush before changing any resources.
    FlushCommandQueue();

    ThrowIfHRFailed(mCommandList->Reset(mDirectCmdListAlloc.Get(), nullptr))

    // Release the previous resources we will be recreating.
    for (auto& swapChainBuffer : mSwapChainBuffers)
        swapChainBuffer.Reset();
    mDepthStencilBuffer.Reset();

    ThrowIfHRFailed(mSwapChain->ResizeBuffers(SwapChainBufferCount, mClientWidth, mClientHeight, mBackBufferFormat, DXGI_SWAP_CHAIN_FLAG_ALLOW_MODE_SWITCH));

    mCurrBackBuffer = 0;

    CD3DX12_CPU_DESCRIPTOR_HANDLE rtvHeapHandle(mRtvHeap->GetCPUDescriptorHandleForHeapStart());
    for (int32_t i = 0; i < SwapChainBufferCount; ++i)
    {
        ThrowIfHRFailed(mSwapChain->GetBuffer(i, IID_PPV_ARGS(mSwapChainBuffers[i].GetAddressOf())));
        // pDesc: for those non-typeless render target, nullptr means using the format at creation time.
        mD3dDevice->CreateRenderTargetView(mSwapChainBuffers[i].Get(), nullptr, rtvHeapHandle);
        rtvHeapHandle.Offset(1, mRtvDescriptorSize);
    }

    // Create depth/stencil buffer and view.
    D3D12_RESOURCE_DESC depthStencilDesc
    {
        .Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D,
        .Alignment = 0,
        .Width = mClientWidth,
        .Height = mClientHeight,
        .DepthOrArraySize = 1,
        .MipLevels = 1,
        // Renderer might require an SRV to the depth buffer to read from 
        // the depth buffer.  Therefore, because we need to create two views to the same resource:
        //   1. SRV format: DXGI_FORMAT_R24_UNORM_X8_TYPELESS
        //   2. DSV Format: DXGI_FORMAT_D24_UNORM_S8_UINT
        // we need to create the depth buffer resource with a typeless format.
        .Format = DXGI_FORMAT_R24G8_TYPELESS,
        .SampleDesc =
        {
            .Count = m4xMsaaState ? 4u : 1u,
            .Quality = m4xMsaaState ? (m4xMsaaQuality - 1) : 0,
        },
        .Layout = D3D12_TEXTURE_LAYOUT_UNKNOWN,
        // Allows a depth stencil view to be created for the resource, as well as enables the resource to
        // transition into the state of D3D12_RESOURCE_STATE_DEPTH_WRITE and/or D3D12_RESOURCE_STATE_DEPTH_READ.
        .Flags = D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL
    };

    D3D12_CLEAR_VALUE depthStencilClear
    {
        .Format = mDepthStencilFormat,
        .DepthStencil =
        {
            .Depth = 1.0f,
            .Stencil = 0,
        },
    };

    // GPU resources all stay in heap, basically, they are blocks of GPU memory with specified properties,
    // CreateCommittedResource would create a resource and a heap, then commit the resource to the heap.
    // The type of heap is crucial (P111):
    // 1. Default heap (Only GPU Access); 2. Upload heap; 3. Readback heap.
    const CD3DX12_HEAP_PROPERTIES dsHeapProps(D3D12_HEAP_TYPE_DEFAULT);
    ThrowIfHRFailed(mD3dDevice->CreateCommittedResource(&dsHeapProps, D3D12_HEAP_FLAG_NONE,
        &depthStencilDesc, D3D12_RESOURCE_STATE_COMMON, &depthStencilClear, IID_PPV_ARGS(mDepthStencilBuffer.GetAddressOf())));

    // Create descriptor to mip level 0 from entire resource using the format of the resource.
    D3D12_DEPTH_STENCIL_VIEW_DESC dsvDesc
    {
        .Format = mDepthStencilFormat,
        .ViewDimension = m4xMsaaState ? D3D12_DSV_DIMENSION_TEXTURE2DMS : D3D12_DSV_DIMENSION_TEXTURE2D,
        .Flags = D3D12_DSV_FLAG_NONE,
        .Texture2D =
        {
            .MipSlice = 0,
        },
    };
    mD3dDevice->CreateDepthStencilView(mDepthStencilBuffer.Get(), &dsvDesc, DepthStencilView());

    // Transition the resource from its initial state to be used as a depth buffer.
    const D3D12_RESOURCE_BARRIER dsBarrier = CD3DX12_RESOURCE_BARRIER::Transition(mDepthStencilBuffer.Get(),
        D3D12_RESOURCE_STATE_COMMON, D3D12_RESOURCE_STATE_DEPTH_WRITE);
    mCommandList->ResourceBarrier(1, &dsBarrier);

    // Execute the resize commands.
    ThrowIfHRFailed(mCommandList->Close());
    ID3D12CommandList* cmdsLists[] = { mCommandList.Get() };
    mCommandQueue->ExecuteCommandLists(_countof(cmdsLists), cmdsLists);

    // Wait until resize complete.
    FlushCommandQueue();

    // Update viewport transform to cover client area.
    mScreenViewport =
    {
        .TopLeftX = 0.0f,
        .TopLeftY = 0.0f,
        .Width = static_cast<float>(mClientWidth),
        .Height = static_cast<float>(mClientHeight),
        .MinDepth = 0.0f,
        .MaxDepth = 1.0f,
    };

    mScissorRect =
    {
        .left = 0l,
        .top = 0l,
        .right = static_cast<LONG>(mClientWidth),
        .bottom = static_cast<LONG>(mClientHeight),
    };
}

void D3DApp::FlushCommandQueue()
{
    // Advance the fence value to mark commands up to this fence point.
    mCurrentFence++;

    // Add an instruction to the command queue to set a new fence point.
    // Because we are on the GPU timeline, the new fence point won't be set until the GPU finishes processing all the commands
    // prior to this Signal().
    ThrowIfHRFailed(mCommandQueue->Signal(mFence.Get(), mCurrentFence));

    if (mFence->GetCompletedValue() < mCurrentFence)
    {
        const HANDLE eventHandle = CreateEventEx(nullptr, nullptr, false, EVENT_ALL_ACCESS);

        // Fire event when GPU hits current fence.
        ThrowIfHRFailed(mFence->SetEventOnCompletion(mCurrentFence, eventHandle));
        WaitForSingleObject(eventHandle, INFINITE);
        CloseHandle(eventHandle);
    }
}

