#include "D3DUtils.h"
#include <wrl.h>
#include <comdef.h>
#include <fstream>

using Microsoft::WRL::ComPtr;

DxException::DxException(HRESULT hr, const std::string& functionName, const std::string& filename, int lineNumber) :
    ErrorCode(hr),
    FunctionName(functionName),
    Filename(filename),
    LineNumber(lineNumber)
{
}

std::string DxException::ToString() const
{
    // Get the string description of the error code.
    _com_error err(ErrorCode);
    std::string msg = err.ErrorMessage();

    return FunctionName + " failed in " + Filename + "; line " + std::to_string(LineNumber) + "; error: " + msg;
}
