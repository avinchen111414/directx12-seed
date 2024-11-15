#include "D3DUtils.h"
#include <wrl.h>
#include <comdef.h>
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
    ThrowIfHRFailed(device->CreateCommittedResource(&uploadHeapProps, D3D12_HEAP_FLAG_NONE, &uploadDesc, D3D12_RESOURCE_STATE_COMMON,
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
        CD3DX12_RESOURCE_BARRIER::Transition(uploadBuffer.Get(), D3D12_RESOURCE_STATE_COMMON, D3D12_RESOURCE_STATE_COPY_SOURCE),
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
