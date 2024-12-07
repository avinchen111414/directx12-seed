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

void ShapesApp::UpdateObjectCBs(const GameTimer& gt)
{
    UploadBuffer<ObjectConstants>* currObjectCB = mCurrentFrameResource->objectCB.get();
    for (auto& renderItem : mAllRenderItems)
    {
        if (renderItem->numFramesDirty > 0)
        {
            const XMMATRIX world = XMLoadFloat4x4(&renderItem->world);

            ObjectConstants objConstants;
            XMStoreFloat4x4(&objConstants.world, XMMatrixTranspose(world));

            currObjectCB->CopyData(renderItem->objCBIndex, objConstants);
            renderItem->numFramesDirty--;
        }
    }
}

void ShapesApp::UpdateMainPassCB(const GameTimer& gt)
{
    const XMMATRIX view = XMLoadFloat4x4(&mView);
    const XMMATRIX proj = XMLoadFloat4x4(&mProj);

    const XMMATRIX viewProj = XMMatrixMultiply(view, proj);
    const XMMATRIX invView = XMMatrixInverse(&XMMatrixDeterminant(view), view);
    const XMMATRIX invProj = XMMatrixInverse(&XMMatrixDeterminant(proj), proj);
}
