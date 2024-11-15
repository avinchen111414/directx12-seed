#pragma once

#include <DirectXMath.h>

#include "D3DApp.h"
#include "D3DUtils.h"
#include "MathHelper.h"

struct Vertex
{
    DirectX::XMFLOAT3 Pos;
    DirectX::XMFLOAT4 Color;
};

struct ObjectConstant
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
    void Update(const GameTimer& gt) override;
    void Draw(const GameTimer& gt) override;

    void BuildInputLayout();
    void BuildBoxGeometry();

private:
    std::vector<D3D12_INPUT_ELEMENT_DESC> mInputLayout;

    std::unique_ptr<MeshGeometry> mBoxGeo = nullptr;
};



