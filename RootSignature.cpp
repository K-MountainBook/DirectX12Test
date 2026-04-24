#include "RootSignature.h"
#include "Engine.h"
#include <d3dx12.h>

RootSignature::RootSignature()
{

}

bool RootSignature::IsValid() {
	return m_IsValid;
}

ID3D12RootSignature* RootSignature::get()
{
	return nullptr;
}
