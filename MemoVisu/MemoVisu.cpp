#define _CRT_SECURE_NO_WARNINGS

#include <windows.h>
#include <commctrl.h>
#include <stdio.h>
#include <tchar.h>
#include <regex>
#include <string>
#include <iostream>
#include <sstream>
#include <vector>
#include <map>
#include <algorithm>

#include "plugin.h"
#include "resource.h"

#pragma comment(lib, "comctl32.lib")      // ツールバーの作成に必要

// グローバル変数:
HANDLE hWrite, hProcess;

// 関数プロトタイプ
BOOL InitInstance(HINSTANCE);
ATOM MyRegisterClass(HINSTANCE);
LRESULT CALLBACK WndProc(HWND, UINT, WPARAM, LPARAM);
DWORD checkAsmCode(char*, t_thread*, DWORD);
void PaintByteMemory(DWORD, char);
//void PrintMemoryBlock(DWORD, int, char);
void MyChildProcess(HWND);
void pushStr(char*);
//void PaintEip(DWORD);

HINSTANCE	hinst;			// DLL instance
HWND		hwmain;			// Handle of main OllyDbg window
HWND		hwplugin;		// Plugin window

std::map<DWORD, int> layer_map;
std::vector<std::vector<DWORD>> vector_w;	//書き込み領域
std::vector<std::vector<DWORD>> vector_r;	//読み込み領域
											//std::vector<std::vector<DWORD>> vector_x;	//実行領域

int margin_x = 0;
int margin_y = 0;
int height = 2;
int width = 2;

BOOL WINAPI DllEntryPoint(HINSTANCE hi, DWORD reason, LPVOID reserved) {
	if (reason == DLL_PROCESS_ATTACH)
		hinst = hi;                          // Mark plugin instance
	return 1;                            // Report success
};

//
// プラグインの存在を知らせるコールバック関数
//
extc int _export cdecl ODBG_Plugindata(char shortname[32]) {
	strcpy(shortname, "MemoVisu");       // Name of plugin
	return PLUGIN_VERSION;
};

//
// プラグインの初期化を行うコールバック関数
//
extc int _export cdecl ODBG_Plugininit(
	int ollydbgversion, HWND hw, ulong *features) {

	if (ollydbgversion<PLUGIN_VERSION)
		return -1;

	hwmain = hw;

	Addtolist(0, 0, "MemoVisu v1.10");
	Addtolist(0, -1, "  Copyright (C) 2015 Tsubasa Bando");

	MyRegisterClass(hinst);

	// アプリケーションの初期化を実行
	if (!InitInstance(hinst))
	{
		return FALSE;
	}

	/*
	for (int i = 0x410000; i < 0x410500; ++i) {
	BYTE b = 0;
	Readmemory(&b, i, sizeof(b), MM_SILENT);
	binary.push_back(b);
	}
	*/

	return 0;
};

//
// デバッグイベントコードに対応するイベント名文字列
//
const char *DbgEvent[64] = {
	"Dummy",
	"Exeption Debug Event",
	"Create Thread Debug Event",
	"Create Process Debug Event",
	"Exit Thread Debug Event",
	"Exit Process Debug Event",
	"Load Dll Debug Event",
	"Unload Dll Debug Event",
	"Output Debug String Debug Event",
	"Rip Event"
};

//
// 逆アセンブル関数
//
ulong Disassemble(ulong addr, ulong threadid, char *command, char *comment) {
	ulong length;
	ulong declength;
	uchar cmd[MAXCMDSIZE], *decode;
	t_disasm da;
	length = Readmemory(cmd, addr, MAXCMDSIZE, MM_SILENT);
	if (length == 0) return 0;
	decode = Finddecode(addr, &declength);
	if (decode != NULL && declength<length)
		decode = NULL;
	length = Disasm(cmd, length, addr, decode, &da, DISASM_ALL, threadid);
	if (length == 0) return 0;
	strcpy(command, da.result);
	strcpy(comment, da.comment);
	return length;
}
//
// OllyDbgのメインループを通るたびに呼び出されるコールバック関数
//
extc void _export cdecl ODBG_Pluginmainloop(DEBUG_EVENT *debugevent) {

	t_thread *pthread;
	int code;
	DWORD eip;
	ulong threadid = Getcputhreadid();
	char command[256];
	char comment[256];

	//デバッグイベントが発生していたら
	if (debugevent) {
		//現在のインタラクションポインタ（EIP）の値を取得
		pthread = Findthread(threadid);
		if (pthread) {
			eip = pthread->reg.ip;
		}
		else {
			eip = 0;
		}

		code = debugevent->dwDebugEventCode;
		//		Addtolist(eip, 1, "TestPlugin - Code:%d  %s", code, DbgEvent[code]);

		//Exeption Debug Eventならログウィンドウに逆アセンブル結果とコメントを出力
		if (code == 1) {
			Disassemble(eip, threadid, command, comment);
			//			Addtolist(eip, -1, "Command - %s", command);	//ログウィンドウに出力
			//			Addtolist(eip, -1, "Comment - %s", comment);

			checkAsmCode(command, pthread, eip);	//メモリアクセス命令の判別
			pushStr(command);	//子プロセスへ渡す
		}

	}
}

//
// ウィンドウ作成
//
BOOL InitInstance(HINSTANCE hInstance) {

	hwplugin = CreateWindow(TEXT("TEST"), TEXT("MemoVisu"), WS_OVERLAPPEDWINDOW | WS_VSCROLL,
		CW_USEDEFAULT, 0, 200, 100, hwmain, NULL, hInstance, NULL);

	if (!hwplugin) {
		return FALSE;
	}

	ShowWindow(hwplugin, SW_SHOWNORMAL);
	UpdateWindow(hwplugin);

	return TRUE;
}

//
//  ウィンドウ クラスを登録
//
ATOM MyRegisterClass(HINSTANCE hInstance)
{
	WNDCLASSEX wcex;

	wcex.cbSize = sizeof(WNDCLASSEX);

	wcex.style = CS_HREDRAW | CS_VREDRAW;
	wcex.lpfnWndProc = WndProc;
	wcex.cbClsExtra = 0;
	wcex.cbWndExtra = 0;
	wcex.hInstance = hInstance;
	wcex.hIcon = LoadIcon(NULL, IDI_APPLICATION);
	wcex.hCursor = LoadCursor(NULL, IDC_ARROW);
	wcex.hbrBackground = (HBRUSH)(COLOR_WINDOW + 1);
	wcex.lpszMenuName = TEXT("IDR_MENU");
	wcex.lpszClassName = TEXT("TEST");
	wcex.hIconSm = LoadIcon(NULL, IDI_APPLICATION);

	return RegisterClassEx(&wcex);
}

//
//  メイン ウィンドウのメッセージを処理
//
LRESULT CALLBACK WndProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam)
{
	PAINTSTRUCT ps;
	HDC hdc;

	static  HBITMAP hDrawBmp = NULL;	//オフスクリーン描画用ビットマップ
	static  HDC     hDrawDC = NULL;		//オフスクリーン描画用デバイスコンテキスト
	RECT rec;

	static int wx, wy;	//画面の大きさ
	static int y;		//スクロール位置
	static int dy;		//増分
	static int range;	//最大スクロール範囲
	static int yclient;	//現在のクライアント領域の高さ

	int wmId, wmEvent;

	switch (message)
	{
	case WM_CREATE:
		wx = GetSystemMetrics(SM_CXSCREEN);
		wy = GetSystemMetrics(SM_CYSCREEN);

		//オフスクリーン描画用デバイスコンテキスト作成
		hDrawDC = CreateCompatibleDC(NULL);       //MDC作成
												  //MDC用ビットマップ作成
		GetClientRect(GetDesktopWindow(), &rec);  //デスクトップのサイズを取得
		hdc = GetDC(hWnd);                        //ウインドウのDCを取得
		hDrawBmp = CreateCompatibleBitmap(hdc, rec.right, rec.bottom);
		ReleaseDC(hWnd, hdc);                     //ウインドウのDCを開放
												  //MDCにビットマップを割り付け
		SelectObject(hDrawDC, hDrawBmp);

		//子プロセス生成
		MyChildProcess(hWnd);
		break;
	case WM_DESTROY:
		if (hProcess)
			CloseHandle(hProcess);
		if (hWrite)
			CloseHandle(hWrite);
		hwplugin = NULL;
		break;
	case WM_SIZE:
		yclient = HIWORD(lParam);
		range = wy - yclient;
		y = min(y, range);
		SetScrollRange(hWnd, SB_VERT, 0, range, FALSE);
		SetScrollPos(hWnd, SB_VERT, y, TRUE);
		break;
	case WM_VSCROLL:
		switch (LOWORD(wParam)) {
		case SB_LINEUP:
			dy = -1;
			break;
		case SB_LINEDOWN:
			dy = 1;
			break;
		case SB_THUMBPOSITION:
			dy = HIWORD(wParam) - y;
			break;
		case SB_PAGEDOWN:
			dy = 10;
			break;
		case SB_PAGEUP:
			dy = -10;
			break;
		default:
			dy = 0;
			break;
		}
		dy = max(-y, min(dy, range - y));
		if (dy != 0) {
			y += dy;
			ScrollWindow(hWnd, 0, -dy, NULL, NULL);
			SetScrollPos(hWnd, SB_VERT, y, TRUE);
			UpdateWindow(hWnd);
		}
		break;
	case WM_PAINT: {
		hdc = BeginPaint(hWnd, &ps);	//描画開始
		if (vector_w.empty() && vector_r.empty()) {
			EndPaint(hWnd, &ps);	//描画終了
			break;
		}

		//HBRUSH hDefBrush = (HBRUSH)SelectObject(hDrawDC, GetStockObject(WHITE_BRUSH));
		GetClientRect(hWnd, &rec);  //ウインドウのサイズを取得
		PatBlt(hDrawDC, 0, 0, rec.right, rec.bottom, PATCOPY);

		BYTE data;	//アドレスの指す値
		int x, y;

		//書込み領域描画
		for (std::vector<DWORD> v : vector_w) {
			for (DWORD d : v) {
				Readmemory(&data, d, sizeof(data), 0);

				d = d - 0x400000;	//オフセット
				x = d % 0x100;
				y = d / 0x100;

				x = x*margin_x + (x - 1)*width + 20;
				y = y*margin_y + (y - 1)*height;

				HPEN hNewPen = CreatePen(PS_SOLID, 1, RGB(0, data, 0));
				HPEN hOldPen = (HPEN)SelectObject(hDrawDC, hNewPen);
				HBRUSH hNewBrush = CreateSolidBrush(RGB(0, data, 0));
				HBRUSH hOldBrush = (HBRUSH)SelectObject(hDrawDC, hNewBrush);

				Rectangle(hDrawDC, x, y, x + width, y + height);

				SelectObject(hDrawDC, hOldPen);
				DeleteObject(hNewPen);
				SelectObject(hDrawDC, hOldBrush);
				DeleteObject(hNewBrush);
			}
		}

		//読込み領域描画
		for (std::vector<DWORD> v : vector_r) {
			for (DWORD d : v) {
				Readmemory(&data, d, sizeof(data), 0);

				d = d - 0x480000;	//オフセット
				x = d % 0x100;
				y = d / 0x100;

				x = x*margin_x + (x - 1)*width + 20;
				y = y*margin_y + (y - 1)*height;

				HPEN hNewPen = CreatePen(PS_SOLID, 1, RGB(data, data, 0));
				HPEN hOldPen = (HPEN)SelectObject(hDrawDC, hNewPen);
				HBRUSH hNewBrush = CreateSolidBrush(RGB(data, data, 0));
				HBRUSH hOldBrush = (HBRUSH)SelectObject(hDrawDC, hNewBrush);

				Rectangle(hDrawDC, x, y, x + width, y + height);

				SelectObject(hDrawDC, hOldPen);
				DeleteObject(hNewPen);
				SelectObject(hDrawDC, hOldBrush);
				DeleteObject(hNewBrush);
			}
		}

		BitBlt(hdc, 0, 0, rec.right, rec.bottom, hDrawDC, 0, 0, SRCCOPY);	//丸コピー
		EndPaint(hWnd, &ps);	//描画終了

		break; }
	case WM_COMMAND:
		wmId = LOWORD(wParam);
		wmEvent = HIWORD(wParam);
		// 選択されたメニューの解析
		switch (wmId)
		{
		case ID_EXIT:
			hwplugin = NULL;
			break;
		default:
			return DefWindowProc(hWnd, message, wParam, lParam);
		}
		break;
	default:
		return DefWindowProc(hWnd, message, wParam, lParam);
	}
	return 0;
}

//メモリアクセス命令の判別と階層分け
DWORD checkAsmCode(char* asmCode, t_thread* pthread, DWORD eip) {

	//pthread->reg.r[7];	//EDIへのアクセス例

	std::string code(asmCode);
	std::smatch match;
	//char *pattern = "MOV (BYTE|WORD|DWORD) PTR DS:\\[(EAX|ECX|EDX|EBX|ESP|EBP|ESI|EDI)\\].*";	//TODO: [EAX+数値]の形式に対応させる
	char *pattern_w = "MOV (BYTE|WORD|DWORD) PTR DS:\\[(..|...)\\].*";
	std::regex regex_w(pattern_w);
	char *pattern_r = "MOV ...?,(BYTE|WORD|DWORD) PTR DS:\\[(..|...)\\]";
	std::regex regex_r(pattern_r);

	std::string size_s;
	int size = 0;
	std::string reg_s;
	int reg_num;
	DWORD reg;

	//PaintEip(eip);

	if (std::regex_match(code, match, regex_w)) {
		size_s = match[1].str();
		reg_s = match[2].str();

		if (size_s == "BYTE") {
			size = 1;
		}
		else if (size_s == "WORD") {
			size = 2;
		}
		else if (size_s == "DWORD") {
			size = 4;
		}

		// 書き込み先アドレス
		if (reg_s == "EDI") {
			reg_num = 7;
		}
		reg = pthread->reg.r[reg_num];

		//EIPで階層マップを検索
		auto itr = layer_map.find(eip);
		if (itr != layer_map.end()) {
			//存在する場合，書き込み先アドレスの階層レベルをEIPの階層レベル+1に設定
			int new_layer_level = layer_map[eip] + 1;
			while (vector_w.size() <= new_layer_level) {
				std::vector<DWORD> v;
				vector_w.push_back(v);
			}
			int i = 0;
			do {
				layer_map[reg + i] = new_layer_level;
				vector_w[new_layer_level].push_back(reg + i);
				PaintByteMemory(reg + i, 'w');	// 描画
				i++;
			} while (i < size);
		}
		else {
			//存在しない場合，書き込み先アドレスの階層レベルを1に設定
			while (vector_w.size() <= 1) {
				std::vector<DWORD> v;
				vector_w.push_back(v);
			}
			int i = 0;
			do {
				layer_map[reg + i] = 1;
				vector_w[1].push_back(reg + i);
				PaintByteMemory(reg + i, 'w');	// 描画
				i++;
			} while (i < size);
		}

		//ログウィンドウにメモリ書込み命令を出力
		std::string buf = " [W]  size:" + size_s + " reg:" + reg_s;
		char info[256];
		sprintf(info, "%s", buf.c_str());
		Addtolist(eip, -1, asmCode);
		Addtolist(eip, -1, info);
	}

	else if (std::regex_match(code, match, regex_r)) {
		size_s = match[1].str();
		reg_s = match[2].str();

		if (size_s == "BYTE") {
			size = 1;
		}
		else if (size_s == "WORD") {
			size = 2;
		}
		else if (size_s == "DWORD") {
			size = 4;
		}


		// 読み込み先アドレス
		if (reg_s == "ESI") {
			reg_num = 6;
		}
		else {
			return NULL;
		}
		reg = pthread->reg.r[reg_num];

		int i = 0;
		do {
			//読み込み先アドレスで階層マップを検索
			auto itr = layer_map.find(reg + i);
			if (itr != layer_map.end()) {
				//存在する場合，読み込み先アドレスを該当階層レベル配列に追加
				int layer_level = layer_map[reg + i];
				while (vector_w.size() <= layer_level) {
					std::vector<DWORD> v;
					vector_w.push_back(v);
				}
				vector_r[layer_level].push_back(reg + i);
				PaintByteMemory(reg + i, 'r');	// 描画
			}
			else {
				//存在しない場合，読み込み先アドレスを階層レベル0配列に追加
				if (vector_r.empty()) {
					std::vector<DWORD> v;
					vector_r.push_back(v);
				}
				vector_r[0].push_back(reg + i);
				PaintByteMemory(reg + i, 'r');	// 描画
			}
			i++;
		} while (i < size);

		//ログウィンドウにメモリ読込み命令を出力
		std::string buf = " [R]  size:" + size_s + " reg:" + reg_s;
		char info[256];
		sprintf(info, "%s", buf.c_str());
		Addtolist(eip, -1, asmCode);
		Addtolist(eip, -1, info);
	}

	return NULL;
}

/*
void PaintEip(DWORD eip) {

HDC hdc = GetDC(hwplugin);

HPEN hPen = CreatePen(PS_SOLID, 1, RGB(255, 0, 0));
HBRUSH hBrush = CreateSolidBrush(RGB(255, 0, 0));
SelectObject(hdc, hPen);   // 作成したペンを使用するように設定
SelectObject(hdc, hBrush); // 取得したブラシを使用するように設定

eip = eip - 0x4D3000;
int x = eip % 0x100;
int y = eip / 0x100;

x = x*margin_x + (x - 1)*width + 20;
y = y*margin_y + (y - 1)*height;

Rectangle(hdc, x, y, x + width, y + height);

ReleaseDC(hwplugin, hdc);
DeleteObject(hPen);
DeleteObject(hBrush);
}
*/

//1バイト分のメモリアクセス描画
void PaintByteMemory(DWORD mem, char mode) {

	BYTE data;

	ulong length = Readmemory(&data, mem, sizeof(data), 0);

	HDC hdc = GetDC(hwplugin);
	HPEN hNewPen = NULL, hOldPen = NULL;
	HBRUSH hNewBrush = NULL, hOldBrush = NULL;

	if (mode == 'w') {
		hNewPen = CreatePen(PS_SOLID, 1, RGB(0, data, 0));
		hOldPen = (HPEN)SelectObject(hdc, hNewPen);
		hNewBrush = CreateSolidBrush(RGB(0, data, 0));
		hOldBrush = (HBRUSH)SelectObject(hdc, hNewBrush);
		//mem = mem - 0x400000;
	}
	else if (mode == 'r') {
		hNewPen = CreatePen(PS_SOLID, 1, RGB(data, data, 0));
		hOldPen = (HPEN)SelectObject(hdc, hNewPen);
		hNewBrush = CreateSolidBrush(RGB(data, data, 0));
		hOldBrush = (HBRUSH)SelectObject(hdc, hNewBrush);
		//mem = mem - 0x480000;
	}

	mem = mem - 0x400000;		//オフセット
	int x = mem % 0x100;
	int y = mem / 0x100;

	x = x*margin_x + (x - 1)*width + 20;
	y = y*margin_y + (y - 1)*height;

	Rectangle(hdc, x, y, x + width, y + height);

	SelectObject(hdc, hOldPen);
	DeleteObject(hNewPen);
	SelectObject(hdc, hOldBrush);
	DeleteObject(hNewBrush);

	char text[64];
	sprintf(text, "mem=%08x x=%d y=%d data=%02x length=%d", mem, x, y, data, length);
	TextOut(hdc, 10, 10, text, lstrlen(text));

	ReleaseDC(hwplugin, hdc);
}

/*
void PrintMemoryBlock(DWORD mem, int size, char mode) {

BYTE* data;
int x, y;

data = (BYTE*)malloc(size);

ulong length = Readmemory(data, mem, size, 0);	//サイズを指定しても何故か1バイトしか読み込まない

HDC hdc = GetDC(hwplugin);
HPEN hNewPen=NULL, hOldPen=NULL;
HBRUSH hNewBrush = NULL, hOldBrush = NULL;

int i = 0;
do {
DWORD d = mem+i;
d = d - 0x400000;
x = d % 0x100;
y = d / 0x100;

x = x*margin_x + (x - 1)*width + 20;
y = y*margin_y + (y - 1)*height;

if (mode == 'w') {
hNewPen = CreatePen(PS_SOLID, 1, RGB(0, data[i], 0));
hOldPen = (HPEN)SelectObject(hdc, hNewPen);
hNewBrush = CreateSolidBrush(RGB(0, data[i], 0));
hOldBrush = (HBRUSH)SelectObject(hdc, hNewBrush);
}
else if (mode == 'r') {
hNewPen = CreatePen(PS_SOLID, 1, RGB(data[i], data[i], 0));
hOldPen = (HPEN)SelectObject(hdc, hNewPen);
hNewBrush = CreateSolidBrush(RGB(data[i], data[i], 0));
hOldBrush = (HBRUSH)SelectObject(hdc, hNewBrush);
}

Rectangle(hdc, x, y, x + width, y + height);

SelectObject(hdc, hOldPen);
DeleteObject(hNewPen);
SelectObject(hdc, hOldBrush);
DeleteObject(hNewBrush);
i++;
} while (i < size);

char text[64];
sprintf(text, "mem=%08x x=%d y=%d data=%08x length=%d", mem, x, y, *data, length);
TextOut(hdc, 10, 10, text, lstrlen(text));

ReleaseDC(hwplugin, hdc);
free(data);
}
*/

//子プロセス作成
void MyChildProcess(HWND hWnd)
{
	HANDLE hRead;
	SECURITY_ATTRIBUTES sa;
	STARTUPINFO si;
	PROCESS_INFORMATION pi;

	sa.nLength = sizeof(SECURITY_ATTRIBUTES);
	sa.lpSecurityDescriptor = NULL;
	sa.bInheritHandle = TRUE;

	if (!CreatePipe(&hRead, &hWrite, &sa, 0)) {
		MessageBox(hWnd, "パイプの作成に失敗しました", "Error", MB_OK);
		return;
	}
	if (!DuplicateHandle(GetCurrentProcess(), //ソースプロセス
		hWrite, //duplicateするハンドル(オリジナルハンドル)
		GetCurrentProcess(), //ターゲットプロセス(行先)
		NULL, //複製ハンドルへのポインタ(コピーハンドル)
		0, //アクセス権
		FALSE, //子供がハンドルを継承するかどうか
		DUPLICATE_SAME_ACCESS)) { //オプション
		MessageBox(hWnd, "DuplicateHandle Error", "Error", MB_OK);
		CloseHandle(hWrite);
		CloseHandle(hRead);
		hWrite = NULL;
		return;
	}

	memset(&si, 0, sizeof(STARTUPINFO));
	si.cb = sizeof(STARTUPINFO);
	si.dwFlags = STARTF_USESTDHANDLES;
	si.hStdInput = hRead;
	si.hStdOutput = GetStdHandle(STD_OUTPUT_HANDLE);
	si.hStdError = GetStdHandle(STD_ERROR_HANDLE);

	if (!CreateProcess(NULL, "child.exe", NULL, NULL, TRUE, 0, NULL, NULL, &si, &pi)) {
		MessageBox(hWnd, "CreateProcess Error", "Error", MB_OK);
		CloseHandle(hWrite);
		hWrite = NULL;
		return;
	}

	hProcess = pi.hProcess;
	CloseHandle(pi.hThread);
	CloseHandle(hRead);

	return;
}

//子プロセスに文字列を渡す
void pushStr(char* szBuf) {
	DWORD dwResult, dwError;
	if (!WriteFile(hWrite, szBuf, 256, &dwResult, NULL)) {
		dwError = GetLastError();
		if (dwError == ERROR_BROKEN_PIPE || dwError == ERROR_NO_DATA) {
			MessageBox(hwplugin, "パイプがありません", "Error", MB_OK);
		}
		else {
			MessageBox(hwplugin, "何らかのエラーです", "Error", MB_OK);
		}
		CloseHandle(hWrite);
		hWrite = NULL;
		CloseHandle(hProcess);
		hProcess = NULL;
	}
}