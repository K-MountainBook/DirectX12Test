#include "Engine.h"
#include <stdio.h>
#include <Windows.h>
#include <DirectXTex.h>
#include <d3dx12.h>

// グローバルなエンジン参照。
// 単純化のためにグローバルポインタを使用しているが、実運用ではシングルトンパターンや
// 所有権の明確化（std::unique_ptr など）を検討すること。
Engine* g_Engine;

/*
 Engine::init
 - 描画エンジンの初期化を行う。
 - hwnd: ウィンドウハンドル（レンダリング先のウィンドウ）
 - windiwWidth / windiwHeight: フレームバッファ（レンダリングターゲット）の幅・高さ
 - 戻り値: 初期化に成功したら true、失敗したら false を返す

 この関数は以下の処理を行う:
 1. メンバ変数にフレームバッファサイズとウィンドウハンドルを保存
 2. Direct3D 12 デバイスの作成（CreateDevice）を呼び出す
 3. コマンドキューの作成（CreateCommandQueue）を呼び出す
 4. 成功・失敗に応じてログを出力して true/false を返す
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


	printf("描画エンジンの初期化に成功しました。\n");
	return true;
}


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
 概要:
 - 深度ステンシルバッファ用のディスクリプタヒープ（DSV）を作成し、
   深度ステンシルリソースをコミットして DSV を生成します。
 - 戻り値: 正常に作成できれば true、失敗すれば false を返します。
 - 前提: m_pDevice が有効であり、m_FrameBufferWidth/Height が正しく設定されていること。
*/

bool Engine::CreateDepthStencil() {

	// 1) DSV 用ディスクリプタヒープの記述子を作成
	//    - DSV は CPU 側ハンドルから作成するため、ヒープは shader-visible ではなく NONE を使うのが一般的
	D3D12_DESCRIPTOR_HEAP_DESC heapDesc = {};
	heapDesc.NumDescriptors = 1;                           // 深度バッファは通常 1 つ
	heapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_DSV;        // DSV タイプ
	heapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE;     // シェーダ可視は不要

	// 2) ディスクリプタヒープを作成
	//    - 失敗したら即座に false を返す
	auto hr = m_pDevice->CreateDescriptorHeap(&heapDesc, IID_PPV_ARGS(&m_pDsvHeap));
	if (FAILED(hr)) {
		return false;
	}

	// 3) DSV ディスクリプタの増分サイズを取得
	//    - 後で複数の DSV を扱う場合に必要
	m_DsvDescriptorSize = m_pDevice->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_DSV);

	// 4) 深度バッファのクリア値を設定
	//    - 深度は 1.0f（最遠）、ステンシルは 0 が一般的な初期値
	D3D12_CLEAR_VALUE dsvClearValue;
	dsvClearValue.Format = DXGI_FORMAT_D32_FLOAT;
	dsvClearValue.DepthStencil.Depth = 1.0f;
	dsvClearValue.DepthStencil.Stencil = 0;

	// 5) 深度ステンシルリソースの説明を作成
	//    - テクスチャ2D、フレームバッファ幅/高さ、1 ミップ、1 配列スライス
	//    - フォーマットは 32-bit 浮動小数点深度（必要に応じて別フォーマットを選択）
	//    - リソースフラグに D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL を指定（深度として使うため）
	//    - パフォーマンス目的でシェーダから参照しないなら DENY_SHADER_RESOURCE をセット
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
	//    - 初期ステートは D3D12_RESOURCE_STATE_DEPTH_WRITE（深度書き込み可能）
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
	//    - nullptr を渡すとリソースの既定ビューを作成
	m_pDevice->CreateDepthStencilView(m_pDepthStencilBuffer.Get(), nullptr, dsvHandle);

	// 10) 正常終了
	return true;
}




/*
 概要:
 - スワップチェインで確保されたバックバッファ数分の RTV 用ディスクリプタヒープを作成し、
   各バックバッファ（IDXGISwapChain の GetBuffer で取得）に対して CreateRenderTargetView を呼んで RTV を作成します。
 - 戻り値: 成功なら true、失敗なら false。
 - 前提: m_pDevice と m_pSwapChain が既に正しく初期化されている必要があります。
*/

bool Engine::CreateRenerTarget() {

	// 1) RTV 用ディスクリプタヒープの記述子を用意する
	//    - NumDescriptors: 生成する RTV の数（バックバッファ数と同じにする）
	//    - Type: RTV 用のディスクリプタヒープ
	//    - Flags: CPU から直接アクセスするために NONE（SRV/UAV の場合は shader visible を検討）
	D3D12_DESCRIPTOR_HEAP_DESC desc = {};
	desc.NumDescriptors = FRAME_BUFFER_COUNT;
	desc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_RTV;
	desc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE;

	// 2) デバイスにヒープを作成させる
	//    - 失敗したら false を返す（リソース不足や不正な引数などが原因）
	auto hr = m_pDevice->CreateDescriptorHeap(&desc, IID_PPV_ARGS(m_pRtvHeap.ReleaseAndGetAddressOf()));
	if (FAILED(hr)) {
		return false;
	}

	// 3) ディスクリプタの増分（1つ分のサイズ）を取得する
	//    - 同じタイプのディスクリプタヒープ内でハンドルを前進させるときに使う
	m_RtvDescriptorSize = m_pDevice->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV);

	// 4) ヒープの先頭から CPU 側ハンドルを取得し、ループで各 RTV を作成していく
	D3D12_CPU_DESCRIPTOR_HANDLE rtvHandle = m_pRtvHeap->GetCPUDescriptorHandleForHeapStart();

	// 5) スワップチェインの各バックバッファを取得して RTV を作成
	//    - IDXGISwapChain::GetBuffer でバックバッファ（ID3D12Resource）を取得
	//    - ID3D12Device::CreateRenderTargetView で RTV を作成（リソースとハンドルを紐付ける）
	for (UINT i = 0; i < FRAME_BUFFER_COUNT; i++) {
		// GetBuffer は IDXGI のインターフェイスから D3D リソースを取得する。
		// 取得したリソースは m_pRenderTargets[i] に格納（ComPtr により参照カウント管理）。
		m_pSwapChain->GetBuffer(i, IID_PPV_ARGS(m_pRenderTargets[i].ReleaseAndGetAddressOf()));

		// CreateRenderTargetView の第二引数に nullptr を渡すと、リソースの既定ビューを作成する。
		// カスタムなビュー（異なるフォーマットやミップ指定等）が必要なら D3D12_RENDER_TARGET_VIEW_DESC を使う。
		m_pDevice->CreateRenderTargetView(m_pRenderTargets[i].Get(), nullptr, rtvHandle);

		// 次のディスクリプタ位置に進める
		rtvHandle.ptr += m_RtvDescriptorSize;
	}

	// 6) 正常終了
	return true;
}



/*
 概要:
 - フェンス値配列 `m_fenceValue` を初期化し、ID3D12Fence を作成、
   さらにフェンス通知用のイベントハンドルを作成します。
 - この関数は GPU/CPU 同期の準備を行うため、レンダリング初期化時に呼び出します。

 処理手順（詳細）:
 1) フレームごとのフェンス値を 0 で初期化する
	-> 各フレームに対応するフェンス値を保持しておくことで、GPU がそのフレームの作業を完了したか
	   を個別に判定できます。コマンド発行ごとに対応するフェンス値を増やして使います。
 2) `ID3D12Device::CreateFence` を呼び初期フェンス（初期値 0）を作成する
	-> `D3D12_FENCE_FLAG_NONE` を指定して標準的なフェンスを作ります。HRESULT をチェックします。
 3) 現在のバックバッファ用フェンス値をインクリメントする
	-> 初期化直後に 1 を進めておくことで、最初の Signal/Wait の準備をします。
 4) `CreateEvent` でフェンス完了通知受信用のイベントハンドルを作成する
	-> 第1引数: セキュリティ属性（nullptr = 既定）
	-> 第2引数: マニュアルリセットかどうか（false = 自動リセット）
	-> 第3引数: 初期状態（false = 非シグナル）
	-> 第4引数: 名前（nullptr = 無名）
 5) イベント生成が成功していれば true を返す。失敗したら false を返す。
*/
bool Engine::CreateFence()
{

	// 1) フレーム用フェンス値を 0 で初期化
	for (auto i = 0u; i < FRAME_BUFFER_COUNT; i++) {
		m_fenceValue[i] = 0;
	}

	// 2) デバイスに対してフェンスを作成する
	//    - 初期値: 0
	//    - フラグ: D3D12_FENCE_FLAG_NONE
	auto hr = m_pDevice->CreateFence(
		0,                         // 初期フェンス値
		D3D12_FENCE_FLAG_NONE,     // フラグ（通常は NONE）
		IID_PPV_ARGS(m_pFence.ReleaseAndGetAddressOf()) // 出力先
	);

	if (FAILED(hr)) {
		// フェンス生成失敗: 初期化不可
		return false;
	}

	// 3) 現在のバックバッファに対応するフェンス値をインクリメント
	//    -> 実際のレンダリングループでは Signal/Wait のベースとして使用します
	m_fenceValue[m_CurrentBackBufferIndex]++;

	// 4) フェンス完了受信用イベントを作成する
	//    - 自動リセットイベント（false = auto-reset）を使用
	m_fenceEvent = CreateEvent(
		nullptr, // セキュリティ属性
		false,   // bManualReset: false = 自動リセット（1 回のシグナルで1つの待ちスレッドを解除）
		false,   // bInitialState: false = 非シグナル状態
		nullptr  // 名前付きイベントではない
	);

	// 5) イベントハンドルが有効かどうかで成功判定を返す
	//    注意: 成功した場合、アプリ終了時に CloseHandle(m_fenceEvent) を呼んで解放する必要があります。
	return m_fenceEvent != nullptr;
}



/*
 Engine::CreateCommandList
 - 概要:
   フレームごとに使用するコマンドアロケータ（ID3D12CommandAllocator）を作成し、
   グラフィックスコマンドを記録するためのコマンドリスト（ID3D12GraphicsCommandList）を生成します。
 - 戻り値:
   成功時に true、失敗時に false を返します。
 - 副作用:
   - m_pAllocator[i] に各フレーム用のコマンドアロケータ（ComPtr）を格納する。
   - m_pCommandList に生成したコマンドリストを格納する。
 - 実装上の注意（初心者向け）:
   - コマンドアロケータは GPU が使用中の間はリセットできないため、フレーム毎に複数個（FRAME_BUFFER_COUNT）保持するのが一般的です。
   - コマンドリストは初期状態で開かれて生成されることが多いですが、このコードでは生成直後に Close() しており、
	 後で Reset() して使うことを想定しています（Reset() の引数にコマンドアロケータを渡して再利用する）。
   - CreateCommandAllocator と CreateCommandList の HRESULT をチェックしてエラー処理を行っています。
*/
bool Engine::CreateCommandList() {
	HRESULT hr;
	// 各フレーム用にコマンドアロケータを作成するループ
	for (size_t i = 0; i < FRAME_BUFFER_COUNT; ++i) {
		// CreateCommandAllocator はコマンドアロケータ（メモリ管理オブジェクト）を生成する。
		hr = m_pDevice->CreateCommandAllocator(
			D3D12_COMMAND_LIST_TYPE_DIRECT,
			IID_PPV_ARGS(m_pAllocator[i].ReleaseAndGetAddressOf()));
	}

	// ループ内最後の hr をチェック。アロケータ作成に失敗していれば false を返す。
	if (FAILED(hr)) {
		return false;
	}

	// コマンドリストを作成する。
	// 第一引数: nodeMask（単一ノード環境なら 0）
	// 第二引数: コマンドリストの種類（DIRECT は描画用）
	// 第三引数: 使用するコマンドアロケータ（ここでは現在のバックバッファ用を指定）
	// 第四引数: 初期パイプラインステート（nullptr で後で設定）
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

	// コマンドリストは生成時に "オープン" 状態になっていることが多いため、
	// 今は使用せずにクローズしておく。実際のフレーム処理では Reset() で再度開く。
	m_pCommandList->Close();

	return true;
}

/*
 Engine::CreateSwapChan
 - 概要:
   DXGI を使ってスワップチェイン（レンダーバッファのフリップ管理）を作成し、
   内部の `m_pSwapChain`（IDXGISwapChain3）に格納します。
 - 戻り値:
   成功時に true、失敗時に false を返します。
 - 副作用:
   - `m_pSwapChain` に有効な IDXGISwapChain3 を格納する（ComPtr により所有）。
   - `m_CurrentBackBufferIndex` を現在のバックバッファインデックスで更新する。
 - 実装上の注意:
   - DXGI ファクトリ（IDXGIFactory4）を生成してからスワップチェインを作成する。
   - サンプルでは `IDXGIFactory::CreateSwapChain` を使用しているが、より新しい API
	 (`CreateSwapChainForHwnd`) を使うことも検討すると良い。
   - スワップチェイン作成には既に作成されたコマンドキュー（m_pQueue）が必要。
   - フリップモデル（DXGI_SWAP_EFFECT_FLIP_DISCARD）を使用しており、モダンなパフォーマンス特性を期待できる。
   - 作成失敗時はローカルで取得した COM ポインタを解放して false を返す。
   - マルチサンプルやフルスクリーン切替の要件がある場合は `DXGI_SWAP_CHAIN_DESC` の設定を調整する。
*/
bool Engine::CreateSwapChan() {
	// DXGI ファクトリを作成する。
	// IDXGIFactory4 はアダプタやスワップチェインの生成に使うファクトリインターフェイス。
	// CreateDXGIFactory1 はファクトリを生成し、戻り値は HRESULT。
	IDXGIFactory4* pFactory = nullptr;
	HRESULT hr = CreateDXGIFactory1(IID_PPV_ARGS(&pFactory));
	if (FAILED(hr)) {
		// ファクトリ生成に失敗したら処理不能のため false を返す
		return false;
	}

	// スワップチェインの設定を行う構造体を初期化する。
	// 各フィールドはスワップチェインの挙動やバッファ形式を決定する。
	DXGI_SWAP_CHAIN_DESC desc = {};
	desc.BufferDesc.Width = m_FrameBufferWidth;                      // クライアント領域幅
	desc.BufferDesc.Height = m_FrameBufferHeight;                    // クライアント領域高さ
	desc.BufferDesc.RefreshRate.Numerator = 60;                      // リフレッシュレート（分子）
	desc.BufferDesc.RefreshRate.Denominator = 1;                     // リフレッシュレート（分母）
	desc.BufferDesc.ScanlineOrdering = DXGI_MODE_SCANLINE_ORDER_UNSPECIFIED; // スキャンライン順序
	desc.BufferDesc.Scaling = DXGI_MODE_SCALING_UNSPECIFIED;         // スケーリング方法
	desc.BufferDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;            // バッファピクセルフォーマット（RGBA 8bit）

	// サンプル（マルチサンプリング）設定
	desc.SampleDesc.Count = 1;     // サンプル数 1 = MSAA 無し
	desc.SampleDesc.Quality = 0;   // サンプル品質

	// バッファの用途、個数、出力先ウィンドウ、ウィンドウモード、スワップ効果など
	desc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;             // レンダーターゲットとして使用
	desc.BufferCount = FRAME_BUFFER_COUNT;                         // バッファ数（フリップモデルでは >=2 推奨）
	desc.OutputWindow = m_hWnd;                                     // 出力先ウィンドウハンドル
	desc.Windowed = TRUE;                                           // ウィンドウモード（FALSE でフルスクリーン）
	desc.SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD;               // 最新のフリップ・モデルを使用（推奨）
	desc.Flags = DXGI_SWAP_CHAIN_FLAG_ALLOW_MODE_SWITCH;           // モード切替（Alt+Enter など）を許可

	// CreateSwapChain は古い API で、CreateSwapChainForHwnd の方が新しい。
	// ここでは互換のため IDXGIFactory::CreateSwapChain を使用している実装。
	IDXGISwapChain* pSwapChain = nullptr;

	// コマンドキュー（m_pQueue）を渡してスワップチェインを作成する。
	// スワップチェインは DXGI 側のオブジェクトで、レンダリングバッファの入れ替えを管理する。
	hr = pFactory->CreateSwapChain(m_pQueue.Get(), &desc, &pSwapChain);
	if (FAILED(hr)) {
		// 失敗時は作成したファクトリを解放してエラーを返す
		pFactory->Release();
		return false;
	}

	// 作成した IDXGISwapChain をより新しいインターフェイス（IDXGISwapChain3）へ問い合わせる。
	// QueryInterface により m_pSwapChain (ComPtr<IDXGISwapChain3>) を取得する。
	// IDXGISwapChain3 は GetCurrentBackBufferIndex など便利なメソッドを持つ。
	hr = pSwapChain->QueryInterface(IID_PPV_ARGS(m_pSwapChain.ReleaseAndGetAddressOf()));
	if (FAILED(hr)) {
		// QueryInterface が失敗した場合は両方解放して終了
		pFactory->Release();
		pSwapChain->Release();
		return false;
	}

	// 現在のバックバッファのインデックスを取得して保存する。
	// フレームレンダリング時にこのインデックスを使って適切なレンダーターゲットを選択する。
	m_CurrentBackBufferIndex = m_pSwapChain->GetCurrentBackBufferIndex();

	// ローカルで取得した COM ポインタは不要になったら解放する。
	// m_pSwapChain は ComPtr に格納済みなので安全に参照できる。
	pFactory->Release();
	pSwapChain->Release();
	return true;
}

/*
 Engine::CreateDevice
 - Direct3D 12 のデバイス（ID3D12Device）を作成するラッパー。
 - ここではデフォルトアダプタ（nullptr）を使用して D3D_FEATURE_LEVEL_11_0 相当の
   機能レベルを要求してデバイスを作成している。

 詳細:
 - D3D12CreateDevice(
	 pAdapter,               // 使用するアダプタ（GPU）へのポインタ。nullptr は既定のアダプタを選択
	 MinimumFeatureLevel,    // 要求する最小の機能レベル（ここでは D3D_FEATURE_LEVEL_11_0）
	 riid,                   // 取得するインターフェイスの IID（IID_PPV_ARGS マクロで簡潔に指定）
	 ppDevice                // 作成されたデバイスの受け取り先
   );
 - IID_PPV_ARGS は COM の初期化を安全に行うためのマクロで、適切な IID と出力ポインタのキャストを行う。
 - m_pDevice はおそらく Microsoft::WRL::ComPtr<ID3D12Device> などのスマートポインタであり、
   ReleaseAndGetAddressOf() は内部ポインタを解放してからアドレスを返す（出力引数用）。
 - SUCCEEDED(hr) マクロで HRESULT を評価し、成功したかどうかを判定する。

 補足（初心者向け）:
 - 実際のアプリでは nullptr を渡して既定のアダプタを使うよりも、使用可能なアダプタを列挙して
   ユーザーやアプリのポリシーに合ったアダプタを選択することが望ましい（例: 高性能 GPU を選ぶ）。
 - デバッグレイヤーを有効にするには開発段階で D3D12GetDebugInterface を使って ID3D12Debug を取得し、
   EnableDebugLayer を呼ぶことで GPU ドライバのデバッグメッセージが得られる（Visual Studio の出力に表示される）。
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
 - コマンドキュー（ID3D12CommandQueue）を作成する。
 - コマンドキューは GPU に対してコマンドリストを送るためのキューで、主に以下の点を設定する:
   - Type: コマンドリストの種類（DIRECT は通常の描画/計算/コピーを含む最も汎用的なタイプ）
   - Priority: 実行優先度（通常は D3D12_COMMAND_QUEUE_PRIORITY_NORMAL）
   - Flags: 実行フラグ（例: D3D12_COMMAND_QUEUE_FLAG_NONE）
   - NodeMask: マルチアダプタやマルチGPU の場合に使用するマスク（通常 0）
 - 戻り値: 作成に成功したら true、失敗したら false を返す
 - 注意点:
   - m_pDevice が nullptr のままでは呼び出せないため、CreateDevice の後で呼ぶ必要がある
   - 作成に失敗した場合は HRESULT をログ出力して原因を調査する（今は簡潔に true/false を返す）
*/
bool Engine::CreateCommandQueue() {
	D3D12_COMMAND_QUEUE_DESC desc = {};
	desc.Type = D3D12_COMMAND_LIST_TYPE_DIRECT;           // 描画等を行う一般的なコマンドキュー
	desc.Priority = D3D12_COMMAND_QUEUE_PRIORITY_NORMAL;  // 通常優先度
	desc.Flags = D3D12_COMMAND_QUEUE_FLAG_NONE;           // 特別なフラグ無し
	desc.NodeMask = 0;                                    // 単一GPU向け

	// ID3D12Device::CreateCommandQueue を呼んでコマンドキューを生成
	auto hr = m_pDevice->CreateCommandQueue(&desc, IID_PPV_ARGS(m_pQueue.ReleaseAndGetAddressOf()));

	// 成功判定を返す (呼び出し元は失敗時に初期化を中止する)
	return SUCCEEDED(hr);
}

