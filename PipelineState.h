#pragma once
#include "ComPtr.h"
#include <d3d12.h>
#include <string>

class PipelineState
{
public:
	PipelineState();
	bool IsValid();

	void SetInputLayout(D3D12_INPUT_LAYOUT_DESC layout);
	void SetRootSignature(ID3D12RootSignature* rootSignature);
	void SetVS(std::wstring filePath);
	void SetPS(std::wstring filePath);
	void Create();

	ID3D12PipelineState* Get();

private:
	bool m_IsValid = false;
	D3D12_GRAPHICS_PIPELINE_STATE_DESC desc = {};
	ComPtr<ID3D12PipelineState> m_pPipelineState = nullptr;
	ComPtr<ID3DBlob> m_pVsBlob;
	ComPtr<ID3DBlob> m_pPsBlob;
};

