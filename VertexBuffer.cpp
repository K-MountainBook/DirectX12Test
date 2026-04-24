/*
 * VertexBuffer.cpp
 *
 * Direct3D12 用の頂点バッファラッパー実装ファイル。
 * このファイルはアップロードヒープ上に頂点バッファ用のリソースを確保します。
 *
 * 変更履歴:
 *  - 2026-04-24 : コメントを追加してコードの意図を明確化
 *
 * 注意:
 *  - エラー時は標準出力へエラーメッセージを出力します（開発時のデバッグ補助）。
 *  - 本コンストラクタはアップロードヒープを使用するため、GPU読み取り用に適した状態で作成します。
 */

#include "VertexBuffer.h"
#include "Engine.h"
#include <d3dx12.h>
#include "ConstantBuffer.h"

 /// コンストラクタ
 /// @param size      バッファ全体のサイズ（バイト）
 /// @param stride    単一頂点のサイズ（バイト）
 /// @param pInitData 初期データへのポインタ（nullptr の場合は初期化しない）
VertexBuffer::VertexBuffer(size_t size, size_t stride, const void* pInitData)
{
	// アップロードヒープを示すヒーププロパティを生成
	auto prop = CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD);

	// サイズ指定でバッファ用のリソース記述子を作成
	auto desc = CD3DX12_RESOURCE_DESC::Buffer(size);

	// デバイスに対してコミット済みリソースを作成する
	// アップロードヒープ上のバッファは GENERIC_READ 状態で作成するのが一般的
	auto hr = g_Engine->Device()->CreateCommittedResource(
		&prop,
		D3D12_HEAP_FLAG_NONE,
		&desc,
		D3D12_RESOURCE_STATE_GENERIC_READ,
		nullptr,
		IID_PPV_ARGS(m_pBuffer.GetAddressOf())
	);

	// リソース生成に失敗した場合はエラーメッセージを出力して戻る
	if (FAILED(hr)) {
		// TODO: 将来的にログ出力や例外に置き換えることを検討
		printf("頂点バッファリソースの生成に失敗");
		return;
	}

	m_View.BufferLocation = m_pBuffer->GetGPUVirtualAddress();
	m_View.SizeInBytes = static_cast<UINT>(size);
	m_View.StrideInBytes = static_cast<UINT>(size);

	if (pInitData != nullptr) {
		void* ptr = nullptr;

		hr = m_pBuffer->Map(0, nullptr, &ptr);
		if (FAILED(hr)) {
			printf("頂点バッファマッピングに失敗");
			return;
		}


		memcpy(ptr, pInitData, size);
		m_pBuffer->Unmap(0, nullptr);
	}
	m_IsValid = true;
}

D3D12_VERTEX_BUFFER_VIEW VertexBuffer::View() const
{
	return m_View;
}

bool VertexBuffer::IsValid() {
	return m_IsValid;
}
