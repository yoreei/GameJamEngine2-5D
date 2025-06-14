// dx2d.cpp : Defines the entry point for the application.

#define NOMINMAX //< remove min and max definitions from windows
#include <SDKDDKVer.h>
#define WIN32_LEAN_AND_MEAN             // Exclude rarely-used stuff from Windows headers
#include <windows.h>
#include <stdlib.h>
#include <malloc.h>
#include <memory.h>
#include <tchar.h>
#include <map>
#include <string>
#include <chrono>

#include <d2d1.h>
#include <tchar.h>
#include "Resource.h"
#include "danny/cpp3rdParty.h"

#pragma comment(lib, "d2d1.lib")

#define FMT_UNICODE 0 //https://github.com/gabime/spdlog/issues/3251
#include <spdlog/spdlog.h>
#include <spdlog/sinks/basic_file_sink.h>

#include "GJGlobals.h"
#include "GameEngine.h"


/// \return true if application should continue, false if application should stop
bool processPendingOsMessages() {
	MSG msg = { };
	while (PeekMessage(&msg, NULL, 0, 0, PM_REMOVE))
	{
		// WM_QUIT will not be dispatched to WindowProc, must handle it here:
		if (msg.message == WM_QUIT)
		{
			std::cout << "PeekMessage got WM_QUIT, quitting...\n";
			return false;
		}
		TranslateMessage(&msg);
		DispatchMessage(&msg);
	}
	return true;

}

void enableConsole()
{
	AllocConsole();

	// Redirect stdout
	FILE* fpOut;
	freopen_s(&fpOut, "CONOUT$", "w", stdout);

	// Redirect stderr
	FILE* fpErr;
	freopen_s(&fpErr, "CONOUT$", "w", stderr);

	// Redirect stdin (optional)
	FILE* fpIn;
	freopen_s(&fpIn, "CONIN$", "r", stdin);

	// Optional: set unbuffered mode
	setvbuf(stdout, NULL, _IONBF, 0);
	setvbuf(stderr, NULL, _IONBF, 0);
}

// Forward declarations of functions
LRESULT CALLBACK WindowProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam);

#define MAX_LOADSTRING 100

// Global Variables:
HINSTANCE hInst;                                // current instance
WCHAR szTitle[MAX_LOADSTRING];                  // The title bar text
WCHAR szWindowClass[MAX_LOADSTRING];            // the main window class name

int WINAPI wWinMain(
	_In_ HINSTANCE hInstance,
	_In_opt_ HINSTANCE hPrevInstance,
	_In_ LPWSTR    lpCmdLine,
	_In_ int       nCmdShow
)
{
	enableConsole();

	auto fileLogger = spdlog::basic_logger_mt("file_logger", "logs/output.log");
	spdlog::set_default_logger(fileLogger);

	// Initialize COM library
	HRESULT hr = CoInitialize(NULL);
	if (FAILED(hr))
	{
		MessageBox(NULL, _T("COM library initialization failed."), _T("Error"), MB_OK);
		return 0;
	}

	// Register the window class
	const wchar_t CLASS_NAME[] = L"Direct2DWindowClass";

	WNDCLASS wc = { };
	wc.lpfnWndProc = WindowProc;
	wc.hInstance = hInstance;
	wc.lpszClassName = CLASS_NAME;
	wc.hCursor = LoadCursor(NULL, IDC_ARROW);
	wc.hbrBackground = (HBRUSH)(COLOR_WINDOW + 1);

	if (!RegisterClass(&wc))
	{
		MessageBox(NULL, _T("Window Registration Failed!"), _T("Error"), MB_OK);
		CoUninitialize();
		return 0;
	}

	// Create the window
	bool fullscreen = true;
	int screenWidth = GetSystemMetrics(SM_CXSCREEN);
	int screenHeight = GetSystemMetrics(SM_CYSCREEN);
	if (!fullscreen) {
		screenWidth /= 2;
		screenHeight /= 2;
	}

	HWND hwnd = CreateWindowEx(
		0,                              // Optional window styles.
		CLASS_NAME,                     // Window class
		L"Game Jam Engine", // Window text
		fullscreen ? WS_POPUP : WS_OVERLAPPEDWINDOW,                       // Window style set to WS_POPUP for borderless

		// Size and position set to cover the entire screen
		0, 0, screenWidth, screenHeight,

		NULL,       // Parent window    
		NULL,       // Menu
		hInstance,  // Instance handle
		NULL        // Additional application data
	);

	if (hwnd == NULL)
	{
		MessageBox(NULL, _T("Window Creation Failed!"), _T("Error"), MB_OK);
		CoUninitialize();
		return 0;
	}

	ShowWindow(hwnd, nCmdShow);

	std::unique_ptr<GameEngine> gameWorld = std::make_unique<GameEngine>(hwnd, "../assets/map1.txt");
	GGameEnginePtr = gameWorld.get();

	// Message loop
	TimePoint currentTime = getTimePoint();
	Seconds accumulator{ 0 };
	const Seconds deltaTime{ 0.01 };
	const Seconds MAX_FRAMETIME{ 0.25 };

	// Game Loop based on https://gafferongames.com/post/fix_your_timestep/
	while (processPendingOsMessages())
	{
		TimePoint newTime = getTimePoint();
		Seconds frameTime = newTime - currentTime;
		frameTime = std::min(frameTime, MAX_FRAMETIME);

		currentTime = newTime;
		accumulator += frameTime;

		for (;accumulator >= deltaTime; accumulator -= deltaTime)
		{
			GGameEnginePtr->tick(deltaTime);
		}

		const float alpha = toF(accumulator / deltaTime);
		GGameEnginePtr->draw(alpha);
	}

	CoUninitialize();
	std::cout << "Press Enter to exit\n";
	std::cin.get();
	return 0;
}

// Window Procedure to handle messages for the main window
LRESULT CALLBACK WindowProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
	switch (uMsg)
	{
	case WM_PAINT:
	{
		PAINTSTRUCT ps;
		BeginPaint(hwnd, &ps);
		EndPaint(hwnd, &ps);
	}
	return 0;
	case WM_SIZE:
	{
		if (GGameEnginePtr) {
			GGameEnginePtr->renderer.wmResize(hwnd);
		}
	}
	return 0;

	case WM_DESTROY:
	{
		PostQuitMessage(0);
	}
	return 0;

	case WM_KEYDOWN:
	{
		if (GGameEnginePtr) {
			GGameEnginePtr->handleInput(wParam, true);
		}
	}
	return 0;

	case WM_KEYUP:
	{
		if (GGameEnginePtr) {
			GGameEnginePtr->handleInput(wParam, false);
		}
	}
	return 0;
	default:
		return DefWindowProc(hwnd, uMsg, wParam, lParam);
	}
}

