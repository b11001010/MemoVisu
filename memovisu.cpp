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

// �֐��v���g�^�C�v
BOOL InitInstance(HINSTANCE);
ATOM MyRegisterClass(HINSTANCE);
LRESULT CALLBACK WndProc(HWND, UINT, WPARAM, LPARAM);
DWORD checkAsmCode(char*, t_thread*, DWORD);

HINSTANCE	hinst;			// DLL instance
HWND		hwmain;			// Handle of main OllyDbg window
HWND		hwplugin;		// Plugin window

std::vector<DWORD> v_w;
std::vector<DWORD> v_r;
std::vector<BYTE> binary;

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
	wcex.lpszMenuName = NULL;
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

	switch (message)
	{
	case WM_CREATE:
		wx = GetSystemMetrics(SM_CXSCREEN);
		wy = GetSystemMetrics(SM_CYSCREEN);
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
		hdc = BeginPaint(hWnd, &ps);	//�`��J�n

		if (v_w.empty() && v_r.empty()) {
			EndPaint(hWnd, &ps);	//�`��I��
			break;
		}

		BYTE data;	//�A�h���X�̎w���l
		int x, y;

		//�����ݗ̈�`��
		for (DWORD d : v_w) {
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

		//�Ǎ��ݗ̈�`��
		for (DWORD d : v_r) {
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

		//char text[64];
		//sprintf(text, "x=%02x y=%03x data=%02x", x, y, data);
		//TextOut(hdc, 10, 10, text, lstrlen(text));

		EndPaint(hWnd, &ps);	//�`��I��
		break; }
	default:
		return DefWindowProc(hWnd, message, wParam, lParam);
	}
	return 0;
}

DWORD checkAsmCode(char* asmCode, t_thread* pthread, DWORD eip){

	//pthread->reg.r[7];	//EDI�ւ̃A�N�Z�X��
	//	DWORD eip = pthread->reg.ip;	�����ꂪ���̂�������

	std::string code(asmCode);
	std::smatch match;
	//char *pattern = "MOV (BYTE|WORD|DWORD) PTR DS:\\[(EAX|ECX|EDX|EBX|ESP|EBP|ESI|EDI)\\].*";	//TODO: [EAX+���l]�̌`���ɑΉ�������
	char *pattern_w = "MOV (BYTE|WORD|DWORD) PTR DS:\\[(..|...)\\].*";
	std::regex regex_w(pattern_w);
	char *pattern_r = "MOV ...?,(BYTE|WORD|DWORD) PTR DS:\\[(..|...)\\]";
	std::regex regex_r(pattern_r);

	std::string size_s;
	int size = 0;
	std::string reg;

	if (std::regex_match(code, match, regex_w)){
		size_s = match[1].str();
		reg = match[2].str();

		if (size_s == "BYTE") {
			size = 1;
		}
		else if (size_s == "WORD") {
			size = 2;
		}
		else if (size_s == "DWORD") {
			size = 4;
		}

		//Vector�֒ǉ�
		int i = 0;
		do{
			if (reg == "EDI") {
				v_w.push_back(pthread->reg.r[7] + i);
			}
			i++;
		} while (i < size);

		//���O�E�B���h�E�Ƀ����������ݖ��߂��o��
		std::string buf = " [W]  size:" + size_s + " reg:" + reg;
		char info[256];
		sprintf(info, "%s", buf.c_str());
		Addtolist(eip, -1, asmCode);
		Addtolist(eip, -1, info);

		//�X�V�̈�ʒm
		DWORD d = pthread->reg.r[7];
		d = d - 0x400000;
		int x = d % 0x100;
		int y = d / 0x100;
		x = x*margin_x + (x - 1)*width + 20;
		y = y*margin_y + (y - 1)*height;
		const RECT rc = {x, y, (x+width)*size, (y+height)*size};
		InvalidateRect(hwplugin, &rc, FALSE);
	}

	else if (std::regex_match(code, match, regex_r)) {
		size_s = match[1].str();
		reg = match[2].str();

		if (size_s == "BYTE") {
			size = 1;
		}
		else if (size_s == "WORD") {
			size = 2;
		}
		else if (size_s == "DWORD") {
			size = 4;
		}

		//Vector�֒ǉ�
		int i = 0;
		do {
			if (reg == "ESI") {
				v_r.push_back(pthread->reg.r[6] + i);
			}
			i++;
		} while (i < size);

		//���O�E�B���h�E�Ƀ������Ǎ��ݖ��߂��o��
		std::string buf = " [R]  size:" + size_s + " reg:" + reg;
		char info[256];
		sprintf(info, "%s", buf.c_str());
		Addtolist(eip, -1, asmCode);
		Addtolist(eip, -1, info);

		//�X�V�̈�ʒm
		DWORD d = pthread->reg.r[6];
		d = d - 0x480000;
		int x = d % 0x100;
		int y = d / 0x100;
		x = x*margin_x + (x - 1)*width + 20;
		y = y*margin_y + (y - 1)*height;
		const RECT rc = { x, y, (x + width)*size, (y + height)*size };
		InvalidateRect(hwplugin, &rc, FALSE);
	}

	return NULL;
}

