#pragma once

#include <wrl.h>
using Microsoft::WRL::ComPtr;

#include "glm/glm.hpp"

#include "CrossWindow/CrossWindow.h"
#include "CrossWindow/Graphics.h"

#include "GameTimer.h"

#pragma comment(lib, "d3dcompiler.lib")
#pragma comment(lib, "D3D12.lib")
#pragma comment(lib, "dxgi.lib")

struct D3DAppInfo
{
    glm::ivec2 windowSize;
    std::string windowTitle;
};

class D3DApp
{
public:
    D3DApp(const D3DApp&) = delete;
    D3DApp& operator=(const D3DApp&) = delete;

    int32_t Run();
    virtual bool Initialize(const D3DAppInfo& appInfo);
    
protected:
    D3DApp() {}
    virtual ~D3DApp() = default;

protected:
    virtual void Update(const class GameTimer& gt) = 0;
    virtual void Draw(const class GameTimer& gt) = 0;
    virtual void OnResize();

protected:
    bool InitWindow(const glm::ivec2& windowSize, const std::string& windowTitle);
    bool InitDirect3D();
    void CreateCommandObjects();
    // TODO: Migrate the swapchain to a standalone WSI component.
    void CreateSwapChain();
    void CreateRtvAndDsvDescriptorHeaps();
    ID3D12Resource* CurrentBackBuffer() const;
    D3D12_CPU_DESCRIPTOR_HANDLE CurrentBackBufferView() const;
    D3D12_CPU_DESCRIPTOR_HANDLE DepthStencilView() const;
    void FlushCommandQueue();
    void CalculateFrameStats();
    
protected:
    // Used to keep track of the delta-time and the game time.
    GameTimer mTimer;
    
    glm::ivec2 mWindowSize;
    std::string mWindowTitle;
    xwin::EventQueue mEventQueue;
    xwin::Window mWindow;

    ComPtr<IDXGIFactory4> mDxgiFactory;
    ComPtr<IDXGIAdapter1> mD3dAdapter;
    ComPtr<ID3D12Device> mD3dDevice;

    ComPtr<ID3D12Fence> mFence;
    uint32_t mCurrentFence = 0;
    
    uint32_t mRtvDescriptorSize;
    uint32_t mDsvDescriptorSize;
    uint32_t mCbvDescriptorSize;
    DXGI_FORMAT mBackBufferFormat = DXGI_FORMAT_R8G8B8A8_UNORM;
    DXGI_FORMAT mDepthStencilFormat = DXGI_FORMAT_D24_UNORM_S8_UINT;

    bool m4xMsaaState = false;
    uint32_t m4xMsaaQuality = 0;

    // Command queue is thread safe, but allocator and list are NOT. 
    ComPtr<ID3D12CommandQueue> mCommandQueue;
    ComPtr<ID3D12CommandAllocator> mDirectCmdListAlloc;
    ComPtr<ID3D12GraphicsCommandList> mCommandList;

    ComPtr<IDXGISwapChain> mSwapChain;
    uint32_t mClientWidth;
    uint32_t mClientHeight;

	static constexpr uint32_t SwapChainBufferCount = 2;
    uint32_t mCurrBackBuffer = 0;
    // Storage backend resources for back buffer resource views. 
    ComPtr<ID3D12Resource> mSwapChainBuffers[SwapChainBufferCount];
    ComPtr<ID3D12Resource> mDepthStencilBuffer;
    // Resource views for back buffer resources.
    ComPtr<ID3D12DescriptorHeap> mRtvHeap;
    ComPtr<ID3D12DescriptorHeap> mDsvHeap;

    D3D12_VIEWPORT mScreenViewport;
    D3D12_RECT mScissorRect;
    
    ComPtr<ID3D12DescriptorHeap> mCbvHeap;
};

