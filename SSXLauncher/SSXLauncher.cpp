#include <Windows.h>
#include <string>
#include <Windowsx.h> //Button_SetCheck macro
#include <CommCtrl.h> //https://docs.microsoft.com/en-us/windows/desktop/api/commctrl/
#include <iostream>
#include "resource.h"
#include "SSXLoader.h"

//CommCtrl includes sliders
static LRESULT CALLBACK WndProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam);
void initialize(HINSTANCE hInstance);

namespace
{

	LPCSTR ResolutionOptions[4] = { "[4:3] 640x480 (Original)", "[4:3] 1024x768", "[16:9] 1280x720", "[16:10] 1280x800" };
	unsigned int SpeedValues[7] = { 1, 4, 8, 16, 24, 32, 64 };

	HWND PatchButton; 
	HWND StartButton; 
	HWND HelpButton;
	HWND UpdateButton;
	HWND settingsHwnd;
	HWND verifyCheckbox;
	HWND fsRadioButton;
	HWND wsRadioButton;

	HWND speedTextbox;
	HWND resolutionCombobox;
	int resolutionValue = 0;
	bool fullscreenValue = true;

	HWND SensitivityBar;
	HWND resolutionTextbox;
	HWND AimbotErrorText;

	WNDCLASSEX SettingsClass;

	bool verifyInstallationValue = false;
	bool CanEnd = false;
	bool end_process = false;

	int speedMS = 24;
	HBITMAP hBitmap = NULL;
}

int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nCmdShow)
{
	HeapSetInformation(NULL, HeapEnableTerminationOnCorruption, NULL, 0);
	hBitmap = LoadBitmap(hInstance, MAKEINTRESOURCE(IDB_BITMAP2));
	initialize(hInstance);

	SSXLoader::LoadFiles();
	if (SSXLoader::GetPatchedSSXVersion() > 0)
	{
		OutputDebugString("Streets of SimCity has been previously patched, ready to play \n");
		Button_Enable(StartButton, TRUE);
	}
	
	MSG msg;	
	while (!end_process && GetMessage(&msg, NULL, 0, 0))
	{
		TranslateMessage(&msg);
		DispatchMessage(&msg);
	}
	return 0;
}

void destroy()
{
	if (settingsHwnd != NULL)
		DestroyWindow(settingsHwnd);
}

void initialize(HINSTANCE hInstance)
{
	SettingsClass.cbClsExtra = NULL;
	SettingsClass.cbWndExtra = NULL;
	SettingsClass.lpszMenuName = NULL;
	SettingsClass.hCursor = LoadCursor(NULL, IDC_ARROW);
	SettingsClass.hbrBackground = CreateSolidBrush(COLORREF(0xf0f0f0));
	SettingsClass.cbSize = sizeof(WNDCLASSEX);
	SettingsClass.hIcon = LoadIcon(hInstance, MAKEINTRESOURCE(IDI_ICON1));
	SettingsClass.hIconSm = LoadIcon(hInstance, MAKEINTRESOURCE(IDI_ICON1));
	SettingsClass.hInstance = hInstance;
	SettingsClass.lpfnWndProc = WndProc;
	SettingsClass.lpszClassName = "SASETTINGS";
	SettingsClass.style = CS_HREDRAW | CS_VREDRAW;
	RegisterClassEx(&SettingsClass);


	settingsHwnd = CreateWindowEx(
		WS_EX_STATICEDGE,
		SettingsClass.lpszClassName, "SimStreetsX",
		WS_VISIBLE | WS_CLIPCHILDREN | WS_BORDER | WS_CAPTION | WS_SYSMENU | WS_MINIMIZEBOX,
		0, 0, 400, 300, NULL, NULL, NULL, NULL);

	verifyCheckbox = CreateWindow(
		"Button", "Verify Install", WS_VISIBLE | WS_CHILDWINDOW | BS_AUTOCHECKBOX,
		225, 60, 100, 25, settingsHwnd, NULL,
		NULL, NULL);
	Button_SetCheck(verifyCheckbox, BST_CHECKED);

	PatchButton = CreateWindow(
		"Button", "Patch Game", WS_VISIBLE | WS_CHILDWINDOW | BS_PUSHBUTTON,
		10, 60, 150, 25, settingsHwnd, NULL,
		NULL, NULL);

	resolutionCombobox = CreateWindow(
		"COMBOBOX", "", WS_VISIBLE | WS_CHILDWINDOW | CBS_DROPDOWNLIST | CBS_HASSTRINGS | WS_BORDER,
		10, 95, 200, 100, settingsHwnd, NULL, NULL, NULL);

	SensitivityBar = CreateWindow(
		TRACKBAR_CLASS, "TEST", WS_VISIBLE | WS_CHILD | TBS_HORZ | TBS_AUTOTICKS,
		220, 95, 150, 20, settingsHwnd, NULL, NULL, NULL);

	speedTextbox = CreateWindow("EDIT", "Game Speed: 24ms",
		WS_CHILD | WS_VISIBLE | ES_LEFT | ES_READONLY | ES_MULTILINE,
		230, 125, 150, 20, settingsHwnd, NULL, NULL, NULL);

	fsRadioButton = CreateWindow(
			"Button", "Fullscreen", WS_VISIBLE | WS_CHILDWINDOW | BS_AUTORADIOBUTTON,
			10, 150, 100, 25, settingsHwnd, NULL,
			NULL, NULL);
	Button_SetCheck(fsRadioButton, BST_CHECKED);

	wsRadioButton = CreateWindow(
		"Button", "Windowed", WS_VISIBLE | WS_CHILDWINDOW | BS_AUTORADIOBUTTON,
		10, 175, 150, 25, settingsHwnd, NULL,
		NULL, NULL);

	UpdateButton = CreateWindow(
		"Button", "Check for updates", WS_VISIBLE | WS_CHILDWINDOW | BS_PUSHBUTTON,
		200, 175, 150, 25, settingsHwnd, NULL,
		NULL, NULL);	

	StartButton = CreateWindow(
		"Button", "Drive!", WS_VISIBLE | WS_CHILDWINDOW | BS_PUSHBUTTON,
		110, 225, 150, 25, settingsHwnd, NULL,
		NULL, NULL);

	HelpButton = CreateWindow(
		"Button", "?", WS_VISIBLE | WS_CHILDWINDOW | BS_PUSHBUTTON,
		345, 225, 30, 25, settingsHwnd, NULL,
		NULL, NULL);

	SendMessage(SensitivityBar, TBM_SETRANGEMIN, WPARAM(FALSE), LPARAM(1));
	SendMessage(SensitivityBar, TBM_SETRANGEMAX, WPARAM(FALSE), LPARAM(7));
	SendMessage(SensitivityBar, TBM_SETPOS, WPARAM(FALSE), LPARAM(5));
	SendMessage(SensitivityBar, TBM_SETTICFREQ, WPARAM(1), LPARAM(0));

	UpdateWindow(PatchButton);
	UpdateWindow(verifyCheckbox);

	UpdateWindow(speedTextbox);
	//UpdateWindow(resolutionTextbox);

	UpdateWindow(UpdateButton);
	UpdateWindow(StartButton);
	UpdateWindow(HelpButton);
	UpdateWindow(SensitivityBar);


	for (int i = 0; i < sizeof(ResolutionOptions) / sizeof(LPCSTR); i++)
		SendMessage(resolutionCombobox, (UINT)CB_ADDSTRING, (WPARAM)0, (LPARAM)ResolutionOptions[i]);

	SendMessage(resolutionCombobox, CB_SETCURSEL, (WPARAM)0, (LPARAM)0);

	UpdateWindow(resolutionCombobox);

	Button_Enable(StartButton, FALSE);

	UpdateWindow(fsRadioButton);
	UpdateWindow(wsRadioButton);

}

LRESULT CALLBACK WndProc(HWND hWnd, UINT Msg, WPARAM wParam, LPARAM lParam)
{
	switch (Msg)
	{
	case WM_PAINT:
		PAINTSTRUCT     ps;
		HDC             hdc;
		BITMAP          bitmap;
		HDC             hdcMem;
		HGDIOBJ         oldBitmap;

		hdc = BeginPaint(hWnd, &ps);

		hdcMem = CreateCompatibleDC(hdc);
		oldBitmap = SelectObject(hdcMem, hBitmap);

		GetObject(hBitmap, sizeof(bitmap), &bitmap);
		BitBlt(hdc, 90, 10, bitmap.bmWidth, bitmap.bmHeight, hdcMem, 0, 0, SRCCOPY);

		SelectObject(hdcMem, oldBitmap);
		DeleteDC(hdcMem);

		EndPaint(hWnd, &ps);
		return 0;
	case WM_ACTIVATE:
		UpdateWindow(UpdateButton);
		UpdateWindow(wsRadioButton);
		UpdateWindow(PatchButton);
		UpdateWindow(verifyCheckbox);
		UpdateWindow(speedTextbox);
		UpdateWindow(HelpButton);
		UpdateWindow(resolutionCombobox);
		UpdateWindow(StartButton);
		UpdateWindow(fsRadioButton);
		return 0;

	case WM_HSCROLL:
		if (true)
		{
			int sliderValue = SendMessage((HWND)lParam, (UINT)TBM_GETPOS, (WPARAM)0, (LPARAM)0);
			speedMS = 16;

			if (sliderValue && sliderValue <= (sizeof(SpeedValues) / 4))			
				speedMS = SpeedValues[sliderValue - 1];		

			std::string speedText("Game Speed: ");
			speedText.append(std::to_string(speedMS)).append("ms");
			SetWindowText(speedTextbox, speedText.c_str());
			UpdateWindow(speedTextbox);
		}
		return 0;
	case WM_COMMAND:

		if ((HWND)lParam == resolutionCombobox)
		{
			if (HIWORD(wParam) == CBN_SELCHANGE)
			{
				int ItemIndex = SendMessage((HWND)lParam, (UINT)CB_GETCURSEL, (WPARAM)0, (LPARAM)0);
				char ListItem[256];
				SendMessage((HWND)lParam, (UINT)CB_GETLBTEXT, (WPARAM)ItemIndex, (LPARAM)ListItem);
				for (int i = 0; i < sizeof(ResolutionOptions) / sizeof(LPCSTR); i++)
				{
					if (_stricmp(std::string(ListItem).c_str(), ResolutionOptions[i]) == 0)
					{
						resolutionValue = i;
						break;
					}
				}
			}
		}
		else if ((HWND)lParam == resolutionTextbox)
		{
			::SetFocus(NULL);
		}
		else if ((HWND)lParam == speedTextbox)
		{
			::SetFocus(NULL);
		}
		else
		{
			switch (HIWORD(wParam))
			{
			case BN_CLICKED:
				if ((HWND)lParam == PatchButton)
				{
					Button_Enable(PatchButton, FALSE);
					::SetFocus(NULL);
					char szFile[256];
					OPENFILENAME ofn;
					ZeroMemory(&ofn, sizeof(ofn));
					ofn.lStructSize = sizeof(ofn);
					ofn.hwndOwner = NULL;
					ofn.lpstrFile = szFile;
					ofn.lpstrFile[0] = '\0';
					ofn.nMaxFile = sizeof(szFile);
					ofn.lpstrFilter = "Streets.exe\0Streets.exe;\0";
					ofn.nFilterIndex = 1;
					ofn.lpstrFileTitle = NULL;
					ofn.nMaxFileTitle = 0;
					ofn.lpstrInitialDir = NULL;
					ofn.Flags = OFN_PATHMUSTEXIST | OFN_FILEMUSTEXIST;
					GetOpenFileName(&ofn);
					bool patch_result = SSXLoader::CreatePatchedGame(ofn.lpstrFile);
					if (patch_result)
					{
						bool verify_install = SendMessage(verifyCheckbox, BM_GETCHECK, 0, 0) == BST_CHECKED;
						if (!verify_install || SSXLoader::VerifyInstallation())
						{
							Button_Enable(StartButton, TRUE);
						}
					}
					Button_Enable(PatchButton, TRUE);				
				}
				else if ((HWND)lParam == StartButton)
				{			
					int dword_5017D0;
					switch (resolutionValue)
					{
					case 0: //640x480
						dword_5017D0 = 1;
						break;
					case 1: //1024x768
						dword_5017D0 = 3;
						break;
					case 2: //1280x720
						dword_5017D0 = 2;
						break;
					case 3: //1280x800
						dword_5017D0 = 0;
						break;
					default:	
						printf("Error: Invalid resolution combobox selection \n");
						return 0;
					}

					int dword_5017A8 = speedMS;
					bool fullscreen = SendMessage(fsRadioButton, BM_GETCHECK, 0, 0) == BST_CHECKED;			
					
					if (SSXLoader::StartSCX(dword_5017A8, dword_5017D0, fullscreen))
					{
						end_process = true;
					}
					::SetFocus(NULL);
					UpdateWindow(StartButton);					
				}
				else if ((HWND)lParam == UpdateButton)
				{
					SSXLoader::CheckForUpdates();
					::SetFocus(NULL);
					UpdateWindow(UpdateButton);
				}
				else if ((HWND)lParam == HelpButton)
				{				
					ShellExecute(0, 0, "http://www.streetsofsimcity.com/help.html", 0, 0, SW_SHOW);
					::SetFocus(NULL);
					UpdateWindow(HelpButton);
				}
				else if ((HWND)lParam == verifyCheckbox)
				{
					::SetFocus(NULL);
					LRESULT chkState = SendMessage((HWND)lParam, BM_GETCHECK, 0, 0);
					verifyInstallationValue = chkState == BST_CHECKED;
				}

			}
		}
		return 0;
	case WM_DESTROY:
		end_process = true;
		return 0;
	default:
		return DefWindowProc(hWnd, Msg, wParam, lParam);
	}
}