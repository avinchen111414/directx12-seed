#include "ShapesApp.h"
#include "D3DUtils.h"
#include "FrameResource.h"

ShapesApp::~ShapesApp()
{
    if (mD3dDevice)
        FlushCommandQueue();
}

bool ShapesApp::Initialize(const D3DAppInfo& appInfo)
{
    if (!D3DApp::Initialize(appInfo))
        return false;

    ThrowIfHRFailed(mCommandList->Reset(mDirectCmdListAlloc.Get(), nullptr));

    
}

void ShapesApp::BuildFrameResources()
{
    for (int32_t i = 0; i < gNumFrameResources; ++i)
    {
        mFrameResources.push_back(std::make_unique<FrameResource>(mD3dDevice.Get(), 1,
            static_cast<uint32_t>(mAllRenderItems.size())));
    }
}
