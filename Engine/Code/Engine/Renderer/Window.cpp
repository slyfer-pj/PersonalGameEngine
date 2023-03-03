#include "Engine/Renderer/Window.hpp"
#include "Engine/Input/InputSystem.hpp"
#include "Engine/Core/ErrorWarningAssert.hpp"
#include "Engine/Core/DevConsole.hpp"
#include "Engine/Core/EventSystem.hpp"
#include "ThirdParty/ImGUI/imgui_impl_win32.h"
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <commdlg.h>

Window* Window::s_mainWindow = nullptr;

extern IMGUI_IMPL_API LRESULT ImGui_ImplWin32_WndProcHandler(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);
LRESULT CALLBACK WindowsMessageHandlingProcedure(HWND windowHandle, UINT wmMessageCode, WPARAM wParam, LPARAM lParam)
{
	if (ImGui_ImplWin32_WndProcHandler(windowHandle, wmMessageCode, wParam, lParam))
		return true;

	Window* windowContext = Window::GetWindowContext();
	GUARANTEE_OR_DIE(windowContext != nullptr, "Window context was null");
	InputSystem* input = windowContext->GetConfig().m_inputSystem;
	GUARANTEE_OR_DIE(input != nullptr, "No input system");

	switch (wmMessageCode)
	{
		// App close requested via "X" button, or right-click "Close Window" on task bar, or "Close" from system menu, or Alt-F4
	case WM_CLOSE:
	{
		FireEvent("quit");
		break;
	}

	// Raw physical keyboard "key-was-just-depressed" event (case-insensitive, not translated)
	case WM_KEYDOWN:
	{
		unsigned char asKey = (unsigned char)wParam;
		input->HandleKeyPressed(asKey);
		break;
	}

	// Raw physical keyboard "key-was-just-released" event (case-insensitive, not translated)
	case WM_KEYUP:
	{
		unsigned char asKey = (unsigned char)wParam;
		input->HandleKeyReleased(asKey);
		break;
	}

	case WM_LBUTTONDOWN:
	{
		unsigned char keyCode = KEYCODE_LEFT_MOUSE;
		input->HandleKeyPressed(keyCode);
		break;
	}

	case WM_LBUTTONUP:
	{
		unsigned char keyCode = KEYCODE_LEFT_MOUSE;
		input->HandleKeyReleased(keyCode);
		break;
	}

	case WM_RBUTTONDOWN:
	{
		unsigned char keyCode = KEYCODE_RIGHT_MOUSE;
		input->HandleKeyPressed(keyCode);
		break;
	}

	case WM_RBUTTONUP:
	{
		unsigned char keyCode = KEYCODE_RIGHT_MOUSE;
		input->HandleKeyReleased(keyCode);
		break;
	}

	case WM_MOUSEWHEEL:
	{
		short delta = GET_WHEEL_DELTA_WPARAM(wParam);
		input->SetMouseWheelDelta(delta);
		break;
	}

	case WM_CHAR:
	{
		int charCode = (int)wParam;
		input->HandleCharInput(charCode);
		break;
	}

	}

	// Send back to Windows any unhandled/unconsumed messages we want other apps to see (e.g. play/pause in music apps, etc.)
	return DefWindowProc(windowHandle, wmMessageCode, wParam, lParam);
}

Window::Window(const WindowConfig& config)
	:m_config(config)
{
	s_mainWindow = this;
}

Window::~Window()
{

}

void Window::Startup()
{
	CreateOSWindow();
}

void Window::BeginFrame()
{
	RunMessagePump();
}

void Window::EndFrame()
{

}

void Window::Shutdown()
{

}

const WindowConfig& Window::GetConfig() const
{
	return m_config;
}

Window* Window::GetWindowContext()
{
	return s_mainWindow;
}

Vec2 Window::GetNormalizedCursorPos() const
{
	HWND windowHandle = HWND(m_osWindowHandle);
	POINT cursorCoords;
	RECT clientRect;
	::GetCursorPos(&cursorCoords);
	::ScreenToClient(windowHandle, &cursorCoords);
	::GetClientRect(windowHandle, &clientRect);
	float cursorX = float(cursorCoords.x) / float(clientRect.right);
	float cursorY = float(cursorCoords.y) / float(clientRect.bottom);
	return Vec2(cursorX, 1.f - cursorY);
}

void Window::CreateOSWindow()
{
	Window* windowContext = Window::GetWindowContext();
	GUARANTEE_OR_DIE(windowContext != nullptr, "Window context was null");
	WindowConfig config = windowContext->GetConfig();

	// Define a window style/class
	WNDCLASSEX windowClassDescription;
	memset(&windowClassDescription, 0, sizeof(windowClassDescription));
	windowClassDescription.cbSize = sizeof(windowClassDescription);
	windowClassDescription.style = CS_OWNDC; // Redraw on move, request own Display Context
	windowClassDescription.lpfnWndProc = static_cast<WNDPROC>(WindowsMessageHandlingProcedure); // Register our Windows message-handling function
	windowClassDescription.hInstance = GetModuleHandle(NULL);
	windowClassDescription.hIcon = NULL;
	windowClassDescription.hCursor = NULL;
	windowClassDescription.lpszClassName = TEXT("Simple Window Class");
	RegisterClassEx(&windowClassDescription);

	// #SD1ToDo: Add support for fullscreen mode (requires different window style flags than windowed mode)
	const DWORD windowStyleFlags = WS_CAPTION | WS_BORDER | WS_SYSMENU | WS_OVERLAPPED;
	const DWORD windowStyleExFlags = WS_EX_APPWINDOW;

	// Get desktop rect, dimensions, aspect
	RECT desktopRect;
	HWND desktopWindowHandle = GetDesktopWindow();
	GetClientRect(desktopWindowHandle, &desktopRect);
	float desktopWidth = (float)(desktopRect.right - desktopRect.left);
	float desktopHeight = (float)(desktopRect.bottom - desktopRect.top);
	float desktopAspect = desktopWidth / desktopHeight;

	// Calculate maximum client size (as some % of desktop size)
	constexpr float maxClientFractionOfDesktop = 0.90f;
	float clientWidth = desktopWidth * maxClientFractionOfDesktop;
	float clientHeight = desktopHeight * maxClientFractionOfDesktop;
	if (config.m_clientAspect > desktopAspect)
	{
		// Client window has a wider aspect than desktop; shrink client height to match its width
		clientHeight = clientWidth / config.m_clientAspect;
	}
	else
	{
		// Client window has a taller aspect than desktop; shrink client width to match its height
		clientWidth = clientHeight * config.m_clientAspect;
	}

	m_clientDimensions = IntVec2((int)clientWidth, (int)clientHeight);

	// Calculate client rect bounds by centering the client area
	float clientMarginX = 0.5f * (desktopWidth - clientWidth);
	float clientMarginY = 0.5f * (desktopHeight - clientHeight);
	RECT clientRect;
	clientRect.left = (int)clientMarginX;
	clientRect.right = clientRect.left + (int)clientWidth;
	clientRect.top = (int)clientMarginY;
	clientRect.bottom = clientRect.top + (int)clientHeight;

	// Calculate the outer dimensions of the physical window, including frame et. al.
	RECT windowRect = clientRect;
	AdjustWindowRectEx(&windowRect, windowStyleFlags, FALSE, windowStyleExFlags);

	HMODULE applicationInstanceHandle = ::GetModuleHandle(NULL);
	const char* APP_NAME = config.m_windowTitle.c_str();
	WCHAR windowTitle[1024];
	MultiByteToWideChar(GetACP(), 0, APP_NAME, -1, windowTitle, sizeof(windowTitle) / sizeof(windowTitle[0]));
	HWND g_hWnd = CreateWindowEx(
		windowStyleExFlags,
		windowClassDescription.lpszClassName,
		windowTitle,
		windowStyleFlags,
		windowRect.left,
		windowRect.top,
		windowRect.right - windowRect.left,
		windowRect.bottom - windowRect.top,
		NULL,
		NULL,
		(HINSTANCE)applicationInstanceHandle,
		NULL);

	ShowWindow(g_hWnd, SW_SHOW);
	SetForegroundWindow(g_hWnd);
	SetFocus(g_hWnd);

	m_osWindowHandle = g_hWnd;

	HCURSOR cursor = LoadCursor(NULL, IDC_ARROW);
	SetCursor(cursor);
}

void Window::RunMessagePump()
{
	MSG queuedMessage;
	for (;; )
	{
		const BOOL wasMessagePresent = PeekMessage(&queuedMessage, NULL, 0, 0, PM_REMOVE);
		if (!wasMessagePresent)
		{
			break;
		}

		TranslateMessage(&queuedMessage);
		DispatchMessage(&queuedMessage); // This tells Windows to call our "WindowsMessageHandlingProcedure" (a.k.a. "WinProc") function
	}
}

bool Window::HasFocus()
{
	HWND activeWindow = ::GetActiveWindow();
	if (activeWindow == m_osWindowHandle)
		return true;

	return false;
}

std::string Window::GetClipboardData() const
{
	::OpenClipboard((HWND)m_osWindowHandle);
	HANDLE data = ::GetClipboardData(CF_TEXT);
	std::string textData((const char*)data);
	::CloseClipboard();
	return textData;
}

std::string Window::GetFileNameFromFileExploreDialogueBox(const char* initialDirectory)
{
	std::string selectedFileName;
	OPENFILENAMEA ofn = { 0 };
	char buffer[256];

	//get current directory so that you can set it back because GetOpenFileName changes the current directory for some reason
	char currDirectory[MAX_PATH] = { '\0' };
	::GetCurrentDirectoryA(MAX_PATH, currDirectory);

	ofn.lStructSize = sizeof(ofn);
	ofn.hwndOwner = (HWND)m_osWindowHandle;
	ofn.lpstrFile = buffer;
	// Set lpstrFile[0] to '\0' so that GetOpenFileName does not 
	// use the contents of szFile to initialize itself.
	ofn.lpstrFile[0] = '\0';
	ofn.nMaxFile = sizeof(buffer);
	ofn.lpstrFilter = "All\0*.*\0Text\0*.TXT\0";
	ofn.nFilterIndex = 1;
	ofn.lpstrFileTitle = NULL;
	ofn.nMaxFileTitle = 0;
	ofn.lpstrInitialDir = initialDirectory;
	ofn.Flags = OFN_PATHMUSTEXIST | OFN_FILEMUSTEXIST;

	if (::GetOpenFileNameA(&ofn))
	{
		char* start = strstr(buffer, "Data");
		selectedFileName.assign(start);
		//::CloseHandle(ofn.hInstance);
	}

	::SetCurrentDirectoryA(currDirectory);

	ofn = { 0 };
	return selectedFileName;
}


