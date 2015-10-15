#define _CRT_SECURE_NO_WARNINGS

#include <windows.h>
#include <stdio.h>
#include <tchar.h>
#include <regex>
#include <string>
#include <iostream>
#include <sstream>
#include <vector>
#include <algorithm>

#include "plugin.h"

// 関数プロトタイプ
BOOL InitInstance(HINSTANCE);
ATOM MyRegisterClass(HINSTANCE);
LRESULT CALLBACK WndProc(HWND, UINT, WPARAM, LPARAM);
DWORD checkAsmCode(char*, t_thread*, DWORD);

HINSTANCE	hinst;			// DLL instance
HWND		hwmain;			// Handle of main OllyDbg window
HWND		hwplugin;		// Plugin window

std::vector<DWORD> v;
std::vector<BYTE> binary;

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

int findIndex(DWORD value)
{
	auto iter = std::find(v.begin(), v.end(), value);
	size_t index = std::distance(v.begin(), iter);

	if (index == v.size())
	{
		return -1;
	}
	return index;
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
		if (pthread){
			eip = pthread->reg.ip;
		}
		else {
			eip = 0;
		}

		code = debugevent->dwDebugEventCode;
		//		Addtolist(eip, 1, "TestPlugin - Code:%d  %s", code, DbgEvent[code]);

		//Exeption Debug Eventならログウィンドウに逆アセンブル結果とコメントを出力
		if (code == 1){
			Disassemble(eip, threadid, command, comment);
			//			Addtolist(eip, -1, "Command - %s", command);
			//			Addtolist(eip, -1, "Comment - %s", comment);

			checkAsmCode(command, pthread, eip);
		}

	}
}

//
// ウィンドウ作成
//
BOOL InitInstance(HINSTANCE hInstance) {

	hwplugin = CreateWindow(TEXT("TEST"), TEXT("MemoVisu"), WS_OVERLAPPEDWINDOW,
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
	wcex.lpszMenuName = NULL;
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
	HPEN hPen;
	HBRUSH hBrush;

	switch (message)
	{
	case WM_DESTROY:
		hwplugin = NULL;
		break;
	case WM_PAINT: {
		hdc = BeginPaint(hWnd, &ps);	//描画開始

		/*
		for (int i = 0; i != binary.size(); ++i) {
			BYTE b;
			ulong addr = 0x410000 + i;
			Readmemory(&b, addr, sizeof(b), MM_SILENT);
			if (binary[i] != b) {
				hPen = CreatePen(PS_SOLID, 1, RGB(0, b, 0));
				hBrush = (HBRUSH)GetStockObject(NULL_BRUSH);
				SelectObject(hdc, hPen);
				SelectObject(hdc, hBrush);

				int margin_x = 0;
				int margin_y = 0;
				int height = 5;
				int width = 5;
				int d = addr - 0x400000;
				int x = d % 0x100;
				int y = d / 0x100;
				char text[64];
				sprintf(text, "x=%02x y=%03x byte=%02x", x, y, b);
				TextOut(hdc, 10, 10, text, lstrlen(text));

				x = x*margin_x + (x - 1)*width;
				y = y*margin_y + (y - 1)*height;

				Rectangle(hdc, x, y, x + width, y + height);
			}
		}
		*/

		/*

		hPen = CreatePen(PS_SOLID, 1, RGB(0, 0, 0));// 黒い点線のペンを作成
		hBrush = (HBRUSH)GetStockObject(NULL_BRUSH);// 空のブラシを取得
		SelectObject(hdc, hPen);		// 作成したペンを使用するように設定
		SelectObject(hdc, hBrush);		// 取得したブラシを使用するように設定
		
		int margin_x = 0;
		int margin_y = 0;
		int height = 5;
		int width = 5;
		
		for (DWORD d : v) {
			d = d - 0x400000;
			int x = d % 0x100;
			int y = d / 0x100;

			char text[64];
			sprintf(text, "x=%02x y=%04x", x, y);
			TextOut(hdc, 10, 10, text, lstrlen(text));

			x = x*margin_x + (x - 1)*width;
			y = y*margin_y + (y - 1)*height;

			Rectangle(hdc, x, y, x + width, y + height);
			//SetPixel(hdc, x, y, RGB(0, 0, 0));
		}

		//DeleteObject(hPen);				// 作成したペンを削除
		*/

		EndPaint(hWnd, &ps);			//描画終了
		break; }
	default:
		return DefWindowProc(hWnd, message, wParam, lParam);
	}
	return 0;
}

DWORD checkAsmCode(char* asmCode, t_thread* pthread, DWORD eip){

	//pthread->reg.r[7];	//EDIへのアクセス例
	//	DWORD eip = pthread->reg.ip;	←これが何故か落ちる

	char *pattern = "MOV (BYTE|WORD|DWORD) PTR DS:\\[(EAX|ECX|EDX|EBX|ESP|EBP|ESI|EDI)\\].*";
	std::string string(asmCode);
	std::regex regex(pattern);
	std::smatch match;

	if (std::regex_match(string, match, regex)){
		std::string size;
		std::string reg;
		std::string addr;
		std::vector< std::string > result;

		size = match[1].str();
		reg = match[2].str();

		for (auto && elem : match) {
			result.push_back(elem.str());
		}

		if (reg == "EDI"){
			//ストリームへ変換
			std::ostringstream ostr;
			ostr << std::hex << pthread->reg.r[7];
			//ストリームから文字列へ
			addr = ostr.str();

			//Vectorへ追加
			v.push_back(pthread->reg.r[7]);
		}
		std::string buf = +"  size:" + size + " reg:" + reg + "=" + addr;
		char info[256];
		sprintf(info, "%s", buf.c_str());

		//ログウィンドウにメモリ書込み命令を出力
		Addtolist(eip, -1, asmCode);
		Addtolist(eip, -1, info);

		//------------------------------
		PAINTSTRUCT ps;
		DWORD d = pthread->reg.r[7];
		BYTE data;
		ulong length;
		length = Readmemory(&data, d, sizeof(data), 0);

		HDC hdc = GetDC(hwplugin);
		HPEN hPen;
		HBRUSH hBrush;
		hPen = CreatePen(PS_SOLID, 1, RGB(0, data, 0));
		hBrush = CreateSolidBrush(RGB(0, data, 0));
		SelectObject(hdc, hPen);
		SelectObject(hdc, hBrush);
		int margin_x = 0;
		int margin_y = 0;
		int height = 4;
		int width = 4;

		d = d - 0x400000;
		int x = d % 0x100;
		int y = d / 0x100;
		
		char text[64];
		sprintf(text, "x=%02x y=%03x data=%02x length=%d", x, y, data, length);
		TextOut(hdc, 10, 10, text, lstrlen(text));

		x = x*margin_x + (x - 1)*width +20;
		y = y*margin_y + (y - 1)*height;

		Rectangle(hdc, x, y, x + width, y + height);

		ReleaseDC(hwplugin, hdc);
		DeleteObject(hPen);
		DeleteObject(hBrush);
		//------------------------------
	}

	return NULL;
}

