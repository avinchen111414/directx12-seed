#include "D3DUtils.h"
#include <wrl.h>
#include <comdef.h>
#include <D3Dcompiler.h>
#include <fstream>

#include "d3dx12.h"

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

ComPtr<ID3D12Resource> D3DUtils::CreateDefaultBuffer(ID3D12Device* device, ID3D12GraphicsCommandList* cmdList,
    const void* initData, size_t byteSize, ComPtr<ID3D12Resource>& uploadBuffer)
{
    ComPtr<ID3D12Resource> defaultBuffer;

    // Create the actual default buffer resource.
    static CD3DX12_HEAP_PROPERTIES defaultHeapProps(D3D12_HEAP_TYPE_DEFAULT);
    CD3DX12_RESOURCE_DESC defaultDesc = CD3DX12_RESOURCE_DESC::Buffer(byteSize);
    ThrowIfHRFailed(device->CreateCommittedResource(&defaultHeapProps, D3D12_HEAP_FLAG_NONE, &defaultDesc, D3D12_RESOURCE_STATE_COMMON,
        nullptr, IID_PPV_ARGS(defaultBuffer.GetAddressOf())));

    // In order to copy CPU memory data into default buffer, we need to create an intermediate upload heap.
    static CD3DX12_HEAP_PROPERTIES uploadHeapProps(D3D12_HEAP_TYPE_UPLOAD);
    CD3DX12_RESOURCE_DESC uploadDesc = CD3DX12_RESOURCE_DESC::Buffer(byteSize);
    // Specs: D3D12_RESOURCE_STATE_GENERIC_READ is the required starting state for an upload heap. But application should generally avoid transitioning
    // to this state when possible, since that can result in premature cache flushes, or resource layout changes (for example, compress/decompress), causing
    // unnecessary pipeline stalls.
    ThrowIfHRFailed(device->CreateCommittedResource(&uploadHeapProps, D3D12_HEAP_FLAG_NONE, &uploadDesc, D3D12_RESOURCE_STATE_GENERIC_READ,
        nullptr, IID_PPV_ARGS(uploadBuffer.GetAddressOf())));

    // Describe the data we want to copy from upload heap to default heap.
    D3D12_SUBRESOURCE_DATA subresourceData
    {
        .pData = initData,
        .RowPitch = (LONG_PTR)byteSize,
        .SlicePitch = (LONG_PTR)byteSize,
    };

    // Schedule to copy the data to the default buffer resource.
    CD3DX12_RESOURCE_BARRIER barriersBeforeCopy [] =
    {
        CD3DX12_RESOURCE_BARRIER::Transition(defaultBuffer.Get(), D3D12_RESOURCE_STATE_COMMON, D3D12_RESOURCE_STATE_COPY_DEST),
    };
    cmdList->ResourceBarrier(_countof(barriersBeforeCopy), barriersBeforeCopy);

    UpdateSubresources<1>(cmdList, defaultBuffer.Get(), uploadBuffer.Get(), 0, 0, 1, &subresourceData);

    CD3DX12_RESOURCE_BARRIER barriersAfterCopy [] =
    {
        CD3DX12_RESOURCE_BARRIER::Transition(defaultBuffer.Get(), D3D12_RESOURCE_STATE_COPY_DEST, D3D12_RESOURCE_STATE_GENERIC_READ),
    };
    cmdList->ResourceBarrier(_countof(barriersAfterCopy), barriersAfterCopy);

    // NOTE: uploadBuffer should be kept alive after the above function calls because the command list has not been executed yet
    // that perform the actual copy.
    // The caller can Release the uploadBuffer after it knows the copy has been executed.
    // TODO: We need a resource management framework to release these intermediate resources automatically.
    
    return defaultBuffer;
}

static std::string WString2String(const std::wstring& ws)
{
    const wchar_t* wchSrc = ws.c_str();
    size_t nDestSize = wcstombs(nullptr, wchSrc, 0) + 1;
    char* chDest = new char [nDestSize];
    memset(chDest, 0, nDestSize);
    wcstombs(chDest, wchSrc, nDestSize);
    std::string strResult = chDest;
    delete [] chDest;
    return strResult;
}

static std::wstring String2WString(const std::string& s)
{
    const char* chSrc = s.c_str();
    size_t nDestSize = mbstowcs(nullptr, chSrc, 0) + 1;
    wchar_t* wchDest = new wchar_t [nDestSize];
    memset(wchDest, 0, nDestSize);
    mbstowcs(wchDest, chSrc, nDestSize);
    std::wstring wstrResult = wchDest;
    delete [] wchDest;
    return wstrResult;
}

ComPtr<ID3DBlob> D3DUtils::LoadBinary(const std::string& filename)
{
    std::ifstream fin(filename, std::ios::binary);

    fin.seekg(0, std::ios_base::end);
    std::ifstream::pos_type size = (int32_t)fin.tellg();
    fin.seekg(0, std::ios_base::beg);

    ComPtr<ID3DBlob> blob;
    ThrowIfHRFailed(D3DCreateBlob(size, blob.GetAddressOf()));

    fin.read((char*)blob->GetBufferPointer(), size);
    fin.close();

    return blob;
}

ComPtr<ID3DBlob> D3DUtils::CompileShader(const std::string& filename, const D3D_SHADER_MACRO* defines,
                                         const std::string& entryPoint, const std::string& target)
{
    uint32_t compileFlags = 0;
#if defined(DEBUG) || defined(_DEBUG)
    compileFlags = D3DCOMPILE_DEBUG | D3DCOMPILE_SKIP_OPTIMIZATION;
#endif

    HRESULT hr = S_OK;

    ComPtr<ID3DBlob> byteCode = nullptr;
    ComPtr<ID3DBlob> errors = nullptr;

    hr = D3DCompileFromFile(String2WString(filename).c_str(), defines, D3D_COMPILE_STANDARD_FILE_INCLUDE, entryPoint.c_str(),
        target.c_str(), compileFlags, 0, &byteCode, &errors);

    if (errors != nullptr)
        OutputDebugString((char*)errors->GetBufferPointer());

    ThrowIfHRFailed(hr);

    return byteCode;
}
