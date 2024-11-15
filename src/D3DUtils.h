#pragma once

#include <string>
#include <d3d12.h>

class DxException
{
public:
    DxException() = default;
    DxException(HRESULT hr, const std::string& functionName, const std::string& filename, int lineNumber);

    std::string ToString()const;

    HRESULT ErrorCode = S_OK;
    std::string FunctionName;
    std::string Filename;
    int LineNumber = -1;
};

#ifndef ThrowIfHRFailed
#define ThrowIfHRFailed(x)                \
{                                       \
    HRESULT hr__ = (x);                 \
    if(FAILED(hr__)) { throw DxException(hr__, #x, __FILE__, __LINE__); } \
}
#endif
