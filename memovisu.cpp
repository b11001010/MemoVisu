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

#pragma comment(lib, "comctl32.lib")      // �c�[���o�[�̍쐬�ɕK�v

// �O���[�o���ϐ�:
HINSTANCE hInst;                          // ���݂̃C���^�[�t�F�C�X
HWND hToolBar;        // �c�[���o�[�̃E�B���h�E�n���h��
WNDPROC defProc;

// TBBUTTON�i�c�[���o�[�{�^���j�^�̔z��
TBBUTTON tbButton[] = {
	{ 0 , ID_BUTTON1 , TBSTATE_ENABLED , TBSTYLE_BUTTON | BTNS_AUTOSIZE , 0 , 0 , 0 } ,
	{ 1 , ID_BUTTON2 , TBSTATE_ENABLED , TBSTYLE_BUTTON | BTNS_AUTOSIZE , 0 , 0 , 0 } ,
	{ 2 , ID_BUTTON3 , TBSTATE_ENABLED , TBSTYLE_BUTTON | BTNS_AUTOSIZE , 0 , 0 , 0 }
};
TBBUTTON tbSPACE = { 0, 0, TBSTATE_ENABLED, TBSTYLE_SEP, 0, 0L };

// �֐��v���g�^�C�v
BOOL InitInstance(HINSTANCE);
ATOM MyRegisterClass(HINSTANCE);
LRESULT CALLBACK WndProc(HWND, UINT, WPARAM, LPARAM);
DWORD checkAsmCode(char*, t_thread*, DWORD);
void PaintByteMemory(DWORD, char);
void PrintMemoryBlock(DWORD, int, char);
LRESULT CALLBACK ToolbarProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);

HINSTANCE	hinst;			// DLL instance
HWND		hwmain;			// Handle of main OllyDbg window
HWND		hwplugin;		// Plugin window


std::map<DWORD, int> layer_map;
std::vector<std::vector<DWORD>> vector_w;
std::vector<std::vector<DWORD>> vector_r;

int margin_x = 0;
int margin_y = 0;
int height = 3;
int width = 3;

BOOL WINAPI DllEntryPoint(HINSTANCE hi, DWORD reason, LPVOID reserved) {
	if (reason == DLL_PROCESS_ATTACH)
		hinst = hi;                          // Mark plugin instance
	return 1;                            // Report success
};

//
// �v���O�C���̑��݂�m�点��R�[���o�b�N�֐�
//
extc int _export cdecl ODBG_Plugindata(char shortname[32]) {
	strcpy(shortname, "MemoVisu");       // Name of plugin
	return PLUGIN_VERSION;
};

//
// �v���O�C���̏��������s���R�[���o�b�N�֐�
//
extc int _export cdecl ODBG_Plugininit(
	int ollydbgversion, HWND hw, ulong *features) {

	if (ollydbgversion<PLUGIN_VERSION)
		return -1;

	hwmain = hw;

	Addtolist(0, 0, "MemoVisu v1.10");
	Addtolist(0, -1, "  Copyright (C) 2015 Tsubasa Bando");

	MyRegisterClass(hinst);

	// �A�v���P�[�V�����̏����������s
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
// �f�o�b�O�C�x���g�R�[�h�ɑΉ�����C�x���g��������
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
// �t�A�Z���u���֐�
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

int findIndex(DWORD value, std::vector<DWORD> v)
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
// OllyDbg�̃��C�����[�v��ʂ邽�тɌĂяo�����R�[���o�b�N�֐�
//
extc void _export cdecl ODBG_Pluginmainloop(DEBUG_EVENT *debugevent) {

	t_thread *pthread;
	int code;
	DWORD eip;
	ulong threadid = Getcputhreadid();
	char command[256];
	char comment[256];

	//�f�o�b�O�C�x���g���������Ă�����
	if (debugevent) {
		//���݂̃C���^���N�V�����|�C���^�iEIP�j�̒l���擾
		pthread = Findthread(threadid);
		if (pthread){
			eip = pthread->reg.ip;
		}
		else {
			eip = 0;
		}

		code = debugevent->dwDebugEventCode;
		//		Addtolist(eip, 1, "TestPlugin - Code:%d  %s", code, DbgEvent[code]);

		//Exeption Debug Event�Ȃ烍�O�E�B���h�E�ɋt�A�Z���u�����ʂƃR�����g���o��
		if (code == 1){
			Disassemble(eip, threadid, command, comment);
			//			Addtolist(eip, -1, "Command - %s", command);
			//			Addtolist(eip, -1, "Comment - %s", comment);

			checkAsmCode(command, pthread, eip);
		}

	}
}

//
// �E�B���h�E�쐬
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
//  �E�B���h�E �N���X��o�^
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
//  ���C�� �E�B���h�E�̃��b�Z�[�W������
//
LRESULT CALLBACK WndProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam)
{
	PAINTSTRUCT ps;
	HDC hdc;

	static int wx, wy;//��ʂ̑傫��
	static int y;//�X�N���[���ʒu
	static int dy;//����
	static int range;//�ő�X�N���[���͈�
	static int yclient;//���݂̃N���C�A���g�̈�̍���

	int wmId, wmEvent;

	int wmax, iString0, iString1, iString2;
	TCHAR szBuf[128];
	static UINT uToolStyle;

	switch (message)
	{
	case WM_CREATE:
		wx = GetSystemMetrics(SM_CXSCREEN);
		wy = GetSystemMetrics(SM_CYSCREEN);

		InitCommonControls();
		wmax = GetSystemMetrics(SM_CXSCREEN);
		hInst = (HINSTANCE)GetWindowLong(hWnd, GWL_HINSTANCE);
		hToolBar = CreateWindowEx(
			0,                          // �g���X�^�C���Ȃ�
			TOOLBARCLASSNAME,           // �N���X�l�[��
			NULL,                       // �E�B���h�E�^�C�g��
			WS_CHILD | WS_VISIBLE,      // �E�B���h�E�X�^�C��
			0, 0,                       // �E�B���h�E�ʒu
			wmax, 40,                   // �E�B���h�E���A����
			hWnd,                       // �e�E�B���h�E
			(HMENU)IDW_TOOL,            // �R���g���[�����ʎq
			hInst,                      // �C���X�^���X�n���h��
			NULL);
		SendMessage(hToolBar, TB_BUTTONSTRUCTSIZE, (WPARAM)sizeof(TBBUTTON), 0);

		/* �c�[���o�[�{�^���ɕ������\��t�� */
		LoadString(hInst, IDC_BUTTON1, (LPTSTR)&szBuf, sizeof(szBuf) - 1);
		iString0 = SendMessage(hToolBar, TB_ADDSTRING, 0, (LPARAM)szBuf);
		LoadString(hInst, IDC_BUTTON2, (LPTSTR)&szBuf, sizeof(szBuf) - 1);
		iString1 = SendMessage(hToolBar, TB_ADDSTRING, 0, (LPARAM)szBuf);
		LoadString(hInst, IDC_BUTTON3, (LPTSTR)&szBuf, sizeof(szBuf) - 1);
		iString2 = SendMessage(hToolBar, TB_ADDSTRING, 0, (LPARAM)szBuf);
		tbButton[0].iString = iString0;
		tbButton[1].iString = iString1;
		tbButton[2].iString = iString2;

		/* �c�[���o�[�Ƀ{�^����}�� */
		SendMessage(hToolBar, TB_ADDBUTTONS,
			(WPARAM)(sizeof(tbButton) / sizeof(TBBUTTON)), (LPARAM)(LPTBBUTTON)&tbButton);
		// �{�^���̋�؂��}��
		SendMessage(hToolBar, TB_INSERTBUTTON, 2, (LPARAM)&tbSPACE);

		/* �c�[���o�[�T�u�N���X�� */
		defProc = (WNDPROC)GetWindowLong(hToolBar, GWL_WNDPROC);
		SetWindowLong(hToolBar, GWL_WNDPROC, (LONG)ToolbarProc);
		SetWindowLong(hToolBar, GWL_USERDATA, (LONG)defProc);

		/* �t���b�g�c�[���o�[ */
		uToolStyle = (UINT)GetWindowLong(hToolBar, GWL_STYLE);
		uToolStyle |= (TBSTYLE_FLAT);
		SetWindowLong(hToolBar, GWL_STYLE, (LONG)uToolStyle);
		SendMessage(hToolBar, TB_AUTOSIZE, 0, 0);
		InvalidateRect(hToolBar, NULL, TRUE);

		break;
	case WM_DESTROY:
		hwplugin = NULL;
		break;
	case WM_SIZE:
		yclient = HIWORD(lParam);
		range = wy - yclient;
		y = min(y, range);
		SetScrollRange(hWnd, SB_VERT, 0, range, FALSE);
		SetScrollPos(hWnd, SB_VERT, y, TRUE);

		MoveWindow(hToolBar, 0, 0, LOWORD(lParam), HIWORD(lParam), TRUE);
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
			InvalidateRect(hWnd, NULL, FALSE);
		}
		break;
	case WM_PAINT: {
		hdc = BeginPaint(hWnd, &ps);	//�`��J�n

		if (vector_w.empty() && vector_r.empty()) {
			EndPaint(hWnd, &ps);	//�`��I��
			break;
		}

		BYTE data;	//�A�h���X�̎w���l
		int x, y;
		
		//�����ݗ̈�`��
		for (std::vector<DWORD> v : vector_w) {
			for (DWORD d : v) {
				Readmemory(&data, d, sizeof(data), 0);

				d = d - 0x400000;
				x = d % 0x100;
				y = d / 0x100;

				x = x*margin_x + (x - 1)*width + 20;
				y = y*margin_y + (y - 1)*height;

				HPEN hNewPen = CreatePen(PS_SOLID, 1, RGB(0, data, 0));
				HPEN hOldPen = (HPEN)SelectObject(hdc, hNewPen);
				HBRUSH hNewBrush = CreateSolidBrush(RGB(0, data, 0));
				HBRUSH hOldBrush = (HBRUSH)SelectObject(hdc, hNewBrush);

				Rectangle(hdc, x, y, x + width, y + height);

				SelectObject(hdc, hOldPen);
				DeleteObject(hNewPen);
				SelectObject(hdc, hOldBrush);
				DeleteObject(hNewBrush);
			}
		}

		//�Ǎ��ݗ̈�`��
		for (std::vector<DWORD> v : vector_r) {
			for (DWORD d : v) {
				Readmemory(&data, d, sizeof(data), 0);

				d = d - 0x480000;
				x = d % 0x100;
				y = d / 0x100;

				x = x*margin_x + (x - 1)*width + 20;
				y = y*margin_y + (y - 1)*height;

				HPEN hNewPen = CreatePen(PS_SOLID, 1, RGB(data, data, 0));
				HPEN hOldPen = (HPEN)SelectObject(hdc, hNewPen);
				HBRUSH hNewBrush = CreateSolidBrush(RGB(data, data, 0));
				HBRUSH hOldBrush = (HBRUSH)SelectObject(hdc, hNewBrush);

				Rectangle(hdc, x, y, x + width, y + height);

				SelectObject(hdc, hOldPen);
				DeleteObject(hNewPen);
				SelectObject(hdc, hOldBrush);
				DeleteObject(hNewBrush);
			}
		}
		

		//char text[64];
		//sprintf(text, "x=%02x y=%03x data=%02x", x, y, data);
		//TextOut(hdc, 10, 10, text, lstrlen(text));
		EndPaint(hWnd, &ps);	//�`��I��
		break; }
	case WM_COMMAND:
		wmId = LOWORD(wParam);
		wmEvent = HIWORD(wParam);
		// �I�����ꂽ���j���[�̉��
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

DWORD checkAsmCode(char* asmCode, t_thread* pthread, DWORD eip){

	//pthread->reg.r[7];	//EDI�ւ̃A�N�Z�X��
	//DWORD eip = pthread->reg.ip;	�����ꂪ���̂�������

	std::string code(asmCode);
	std::smatch match;
	//char *pattern = "MOV (BYTE|WORD|DWORD) PTR DS:\\[(EAX|ECX|EDX|EBX|ESP|EBP|ESI|EDI)\\].*";	//TODO: [EAX+���l]�̌`���ɑΉ�������
	char *pattern_w = "MOV (BYTE|WORD|DWORD) PTR DS:\\[(..|...)\\].*";
	std::regex regex_w(pattern_w);
	char *pattern_r = "MOV ...?,(BYTE|WORD|DWORD) PTR DS:\\[(..|...)\\]";
	std::regex regex_r(pattern_r);

	std::string size_s;
	int size = 0;
	std::string reg_s;
	int reg_num;
	DWORD reg;

	if (std::regex_match(code, match, regex_w)){
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

		// �������ݐ�A�h���X
		if (reg_s == "EDI") {
			reg_num = 7;
		}
		reg = pthread->reg.r[reg_num];
		
		//EIP�ŊK�w�}�b�v������
		auto itr = layer_map.find(eip);
		if (itr != layer_map.end()) {
			//���݂���ꍇ�C�������ݐ�A�h���X�̊K�w���x����EIP�̊K�w���x��+1�ɐݒ�
			int new_layer_level = layer_map[eip] + 1;
			while (vector_w.size() <= new_layer_level) {
				std::vector<DWORD> v;
				vector_w.push_back(v);
			}
			int i = 0;
			do {
				layer_map[reg + i] = new_layer_level;
				vector_w[new_layer_level].push_back(reg + i);
				PaintByteMemory(reg + i, 'w');	// �`��
				i++;
			} while (i < size);
		}
		else {
			//���݂��Ȃ��ꍇ�C�������ݐ�A�h���X�̊K�w���x����1�ɐݒ�
			while (vector_w.size() <= 1) {
				std::vector<DWORD> v;
				vector_w.push_back(v);
			}
			int i = 0;
			do {
				layer_map[reg + i] = 1;
				vector_w[1].push_back(reg+i);
				PaintByteMemory(reg + i, 'w');	// �`��
				i++;
			} while (i < size);
		}

		//���O�E�B���h�E�Ƀ����������ݖ��߂��o��
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


		// �ǂݍ��ݐ�A�h���X
		if (reg_s == "ESI") {
			reg_num = 6;
		}
		else {
			return NULL;
		}
		reg = pthread->reg.r[reg_num];

		int i = 0;
		do {
			//�ǂݍ��ݐ�A�h���X�ŊK�w�}�b�v������
			auto itr = layer_map.find(reg + i);
			if (itr != layer_map.end()) {
				//���݂���ꍇ�C�ǂݍ��ݐ�A�h���X���Y���K�w���x���z��ɒǉ�
				int layer_level = layer_map[reg+i];
				while (vector_w.size() <= layer_level) {
					std::vector<DWORD> v;
					vector_w.push_back(v);
				}
				vector_r[layer_level].push_back(reg + i);
				PaintByteMemory(reg + i, 'r');	// �`��
			}
			else {
				//���݂��Ȃ��ꍇ�C�ǂݍ��ݐ�A�h���X���K�w���x��0�z��ɒǉ�
				if (vector_r.empty()) {
					std::vector<DWORD> v;
					vector_r.push_back(v);
				}
				vector_r[0].push_back(reg + i);
				PaintByteMemory(reg + i, 'r');	// �`��
			}
			i++;
		} while (i < size);

		//���O�E�B���h�E�Ƀ������Ǎ��ݖ��߂��o��
		std::string buf = " [R]  size:" + size_s + " reg:" + reg_s;
		char info[256];
		sprintf(info, "%s", buf.c_str());
		Addtolist(eip, -1, asmCode);
		Addtolist(eip, -1, info);
	}

	return NULL;
}

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
		mem = mem - 0x400000;
	}
	else if (mode == 'r') {
		hNewPen = CreatePen(PS_SOLID, 1, RGB(data, data, 0));
		hOldPen = (HPEN)SelectObject(hdc, hNewPen);
		hNewBrush = CreateSolidBrush(RGB(data, data, 0));
		hOldBrush = (HBRUSH)SelectObject(hdc, hNewBrush);
		mem = mem - 0x480000;
	}

	//mem = mem - 0x400000;
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

void PrintMemoryBlock(DWORD mem, int size, char mode) {

	BYTE* data;
	int x, y;

	data = (BYTE*)malloc(size);

	ulong length = Readmemory(data, mem, size, 0);	//�T�C�Y���w�肵�Ă����̂�1�o�C�g�����ǂݍ��܂Ȃ�

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

/* �c�[���o�[�T�u�N���X */
LRESULT CALLBACK ToolbarProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
	switch (msg)
	{
	case WM_RBUTTONDOWN:
	case WM_RBUTTONUP:
		if (SendMessage(hWnd, TB_GETHOTITEM, 0, 0) >= 0)
		{
			ReleaseCapture();
		}
		return 0;
	}
	return CallWindowProc(defProc, hWnd, msg, wParam, lParam);
}
