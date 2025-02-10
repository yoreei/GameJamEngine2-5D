// dx2d.cpp : Defines the entry point for the application.
//


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

#pragma comment(lib, "d2d1.lib")

#define FMT_UNICODE 0 //https://github.com/gabime/spdlog/issues/3251
#include <spdlog/spdlog.h>
#include <spdlog/sinks/basic_file_sink.h>
#include "GJRenderer.h"
#include "GJScene.h"
#include "GJSimulation.h"
#include "GJGlobals.h"

GJSimulation simulation;
GJRenderer renderer;


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
	int screenWidth = GetSystemMetrics(SM_CXSCREEN);
	int screenHeight = GetSystemMetrics(SM_CYSCREEN);

	HWND hwnd = CreateWindowEx(
		0,                              // Optional window styles.
		CLASS_NAME,                     // Window class
		L"Game Jam Engine", // Window text
		WS_POPUP,                       // Window style set to WS_POPUP for borderless

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

	GJScene prevScene{};
	GJScene interpScene{};
	const GJScene* nextScene = simulation.getScene();
	renderer.init(hwnd, &interpScene);

	// Message loop
	MSG msg = { };
	bool quit = false;
	Time currentTime = getTime();
	Seconds accumulator{ 0 };
	Seconds dt{ 0.01 };
	while (!quit)
	{
		// Process OS messages once per frame
		while (PeekMessage(&msg, NULL, 0, 0, PM_REMOVE))
		{
			if (msg.message == WM_QUIT)
			{
				quit = true;
				break;
			}
			TranslateMessage(&msg);
			DispatchMessage(&msg);
		}

		// Now run the rest of your game loop:
		Time newTime = getTime();
		Seconds frameTime = newTime - currentTime;
		if (frameTime > Seconds{ 0.25 })
			frameTime = Seconds{ 0.25 };
		currentTime = newTime;

		accumulator += frameTime;

		while (accumulator >= dt)
		{
			prevScene = *nextScene;
			simulation.tick(dt);
			GEngineTime += dt;
			accumulator -= dt;
		}

		const float alpha = accumulator / dt;
		interpScene.interpolate(prevScene, *nextScene, alpha);

		renderer.draw(dt);
	}

	CoUninitialize();
	return 0;
}



// Window Procedure to handle messages
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
		renderer.wmResize(hwnd);
	}
	return 0;

	case WM_DESTROY:
	{
		PostQuitMessage(0);
	}
	return 0;

	case WM_KEYDOWN:
	{
		simulation.handleInput(wParam, true);
	}
	return 0;

	case WM_KEYUP:
	{
		simulation.handleInput(wParam, false);
	}
	return 0;
	default:
		return DefWindowProc(hwnd, uMsg, wParam, lParam);
	}
}

