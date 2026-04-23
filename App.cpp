#include "App.h"
#include "Engine.h"

// グローバル変数
// アプリケーションインスタンスハンドルとメインウィンドウハンドルを保持します。
// 小規模なサンプルではグローバルを使っていますが、実運用では適切なスコープ管理を検討してください。
HINSTANCE g_hInst;
HWND g_hWnd = NULL;

/*
 WndProc
 - ウィンドウプロシージャ（メッセージコールバック）
 - Windows がウィンドウに対して送るメッセージ（入力、再描画、終了など）を処理します。
 - ここではアプリ終了時のメッセージ（WM_DESTROY）を受け取ってアプリ終了処理を行っています。
 - その他のメッセージは既定の処理に委譲します（DefWindowProc）。
*/
LRESULT CALLBACK WndProc(HWND hWnd, UINT msg, WPARAM wp, LPARAM lp)
{
	switch (msg)
	{
	case WM_DESTROY:
		// ウィンドウが破棄されたらメインループに終了要求を送る
		PostQuitMessage(0);
		break;
	default:
		break;
	}

	// 未処理のメッセージは OS にデフォルト処理を任せる
	return DefWindowProc(hWnd, msg, wp, lp);
}

/*
 InitWindow
 - ウィンドウクラスの登録とウィンドウ生成を行うヘルパー関数
 - appName: ウィンドウクラス名兼ウィンドウタイトルに使用される文字列
 - 主な処理:
   1. モジュールハンドルを取得（GetModuleHandle）
   2. WNDCLASSEX を初期化して RegisterClassEx に渡す
   3. ウィンドウ矩形（クライアント領域サイズ）を調整（AdjustWindowRect）
   4. CreateWindowEx でウィンドウを生成
   5. ShowWindow / SetFocus で表示・フォーカスを設定
*/
void InitWindow(const TCHAR* appName) {

	g_hInst = GetModuleHandle(nullptr);

	if (g_hInst == NULL)
	{
		// モジュールハンドルが取得できなければ初期化失敗として戻る
		return;
	}

	// WNDCLASSEX のゼロ初期化と必要フィールドの設定
	WNDCLASSEX wc = {};
	wc.cbSize = sizeof(WNDCLASSEX);
	wc.style = CS_HREDRAW | CS_VREDRAW; // 横・縦方向のリサイズで再描画
	wc.lpfnWndProc = WndProc;          // ウィンドウプロシージャ
	wc.hIcon = LoadIcon(g_hInst, IDI_APPLICATION);
	wc.hCursor = LoadCursor(g_hInst, IDC_ARROW);
	wc.hbrBackground = GetSysColorBrush(COLOR_BACKGROUND);
	wc.lpszMenuName = nullptr;
	wc.lpszClassName = appName;
	wc.hIconSm = LoadIcon(g_hInst, IDI_APPLICATION);

	RegisterClassEx(&wc);

	RECT rect = {};

	// 指定したクライアント領域サイズ（サンプル定数）
	rect.right = static_cast<LONG>(WINDOW_WIDTH);
	rect.bottom = static_cast<LONG>(WINDOW_HEIGHT);

	// ウィンドウスタイル（枠、タイトル、システムメニュー）
	auto style = WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU;
	// AdjustWindowRect でウィンドウの外枠を含めたサイズに変換する
	AdjustWindowRect(&rect, style, FALSE);

	// Window の生成
	// CreateWindowEx の引数:
	//  - 拡張スタイル, クラス名, ウィンドウ名, ウィンドウスタイル,
	//  - 位置 (x,y), サイズ (width,height), 親ウィンドウ, メニュー, インスタンスハンドル, 追加パラメータ
	g_hWnd = CreateWindowEx(
		0,
		appName,
		appName,
		style,
		CW_USEDEFAULT,
		CW_USEDEFAULT,
		rect.right - rect.left,
		rect.bottom - rect.top,
		nullptr,
		nullptr,
		g_hInst,
		nullptr
	);

	// ウィンドウを表示してフォーカスを与える
	ShowWindow(g_hWnd, SW_SHOWNORMAL);
	SetFocus(g_hWnd);
}

/*
 MainLoop
 - メインメッセージループ
 - PeekMessage を使ってメッセージをポーリングし、無ければ更新・描画処理を行う想定
 - 注意点: 元コードの PeekMessage 呼び出し内のフラグ引数が `PM_REMOVE == TRUE` と記述されていますが、
   これは意図的なものではなく可読性/正確性の観点から `PM_REMOVE` を直接渡すべきです。
   （現在のコードは結果的に動作することが多いが、誤解を招く書き方です）
*/
void MainLoop() {
	MSG msg = {};

	while (WM_QUIT != msg.message)
	{
		// 正しくは: PeekMessage(&msg, nullptr, 0, 0, PM_REMOVE)
		if (PeekMessage(&msg, nullptr, 0, 0, PM_REMOVE))
		{
			TranslateMessage(&msg);
			DispatchMessage(&msg);
		}
		else
		{
			// メッセージが無い時の処理:
			//  - 更新処理（Update）
			//  - 描画処理（Render）
			// ここにフレーム毎のロジックを入れる
			g_Engine->BeginRender();

			g_Engine->EndRender();

		}
	}

}

/*
 StartApp
 - アプリのエントリポイント的な初期化関数（main/WinMain から呼ぶ想定）
 - ウィンドウ初期化 -> エンジン生成 -> エンジン初期化 -> メインループ開始
 - 注意:
   - `new Engine()` を直接使っており終了時に delete していません。実運用ではスマートポインタや明示的な破棄を検討してください。
   - Engine::init が失敗した場合は適切にリソース解放（ウィンドウ破棄など）を行うことが望ましいです。
*/
void StartApp(const TCHAR* appName) {
	InitWindow(appName);

	g_Engine = new Engine();

	if (!g_Engine->init(g_hWnd, WINDOW_WIDTH, WINDOW_HEIGHT)) {
		// 初期化失敗時は現状ただ戻るだけ。リソース解放やエラーメッセージ表示を追加すること。
		return;
	}

	// メインループへ
	MainLoop();
}
