#include "w3_Context.h"
#include "w3_Core.h"
#include "w3_DLL.h"

#include <windows.h>
#include "Shlwapi.h"
#include <vector>

extern LRESULT CALLBACK WndProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam);
BOOL CALLBACK MonitorProc(HMONITOR hMonitor, HDC hdcMonitor, LPRECT lprcMonitor, LPARAM dwData);

std::vector<MonitorInfo> g_SecondaryMonitors;
MonitorInfo g_PrimaryMonitor;
bool g_IsPrimarySet = false;

w3Context::w3Context() :
	m_IsInitialized(false)
{}

bool w3Context::Initialize(HINSTANCE hInstance)
{
	WNDCLASSEX wc;
	wc.cbSize = sizeof(WNDCLASSEX);
	wc.style = 0;
	wc.lpfnWndProc = WndProc;
	wc.cbClsExtra = 0;
	wc.cbWndExtra = 0;
	wc.hInstance = hInstance;
	wc.hIcon = NULL;
	wc.hIconSm = NULL;
	wc.hCursor = NULL;
	wc.hbrBackground = NULL;
	wc.lpszMenuName = NULL;
	wc.lpszClassName = T_WNDCLASS_NAME;

	if(!RegisterClassEx(&wc))
	{
		MessageBoxEx(NULL, _T("w3wm failed to register the window class"), _T("Error"), MB_OK | MB_ICONERROR, 0);
		return false;
	}

	m_Hwnd = CreateWindowEx(0, T_WNDCLASS_NAME, T_APP_NAME, 0, 0, 0, 0, 0, HWND_MESSAGE, NULL, hInstance, NULL);
	if(!m_Hwnd)
	{
		MessageBoxEx(NULL, _T("w3wm failed to create the window"), _T("Error"), MB_OK | MB_ICONERROR, 0);
		return false;
	}

	InstallHooks(m_Hwnd);

	return Start();
}

void w3Context::Shutdown()
{
	RemoveHooks();
}

bool w3Context::Restart()
{
	// Clear state that will be replaced after calling Start()
	m_Monitors.Clear();
	g_IsPrimarySet = false;

	return Start();
}

bool w3Context::Start()
{
	if(!EnumDisplayMonitors(NULL, NULL, MonitorProc, 0))
	{
		MessageBoxEx(NULL, _T("Failed to enumerate monitors!"), _T("Error"), MB_OK | MB_ICONERROR, 0);
		return false;
	}

	// Add monitors to m_Monitors if we found the primary monitor
	if(!g_IsPrimarySet)
	{
		return false;
	}

	m_Monitors.Initialize(g_PrimaryMonitor);
	for(MonitorInfo &info : g_SecondaryMonitors)
	{
		m_Monitors.Insert(info);
	}
	g_SecondaryMonitors.clear();

	UpdateHotkeys();

	return true;
}

void w3Context::UpdateHotkeys()
{
	// Initialize hotkeys with the defaults
	HotkeyDef defs[] = { HOTKEYS(F_HOTKEY_ARR) };

	// Modify defaults with the ini
	LPCTSTR names[] = { HOTKEYS(F_HOTKEY_NAME_ARR) };
	int max = ARR_SIZE(names);

	TCHAR iniDir[512];
	GetCurrentDirectory(512, iniDir);
	_tcscat_s(iniDir, 100, _T("\\config.ini"));

	if(!PathFileExists(iniDir))
	{
		max = 0;
		MessageBoxEx(NULL, iniDir, _T("Ini not found"), MB_OK, 0);
	}

	for(int i = 0; i < max; ++i)
	{
		TCHAR inBuf[80];
		DWORD res = GetPrivateProfileString(_T("Keybindings"), names[i], _T(""), inBuf, 80, iniDir);

		if(res != 0)
		{
			ParseHotkey(&defs[i], inBuf);
		}
	}

	// Inform DLL of hotkey mappings
	SetHotkeys(defs, ARR_SIZE(defs));
}

BOOL CALLBACK MonitorProc(HMONITOR hMonitor, HDC hdcMonitor, LPRECT lprcMonitor, LPARAM dwData)
{
	if(lprcMonitor->left == 0 && lprcMonitor->top == 0)
	{
		g_PrimaryMonitor = {hdcMonitor, *lprcMonitor};
		g_IsPrimarySet = true;
	}
	else
	{
		g_SecondaryMonitors.push_back({hdcMonitor, *lprcMonitor});
	}

	return TRUE;
}