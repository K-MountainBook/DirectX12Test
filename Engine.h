#pragma once
#include <d3d12.h>
#include <dxgi.h>
#include <dxgi1_4.h>
#include "ComPtr.h"
#include <stdio.h>

#pragma comment(lib, "d3d12.lib")
#pragma comment(lib, "dxgi.lib")

class Engine
{
public:
	enum { FRAME_BUFFER_COUNT = 2 };

public:
	bool Init(HWND hwnd, UINT windiwWidth, UINT windiwHeight);

	void BeginRender();
	void EndRender();

public:
	ID3D12Device6* Device();
	ID3D12GraphicsCommandList* CommandList();
	UINT CurrentBackBufferIndex();

private:
	bool CreateDevice();
	bool CreateCommandQueue();
	bool CreateSwapChan();
	bool CreateCommandList();
	bool CreateFence();
	void CreateViewPort();
	void CreateScissorRect();

private:
	HWND m_hWnd;
	UINT m_FrameBufferWidth = 0;
	UINT m_FrameBufferHeight = 0;
	UINT m_CurrentBackBufferIndex = 0;

	ComPtr<ID3D12Device6> m_pDevice = nullptr;
	ComPtr<ID3D12CommandQueue> m_pQueue = nullptr;
	ComPtr<IDXGISwapChain3> m_pSwapChain = nullptr;
	ComPtr<ID3D12CommandAllocator> m_pAllocator[FRAME_BUFFER_COUNT] = { nullptr };
	ComPtr<ID3D12GraphicsCommandList> m_pCommandList = nullptr;
	HANDLE m_fenceEvent = nullptr;
	ComPtr<ID3D12Fence> m_pFence = nullptr;
	UINT64 m_fenceValue[FRAME_BUFFER_COUNT];
	D3D12_VIEWPORT m_Viewport;
	D3D12_RECT m_Scissor;

private:
	bool CreateRenderTarget();
	bool CreateDepthStencil();

	UINT m_RtvDescriptorSize = 0;
	ComPtr<ID3D12DescriptorHeap> m_pRtvHeap = nullptr;
	ComPtr<ID3D12Resource> m_pRenderTargets[FRAME_BUFFER_COUNT] = { nullptr };

	UINT m_DsvDescriptorSize = 0;
	ComPtr<ID3D12DescriptorHeap> m_pDsvHeap = nullptr;
	ComPtr<ID3D12Resource> m_pDepthStencilBuffer = nullptr;

private:
	ID3D12Resource* m_currentRenderTarget = nullptr;
	void WaitRender();
};

extern Engine* g_Engine;	// ÉOÉçÅ[ÉoÉãâªÇ∑ÇÈ
