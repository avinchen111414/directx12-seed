#include "InitD3DApp.h"

#include <DirectXColors.h>

#include "D3DUtils.h"
#include "d3dx12.h"

bool InitD3DApp::Initialize(const D3DAppInfo& appInfo)
{
    return D3DApp::Initialize(appInfo); 
}

void InitD3DApp::Draw(const GameTimer& gt)
{
    // Reuse the memory associated with command recording.
    // We can only reset when the associated command lists have finished execution on the GPU.
    // The "FlushCommandQueue" call at the end of Draw would make guarantee of this assumption.
    ThrowIfHRFailed(mDirectCmdListAlloc->Reset());
    
	// A command list can be reset after it has been added to the command queue via ExecuteCommandList.
    // Reusing the command list reuses memory.
    ThrowIfHRFailed(mCommandList->Reset(mDirectCmdListAlloc.Get(), nullptr));

    // --------------------------------------------------
    // Beginning of command recording (aka encoding in Vulkan/Metal).

    // Indicate a state transition on the resource usage.
    CD3DX12_RESOURCE_BARRIER barrier = CD3DX12_RESOURCE_BARRIER::Transition(CurrentBackBuffer(),
        D3D12_RESOURCE_STATE_PRESENT, D3D12_RESOURCE_STATE_RENDER_TARGET);
    mCommandList->ResourceBarrier(1, &barrier);

    mCommandList->RSSetViewports(1, &mScreenViewport);
    mCommandList->RSSetScissorRects(1, &mScissorRect);

    // Clear the back buffer and depth buffer.
    mCommandList->ClearRenderTargetView(CurrentBackBufferView(), DirectX::Colors::LightSteelBlue, 0, nullptr);
    mCommandList->ClearDepthStencilView(DepthStencilView(), D3D12_CLEAR_FLAG_DEPTH | D3D12_CLEAR_FLAG_STENCIL, 1.0f, 0, 0, nullptr);

    // Specify the buffers we are going to render to.
    D3D12_CPU_DESCRIPTOR_HANDLE backbufferView = CurrentBackBufferView();
    D3D12_CPU_DESCRIPTOR_HANDLE depthStencilView = DepthStencilView();
    mCommandList->OMSetRenderTargets(1, &backbufferView, true, &depthStencilView);

    // Indicate a state transition on the resource usage.
    barrier = CD3DX12_RESOURCE_BARRIER::Transition(CurrentBackBuffer(),
        D3D12_RESOURCE_STATE_RENDER_TARGET, D3D12_RESOURCE_STATE_PRESENT);
    mCommandList->ResourceBarrier(1, &barrier);

    // End of command recording.
    // --------------------------------------------------

    // Done recording commands.
    ThrowIfHRFailed(mCommandList->Close());

    // Submit the command list to the queue for execution.
    ID3D12CommandList* cmdsList[] = { mCommandList.Get() };
    mCommandQueue->ExecuteCommandLists(_countof(cmdsList), cmdsList);
    
    ThrowIfHRFailed(mSwapChain->Present(0, 0));
    mCurrBackBuffer = (mCurrentFence + 1u) % SwapChainBufferCount;
    
	// Wait until frame commands are complete.  This waiting is inefficient and is
	// done for simplicity.  Later we will show how to organize our rendering code
	// so we do not have to wait per frame, aka "In-Flight Frames".
    FlushCommandQueue();
}
