#pragma once

#include <DirectXMath.h>

#include "UploadBuffer.h"

using namespace DirectX;

#include "D3DApp.h"
#include "D3DUtils.h"
#include "MathHelper.h"

struct Vertex
{
    DirectX::XMFLOAT3 Pos;
    DirectX::XMFLOAT4 Color;
};

struct ObjectConstants
{
    DirectX::XMFLOAT4X4 WorldViewProj = MathHelper::Identity4x4(); 
};

class BoxApp : public D3DApp
{
public:
    BoxApp() = default;

    bool Initialize(const D3DAppInfo& appInfo) override;

private:
    void OnResize() override;
    void OnMouseMove() override;
    void OnMouseInput() override;
    
    void UpdateCamera();
    void Update(const GameTimer& gt) override;
    void Draw(const GameTimer& gt) override;

    void BuildShaders();
    void BuildInputLayout();
    void BuildPSO();
    void BuildBoxGeometry();
    void BuildDescriptorHeaps();
    void BuildConstantBuffers();
    void BuildRootSignature();

private:
    ComPtr<ID3D12RootSignature> mRootSignature = nullptr;
    ComPtr<ID3D12DescriptorHeap> mCbvHeap = nullptr;
    std::unique_ptr<UploadBuffer<ObjectConstants>> mObjectCB = nullptr;

    ComPtr<ID3DBlob> mvsByteCode = nullptr;
    ComPtr<ID3DBlob> mpsByteCode = nullptr;
    std::vector<D3D12_INPUT_ELEMENT_DESC> mInputLayout;

    ComPtr<ID3D12PipelineState> mPSO = nullptr;

    std::unique_ptr<MeshGeometry> mBoxGeo = nullptr;

    XMFLOAT4X4 mWorld = MathHelper::Identity4x4();
    XMFLOAT4X4 mView = MathHelper::Identity4x4();
    XMFLOAT4X4 mProj = MathHelper::Identity4x4();  

    float mTheta = 1.5f * XM_PI;
    float mPhi = XM_PIDIV4;
    float mRadius = 5.0f;
};



