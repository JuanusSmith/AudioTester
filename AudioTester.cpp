/*
AudioTester v1.7
Copyright © 2015 James Chapman

Permission is hereby granted, free of charge, to any person
obtaining a copy of this software and associated documentation
files (the "Software"), to deal in the Software without
restriction, including without limitation the rights to use,
copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the
Software is furnished to do so, subject to the following
conditions:

The above copyright notice and this permission notice shall be
included in all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES
OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT
HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY,
WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR
OTHER DEALINGS IN THE SOFTWARE.
*/

#include "stdafx.h"
#include "AudioTester.h"

#include <tchar.h>
#include <commctrl.h>
#include <shellapi.h>
#include <shlwapi.h>
#include <vector>
#include "decoders.h"
#include "stream.h"
#include <FLAC/format.h>
#include <vorbis/codec.h>
#include <wavpack/wavpack.h>

using namespace std;

#pragma comment(linker,"\"/manifestdependency:type='win32' name='Microsoft.Windows.Common-Controls' version='6.0.0.0' processorArchitecture='*' publicKeyToken='6595b64144ccf1df' language='*'\"")
#pragma comment(lib,"shlwapi.lib")

// Sorts results the way Explorer does: ignoring case, with numbers in numeric order
struct PathLess
{
	bool operator()(const wstring& a, const wstring& b) const { return StrCmpLogicalW(a.c_str(), b.c_str()) < 0; }
};

#define WINDOWTITLE					_TEXT("AudioTester")
#define STR_ABOUT						_TEXT("AudioTester version 1.7.1 (unofficial build)\r\nCopyright © 2015 James Chapman \r\nhttp://www.vuplayer.com\r\nUpdated 2026 with bug fixes and current codec libraries\r\n")
#define STR_ABOUTMENU				_TEXT("About AudioTester")
#define STR_START						_TEXT("Drop files/folders here")
#define STR_FILEERROR				_TEXT("UNABLE TO OPEN DECODER")
#define STR_STOP						_TEXT("Stop")
#define STR_RESULT					_TEXT("%d %s scanned in %.2f seconds")
#define STR_FILE						_TEXT("file")
#define STR_FILES						_TEXT("files")
#define STR_ERROR						_TEXT("failed")
#define STR_PASS						_TEXT("passed")
#define STR_OK							_TEXT("OK")

#define STR_VERSION_OGG			_TEXT("\r\nlibogg: v1.3.6")	// libogg has no version query

#define PADDING							12
#define BUTTON_WIDTH				80
#define BUTTON_HEIGHT				24
#define MAXBARS							16		// more threads than this share one progress bar
#define AGGREGATE						1000	// range of the shared progress bar
#define MAXTEXT							256
#define MSG_TEXT						WM_APP+42
#define MSG_FINISHED				WM_APP+43
#define MSG_PERCENT					WM_APP+44
#define ID_ABOUT						1974
#define TIMER								20
#define SHELL_MUSIC					237

void Startup(HWND hwnd);
void Cleanup();
void ResizeControls(HWND hwnd);
void UpdateFont();
void StartThreads(HWND hwnd);
void StopThreads();
bool AllFinished();
void UpdateProgress();
wchar_t* CopyString(const wchar_t* str);
void OnDrop(HDROP drop);
void CheckFile(const wchar_t* filename);
void ScanFolder(const wchar_t* folder);
void AddFile(const wchar_t* filename);
bool GetNext(wstring& filename);
void SetText(wchar_t* filename, wchar_t* error);
wstring MakeLongPath(const wchar_t* filename);
wstring DisplayPath(const wchar_t* path);
void AddArguments(int argc, LPWSTR* argv);
void ForwardArguments(HWND hwnd, int argc, LPWSTR* argv);
wstring Widen(const char* str);
void SetPending();
void StartTimer();
void StopTimer();
void UpdatePercent();
LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam);
LRESULT CALLBACK EditProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam);
DWORD WINAPI ThreadProc(LPVOID param);

HINSTANCE g_hInst;
HWND g_hWnd;
HWND g_hEdit;
HWND g_hButton;
CDecoders* g_decoders;
int g_cpus;
int g_filecount;
int g_filetotal;
ULONGLONG g_timer;
float g_elapsed;
HANDLE g_hTerminate;
HANDLE g_hPending;
vector<HANDLE> g_hThread;
vector<HANDLE> g_hFinished;
vector<int> g_progress;		// per thread, written by the thread
vector<int> g_range;		// per thread, written by the thread
int g_bars;					// one bar per thread, or one shared bar when there are more than MAXBARS threads
vector<HWND> g_hProgress;
vector<int> g_barrange;
int g_width;
CRITICAL_SECTION g_critical;
wstring g_text;
wchar_t g_buf[MAXTEXT];
list<wstring> g_list;
list<wstring> g_passes;
map<wstring, wstring, PathLess> g_errors;
WNDPROC g_editproc;
HFONT g_font;
int g_dpi=96;			// of the monitor the window is on
int g_sysdpi=96;		// that SystemParametersInfo scales for
typedef BOOL (WINAPI *SETDPICONTEXT)(DPI_AWARENESS_CONTEXT);
typedef UINT (WINAPI *GETDPIFORWINDOW)(HWND);
GETDPIFORWINDOW g_getdpi;

#define SCALE(x)	MulDiv((x), g_dpi, 96)

int WINAPI WinMain(_In_ HINSTANCE hInstance, _In_opt_ HINSTANCE, _In_ LPSTR, _In_ int nCmdShow)
{
	g_hInst = hInstance;
	// Per-monitor DPI where Windows supports it (10 1703+), looked up at run time so older Windows still starts
	HMODULE user32 = GetModuleHandle(_TEXT("user32.dll"));
	SETDPICONTEXT setdpicontext = user32 ? (SETDPICONTEXT)GetProcAddress(user32, "SetProcessDpiAwarenessContext") : 0;
	if (!setdpicontext || !setdpicontext(DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2))
		SetProcessDPIAware();
	else
		g_getdpi = (GETDPIFORWINDOW)GetProcAddress(user32, "GetDpiForWindow");
	HDC dc = GetDC(0);
	g_dpi = g_sysdpi = GetDeviceCaps(dc, LOGPIXELSY);
	ReleaseDC(0, dc);
	int argc = 0;
	LPWSTR* argv = CommandLineToArgvW(GetCommandLine(), &argc);
	WNDCLASSEX wc;
	ZeroMemory(&wc, sizeof(WNDCLASSEX));
	wc.cbSize = sizeof(WNDCLASSEX);
	wc.hCursor = LoadCursor(0, IDC_ARROW);
	wc.hInstance = hInstance;
	wc.lpfnWndProc = WndProc;
	wc.lpszClassName = WINDOWTITLE;
	wc.hbrBackground = (HBRUSH)(COLOR_BTNFACE + 1);
	wc.hIcon = LoadIcon(hInstance, MAKEINTRESOURCE(IDI_AUDIOTESTER));
	wc.hIconSm = LoadIcon(hInstance, MAKEINTRESOURCE(IDI_SMALL));
	HWND h = FindWindow(WINDOWTITLE, NULL);
	if (h)
	{
		// Only allow one instance, so pass any files on to the one that's already running
		ForwardArguments(h, argc, argv);
		if (argv)
			LocalFree(argv);
		return 0;
	}
	if (!RegisterClassEx(&wc))
		return 0;
	InitCommonControls();
	g_hWnd = CreateWindowEx(WS_EX_ACCEPTFILES, WINDOWTITLE, WINDOWTITLE, WS_VISIBLE | WS_OVERLAPPEDWINDOW, CW_USEDEFAULT, 0, CW_USEDEFAULT, 0, 0, 0, hInstance, 0);
	if (g_hWnd)
	{
		HMENU menu = GetSystemMenu(g_hWnd, 0);
		InsertMenu(menu, 0, MF_BYPOSITION | MF_SEPARATOR, 0, 0);
		InsertMenu(menu, 0, MF_BYPOSITION | MF_STRING, ID_ABOUT, STR_ABOUTMENU);
		ShowWindow(g_hWnd, nCmdShow);
		AddArguments(argc, argv);
		if (argv)
			LocalFree(argv);
	}
	else
		return 0;
	MSG msg;
	BOOL bReturn;
	while ((bReturn = GetMessage(&msg, 0, 0, 0)) != 0)
	{
		if (bReturn == -1)
			bReturn = 0;
		else
		{
			TranslateMessage(&msg);
			DispatchMessage(&msg);
		}
	}
	return (int)msg.wParam;
}

void Startup(HWND hwnd)
{
	g_decoders = new CDecoders;
	if (g_getdpi)
		g_dpi = g_getdpi(hwnd);
	g_hEdit = CreateWindow(_TEXT("EDIT"), 0, WS_CHILD | WS_VISIBLE | WS_VSCROLL | WS_BORDER | ES_MULTILINE | ES_READONLY, 0, 0, 0, 0, hwnd, 0, g_hInst, 0);
	g_editproc = (WNDPROC)SetWindowLongPtr(g_hEdit, GWLP_WNDPROC, (LONG_PTR)EditProc);
	SendMessage(g_hEdit, EM_SETLIMITTEXT, 0, 0);
	g_hButton = CreateWindow(_TEXT("BUTTON"), STR_STOP, WS_CHILD | WS_VISIBLE | WS_DISABLED | BS_CENTER | BS_VCENTER, 0, 0, 0, 0, hwnd, 0, g_hInst, 0);
	UpdateFont();
	SendMessage(g_hEdit, WM_SETTEXT, 0, (LPARAM)STR_START );
	StartThreads(hwnd);
	SetTimer(hwnd, TIMER, TIMER, 0);
}

void Cleanup()
{
	KillTimer(g_hWnd, TIMER);
	StopThreads();
	// Free the strings of any results the threads posted that never got shown
	MSG msg;
	while (PeekMessage(&msg, g_hWnd, MSG_TEXT, MSG_TEXT, PM_REMOVE))
	{
		delete[] (wchar_t*)msg.wParam;
		delete[] (wchar_t*)msg.lParam;
	}
	delete g_decoders;
	if (g_font)
		DeleteObject(g_font);
}

void UpdateFont()
{
	// The message font at the window's DPI (SystemParametersInfo gives it at the system DPI)
	HFONT old = g_font;
	g_font = 0;
	NONCLIENTMETRICS ncm;
	ZeroMemory(&ncm, sizeof(ncm));
	ncm.cbSize = sizeof(ncm);
	if (SystemParametersInfo(SPI_GETNONCLIENTMETRICS, sizeof(ncm), &ncm, 0))
	{
		ncm.lfMessageFont.lfHeight = MulDiv(ncm.lfMessageFont.lfHeight, g_dpi, g_sysdpi);
		g_font = CreateFontIndirect(&ncm.lfMessageFont);
	}
	HFONT h = g_font ? g_font : (HFONT)GetStockObject(DEFAULT_GUI_FONT);
	SendMessage(g_hButton, WM_SETFONT, (WPARAM)h, 1);
	SendMessage(g_hEdit, WM_SETFONT, (WPARAM)h, 1);
	if (old)
		DeleteObject(old);
}

void ResizeControls(HWND hwnd)
{
	RECT rect;
	GetClientRect(hwnd, &rect);
	int width = rect.right - rect.left;
	int height = rect.bottom - rect.top;
	int padding = SCALE(PADDING);
	int buttonwidth = SCALE(BUTTON_WIDTH);
	int buttonheight = SCALE(BUTTON_HEIGHT);
	// Sized as well as moved, because the scaled sizes change when the DPI does
	SetWindowPos(g_hButton, 0, width - padding - buttonwidth, height - padding - buttonheight, buttonwidth, buttonheight, SWP_NOZORDER);
	SetWindowPos(g_hEdit, 0, padding, padding, width - padding * 2, height - padding * 3 - buttonheight, SWP_NOZORDER);
	g_width = (width - padding*(2 + g_bars) - buttonwidth) / g_bars;
	for (int i = 0; i<g_bars; i++)
		SetWindowPos(g_hProgress[i], 0, padding*(i + 1) + (i*g_width), height - padding - buttonheight, g_width, buttonheight, SWP_NOZORDER);
}

void StartThreads(HWND hwnd)
{
	// One thread per logical processor, across all processor groups
	g_cpus = (int)GetActiveProcessorCount(ALL_PROCESSOR_GROUPS);
	if (g_cpus<1)
		g_cpus = 1;
	if (g_cpus>1)
		CDecoder::m_usemem = 1;
	g_bars = (g_cpus>MAXBARS) ? 1 : g_cpus;
	g_hTerminate = CreateEvent(0, 1, 0, 0);
	g_hPending = CreateEvent(0, 1, 0, 0);
	InitializeCriticalSection(&g_critical);
	g_hThread.resize(g_cpus);
	g_hFinished.resize(g_cpus);
	g_progress.assign(g_cpus, 0);
	g_range.assign(g_cpus, 1);
	g_hProgress.resize(g_bars);
	g_barrange.assign(g_bars, 0);
	for (int i = 0; i<g_bars; i++)
		g_hProgress[i] = CreateWindow(PROGRESS_CLASS, 0, WS_CHILD | WS_VISIBLE | PBS_SMOOTH, 0, 0, 0, 0, hwnd, 0, g_hInst, 0);
	// Active processors of each group, so threads can be spread across groups
	vector<KAFFINITY> masks;
	DWORD length = 0;
	GetLogicalProcessorInformationEx(RelationGroup, 0, &length);
	vector<BYTE> buf(length);
	PSYSTEM_LOGICAL_PROCESSOR_INFORMATION_EX info = (PSYSTEM_LOGICAL_PROCESSOR_INFORMATION_EX)buf.data();
	if (length && GetLogicalProcessorInformationEx(RelationGroup, info, &length))
	{
		for (WORD g = 0; g<info->Group.ActiveGroupCount; g++)
			masks.push_back(info->Group.GroupInfo[g].ActiveProcessorMask);
	}
	size_t group = 0;
	DWORD ingroup = 0;
	for (int i = 0; i<g_cpus; i++)
	{
		g_hFinished[i] = CreateEvent(0, 1, 1, 0);
		g_hThread[i] = CreateThread(0, 0, ThreadProc, (LPVOID)(INT_PTR)i, CREATE_SUSPENDED, 0);
		if (!g_hThread[i])
		{
			// Run with the threads we have
			CloseHandle(g_hFinished[i]);
			g_cpus = i;
			g_hThread.resize(i);
			g_hFinished.resize(i);
			break;
		}
		if (masks.size()>1)
		{
			// Windows before 11 keeps a process in one processor group unless its threads are moved
			while (ingroup >= GetActiveProcessorCount((WORD)group))
			{
				group = (group + 1) % masks.size();
				ingroup = 0;
			}
			ingroup++;
			GROUP_AFFINITY affinity;
			ZeroMemory(&affinity, sizeof(affinity));
			affinity.Group = (WORD)group;
			affinity.Mask = masks[group];
			SetThreadGroupAffinity(g_hThread[i], &affinity, 0);
		}
		SetThreadPriority(g_hThread[i],THREAD_PRIORITY_BELOW_NORMAL);
		ResumeThread(g_hThread[i]);
	}
}

void StopThreads()
{
	SetEvent(g_hTerminate);
	// WaitForMultipleObjects takes at most MAXIMUM_WAIT_OBJECTS handles, so wait in batches
	for (int i = 0; i<g_cpus; i += MAXIMUM_WAIT_OBJECTS)
		WaitForMultipleObjects((g_cpus - i > MAXIMUM_WAIT_OBJECTS) ? MAXIMUM_WAIT_OBJECTS : g_cpus - i, &g_hThread[i], 1, INFINITE);
	for (int i = 0; i<g_cpus; i++)
	{
		CloseHandle(g_hThread[i]);
		CloseHandle(g_hFinished[i]);
	}
	DeleteCriticalSection(&g_critical);
}

bool AllFinished()
{
	for (int i = 0; i<g_cpus; i += MAXIMUM_WAIT_OBJECTS)
	{
		if (WaitForMultipleObjects((g_cpus - i > MAXIMUM_WAIT_OBJECTS) ? MAXIMUM_WAIT_OBJECTS : g_cpus - i, &g_hFinished[i], 1, 0) != WAIT_OBJECT_0)
			return 0;
	}
	return 1;
}

void UpdateProgress()
{
	if (g_bars == g_cpus)
	{
		for (int i = 0; i<g_bars; i++)
		{
			int range = g_range[i];
			if (g_barrange[i] != range)
			{
				g_barrange[i] = range;
				SendMessage(g_hProgress[i], PBM_SETRANGE32, 0, range);
			}
			if (g_progress[i] != SendMessage(g_hProgress[i], PBM_GETPOS, 0, 0))
				SendMessage(g_hProgress[i], PBM_SETPOS, g_progress[i], 0);
		}
	}
	else
	{
		// One bar for all the threads: the files done, plus the part done of each file in progress
		if (g_barrange[0] != AGGREGATE)
		{
			g_barrange[0] = AGGREGATE;
			SendMessage(g_hProgress[0], PBM_SETRANGE32, 0, AGGREGATE);
		}
		__int64 done = (__int64)g_filecount * AGGREGATE;
		for (int i = 0; i<g_cpus; i++)
		{
			int range = g_range[i];
			int progress = g_progress[i];
			if (range>0 && progress>0)
				done += (progress >= range) ? AGGREGATE : (__int64)progress * AGGREGATE / range;
		}
		int pos = 0;
		if (g_filetotal)
			pos = (done >= (__int64)g_filetotal * AGGREGATE) ? AGGREGATE : (int)(done / g_filetotal);
		if (pos != SendMessage(g_hProgress[0], PBM_GETPOS, 0, 0))
			SendMessage(g_hProgress[0], PBM_SETPOS, pos, 0);
	}
}

wchar_t* CopyString(const wchar_t* str)
{
	const size_t size = wcslen(str) + 1;
	wchar_t* copy = new wchar_t[size];
	wcscpy_s(copy, size, str);
	return copy;
}

void OnDrop(HDROP drop)
{
	int count = DragQueryFile(drop, 0xffffffff, 0, 0);
	wchar_t buf[4096];
	for (int i = 0; i<count; i++)
	{
		if (DragQueryFile(drop, i, buf, 4096))
			CheckFile(buf);
	}
	DragFinish(drop);
	SetPending();
}

void CheckFile(const wchar_t* filename)
{
	wstring path = MakeLongPath(filename);
	DWORD attributes = GetFileAttributes(path.c_str());
	if (attributes == INVALID_FILE_ATTRIBUTES)
		return;
	HCURSOR old = SetCursor(LoadCursor(0, IDC_WAIT));
	if (attributes&FILE_ATTRIBUTE_DIRECTORY)
		ScanFolder(path.c_str());
	else
	{
		if (g_decoders->IsSupportedType(path.c_str()))
			AddFile(path.c_str());
	}
	SetCursor(old);
}

void ScanFolder(const wchar_t* folder)
{
	WIN32_FIND_DATA data;
	wstring path(folder);
	if (*(folder + wcslen(folder) - 1) != '\\')
		path += '\\';
	wstring str = path + _TEXT("*.*");
	HANDLE h = FindFirstFileEx(str.c_str(), FindExInfoBasic, &data, FindExSearchNameMatch, 0, FIND_FIRST_EX_LARGE_FETCH);
	if (h != INVALID_HANDLE_VALUE)
	{
		BOOL b = 1;
		while (b)
		{
			str = path + data.cFileName;
			if (data.dwFileAttributes&FILE_ATTRIBUTE_DIRECTORY)
			{
				// Skip . and .., and junctions/symlinks, which can loop back on themselves
				if (wcscmp(data.cFileName, _TEXT(".")) && wcscmp(data.cFileName, _TEXT("..")) && !(data.dwFileAttributes&FILE_ATTRIBUTE_REPARSE_POINT))
					ScanFolder(str.c_str());
			}
			else
			{
				if (g_decoders->IsSupportedType(str.c_str()))
					AddFile(str.c_str());
			}
			b = FindNextFile(h, &data);
		}
		FindClose(h);
	}
}

void AddFile(const wchar_t* filename)
{
	EnterCriticalSection(&g_critical);
	g_list.push_back(filename);
	LeaveCriticalSection(&g_critical);
	g_filetotal++;
}

bool GetNext(wstring& filename)
{
	bool success = 1;
	EnterCriticalSection(&g_critical);
	if (g_list.empty())
	{
		// Reset while still holding the lock, so a file added after this check also sets the event again
		success = 0;
		ResetEvent(g_hPending);
	}
	else
	{
		filename = g_list.front();
		g_list.pop_front();
	}
	LeaveCriticalSection(&g_critical);
	return success;
}

void SetText(wchar_t* filename, wchar_t* error)
{
	if (filename)
	{
		swprintf_s(g_buf, MAXTEXT, _TEXT("[%d/%d]  "), ++g_filecount, g_filetotal);
		g_text = g_buf;
		g_text += DisplayPath(filename);
		if (error)
			g_errors[DisplayPath(filename)] = error;
		else
			g_passes.push_back(DisplayPath(filename));
	}
	else
	{
		swprintf_s(g_buf, MAXTEXT, STR_RESULT, g_filecount, (g_filecount == 1) ? (STR_FILE) : (STR_FILES), g_filecount ? g_elapsed : 0);
		g_text = g_buf;
		swprintf_s(g_buf, MAXTEXT, _TEXT("\r\n---\r\n%zu %s %s"), g_errors.size(), (g_errors.size() == 1) ? (STR_FILE) : (STR_FILES), STR_ERROR);
		g_text += g_buf;
		for (map<wstring, wstring, PathLess>::const_iterator i = g_errors.begin(); i != g_errors.end(); i++)
		{
			g_text += _TEXT("\r\n");
			g_text += (*i).first;
			if (!(*i).second.empty())
			{
				g_text += _TEXT("\t(");
				g_text += (*i).second;
				g_text += ')';
			}
		}
		swprintf_s(g_buf, MAXTEXT, _TEXT("\r\n---\r\n%zu %s %s"), g_passes.size(), (g_passes.size() == 1) ? (STR_FILE) : (STR_FILES), STR_PASS);
		g_text += g_buf;
		g_passes.sort(PathLess());
		for (list<wstring>::const_iterator i = g_passes.begin(); i != g_passes.end(); i++)
		{
			g_text += _TEXT("\r\n");
			g_text += *i;
		}
	}
	g_text += _TEXT("\r\n");
	SetWindowText(g_hEdit, (g_text.c_str()));
	if (filename)
		delete[] filename;
	if (error)
		delete[] error;
}

void SetPending()
{
	if (AllFinished())
		StartTimer();
	SetEvent(g_hPending);
	EnableWindow(g_hButton, 1);
	UpdatePercent();
}

void StartTimer()
{
	g_timer = GetTickCount64();
}

void StopTimer()
{
	g_elapsed = (float)(GetTickCount64() - g_timer) / 1000;
}

void UpdatePercent()
{
	wchar_t buf[32];
	swprintf_s(buf, 32, _TEXT("%s - %d%%"), WINDOWTITLE, ((g_filetotal) ? (g_filecount * 100 / g_filetotal) : 0));
	SetWindowText(g_hWnd, buf);
}

wstring MakeLongPath(const wchar_t* filename)
{
	// Full path with the \\?\ prefix, so paths longer than MAX_PATH work
	if (!wcsncmp(filename, _TEXT("\\\\?\\"), 4))
		return filename;
	DWORD size = GetFullPathName(filename, 0, 0, 0);
	if (!size)
		return filename;
	wstring full(size, 0);
	size = GetFullPathName(filename, size, &full[0], 0);
	full.resize(size);
	if (!wcsncmp(full.c_str(), _TEXT("\\\\"), 2))
		return _TEXT("\\\\?\\UNC\\") + full.substr(2);	// \\server\share becomes \\?\UNC\server\share
	return _TEXT("\\\\?\\") + full;
}

wstring DisplayPath(const wchar_t* path)
{
	// Strip the prefix added by MakeLongPath (UNC paths get their leading \\ back)
	if (!wcsncmp(path, _TEXT("\\\\?\\UNC\\"), 8))
		return _TEXT("\\\\") + wstring(path + 8);
	if (!wcsncmp(path, _TEXT("\\\\?\\"), 4))
		return path + 4;
	return path;
}

void AddArguments(int argc, LPWSTR* argv)
{
	if (argv && argc > 1)
	{
		for (int i = 1; i < argc; i++)
			CheckFile(argv[i]);
		SetPending();
	}
}

void ForwardArguments(HWND hwnd, int argc, LPWSTR* argv)
{
	// Send full paths, because the running instance has a different current folder
	for (int i = 1; argv && i < argc; i++)
	{
		wstring path = MakeLongPath(argv[i]);
		COPYDATASTRUCT cds;
		cds.dwData = ID_ABOUT;
		cds.cbData = (DWORD)((path.size() + 1) * sizeof(wchar_t));
		cds.lpData = (PVOID)path.c_str();
		SendMessage(hwnd, WM_COPYDATA, 0, (LPARAM)&cds);
	}
}

wstring Widen(const char* str)
{
	wstring result;
	while (str && *str)
		result += (wchar_t)(unsigned char)*str++;
	return result;
}

LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
	switch (msg)
	{
	case WM_CREATE:
		Startup(hwnd);
		return 0;
	case WM_DESTROY:
		Cleanup();
		PostQuitMessage(0);
		return 0;
	case WM_CLOSE:
		DestroyWindow(hwnd);
		return 0;
	case WM_SIZE:
		if (wParam != SIZE_MINIMIZED)
			ResizeControls(hwnd);
		return 0;
	case WM_DROPFILES:
		OnDrop((HDROP)wParam);
		return 0;
	case WM_COMMAND:
		if ((HIWORD(wParam) == BN_CLICKED) && ((HWND)lParam == g_hButton))
		{
			EnterCriticalSection(&g_critical);
			g_list.clear();
			LeaveCriticalSection(&g_critical);
			EnableWindow(g_hButton, 0);
			return 0;
		}
		break;
	case WM_SYSCOMMAND:
		if (wParam == ID_ABOUT)
		{
			wstring message(STR_ABOUT);
			message+=STR_VERSION_OGG;
			message+=_TEXT("\r\nlibvorbis: ") + Widen(vorbis_version_string());
			message+=_TEXT("\r\nlibFLAC: v") + Widen(FLAC__VERSION_STRING);
			message+=_TEXT("\r\nlibWavPack: v") + Widen(WavpackGetLibraryVersionString());
			MessageBox(hwnd, message.c_str(), WINDOWTITLE, MB_ICONINFORMATION | MB_OK);
			return 0;
		}
		break;
	case WM_COPYDATA:
	{
		PCOPYDATASTRUCT cds = (PCOPYDATASTRUCT)lParam;
		if ((cds->dwData == ID_ABOUT) && cds->lpData && (cds->cbData >= sizeof(wchar_t)))
		{
			// Don't trust the sender to have null terminated the string
			wstring path((const wchar_t*)cds->lpData, cds->cbData / sizeof(wchar_t));
			path.resize(wcslen(path.c_str()));
			CheckFile(path.c_str());
			SetPending();
		}
		return 1;
	}
	case WM_TIMER:
		UpdateProgress();
		return 0;
	case WM_DPICHANGED:
	{
		// Moved to a monitor with a different scale: resize to the rectangle Windows suggests, at the new DPI
		g_dpi = HIWORD(wParam);
		UpdateFont();
		const RECT* rect = (const RECT*)lParam;
		SetWindowPos(hwnd, 0, rect->left, rect->top, rect->right - rect->left, rect->bottom - rect->top, SWP_NOZORDER | SWP_NOACTIVATE);
		ResizeControls(hwnd);
		return 0;
	}
	case MSG_TEXT:
		SetText((wchar_t*)wParam, (wchar_t*)lParam);
		return 0;
	case MSG_PERCENT:
		UpdatePercent();
		return 0;
	case MSG_FINISHED:
		SetEvent(g_hFinished[wParam]);
		if (AllFinished())
		{
			StopTimer();
			SetText(0, 0);
			g_filecount = g_filetotal = 0;
			g_text = _TEXT("");
			g_passes.clear();
			g_errors.clear();
			EnableWindow(g_hButton, 0);
			SetWindowText(g_hWnd, WINDOWTITLE);
		}
		return 0;
	}
	return DefWindowProc(hwnd, msg, wParam, lParam);
}

LRESULT CALLBACK EditProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
	switch (msg)
	{
	case WM_KEYDOWN:
	{
		if( GetKeyState(VK_CONTROL) & 0x8000 ) {
			if (wParam == 'A') {
				SendMessage(hwnd, EM_SETSEL, 0, -1);
			}
			if ((wParam == 'C') || (wParam == 'X')) {
				SendMessage(hwnd, WM_COPY, 0, 0);
			}
		}
	}
	default:
		break;
	}
	return CallWindowProc(g_editproc, hwnd, msg, wParam, lParam);
}

DWORD WINAPI ThreadProc(LPVOID param)
{
	int n = (int)(INT_PTR)param;
	wstring filename;
	CDecoder* decoder;
	__int64 total;
	long count;
	wchar_t* file;
	wchar_t* error;
	__int64 samplecount;
	g_progress[n] = 0;
	HANDLE h[2];
	h[0] = g_hTerminate;
	h[1] = g_hPending;
	DWORD a = WaitForMultipleObjects(2, h, 0, INFINITE);
	while (a != WAIT_OBJECT_0)
	{
		ResetEvent(g_hFinished[n]);
		while ((WaitForSingleObject(g_hTerminate, 0) != WAIT_OBJECT_0) && (GetNext(filename)))
		{
			file = CopyString(filename.c_str());
			// No lock needed: the decoder list is read-only, and CStream serialises whole-file loads itself
			decoder = g_decoders->OpenDecoder(filename.c_str());
			if (decoder)
			{
				samplecount = decoder->GetSampleCount();
				if (!samplecount)
					samplecount = 0xffffff;
				g_progress[n] = 0;
				g_range[n] = (int)(samplecount >> 8);	// the UI thread's timer applies it
				total = 0;
				count = decoder->Read();
				while ((WaitForSingleObject(g_hTerminate, 0) != WAIT_OBJECT_0) && (count>0))
				{
					total += count;
					g_progress[n] = (int)(total >> 8);
					count = decoder->Read();
				}
				error = (count<0) ? CopyString(decoder->GetLastError()) : 0;
				delete decoder;
			}
			else
				error = CopyString(STR_FILEERROR);
			// The UI thread frees the strings, or Cleanup does if the window closes first
			if (!PostMessage(g_hWnd, MSG_TEXT, (WPARAM)file, (LPARAM)error))
			{
				delete[] file;
				delete[] error;
			}
			PostMessage(g_hWnd, MSG_PERCENT, 0, 0);
		}
		g_progress[n] = 0;
		PostMessage(g_hWnd, MSG_FINISHED, n, 0);
		a = WaitForMultipleObjects(2, h, 0, INFINITE);
	}
	return 0;
}
