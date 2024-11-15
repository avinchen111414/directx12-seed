#pragma once

#include "D3DApp.h"

class InitD3DApp : public D3DApp
{
public:
    InitD3DApp() = default;

    bool Initialize(const D3DAppInfo& appInfo) override;
    void Update(const GameTimer& gt) override {}
    void Draw(const GameTimer& gt) override;
};


