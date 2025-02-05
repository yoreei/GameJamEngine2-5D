#pragma once
#include <map>
#include <string>
#include <tchar.h>
#include <chrono>
#include <array>
#include <stdexcept>
#include <format>

#include <DirectXMath.h>
#include <DirectXCollision.h>
#include <windows.h>
#include <d2d1.h>
#include <d2d1helper.h>
#include <dwrite.h>
#include <wrl/client.h>
#include <wincodec.h>
using Microsoft::WRL::ComPtr;

#define FMT_UNICODE 0 //https://github.com/gabime/spdlog/issues/3251
#include "spdlog/spdlog.h"

#include "GJScene.h"

constexpr size_t toId(auto someEnum) {
	return static_cast<size_t>(someEnum);
}
enum class TextFormat {
	HEADING = 0,
	NORMAL,
	SMALL,
	size
};
enum class EGPUBitmap : size_t {
	QLeap = 0,
	Explode,
	size
};

enum class ECPUBitmap : size_t {
	Floor = 0,
	size
};

struct CPUBitmap {
	size_t width;
	size_t height;
	uint8_t channels;
	std::vector<uint8_t> data;
	size_t getPixel(size_t x, size_t y) {
		return (y * width + x) * channels;
	}
};
class GJRenderer {
public:
	void init(HWND _hWnd, const GJScene* _scene) {
		scene = _scene;
		hWnd = _hWnd;
		updateImgPlaneDist();

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
			D2D1::SizeF(scene->ScrW(), scene->ScrH()),
			&pLowResRenderTarget
		);
		checkFailed(hr, hWnd, "createCompatibleRenderTarget failed");

		// Disable anti-aliasing for pixelated look
		pLowResRenderTarget->SetAntialiasMode(D2D1_ANTIALIAS_MODE_ALIASED);
		pRenderTarget->SetAntialiasMode(D2D1_ANTIALIAS_MODE_ALIASED);

		loadGPUBitmap(L"assets/qLeap.png", EGPUBitmap::QLeap);
		loadGPUBitmap(L"assets/explode.png", EGPUBitmap::Explode);
		initDrawBuffer(L"assets/textures/4.png");
		if (toId(EGPUBitmap::size) != 2) {
			MessageBox(NULL, L"update bitmaps!", L"Error", MB_OK);
		}

		// Create a solid color brush
		hr = pRenderTarget->CreateSolidColorBrush(
			D2D1::ColorF(D2D1::ColorF(0.1f, 0.1f, 0.1f)),
			&brush
		);
		checkFailed(hr, hWnd, "createSolidColorBrush failed");

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

		float scaleF = static_cast<float>(screenHeight) / scene->ScrHf();
		D2D1::Matrix3x2F scale = D2D1::Matrix3x2F::Scale(
			scaleF, scaleF,
			D2D1::Point2F(0.f, 0.f)
		);

		float offsetF = (screenWidth - scaleF * scene->ScrWf()) / 2.f;
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

	void draw(std::chrono::duration<double, std::milli> delta) {
		pRenderTarget->BeginDraw();
		pLowResRenderTarget->BeginDraw();

		pRenderTarget->Clear(D2D1::ColorF(D2D1::ColorF::Black));
		pLowResRenderTarget->Clear(D2D1::ColorF(D2D1::ColorF::Black, 0.f));

		//drawBorder();
		(this->*drawCallTable[static_cast<size_t>(scene->state)])();


		// draw low res rt to main rt
		pLowResRenderTarget->EndDraw();

		ID2D1Bitmap* pLowResBitmap = nullptr;
		HRESULT hr = pLowResRenderTarget->GetBitmap(&pLowResBitmap);
		checkFailed(hr, hWnd, "getBitmap failed");

		D2D1_RECT_F destRect = D2D1::RectF(
			0.0f,                         // Left
			0.0f,                         // Top
			scene->ScrW(),  // Right
			scene->ScrH()  // Bottom
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

	void drawMinimap() {
		//draw minimap
		int size = 2;
		float minimapAlpha = 0.7;
		D2D1_RECT_F pos = D2D1::RectF(0, 0, 0, 0);
		for (int y = 0; y < scene->height; ++y) {
			pos.top = y * size;
			pos.bottom = pos.top + size;
			for (int x = 0; x < scene->width; ++x) {
				const char& tile = scene->get(x, y);
				if (tile == '#') {
					//brush->SetColor(D2D1_COLOR_F{});
					brush->SetColor(D2D1::ColorF(0.2f, 0.2f, 0.2f, minimapAlpha));
				}
				else if (tile == ' ') {
					brush->SetColor(D2D1::ColorF{ 1.f, 1.f, 1.f, minimapAlpha });
				}
				else {
					throw std::runtime_error("");
				}
				pos.left = x * size;
				pos.right = pos.left + size;

				pLowResRenderTarget->FillRectangle(pos, brush);

			}
		}

		// draw entities on minimap
		XMVECTOR posV = size * XMVectorFloor(scene->position);
		const float pos_x = XMVectorGetX(posV);
		const float pos_y = XMVectorGetY(posV);
		pos = D2D1::RectF(pos_x, pos_y, pos_x + size, pos_y + size);
		brush->SetColor(D2D1::ColorF{ 1.f, 0.7f, 0.7f, minimapAlpha });
		pLowResRenderTarget->FillRectangle(pos, brush);

		//v Draw LOS
		brush->SetColor(D2D1::ColorF{ 1.f, 1.f, 0.7, minimapAlpha });
		XMVECTOR endV = posV + scene->getDir() * 20;
		const float end_x = XMVectorGetX(endV);
		const float end_y = XMVectorGetY(endV); //< '-' because screen-space is inverted
		pLowResRenderTarget->DrawLine(
			D2D1_POINT_2F(pos_x, pos_y),
			D2D1_POINT_2F(end_x, end_y),
			brush, 1.f);

	}

	void drawWalls() {
		float dist;
		XMVECTOR dir;
		for (int x = 0; x < scene->ScrW(); ++x) {
			float fixPersp = getPixelDir(x, OUT dir);
			dist = intersect(scene->position, dir);
			float color = -(dist / MAXVIEWDIST) + 1.f;
			brush->SetColor(D2D1::ColorF(color, color, color));
			dist = std::clamp(dist, 0.f, MAXVIEWDIST);
			float lineH = scene->ScrHf() / (dist * fixPersp);
			float midLine = scene->HscrHf(); // fix when implementing look up-down
			pLowResRenderTarget->DrawLine(D2D1_POINT_2F(static_cast<float>(x), midLine + lineH), D2D1_POINT_2F(static_cast<float>(x), midLine - lineH), brush, 1.f);

		}

	}

	void drawScene() {
		updateDrawBuffer();

		D2D1_RECT_U destRect = { 0, 0, scene->ScrW(), scene->ScrH() };
		pFloorGPUBitmap->CopyFromMemory(&destRect, drawBuffer.data(), scene->ScrW() * sizeof(uint32_t));

		D2D1_SIZE_F bitmapSize = pFloorGPUBitmap->GetSize();
		pLowResRenderTarget->DrawBitmap(
			pFloorGPUBitmap.Get(),
			D2D1::RectF(0, 0, bitmapSize.width, bitmapSize.height),
			1.0f, // Opacity (1.0f = fully opaque)
			D2D1_BITMAP_INTERPOLATION_MODE_NEAREST_NEIGHBOR,
			nullptr // Source rectangle (nullptr to use entire bitmap)
		);

		//drawWalls();
	}

	void drawUI() {
		if (scene->explodeCd) {
			D2D1_SIZE_F bitmapSize = GPUBitmaps[toId(EGPUBitmap::Explode)]->GetSize();
			bitmapSize.height *= 2;
			bitmapSize.width *= 2;
			pLowResRenderTarget->DrawBitmap(
				GPUBitmaps[toId(EGPUBitmap::Explode)].Get(),
				D2D1::RectF(160, 325, 160 + bitmapSize.width, 325 + bitmapSize.height),
				1.0f, // Opacity (1.0f = fully opaque)
				D2D1_BITMAP_INTERPOLATION_MODE_NEAREST_NEIGHBOR,
				nullptr // Source rectangle (nullptr to use entire bitmap)
			);
		}
		std::wstring wPoints = std::to_wstring(scene->points);

		textFormats[toId(TextFormat::SMALL)]->SetTextAlignment(DWRITE_TEXT_ALIGNMENT_TRAILING);
		pRenderTarget->DrawText(
			wPoints.c_str(),    // Text to render
			wcslen(wPoints.c_str()),
			textFormats[toId(TextFormat::SMALL)],            // Text format
			D2D1::RectF(0, 335, 345, 360), // Layout rectangle
			brushes["white"]
		);

		if (!scene->qLeapCd) {
			D2D1_SIZE_F bitmapSize = GPUBitmaps[toId(EGPUBitmap::QLeap)]->GetSize();
			bitmapSize.height *= 2;
			bitmapSize.width *= 2;
			pLowResRenderTarget->DrawBitmap(
				GPUBitmaps[toId(EGPUBitmap::QLeap)].Get(),
				D2D1::RectF(15, 325, 15 + bitmapSize.width, 325 + bitmapSize.height),
				1.0f, // Opacity (1.0f = fully opaque)
				D2D1_BITMAP_INTERPOLATION_MODE_NEAREST_NEIGHBOR,
				nullptr // Source rectangle (nullptr to use entire bitmap)
			);
		}
		drawMinimap();

	}

	void drawBorder() {
		D2D1_RECT_F unitSquare = D2D1::RectF(
			0.f, 0.f,
			scene->ScrWf(), scene->ScrHf()
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

	/* output: perspective correction coefficient */
	float getPixelDir(int x, OUT XMVECTOR& dir) {
		dir = { imgPlaneDist, static_cast<float>(x) - scene->HscrW(), 0.f, 0.f };
		dir = XMVector3Normalize(dir);
		float screenSpaceAngle = std::atan2f(XMVectorGetY(dir), XMVectorGetX(dir));
		float angle = std::atan2f(XMVectorGetY(scene->getDir()), XMVectorGetX(scene->getDir()));
		XMVECTOR worldFromScreen = XMQuaternionRotationAxis({ 0,0,1,0 }, angle);
		dir = XMVector3Rotate(dir, worldFromScreen);

		return std::abs(cos(screenSpaceAngle));
	}

	void updateImgPlaneDist() {
		float ratio = std::tan(scene->getFov() / 2.f); // todo use cotan
		//v     cos  = sin			   / tan
		imgPlaneDist = scene->HscrWf() / ratio;
	}

	void wmResize(HWND hwnd) {
		if (pRenderTarget)
		{
			RECT rc;
			GetClientRect(hwnd, &rc);
			pRenderTarget->Resize(D2D1::SizeU(rc.right - rc.left, rc.bottom - rc.top));
		}
	}

	void loadGPUBitmap(const std::wstring& filePath, EGPUBitmap eBitmap) {
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
			&GPUBitmaps[toId(eBitmap)]
		);
		checkFailed(hr, hWnd, "createBitmapFromWicBitmap failed");
	}

	float intersect(const XMVECTOR& origin, const XMVECTOR& dir) {
		BoundingBox b{ XMFLOAT3{0,0,0}, XMFLOAT3{0.5,0.5,0} };
		float dist = FLT_MAX;
		float bestDist = FLT_MAX;

		for (int y = 0; y < scene->height; ++y) {
			for (int x = 0; x < scene->width; ++x) {
				if (scene->get(x, y) != '#') { continue; }
				b.Center = XMFLOAT3{ static_cast<float>(x) + 0.5f,static_cast<float>(y) + 0.5f,0 };
				bool intersects = b.Intersects(origin, dir, OUT dist);
				if (intersects) {
					bestDist = std::min(bestDist, dist);
				}

			}

		}
		return bestDist;
	}

	void initDrawBuffer(const std::wstring& filePath) {
		// CPU Side: 
		ComPtr<IWICBitmapDecoder> decoder;
		HRESULT hr = pWICFactory->CreateDecoderFromFilename(filePath.c_str(), nullptr, GENERIC_READ, WICDecodeMetadataCacheOnDemand, &decoder);
		checkFailed(hr, hWnd, "Failed to load image");

		// Get the first frame of the image
		ComPtr<IWICBitmapFrameDecode> frame;
		hr = decoder->GetFrame(0, &frame);
		checkFailed(hr, hWnd, "failed to get image frame");

		UINT width, height;
		frame->GetSize(&width, &height);

		ComPtr<IWICFormatConverter> pConverter;
		hr = pWICFactory->CreateFormatConverter(&pConverter);
		checkFailed(hr, hWnd, "failed to create format converter\n");

		hr = pConverter->Initialize(frame.Get(), GUID_WICPixelFormat32bppBGRA, WICBitmapDitherTypeNone, nullptr, 0, WICBitmapPaletteTypeCustom);
		checkFailed(hr, hWnd, "failed to init format converter");

		// Allocate memory for pixel data
		floorCPUTex = CPUBitmap(width, height, 4, {});
		floorCPUTex.data.resize(width * height * 4);

		// Copy pixels to the vector
		hr = pConverter->CopyPixels(nullptr, width * 4, static_cast<UINT>(floorCPUTex.data.size()), floorCPUTex.data.data());
		checkFailed(hr, hWnd, "failed to copy pixels");

		// GPU Side:
		D2D1_SIZE_U size = { scene->ScrW(), scene->ScrH() };
		D2D1_BITMAP_PROPERTIES props = {
			{ DXGI_FORMAT_B8G8R8A8_UNORM, D2D1_ALPHA_MODE_PREMULTIPLIED }, // BGRA format required for Direct2D
			96.0f, 96.0f // DPI
		};

		drawBuffer = std::vector<uint32_t>(scene->ScrW() * scene->ScrH(), 0x000000FF);
		pLowResRenderTarget->CreateBitmap(size, nullptr, 0, &props, &pFloorGPUBitmap);

	}

	void updateDrawBuffer() {
		// Sky

		float t_horizon = scene->zDir / scene->minZ;
		int horizon = scene->HscrHf() + int(t_horizon * scene->HscrHf()) -1; //< from -1 to (scene->ScrH - 1)
		//horizon = std::clamp(horizon, 0, scene->ScrH() - 1); //< todo delete?
		int topBandHeight = 0.3f * scene->ScrHf();
		int r_top = 200;
		int g_top = 150;
		int b_top = 150;
		int r_min = 50;
		int g_min = 25;
		int b_min = 25;
		// Compute the geometric factor f so that (h0 - f) / (1 - f) == horizon.
		// Derived from:
		//   Sum S = (topBandHeight - f)/(1 - f)  must equal horizon.

		float f = (scene->ScrHf() - topBandHeight) / (scene->ScrHf() - 1.f);

		// Compute the (non-integer) number of bands so that last band = 1 pixel:
		float Nf = 1 + log(1.0f / topBandHeight) / log(f);
		int N = std::ceil(Nf);

		int yPos = horizon;
		int bandHeight;
		float bandHeightF;
		for (int i = N; i >= 0 && yPos >= 0; --i)
		{
			// Compute this band's height (using the geometric progression)
			bandHeightF = topBandHeight * std::pow(f, i);
			bandHeight = std::max(1, int(std::round(bandHeightF)));

			// Compute color interpolation factor (0 at top, 1 at bottom)
			float t_r = (N > 1) ? float(i) / (N - 1) : 0.f;
			float t_g = (N > 1) ? float(i) / (N - 1) : 0.f;
			uint8_t r = uint8_t(std::round(r_top + t_r * (r_min - r_top)));
			uint8_t g = uint8_t(std::round(g_top + t_g * (g_min - g_top)));
			uint8_t b = uint8_t(std::round(b_top + t_g * (b_min - b_top)));
			uint32_t color = (0xFF << 24) | (r << 16) | (g << 8) | b;

			// Draw the horizontal band.
			for (int y = int(yPos); y >= 0; --y)
			{
				for (int x = 0; x < scene->ScrW(); ++x)
				{
					drawBuffer[y * scene->ScrW() + x] = color;
				}
			}
			yPos -= bandHeight;
		}


		//v Floor:
		float bottom = std::sin(scene->getVfov() / 2.f); //< todo move to scene
		float horTan = std::tan(scene->getFov() / 2.f); //< horizontal tan
		float _x = 0.f;
		float _y = 0.f;

		for (int y = horizon + 1; y < scene->ScrH(); ++y) {
			float y_t = (y - horizon) / (scene->ScrHf() - horizon - 1.f);

			float rayZ = y_t * bottom;
			float d = scene->camHeight / rayZ; //< distance from scene->position to ray hit
			//v   sin      = tan  * cos
			float horWidth = horTan * d; //< half horizontal width (how many units we can see horizontally to the left of center in this scanline)
			for (int x = 0; x < scene->ScrW(); ++x) {
				float x_t = float(scene->HscrWf() - x) / scene->HscrWf(); // from 1 to -1
				//float screenAngle = (scene->getFov() / 2.f) * x_t; // I think I see a very slight perspective warp at the left/right edges with this one
				float screenAngle = std::atan2f(horWidth * x_t, d);
				float angle = scene->getAngle() + screenAngle;

				float perspCorrect = d / std::cosf(screenAngle);

				_x = std::cosf(angle) * perspCorrect;
				_y = std::sinf(angle) * perspCorrect;

				// translate
				_y += XMVectorGetY(scene->position);
				_x += XMVectorGetX(scene->position);

				//spdlog::info("x: {}, y: {}, _x: {}, _y: {}\n",x, y, _x, _y);
				uint32_t sample = scene->sampleFloor(_x, _y);
				drawBuffer[y * scene->ScrW() + x] = sample;
			}

		}
	}

	//void oldFloor() {
	//	const UINT& texW = floorCPUTex.width;
	//	const UINT& texH = floorCPUTex.height;

	//	float _x = 0.f;
	//	float _y = 0.f;
	//	float t_horizon = scene->zDir / scene->minZ;
	//	int horizon = scene->HSCR + t_horizon * scene->HSCR;
	//	horizon = std::clamp(horizon, 0, scene->SCR_WIDTH - 1);
	//	float scale = 150;
	//	XMVECTOR pos = scene->position * 10; // adjust scale for believable movement speed

	//	//v turned out we don't need these?
	//	//float zStart = scene->HSCR_F * std::abs(scene->zDir) + 1;
	//	//float zEnd = scene->HSCR_F;
	//	const float& cos = XMVectorGetX(scene->getDir());
	//	const float& sin = XMVectorGetY(scene->getDir());

	//	float z = 1;
	//	for (int y = horizon; y < scene->SCR_WIDTH; ++y) {

	//		//v turned out we don't need these?
	//		//float t = float(y - horizon) / float(scene->SCR_WIDTH - horizon);
	//		//float z = zStart + t * (zEnd - zStart);

	//		//v `-horizon` makes sure texture does not appear to move when horizon changes. Not sure why `HSCR_F` worked so well 
	//		//float _y = (y - horizon + scene->HSCR_F) / z;
	//		//_y *= scale;
	//		//_y -= XMVectorGetY(pos);
	//		//_y = std::abs(fmod(_y, texH));

	//		for (int x = 0; x < scene->SCR_WIDTH; ++x) {
	//			_y = ((scene->SCR_WIDTH_F - x) * cos - (x)*sin) / z;
	//			_y *= scale;
	//			_y -= XMVectorGetY(pos);
	//			_y = std::abs(fmod(_y, texH));

	//			//_x = (scene->HSCR - x) / z;
	//			_x = ((scene->SCR_WIDTH_F - x) * sin + (x)*cos) / z;
	//			_x *= scale;
	//			_x -= XMVectorGetX(pos);
	//			_x = std::abs(fmod(_x, texW));
	//			size_t pixId = floorCPUTex.getPixel(toId(_x), toId(_y));
	//			uint8_t b = floorCPUTex.data[pixId];
	//			uint8_t g = floorCPUTex.data[pixId + 1];
	//			uint8_t r = floorCPUTex.data[pixId + 2];
	//			uint8_t a = floorCPUTex.data[pixId + 3];
	//			drawBuffer[y * scene->SCR_WIDTH + x] = (0xFF << 24) | (r << 16) | (g << 8) | b;
	//		}
	//		++z;
	//	}

	//}
private:
	void checkFailed(HRESULT hr, HWND hwnd, const std::string& message) {
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
	ID2D1SolidColorBrush* brush = nullptr; //< multi-purpose brush to be used with .SetColor();
	std::array<IDWriteTextFormat*, static_cast<size_t>(TextFormat::size)> textFormats;
	using DrawFunction = void(GJRenderer::*)();
	std::array<DrawFunction, static_cast<size_t>(State::size)> drawCallTable;

	std::array<ComPtr<ID2D1Bitmap>, toId(EGPUBitmap::size)> GPUBitmaps;
	std::array<CPUBitmap, toId(ECPUBitmap::size)> CPUBitmaps;
	ComPtr<ID2D1Bitmap> pFloorGPUBitmap; // in initDrawBuffer
	std::vector<uint32_t> drawBuffer;  // in initDrawBuffer
	CPUBitmap floorCPUTex;
	float imgPlaneDist;
	float MAXVIEWDIST = 90.f;

};

