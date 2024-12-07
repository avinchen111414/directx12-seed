#pragma once

#include "D3DApp.h"
#include "MathHelper.h"
#include "D3DUtils.h"

struct FrameResource;
using namespace DirectX;

constexpr int32_t gNumFrameResources = 3;

// Lightweight structure stores parameters to draw a shape.  This will
// vary from app-to-app.
struct RenderItem
{
	RenderItem() = default;

    // World matrix of the shape that describes the object's local space
    // relative to the world space, which defines the position, orientation,
    // and scale of the object in the world.
    XMFLOAT4X4 world = MathHelper::Identity4x4();

	// Dirty flag indicating the object data has changed and we need to update the constant buffer.
	// Because we have an object cbuffer for each FrameResource, we have to apply the
	// update to each FrameResource.  Thus, when we modify obect data we should set 
	// NumFramesDirty = gNumFrameResources so that each frame resource gets the update.
	int numFramesDirty = gNumFrameResources;

	// Index into GPU constant buffer corresponding to the ObjectCB for this render item.
	UINT objCBIndex = -1;

	MeshGeometry* Geo = nullptr;

    // Primitive topology.
    D3D12_PRIMITIVE_TOPOLOGY primitiveType = D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST;

    // DrawIndexedInstanced parameters.
    UINT indexCount = 0;
    UINT startIndexLocation = 0;
    int baseVertexLocation = 0;
    
};

class ShapesApp : public D3DApp
{
public:
    ShapesApp() = default;
    ~ShapesApp() override;

    bool Initialize(const D3DAppInfo& appInfo) override;
    
protected:
    void OnResize() override;

    void Update(const GameTimer& gt) override;
    void Draw(const GameTimer& gt) override;

    void BuildFrameResources();
    void BuildShaders();
    void BuildInputLayout();
    void BuildPSO();
    void BuildBoxGeometry();
    void BuildDescriptorHeaps();
    void BuildConstantBuffers();
    void BuildRootSignature();
    void BuildRenderItems();
    void DrawRenderItems(ID3D12GraphicsCommandList* cmdList, const std::vector<RenderItem*>& renderItems);

    void UpdateObjectCBs(const GameTimer& gt);
    void UpdateMainPassCB(const GameTimer& gt);

private:
    std::vector<std::unique_ptr<FrameResource>> mFrameResources;
    FrameResource* mCurrentFrameResource = nullptr;
    int32_t mCurrentFrameResourceIndex = 0;

    // List of all the render items.
    std::vector<std::unique_ptr<RenderItem>> mAllRenderItems;

    XMFLOAT3 mEyePos = { 0.0f, 0.0f, 0.0f };
    XMFLOAT4X4 mView = MathHelper::Identity4x4();
    XMFLOAT4X4 mProj = MathHelper::Identity4x4();
};