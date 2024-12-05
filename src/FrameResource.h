#pragma once

#include <utility>
#include <d3d12.h>
#include <memory>

#include "D3DUtils.h"
#include "MathHelper.h"
#include "UploadBuffer.h"

struct ObjectConstants
{
    DirectX::XMFLOAT4X4 world = MathHelper::Identity4x4();
};

struct PassConstants
{
    DirectX::XMFLOAT4X4 view = MathHelper::Identity4x4();
    DirectX::XMFLOAT4X4 invView = MathHelper::Identity4x4();
    DirectX::XMFLOAT4X4 proj = MathHelper::Identity4x4();
    DirectX::XMFLOAT4X4 invProj = MathHelper::Identity4x4();
    DirectX::XMFLOAT4X4 viewProj = MathHelper::Identity4x4();
    DirectX::XMFLOAT4X4 invViewProj = MathHelper::Identity4x4();
    DirectX::XMFLOAT3 eyePosW = { 0.0f, 0.0f, 0.0f };
    float cbPerObjectPad1 = 0.0f;
    DirectX::XMFLOAT2 renderTargetSize = { 0.0f, 0.0f };
    DirectX::XMFLOAT2 invRenderTargetSize = { 0.0f, 0.0f };
    float nearZ = 0.0f;
    float farZ = 0.0f;
    float totalTime = 0.0f;
    float deltaTime = 0.0f;
};

struct Vertex
{
    DirectX::XMFLOAT3 pos;
    DirectX::XMFLOAT4 color;
};

struct FrameResource
{
public:

    FrameResource(ID3D12Device* device, uint32_t passCount, uint32_t objectCount);
    ~FrameResource();

    FrameResource(const FrameResource& rhs) = delete;
    FrameResource& operator=(const FrameResource&) = delete;

    // We cannot reset the allocator until the GPU is done processing the commands.
    // So each frame needs their own allocator.
    ComPtr<ID3D12CommandAllocator> cmdListAlloc;

    // We cannot update a cbuffer until the GPU is done processing the commands that reference it.
    // So each frame needs their own cbuffers.
    std::unique_ptr<UploadBuffer<PassConstants>> passCB = nullptr;
    std::unique_ptr<UploadBuffer<ObjectConstants>> objectCB = nullptr;

    // Fence value to mark commands up the this fence point.
    // This lets us check if these frame resources are still in use by the GPU.
    uint64_t fence = 0;
};
