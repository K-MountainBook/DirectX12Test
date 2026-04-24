#pragma once
#include <d3dx12.h>
#include "ComPtr.h"

class ConstantBuffer
{
public:
	ConstantBuffer(size_t size);
	bool IsValid();
	D3D12_GPU_VIRTUAL_ADDRESS GetAddress() const;
	D3D12_CONSTANT_BUFFER_VIEW_DESC ViewDesc();

	void* GetPtr() const;

	template<typename T>
	T* GetPtr() {
		return reinterpret_cast<T*>(GetPtr());
	}

private:
	bool m_IsValid = false;
	ComPtr<ID3D12Resource> m_pBuffer;
	D3D12_CONSTANT_BUFFER_VIEW_DESC m_Desc;
	void* m_pMappedPtr = nullptr;

	ConstantBuffer(const ConstantBuffer&) = delete;
	void operator = (const ConstantBuffer&) = delete;
};

