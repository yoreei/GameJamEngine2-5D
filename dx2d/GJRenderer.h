#pragma once
#include <map>
#include <string>
#include <tchar.h>
#include <chrono>
#include <array>
#include <stdexcept>
#include <format>

#include <windows.h>
#include <d2d1.h>
#include <d2d1helper.h>
#include <dwrite.h>
#include <wrl/client.h>
#include <wincodec.h>
using Microsoft::WRL::ComPtr;

#include "GJScene.h"

enum class TextFormat {
	HEADING = 0,
	NORMAL,
	SMALL,
	size
};
enum EBitmap {
	QLeap = 0,
	Explode,
	size
};

class GJRenderer {
public:
	void init(HWND _hWnd, const GJScene* _scene) {

		hWnd = _hWnd;
		scene = _scene;

		// Create WIC Factory
		HRESULT hr = CoCreateInstance(
			CLSID_WICImagingFactory,
			nullptr,
			CLSCTX_INPROC_SERVER,
			IID_PPV_ARGS(&pWICFactory)
		);
		checkFailed(hr, hWnd, "CoCreateInstance failed");


		hr = D2D1CreateFactory(D2D1_FACTORY_TYPE_SINGLE_THREADED, &pFactory);
		checkFailed(hr, hWnd, "CreateFactory");


		// Get the size of the client area
		RECT rc;
		GetClientRect(hWnd, &rc);
		hr = pFactory->CreateHwndRenderTarget(
			D2D1::RenderTargetProperties(),
			D2D1::HwndRenderTargetProperties(
				hWnd,
				D2D1::SizeU(rc.right - rc.left, rc.bottom - rc.top)
			),
			&pRenderTarget
		);
		checkFailed(hr, hWnd, "createHwndRenderTarget failed");

		// Create a compatible render target for low-res drawing
		hr = pRenderTarget->CreateCompatibleRenderTarget(
			D2D1::SizeF(360.0f, 360.0f),
			&pLowResRenderTarget
		);
		checkFailed(hr, hWnd, "createCompatibleRenderTarget failed");

		// Disable anti-aliasing for pixelated look
		pLowResRenderTarget->SetAntialiasMode(D2D1_ANTIALIAS_MODE_ALIASED);
		pRenderTarget->SetAntialiasMode(D2D1_ANTIALIAS_MODE_ALIASED);

		loadImage(L"assets/qLeap.png", EBitmap::QLeap);
		loadImage(L"assets/explode.png", EBitmap::Explode);
		if (EBitmap::size != 2) {
			MessageBox(NULL, L"update bitmaps!", L"Error", MB_OK);
		}

		// Create a solid color brush
		hr = pRenderTarget->CreateSolidColorBrush(
			D2D1::ColorF(D2D1::ColorF(0.1f, 0.1f, 0.1f)),
			&brushes["black"]
		);
		checkFailed(hr, hWnd, "createSolidColorBrush failed");

		hr = pRenderTarget->CreateSolidColorBrush(
			D2D1::ColorF(D2D1::ColorF(0.7f, 1.f, 0.7f)),
			&brushes["green"]
		);
		checkFailed(hr, hWnd, "createSolidColorBrush failed");

		hr = pRenderTarget->CreateSolidColorBrush(
			D2D1::ColorF(D2D1::ColorF(1.f, 1.f, 0.7f)),
			&brushes["amber"]
		);
		checkFailed(hr, hWnd, "createSolidColorBrush failed");

		//hr = pRenderTarget->CreateSolidColorBrush(
		//	D2D1::ColorF(D2D1::ColorF(0.7f, 0.7f, 1.f)),
		//	&brushes["blue"]
		//);
		hr = pRenderTarget->CreateSolidColorBrush(
			D2D1::ColorF(D2D1::ColorF(0.49f, 0.995f, 0.995f)),
			&brushes["blue"]
		);

		checkFailed(hr, hWnd, "createSolidColorBrush failed");
		hr = pRenderTarget->CreateSolidColorBrush(
			D2D1::ColorF(D2D1::ColorF(1.f, 1.f, 1.f)),
			&brushes["white"]
		);
		checkFailed(hr, hWnd, "createSolidColorBrush failed");


		IDWriteFactory* pDWriteFactory = nullptr;
		hr = DWriteCreateFactory(DWRITE_FACTORY_TYPE_SHARED, __uuidof(IDWriteFactory), reinterpret_cast<IUnknown**>(&pDWriteFactory));

		ComPtr<IDWriteFontCollection> pFontCollection;
		hr = pDWriteFactory->GetSystemFontCollection(&pFontCollection, FALSE);
		checkFailed(hr, hWnd, "getSystemFontCollection failed");

		// Find the font family by name
		UINT32 index = 0;
		BOOL exists = FALSE;
		hr = pFontCollection->FindFamilyName(
			fontName.c_str(),
			&index,
			&exists
		);
		if (FAILED(hr) || !exists)
		{
			MessageBox(NULL, L"Please, exit the game, install the 'pressStart2P.ttf font file in the game folder for the game to render text properly", L"Error", MB_OK);
			std::wcerr << L"FindFamilyName failed: " << hr << std::endl;
		}
		hr = pDWriteFactory->CreateTextFormat(
			fontName.c_str(),                // Font family
			nullptr,                 // Font collection
			DWRITE_FONT_WEIGHT_NORMAL,
			DWRITE_FONT_STYLE_NORMAL,
			DWRITE_FONT_STRETCH_NORMAL,
			30.0f,                   // Font size
			L"",                     // Locale
			&textFormats[static_cast<size_t>(TextFormat::HEADING)]
		);
		hr = textFormats[static_cast<size_t>(TextFormat::HEADING)]->SetTextAlignment(DWRITE_TEXT_ALIGNMENT_CENTER);
		hr = textFormats[static_cast<size_t>(TextFormat::HEADING)]->SetParagraphAlignment(DWRITE_PARAGRAPH_ALIGNMENT_NEAR);

		hr = pDWriteFactory->CreateTextFormat(
			fontName.c_str(),                // Font family
			nullptr,                 // Font collection
			DWRITE_FONT_WEIGHT_NORMAL,
			DWRITE_FONT_STYLE_NORMAL,
			DWRITE_FONT_STRETCH_NORMAL,
			16.0f,                   // Font size
			L"",                     // Locale
			&textFormats[static_cast<size_t>(TextFormat::NORMAL)]
		);
		hr = textFormats[static_cast<size_t>(TextFormat::NORMAL)]->SetTextAlignment(DWRITE_TEXT_ALIGNMENT_CENTER);
		hr = textFormats[static_cast<size_t>(TextFormat::NORMAL)]->SetParagraphAlignment(DWRITE_PARAGRAPH_ALIGNMENT_NEAR);

		hr = pDWriteFactory->CreateTextFormat(
			fontName.c_str(),                // Font family
			nullptr,                 // Font collection
			DWRITE_FONT_WEIGHT_NORMAL,
			DWRITE_FONT_STYLE_NORMAL,
			DWRITE_FONT_STRETCH_NORMAL,
			12.0f,                   // Font size
			L"",                     // Locale
			&textFormats[static_cast<size_t>(TextFormat::SMALL)]
		);
		hr = textFormats[static_cast<size_t>(TextFormat::SMALL)]->SetTextAlignment(DWRITE_TEXT_ALIGNMENT_LEADING);
		hr = textFormats[static_cast<size_t>(TextFormat::SMALL)]->SetParagraphAlignment(DWRITE_PARAGRAPH_ALIGNMENT_NEAR);


		if (static_cast<size_t>(TextFormat::size) != 3) {
			MessageBox(NULL, L"update textformat", L"Error", MB_OK);
			exit(-1);
		}

		// Setup transformations
		int screenWidth = GetSystemMetrics(SM_CXSCREEN);
		int screenHeight = GetSystemMetrics(SM_CYSCREEN);

		float scaleF = static_cast<float>(screenHeight) / 360.f;
		D2D1::Matrix3x2F scale = D2D1::Matrix3x2F::Scale(
			scaleF, scaleF,
			D2D1::Point2F(0.f, 0.f)
		);

		float offsetF = (screenWidth - scaleF * 360.f) / 2.f;
		D2D1::Matrix3x2F translation = D2D1::Matrix3x2F::Translation(
			offsetF, // X offset
			0.f  // Y offset
		);

		D2D1::Matrix3x2F transform = scale * translation;

		pRenderTarget->SetTransform(transform);

		// Setup Draw Call Table
		drawCallTable[static_cast<size_t>(State::INGAME)] = &GJRenderer::drawINGAME;
		drawCallTable[static_cast<size_t>(State::PREGAME)] = &GJRenderer::drawPREGAME;
		drawCallTable[static_cast<size_t>(State::LOSS)] = &GJRenderer::drawLOSS;
		drawCallTable[static_cast<size_t>(State::WIN)] = &GJRenderer::drawWIN;
		drawCallTable[static_cast<size_t>(State::MAINMENU)] = &GJRenderer::drawMAINMENU;
		drawCallTable[static_cast<size_t>(State::PAUSED)] = &GJRenderer::drawPAUSED;
		if (static_cast<uint32_t>(State::size) != 6) {
			throw std::runtime_error("update state handling in renderer\n");
		}

	}

	~GJRenderer() {
		releaseResources();
	}

	void releaseResources() {
		for (auto& pair : brushes) {
			auto& pBrush = std::get<ID2D1SolidColorBrush*>(pair);
			pBrush->Release();
			pBrush = nullptr;
		}

		if (pRenderTarget)
		{
			pRenderTarget->Release();
			pRenderTarget = nullptr;
		}

		if (pFactory)
		{
			pFactory->Release();
			pFactory = nullptr;
		}

		if (pLowResRenderTarget) {
			pLowResRenderTarget->Release();

		}

	}

	void wmResize(HWND hwnd) {
		if (pRenderTarget)
		{
			RECT rc;
			GetClientRect(hwnd, &rc);
			pRenderTarget->Resize(D2D1::SizeU(rc.right - rc.left, rc.bottom - rc.top));
		}
	}

	void draw(std::chrono::duration<double, std::milli> delta) {
		pRenderTarget->BeginDraw();
		pLowResRenderTarget->BeginDraw();

		pRenderTarget->Clear(D2D1::ColorF(D2D1::ColorF::Black));
		pLowResRenderTarget->Clear(D2D1::ColorF(D2D1::ColorF::Black, 0.f));

		drawBorder();
		(this->*drawCallTable[static_cast<size_t>(scene->state)])();

		// draw low res rt to main rt
		pLowResRenderTarget->EndDraw();

		ID2D1Bitmap* pLowResBitmap = nullptr;
		HRESULT hr = pLowResRenderTarget->GetBitmap(&pLowResBitmap);
		checkFailed(hr, hWnd, "getBitmap failed");

		D2D1_RECT_F destRect = D2D1::RectF(
			0.0f,                         // Left
			0.0f,                         // Top
			360.f,  // Right
			360.f  // Bottom
		);
		pRenderTarget->DrawBitmap(
			pLowResBitmap,                                    // The bitmap to draw
			&destRect,                                      // Destination rectangle
			1.0f,                                           // Opacity (1.0f = fully opaque)
			D2D1_BITMAP_INTERPOLATION_MODE_NEAREST_NEIGHBOR, // Nearest-neighbor for pixelated scaling
			NULL                                            // Source rectangle (NULL to use the entire bitmap)
		);
		pLowResBitmap->Release();
		hr = pRenderTarget->EndDraw();

		if (hr == D2DERR_RECREATE_TARGET)
		{
			releaseResources();
			init(hWnd, scene);
		}

	}

	void drawINGAME() {
		drawScene();
		drawUI();
	}
	void drawPREGAME() {
		drawInstructions();
		drawUI();
	}
	void drawMAINMENU() {
		drawScene();
		drawMenu("Main Menu");
	}
	void drawLOSS() {
		drawScene();
		drawEnd(L"Loss");
		drawUI();
	}
	void drawWIN() {
		drawScene();
		drawEnd(L"New"); //> i.e. "New HiScore" will be written
		drawUI();
	}
	void drawPAUSED() {
		drawScene();
		drawPaused();
		drawUI();
	}

	void drawInstructions() {
		std::wstring instructions = L"You are an electron, running along the path of least resistance. Do not hit the air bubbles!\n\n[Q],[W],[A],[S]: Quantum Scatter\n[Space] Quantum Leap\n";
		pRenderTarget->DrawText(
			instructions.c_str(),    // Text to render
			wcslen(instructions.c_str()),
			textFormats[static_cast<size_t>(TextFormat::NORMAL)],            // Text format
			D2D1::RectF(20, 20, 340, 340), // Layout rectangle
			brushes["white"]
		);

	}

	void drawScene() {

		// draw entities
		if (!scene->qLeapActive) {
			for (const Entity& e : scene->entities) {
				if (e.health <= 0) {
					continue;
				}

				vec3 pos = e.getPos();
				D2D1_RECT_F unitSquare = D2D1::RectF(
					e.getPos().e[0], pos.e[1],
					e.getPos().e[0] + e.size, pos.e[1] + e.size
				);
				pLowResRenderTarget->DrawRectangle(unitSquare, brushes["green"], 2.f);
			}
		}
		// draw obstacles
		for (const Entity& o : scene->obstacles) {
			if (o.health <= 0) {
				continue;
			}
			vec3 pos = o.getPos();
			D2D1_ELLIPSE ellipse = D2D1::Ellipse(
				D2D1::Point2F(pos.x(), pos.y()), // Center point (x, y)
				o.size,                         // Radius X
				o.size                          // Radius Y
			);
			pLowResRenderTarget->FillEllipse(ellipse, brushes["blue"]);

		}
	}

	void loadImage(const std::wstring& filePath, EBitmap eBitmap) {

		ComPtr<IWICBitmapDecoder> pDecoder;
		HRESULT hr = pWICFactory->CreateDecoderFromFilename(
			filePath.c_str(),
			nullptr,
			GENERIC_READ,
			WICDecodeMetadataCacheOnLoad,
			&pDecoder
		);
		checkFailed(hr, hWnd, "CreateDecoderFromFileName failed");

		// Get the first frame of the image
		ComPtr<IWICBitmapFrameDecode> pFrame;
		hr = pDecoder->GetFrame(0, &pFrame);
		checkFailed(hr, hWnd, "GetFrame failed");

		// Convert the image format to BGRA, which Direct2D expects
		ComPtr<IWICFormatConverter> pConverter;
		hr = pWICFactory->CreateFormatConverter(&pConverter);
		checkFailed(hr, hWnd, "CreateFormatConverter failed");

		hr = pConverter->Initialize(
			pFrame.Get(),
			GUID_WICPixelFormat32bppPBGRA, // Direct2D expects BGRA format with premultiplied alpha
			WICBitmapDitherTypeNone,
			nullptr,
			0.0,
			WICBitmapPaletteTypeMedianCut
		);
		checkFailed(hr, hWnd, "initialize failed");

		hr = pRenderTarget->CreateBitmapFromWicBitmap(
			pConverter.Get(),
			nullptr, // Bitmap properties; nullptr for default
			&bitmaps[eBitmap]
		);
		checkFailed(hr, hWnd, "createBitmapFromWicBitmap failed");
	}

	void drawUI() {
		if (scene->explodeCd) {
			D2D1_SIZE_F bitmapSize = bitmaps[EBitmap::Explode]->GetSize();
			bitmapSize.height *= 2;
			bitmapSize.width *= 2;
			pLowResRenderTarget->DrawBitmap(
				bitmaps[EBitmap::Explode].Get(),
				D2D1::RectF(160, 325, 160 + bitmapSize.width, 325 + bitmapSize.height),
				1.0f, // Opacity (1.0f = fully opaque)
				D2D1_BITMAP_INTERPOLATION_MODE_NEAREST_NEIGHBOR,
				nullptr // Source rectangle (nullptr to use entire bitmap)
			);
		}
		std::wstring wPoints = std::to_wstring(scene->points);

		textFormats[static_cast<size_t>(TextFormat::SMALL)]->SetTextAlignment(DWRITE_TEXT_ALIGNMENT_TRAILING);
		pRenderTarget->DrawText(
			wPoints.c_str(),    // Text to render
			wcslen(wPoints.c_str()),
			textFormats[static_cast<size_t>(TextFormat::SMALL)],            // Text format
			D2D1::RectF(0, 335, 345, 360), // Layout rectangle
			brushes["white"]
		);

		if (!scene->qLeapCd) {
			D2D1_SIZE_F bitmapSize = bitmaps[EBitmap::QLeap]->GetSize();
			bitmapSize.height *= 2;
			bitmapSize.width *= 2;
			pLowResRenderTarget->DrawBitmap(
				bitmaps[EBitmap::QLeap].Get(),
				D2D1::RectF(15, 325, 15 + bitmapSize.width, 325 + bitmapSize.height),
				1.0f, // Opacity (1.0f = fully opaque)
				D2D1_BITMAP_INTERPOLATION_MODE_NEAREST_NEIGHBOR,
				nullptr // Source rectangle (nullptr to use entire bitmap)
			);
		}

	}

	void drawBorder() {
		D2D1_RECT_F unitSquare = D2D1::RectF(
			0.f, 0.f,
			360.f, 360.f
		);
		pRenderTarget->DrawRectangle(unitSquare, brushes["amber"], 2.f);

	}

	void drawPaused() {
		pLowResRenderTarget->DrawText(
			L"Paused",    // Text to render
			wcslen(L"Paused"),
			textFormats[static_cast<size_t>(TextFormat::HEADING)],            // Text format
			D2D1::RectF(0, 40, 360, 180), // Layout rectangle
			brushes["white"]
		);

		std::wstring t = L"[ESC] Resume\n[R] Reload\n[BSPACE] Quit";
		pLowResRenderTarget->DrawText(
			t.c_str(),    // Text to render
			wcslen(t.c_str()),
			textFormats[static_cast<size_t>(TextFormat::NORMAL)],            // Text format
			D2D1::RectF(0, 180, 320, 210), // Layout rectangle
			brushes["white"]
		);
	}

	void drawEnd(const std::wstring& text) {
		std::wstring heading = std::format(L"{}\nHiScore: {}", text, scene->hiScore);
		pLowResRenderTarget->DrawText(
			heading.c_str(),    // Text to render
			wcslen(heading.c_str()),
			textFormats[static_cast<size_t>(TextFormat::HEADING)],            // Text format
			D2D1::RectF(0, 40, 360, 180), // Layout rectangle
			brushes["white"]
		);

		pLowResRenderTarget->DrawText(
			L"[R] Reload\n[BSPACE] Quit",    // Text to render
			wcslen(L"[R] Reload\n[BSPACE] Quit"),
			textFormats[static_cast<size_t>(TextFormat::NORMAL)],            // Text format
			D2D1::RectF(0, 220, 320, 300), // Layout rectangle
			brushes["white"]
		);

	}
	void drawMenu(const std::string& text) {
		// todo
		pLowResRenderTarget->DrawText(
			L"Electric\nBubble\nBath!",    // Text to render
			wcslen(L"Electric\nBubble\nBath!"),
			textFormats[static_cast<size_t>(TextFormat::HEADING)],            // Text format
			D2D1::RectF(0, 40, 360, 180), // Layout rectangle
			brushes["blue"]
		);

		pLowResRenderTarget->DrawText(
			L"[ENTER] Game\n[BSPACE] Quit",    // Text to render
			wcslen(L"[ENTER] Game\nF[BSPACE] Quit"),
			textFormats[static_cast<size_t>(TextFormat::NORMAL)],            // Text format
			D2D1::RectF(0, 180, 320, 210), // Layout rectangle
			brushes["blue"]
		);

		//pRenderTarget->DrawText(
		//	L"[F10] Terminate Program",    // Text to render
		//	wcslen(L"[F10] Terminate Program"),
		//	textFormats[static_cast<size_t>(TextFormat::NORMAL)],            // Text format
		//	D2D1::RectF(0, 210, 320, 210), // Layout rectangle
		//	brushes["blue"]
		//);

		//D2D1_RECT_F unitSquare = D2D1::RectF(
		//	20.f, 20.f,
		//	100.f, 100.f
		//);
		//pRenderTarget->DrawRectangle(unitSquare, brushes["blue"], 3.f);

	}

private:
	void checkFailed(HRESULT hr, HWND hwnd, const std::string& message ) {
		if (FAILED(hr))
		{
			MessageBoxA(NULL, message.c_str(), "Error", MB_OK);
			DestroyWindow(hwnd);
			CoUninitialize();
			exit(-1);
		}

	}

private:
	std::wstring fontName = L"Press Start 2P";
	HWND hWnd;
	const GJScene* scene = nullptr;
	ID2D1HwndRenderTarget* pRenderTarget = nullptr;
	ID2D1BitmapRenderTarget* pLowResRenderTarget = nullptr;
	ID2D1Factory* pFactory = nullptr;
	ComPtr<IWICImagingFactory> pWICFactory = nullptr;
	std::map<std::string, ID2D1SolidColorBrush*> brushes = {
		{"black", nullptr },
		{"green", nullptr },
		{"amber", nullptr },
		{"blue", nullptr } };
	std::array<IDWriteTextFormat*, static_cast<size_t>(TextFormat::size)> textFormats;
	using DrawFunction = void(GJRenderer::*)();
	std::array<DrawFunction, static_cast<size_t>(State::size)> drawCallTable;

	std::array<ComPtr<ID2D1Bitmap>, EBitmap::size> bitmaps;
};

