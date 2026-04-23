// Engine.cpp
// Direct3D12 描画エンジンの実装
// - 本ファイルはレンダリング初期化、スワップチェイン、コマンドキュー／コマンドリスト、
//   レンダーターゲット／深度ステンシルの作成、GPU/CPU 同期（フェンス）など、
//   描画パイプラインのコア部分を実装する。
// - 依存: D3D12, DXGI, DirectXTex, d3dx12 ヘルパー（CD3DX12_*）
// - スレッド安全性: 本実装は主にメインスレッド（レンダリングスレッド）から呼び出す前提。
//   マルチスレッドでの使用時は呼び出し側で同期を行うこと。
// 最終更新: 2026-04-23

#include "Engine.h"
#include <Windows.h>
#include <DirectXTex.h>
#include <d3dx12.h>

// グローバルなエンジン参照。
// 簡潔化のためにグローバルポインタを使用している。実運用では所有権の明確化を推奨。
Engine* g_Engine;

/*
 Engine::init
 - 機能:
   描画エンジンを初期化する。デバイスとコマンドキューを作成し、
   フレームバッファサイズとウィンドウハンドルを内部に保持する。
 - 引数:
   hwnd            : レンダリング先ウィンドウのハンドル
   windiwWidth     : フレームバッファの幅
   windiwHeight    : フレームバッファの高さ
 - 戻り値:
   true  : 初期化成功
   false : 初期化失敗
 - 備考:
   CreateDevice / CreateCommandQueue の失敗時に false を返す。
*/
bool Engine::init(HWND hwnd, UINT windiwWidth, UINT windiwHeight)
{

	// フレームバッファ幅・高さを保存
	m_FrameBufferWidth = windiwWidth;
	m_FrameBufferHeight = windiwHeight;

	// ウィンドウハンドルを保存
	m_hWnd = hwnd;

	// Direct3D 12 デバイスを作成
	// CreateDevice が false を返したら初期化失敗
	if (!CreateDevice())
	{
		// デバイスの生成に失敗
		printf("デバイスの生成に失敗しました。");
		return false;
	}

	// コマンドキューの作成
	if (!CreateCommandQueue()) {
		printf("コマンドキューの生成に失敗しました。\n");
		return false;
	}

	if (!CreateSwapChan()) {
		printf("スワップチェインの生成に失敗しました。\n");
		return false;
	}

	if (!CreateCommandList()) {
		printf("コマンドリストの生成に失敗しました。\n");
		return false;
	}

	if (!CreateFence()) {
		printf("フェンスの生成に失敗しました。\n");
		return false;
	}

	if (!CreateRenderTarget()) {
		printf("レンダーターゲットの生成に失敗しました。\n");
		return false;
	}

	if (!CreateDepthStencil()) {
		printf("深度ステンシルバッファの生成に失敗しました。\n");
		return false;
	}

	printf("描画エンジンの初期化に成功しました。\n");
	return true;
}


// Engine::CommandList
// - 機能:
//   内部で保持しているグラフィックスコマンドリスト (ID3D12GraphicsCommandList) への非所有ポインタを返す。
// - 返り値の扱い:
//   - 返されるポインタは `m_pCommandList` (ComPtr) から取得した生ポインタであり、所有権は移動しません。
//   - 呼び出し側はこのポインタを解放してはいけません。
//   - 初期化前や破棄後は nullptr が返る可能性があるため、使用前に必ず nullptr チェックを行ってください。
// - スレッド安全性:
//   - `m_pCommandList` のライフサイクルは `Engine` 側で管理される前提です。別スレッドからアクセスする場合は呼び出し側で同期を行ってください。
ID3D12GraphicsCommandList* Engine::CommandList() {
	return m_pCommandList.Get();
}

// Engine::CurrentBackBufferIndex
// - 機能:
//   現在使用中のバックバッファ（スワップチェイン内インデックス）を返す。
// - 戻り値:
//   m_CurrentBackBufferIndex: 現在のバックバッファインデックス（0 ～ FRAME_BUFFER_COUNT-1）
// - 注意:
//   - この値はレンダリングループや Present/Wait の後に更新されるため、読み取りはレンダリングスレッド内で行うことを推奨します。
//   - スレッド間で共有する場合は呼び出し側で適切に同期してください。
UINT Engine::CurrentBackBufferIndex() {
	return m_CurrentBackBufferIndex;
}

// Engine::Device
// -------------------------------------------------------------
// デバイスへの非所有参照を返すユーティリティ。
// - 戻り値は Engine が保持する内部スマートポインタ `m_pDevice` から取得した生ポインタです。
// - 所有権は呼び出し側には移動しません。呼び出し側で解放しないでください。
// - 初期化前または破棄後は nullptr を返す可能性があるため、使用前に必ず nullptr チェックを行ってください。
// - スレッドセーフ性: `m_pDevice` のライフサイクル管理は外部で行われる前提です。
// -------------------------------------------------------------
ID3D12Device6* Engine::Device() {
	return m_pDevice.Get();
}

/*
 Engine::EndRender
 - 機能:
   現フレームのレンダリングを終了し、コマンドリストをクローズ、
   キューへ送信してスワップチェインに Present を発行する。
 - 副作用:
   m_CurrentBackBufferIndex を次フレームの値に更新する。
 - 注意:
   GPU の処理完了を WaitRender() で待機する。
*/
void Engine::EndRender() {
	auto barrier = CD3DX12_RESOURCE_BARRIER::Transition(m_currentRenderTarget, D3D12_RESOURCE_STATE_RENDER_TARGET, D3D12_RESOURCE_STATE_PRESENT);
	m_pCommandList->ResourceBarrier(1, &barrier);

	m_pCommandList->Close();

	ID3D12CommandList* ppCmdLists[] = { m_pCommandList.Get() };
	m_pQueue->ExecuteCommandLists(1, ppCmdLists);

	m_pSwapChain->Present(1, 0);

	WaitRender();

	m_CurrentBackBufferIndex = m_pSwapChain->GetCurrentBackBufferIndex();

}

/*
 Engine::WaitRender
 - 機能:
   フェンスを用いて GPU が現在のフレーム処理を完了するのを待つ。
 - 動作:
   - 現在のバックバッファに対応するフェンス値を Signal し、
	 フェンスが完了するまでイベントで待機する。
 - 備考:
   SetEventOnCompletion に失敗した場合は早期リターンする。
*/
void Engine::WaitRender() {

	const UINT64 fenceValue = m_fenceValue[m_CurrentBackBufferIndex];
	m_pQueue->Signal(m_pFence.Get(), fenceValue);
	m_fenceValue[m_CurrentBackBufferIndex]++;

	if (m_pFence->GetCompletedValue() < fenceValue) {

		auto hr = m_pFence->SetEventOnCompletion(fenceValue, m_fenceEvent);
		if (FAILED(hr)) {
			return;
		}

		if (WAIT_OBJECT_0 != WaitForSingleObjectEx(m_fenceEvent, INFINITE, FALSE)) {

		}
	}

}

/*
 Engine::BeginRender
 - 機能:
   フレームの描画開始処理を行う。
   コマンドアロケータをリセット、コマンドリストを Reset し、
   ビューポート・シザーの設定、レンダーターゲット／深度ステンシルのクリアを行う。
 - 副作用:
   m_currentRenderTarget を現在のバックバッファへ設定する。
 - 注意:
   コマンドリストはこの関数内でレンダー記録の準備状態になる。
*/
void Engine::BeginRender() {
	m_currentRenderTarget = m_pRenderTargets[m_CurrentBackBufferIndex].Get();

	m_pAllocator[m_CurrentBackBufferIndex]->Reset();
	m_pCommandList->Reset(m_pAllocator[m_CurrentBackBufferIndex].Get(), nullptr);

	m_pCommandList->RSSetViewports(1, &m_Viewport);
	m_pCommandList->RSSetScissorRects(1, &m_Scissor);

	auto currentRtvHandle = m_pRtvHeap->GetCPUDescriptorHandleForHeapStart();
	currentRtvHandle.ptr += m_CurrentBackBufferIndex * m_RtvDescriptorSize;

	auto currentDsvHandle = m_pDsvHeap->GetCPUDescriptorHandleForHeapStart();

	auto barrier = CD3DX12_RESOURCE_BARRIER::Transition(m_currentRenderTarget, D3D12_RESOURCE_STATE_PRESENT, D3D12_RESOURCE_STATE_RENDER_TARGET);
	m_pCommandList->ResourceBarrier(1, &barrier);

	m_pCommandList->OMSetRenderTargets(1, &currentRtvHandle, FALSE, &currentDsvHandle);

	const float clearColor[] = { 0.25f,0.25f,0.25f,1.0f };
	m_pCommandList->ClearRenderTargetView(currentRtvHandle, clearColor, 0, nullptr);

	m_pCommandList->ClearDepthStencilView(currentDsvHandle, D3D12_CLEAR_FLAG_DEPTH, 1.0f, 0, 0, nullptr);

}

/*
 Engine::CreateDepthStencil
 - 機能:
   深度ステンシル用のディスクリプタヒープ（DSV）を作成し、
   深度ステンシル用コミット済みリソースを生成して DSV を作成する。
 - 戻り値:
   true  : 成功
   false : 失敗
 - 前提:
   m_pDevice が初期化済みで、m_FrameBufferWidth / m_FrameBufferHeight が正しいこと。
 - 備考:
   リソースは D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL を使用し、
   シェーダから参照しない場合は DENY_SHADER_RESOURCE を設定している。
*/
bool Engine::CreateDepthStencil() {

	// 1) DSV 用ディスクリプタヒープの記述子を作成
	D3D12_DESCRIPTOR_HEAP_DESC heapDesc = {};
	heapDesc.NumDescriptors = 1;                           // 深度バッファは通常 1 つ
	heapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_DSV;        // DSV タイプ
	heapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE;     // シェーダ可視は不要

	// 2) ディスクリプタヒープを作成
	auto hr = m_pDevice->CreateDescriptorHeap(&heapDesc, IID_PPV_ARGS(&m_pDsvHeap));
	if (FAILED(hr)) {
		return false;
	}

	// 3) DSV ディスクリプタの増分サイズを取得
	m_DsvDescriptorSize = m_pDevice->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_DSV);

	// 4) 深度バッファのクリア値を設定
	D3D12_CLEAR_VALUE dsvClearValue;
	dsvClearValue.Format = DXGI_FORMAT_D32_FLOAT;
	dsvClearValue.DepthStencil.Depth = 1.0f;
	dsvClearValue.DepthStencil.Stencil = 0;

	// 5) 深度ステンシルリソースの説明を作成
	auto heapProp = CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT);
	CD3DX12_RESOURCE_DESC resourceDesc(
		D3D12_RESOURCE_DIMENSION_TEXTURE2D,                  // 2D テクスチャ
		0,                                                  // 1 行サイズ（通常 0）
		m_FrameBufferWidth,                                 // 幅
		m_FrameBufferHeight,                                // 高さ
		1,                                                  // arraySize
		1,                                                  // mipLevels
		DXGI_FORMAT_D32_FLOAT,                              // フォーマット
		1,                                                  // sample count
		0,                                                  // sample quality
		D3D12_TEXTURE_LAYOUT_UNKNOWN,                       // レイアウト
		D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL | D3D12_RESOURCE_FLAG_DENY_SHADER_RESOURCE // フラグ
	);

	// 6) 深度ステンシル用のコミット済みリソースを作成
	hr = m_pDevice->CreateCommittedResource(
		&heapProp,
		D3D12_HEAP_FLAG_NONE,
		&resourceDesc,
		D3D12_RESOURCE_STATE_DEPTH_WRITE,
		&dsvClearValue,
		IID_PPV_ARGS(m_pDepthStencilBuffer.ReleaseAndGetAddressOf())
	);

	// 7) リソース作成失敗チェック
	if (FAILED(hr)) {
		return false;
	}

	// 8) DSV を作成するための CPU ハンドルを取得
	D3D12_CPU_DESCRIPTOR_HANDLE dsvHandle = m_pDsvHeap->GetCPUDescriptorHandleForHeapStart();

	// 9) 深度ステンシルビューを作成
	m_pDevice->CreateDepthStencilView(m_pDepthStencilBuffer.Get(), nullptr, dsvHandle);

	// 10) 正常終了
	return true;
}

/*
 Engine::CreateRenerTarget
 - 機能:
   スワップチェインのバックバッファ数に合わせて RTV 用ディスクリプタヒープを作成し、
   各バックバッファに対して Render Target View を作成する。
 - 戻り値:
   true  : 成功
   false : 失敗
 - 前提:
   m_pDevice と m_pSwapChain が初期化済みであること。
 - 備考:
   RTV は CPU 側からのハンドルで操作するため、ヒープは shader-visible ではない。
*/
bool Engine::CreateRenderTarget() {

	// 1) RTV 用ディスクリプタヒープの記述子を用意する
	D3D12_DESCRIPTOR_HEAP_DESC desc = {};
	desc.NumDescriptors = FRAME_BUFFER_COUNT;
	desc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_RTV;
	desc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE;

	// 2) デバイスにヒープを作成させる
	auto hr = m_pDevice->CreateDescriptorHeap(&desc, IID_PPV_ARGS(m_pRtvHeap.ReleaseAndGetAddressOf()));
	if (FAILED(hr)) {
		return false;
	}

	// 3) ディスクリプタの増分（1つ分のサイズ）を取得する
	m_RtvDescriptorSize = m_pDevice->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV);

	// 4) ヒープの先頭から CPU 側ハンドルを取得し、ループで各 RTV を作成していく
	D3D12_CPU_DESCRIPTOR_HANDLE rtvHandle = m_pRtvHeap->GetCPUDescriptorHandleForHeapStart();

	// 5) スワップチェインの各バックバッファを取得して RTV を作成
	for (UINT i = 0; i < FRAME_BUFFER_COUNT; i++) {
		m_pSwapChain->GetBuffer(i, IID_PPV_ARGS(m_pRenderTargets[i].ReleaseAndGetAddressOf()));
		m_pDevice->CreateRenderTargetView(m_pRenderTargets[i].Get(), nullptr, rtvHandle);
		rtvHandle.ptr += m_RtvDescriptorSize;
	}

	// 6) 正常終了
	return true;
}

/*
 Engine::CreateFence
 - 機能:
   GPU/CPU 同期用のフェンスオブジェクトを作成し、フェンス値配列とイベントハンドルを初期化する。
 - 戻り値:
   true  : 成功（m_pFence と m_fenceEvent が有効）
   false : 失敗
 - 備考:
   作成したイベントハンドルはアプリ終了時に CloseHandle で解放すること。
*/
bool Engine::CreateFence()
{

	// 1) フレーム用フェンス値を 0 で初期化
	for (auto i = 0u; i < FRAME_BUFFER_COUNT; i++) {
		m_fenceValue[i] = 0;
	}

	// 2) デバイスに対してフェンスを作成する
	auto hr = m_pDevice->CreateFence(
		0,                         // 初期フェンス値
		D3D12_FENCE_FLAG_NONE,     // フラグ
		IID_PPV_ARGS(m_pFence.ReleaseAndGetAddressOf()) // 出力先
	);

	if (FAILED(hr)) {
		// フェンス生成失敗
		return false;
	}

	// 3) 現在のバックバッファに対応するフェンス値をインクリメント
	m_fenceValue[m_CurrentBackBufferIndex]++;

	// 4) フェンス完了受信用イベントを作成する（自動リセット）
	m_fenceEvent = CreateEvent(
		nullptr, // セキュリティ属性
		false,   // bManualReset: false = 自動リセット
		false,   // bInitialState: false = 非シグナル
		nullptr  // 名前なし
	);

	// 5) イベントハンドルが有効かどうかで成功判定を返す
	return m_fenceEvent != nullptr;
}

/*
 Engine::CreateCommandList
 - 機能:
   フレームごとのコマンドアロケータを作成し、グラフィックス用コマンドリストを生成する。
 - 戻り値:
   true  : 成功
   false : 失敗
 - 注意:
   コマンドアロケータは FRAME_BUFFER_COUNT 個を用意し、コマンドリストは一つを生成する想定。
*/
bool Engine::CreateCommandList() {
	HRESULT hr;
	// 各フレーム用にコマンドアロケータを作成するループ
	for (size_t i = 0; i < FRAME_BUFFER_COUNT; ++i) {
		// CreateCommandAllocator はコマンドアロケータを生成する。
		hr = m_pDevice->CreateCommandAllocator(
			D3D12_COMMAND_LIST_TYPE_DIRECT,
			IID_PPV_ARGS(m_pAllocator[i].ReleaseAndGetAddressOf()));
	}

	// ループ内最後の hr をチェック。アロケータ作成に失敗していれば false を返す。
	if (FAILED(hr)) {
		return false;
	}

	// コマンドリストを作成する。
	hr = m_pDevice->CreateCommandList(
		0,
		D3D12_COMMAND_LIST_TYPE_DIRECT,
		m_pAllocator[m_CurrentBackBufferIndex].Get(),
		nullptr,
		IID_PPV_ARGS(&m_pCommandList)
	);

	// 作成失敗チェック
	if (FAILED(hr))
	{
		return false;
	}

	// コマンドリストは生成時に開いている場合があるため、ここでは一旦クローズしておく。
	m_pCommandList->Close();

	return true;
}

/*
 Engine::CreateSwapChan
 - 機能:
   DXGI を使用してスワップチェインを作成し、IDXGISwapChain3 を取得する。
 - 戻り値:
   true  : 成功（m_pSwapChain を設定）
   false : 失敗
 - 備考:
   サンプルでは IDXGIFactory::CreateSwapChain を使用しているが、より新しい
   CreateSwapChainForHwnd の利用が推奨される場合がある。
*/
bool Engine::CreateSwapChan() {
	// DXGI ファクトリを作成する
	IDXGIFactory4* pFactory = nullptr;
	HRESULT hr = CreateDXGIFactory1(IID_PPV_ARGS(&pFactory));
	if (FAILED(hr)) {
		return false;
	}

	// スワップチェイン設定
	DXGI_SWAP_CHAIN_DESC desc = {};
	desc.BufferDesc.Width = m_FrameBufferWidth;                      // クライアント領域幅
	desc.BufferDesc.Height = m_FrameBufferHeight;                    // クライアント領域高さ
	desc.BufferDesc.RefreshRate.Numerator = 60;                      // リフレッシュレート（分子）
	desc.BufferDesc.RefreshRate.Denominator = 1;                     // リフレッシュレート（分母）
	desc.BufferDesc.ScanlineOrdering = DXGI_MODE_SCANLINE_ORDER_UNSPECIFIED;
	desc.BufferDesc.Scaling = DXGI_MODE_SCALING_UNSPECIFIED;
	desc.BufferDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;

	// マルチサンプル設定
	desc.SampleDesc.Count = 1;
	desc.SampleDesc.Quality = 0;

	desc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
	desc.BufferCount = FRAME_BUFFER_COUNT;
	desc.OutputWindow = m_hWnd;
	desc.Windowed = TRUE;
	desc.SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD;
	desc.Flags = DXGI_SWAP_CHAIN_FLAG_ALLOW_MODE_SWITCH;

	IDXGISwapChain* pSwapChain = nullptr;

	hr = pFactory->CreateSwapChain(m_pQueue.Get(), &desc, &pSwapChain);
	if (FAILED(hr)) {
		pFactory->Release();
		return false;
	}

	hr = pSwapChain->QueryInterface(IID_PPV_ARGS(m_pSwapChain.ReleaseAndGetAddressOf()));
	if (FAILED(hr)) {
		pFactory->Release();
		pSwapChain->Release();
		return false;
	}

	m_CurrentBackBufferIndex = m_pSwapChain->GetCurrentBackBufferIndex();

	pFactory->Release();
	pSwapChain->Release();
	return true;
}

/*
 Engine::CreateDevice
 - 機能:
   Direct3D12 デバイスを作成するラッパー。
 - 戻り値:
   true  : デバイス作成成功
   false : 失敗
 - 備考:
   ここでは既定アダプタ（nullptr）と D3D_FEATURE_LEVEL_11_0 を要求している。
   より厳密なアダプタ選択やデバッグレイヤの有効化は呼び出し元で行うこと。
*/
bool Engine::CreateDevice() {
	// D3D12CreateDevice を呼んでデバイスを取得
	auto hr = D3D12CreateDevice(
		nullptr,                        // pAdapter: nullptr は既定アダプタを意味する
		D3D_FEATURE_LEVEL_11_0,         // 要求する最小機能レベル
		IID_PPV_ARGS(m_pDevice.ReleaseAndGetAddressOf()) // 出力先: m_pDevice にデバイスを格納
	);

	// HRESULT が成功コードかどうかを判定して true/false を返す
	return SUCCEEDED(hr);
}

/*
 Engine::CreateCommandQueue
 - 機能:
   描画用のコマンドキュー（ID3D12CommandQueue）を作成する。
 - 戻り値:
   true  : 成功
   false : 失敗
 - 備考:
   m_pDevice が有効であることを前提とする。Priority/Flags/NodeMask はデフォルトに設定している。
*/
bool Engine::CreateCommandQueue() {
	D3D12_COMMAND_QUEUE_DESC desc = {};
	desc.Type = D3D12_COMMAND_LIST_TYPE_DIRECT;           // 描画等を行う一般的なコマンドキュー
	desc.Priority = D3D12_COMMAND_QUEUE_PRIORITY_NORMAL;  // 通常優先度
	desc.Flags = D3D12_COMMAND_QUEUE_FLAG_NONE;           // 特別なフラグ無し
	desc.NodeMask = 0;                                    // 単一GPU向け

	// ID3D12Device::CreateCommandQueue を呼んでコマンドキューを生成
	auto hr = m_pDevice->CreateCommandQueue(&desc, IID_PPV_ARGS(m_pQueue.ReleaseAndGetAddressOf()));

	// 成功判定を返す
	return SUCCEEDED(hr);
}