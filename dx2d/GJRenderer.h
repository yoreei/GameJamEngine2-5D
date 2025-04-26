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

#include "common/cppUtil.h"
#include "GJScene.h"

#define FMT_UNICODE 0 //https://github.com/gabime/spdlog/issues/3251
#include "spdlog/spdlog.h"

using Microsoft::WRL::ComPtr;
constexpr float PI = std::numbers::pi;

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
	void init(HWND _hWnd, const GameplayState* _gameplayState) {
		gameplayState = _gameplayState;
		hWnd = _hWnd;

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
		{
			RECT clientRect; //< actual window size
			GetClientRect(hWnd, &clientRect);
			ASSERT_EXPR(clientRect.left == 0, "API promises this, but we do a sanity check");
			ASSERT_EXPR(clientRect.top == 0);

			hr = pFactory->CreateHwndRenderTarget(
				D2D1::RenderTargetProperties(),
				D2D1::HwndRenderTargetProperties(
					hWnd,
					D2D1::SizeU(clientRect.right, clientRect.bottom)
				),
				&pRenderTarget
			);
			checkFailed(hr, hWnd, "createHwndRenderTarget failed");

			constexpr int UPSCALE_FACTOR = 2;
			viewportWidth = clientRect.right / UPSCALE_FACTOR;
			viewportHeight = clientRect.bottom / UPSCALE_FACTOR;

			// todo delete these lines?
			//float maxSide = std::max(float(viewportWidth), float(viewportHeight));
			//float minSide = std::min(float(viewportWidth), float(viewportHeight));

			//float offsetF = (maxSide - minSide) / 2.f;
			//D2D1::Matrix3x2F translation = D2D1::Matrix3x2F::Translation(
			//	offsetF, // X offset
			//	0.f	     // Y offset
			//);
			//clientRect.right = minSide;
			//clientRect.bottom = minSide;

			//D2D1::Matrix3x2F transform = translation;

			//pRenderTarget->SetTransform(transform);
		}
		// Make viewport square:
		viewportWidth = std::min(viewportWidth, viewportHeight);
		viewportHeight = std::min(viewportWidth, viewportHeight);

		// Create a compatible render target for low-res drawing
		hr = pRenderTarget->CreateCompatibleRenderTarget(
			D2D1::SizeF(viewportWidth, viewportHeight),
			&pLowResRenderTarget
		);
		checkFailed(hr, hWnd, "createCompatibleRenderTarget failed");
		// Setup transformations
		//float scaleF = float(viewportWidth) / float(viewportHeight);
		//D2D1::Matrix3x2F scale = D2D1::Matrix3x2F::Scale(
		//	scaleF, scaleF,
		//	D2D1::Point2F(0.f, 0.f)
		//);

		// v todo delete this comment:
		//float offsetF = (float(viewportHeight) - scaleF * float(viewportWidth)) / 2.f;


		// Disable anti-aliasing for pixelated look
		pLowResRenderTarget->SetAntialiasMode(D2D1_ANTIALIAS_MODE_ALIASED);
		pRenderTarget->SetAntialiasMode(D2D1_ANTIALIAS_MODE_ALIASED);

		loadGPUBitmap(L"../assets/qLeap.png", EGPUBitmap::QLeap);
		loadGPUBitmap(L"../assets/explode.png", EGPUBitmap::Explode);
		initDrawBuffer(L"../assets/textures/4.png");
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

	void interpolate(const GJScene& scene1, const GJScene& scene2, float alpha) {
		scene.interpolate(scene1, scene2, alpha);
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

	void draw(Seconds delta) {
		pRenderTarget->BeginDraw();
		pLowResRenderTarget->BeginDraw();

		pRenderTarget->Clear(D2D1::ColorF(D2D1::ColorF::Black));
		pLowResRenderTarget->Clear(D2D1::ColorF(D2D1::ColorF::Black, 0.f));

		//drawBorder();
		(this->*drawCallTable[static_cast<size_t>(gameplayState->state)])();

		// draw low res rt to main rt
		pLowResRenderTarget->EndDraw();

		ID2D1Bitmap* pLowResBitmap = nullptr;
		HRESULT hr = pLowResRenderTarget->GetBitmap(&pLowResBitmap);
		checkFailed(hr, hWnd, "getBitmap failed");

		// letterboxing:

		RECT clientRect; //< actual window size
		GetClientRect(hWnd, &clientRect);
		ASSERT_EXPR(clientRect.top == 0 && clientRect.left == 0, "API promises this, but let's sanity check");
		int clientW = clientRect.right;
		int clientH = clientRect.bottom;

		int squareSize = std::min(clientW, clientH);
		int offsetX = (clientW - squareSize) / 2;
		int offsetY = (clientH - squareSize) / 2;

		D2D1_RECT_F destRect = D2D1::RectF(
			float(offsetX),
			float(offsetY),
			float(offsetX + squareSize),
			float(offsetY + squareSize)
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
			assert(false); // this case has not been tested / implemented yet
			releaseResources();
			// warning viewportWidth is already manilated here:
			//init(hWnd, viewportWidth, viewportHeight, gameplayState);
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
		for (int y = 0; y < gameplayState->height; ++y) {
			pos.top = y * size;
			pos.bottom = pos.top + size;
			for (int x = 0; x < gameplayState->width; ++x) {
				const char& tile = gameplayState->getTile(x, y);
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
		XMVECTOR posV = size * XMVectorFloor(scene.camera.position);
		const float pos_x = XMVectorGetX(posV);
		const float pos_y = XMVectorGetY(posV);
		pos = D2D1::RectF(pos_x, pos_y, pos_x + size, pos_y + size);
		brush->SetColor(D2D1::ColorF{ 1.f, 0.7f, 0.7f, minimapAlpha });
		pLowResRenderTarget->FillRectangle(pos, brush);

		//v Draw LOS
		brush->SetColor(D2D1::ColorF{ 1.f, 1.f, 0.7, minimapAlpha });
		XMVECTOR endV = posV + scene.camera.getDirectionVector() * 20;
		const float end_x = XMVectorGetX(endV);
		const float end_y = XMVectorGetY(endV); //< '-' because screen-space is inverted
		pLowResRenderTarget->DrawLine(
			D2D1_POINT_2F(pos_x, pos_y),
			D2D1_POINT_2F(end_x, end_y),
			brush, 1.f);

	}

	float intersect(const XMVECTOR& origin, const XMVECTOR& dir) const {
		BoundingBox b{ XMFLOAT3{0,0,0}, XMFLOAT3{0.5,0.5,0} };
		float dist = FLT_MAX;
		float bestDist = FLT_MAX;

		for (int y = 0; y < gameplayState->height; ++y) {
			for (int x = 0; x < gameplayState->width; ++x) {
				if (gameplayState->getTile(x, y) != '#') { continue; }
				b.Center = XMFLOAT3{ static_cast<float>(x) + 0.5f,static_cast<float>(y) + 0.5f,0 };
				bool intersects = b.Intersects(origin, dir, OUT dist);
				if (intersects) {
					bestDist = std::min(bestDist, dist);
				}

			}

		}
		return bestDist;
	}
	//v ABGR
	uint32_t sampleFloor(float x, float y) const {
		uint8_t c = int(std::floor(x)) + int(std::floor(y));
		c %= 2;
		c *= 255;

		if (x >= gameplayState->width || y >= gameplayState->height || x <= 0 || y <= 0) {
			return (0xFF << 24) | (c << 16) | (c << 8) | (c / 2);
		}

		bool wall = XMVector4Less(XMVectorAbs(scene.camera.position - XMVECTOR{ x,y + 3.f,0.f,0.f }), XMVECTOR{ 0.5f,0.5f,0.5f,0.5f });
		if (gameplayState->getTile(size_t(x), size_t(y)) == '#')
		{
			return 0xFFAAAAFF;
		}

		bool less = XMVector4Less(XMVectorAbs(scene.camera.position - XMVECTOR{ x,y + 3.f,0.f,0.f }), XMVECTOR{ 0.5f,0.5f,0.5f,0.5f });
		if (less)
		{
			return 0xFFAAFFAA;
		}

		less = XMVector4Less(XMVectorAbs(scene.camera.position - XMVECTOR{ x,y,0.f,0.f }), XMVECTOR{ 0.5f,0.5f,0.5f,0.5f });
		if (less)
		{
			return 0xFFFFAAAA;
		}
		else {
			return (0xFF << 24) | (c << 16) | (c << 8) | (c);
		}

	}

	template<typename T = int>
	T HscrH() {
		return T(viewportHeight) / T(2);
	}

	template<typename T = int>
	T HscrW() {
		return T(viewportWidth) / T(2);
	}

	int getHorizon(float pitch) {
		return HscrH<float>() + int(-pitch * HscrH<float>()) - 1; //< from -1 to (scene->ScrH - 1)
	}

	//v height in world units
	void drawWall(float x, float dist, float height, float color) {
		const float& FOCAL_LENGTH = HscrH<float>();
		float pixHeight = (FOCAL_LENGTH * height) / dist;
		brush->SetColor(D2D1::ColorF(color, color, color, 0.5f));
		float pixBottom = getHorizon(scene.camera.pitch) + (FOCAL_LENGTH * (scene.camera.camHeight)) / dist;
		pLowResRenderTarget->DrawLine(D2D1_POINT_2F(x, pixBottom), D2D1_POINT_2F(x, pixBottom - pixHeight), brush, 1.f);
	}

	void drawWalls() {
		float dist;
		XMVECTOR dir;
		for (int x = 0; x < viewportWidth; ++x) {
			float fixPersp = getPixelDir(x, OUT dir);
			dist = intersect(scene.camera.position, dir);
			dist = std::clamp(dist, 0.f, MAXVIEWDIST);
			float color = -(dist / MAXVIEWDIST) + 1.f;
			drawWall(x, dist * fixPersp, 1.f, color);
		}

	}

	void drawScene() {
		// * mode7 & sky
		updateDrawBuffer();

		D2D1_RECT_U destRect = { 0, 0, viewportWidth, viewportHeight };
		pFloorGPUBitmap->CopyFromMemory(&destRect, drawBuffer.data(), viewportWidth * sizeof(uint32_t));

		D2D1_SIZE_F bitmapSize = pFloorGPUBitmap->GetSize();
		pLowResRenderTarget->DrawBitmap(
			pFloorGPUBitmap.Get(),
			D2D1::RectF(0, 0, bitmapSize.width, bitmapSize.height),
			1.0f, // Opacity (1.0f = fully opaque)
			D2D1_BITMAP_INTERPOLATION_MODE_NEAREST_NEIGHBOR,
			nullptr // Source rectangle (nullptr to use entire bitmap)
		);

		// * walls
		drawWalls();
	}

	void drawUI() {
		if (gameplayState->explodeCd) {
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
		std::wstring wPoints = std::to_wstring(gameplayState->points);

		textFormats[toId(TextFormat::SMALL)]->SetTextAlignment(DWRITE_TEXT_ALIGNMENT_TRAILING);
		pRenderTarget->DrawText(
			wPoints.c_str(),    // Text to render
			wcslen(wPoints.c_str()),
			textFormats[toId(TextFormat::SMALL)],            // Text format
			D2D1::RectF(0, 335, 345, 360), // Layout rectangle
			brushes["white"]
		);

		if (!gameplayState->qLeapCd) {
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
			float(viewportWidth), float(viewportHeight)
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
		std::wstring heading = std::format(L"{}\nHiScore: {}", text, gameplayState->hiScore);
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
		float imagePlaneDistance = scene.camera.getImagePlaneDistance(); // depends on field of view
		float pixelDirection = (float(x) - HscrW()) / viewportWidth; // -0.5 to 0.5 (because image plane has width 1) 
		dir = { imagePlaneDistance, pixelDirection, 0.f, 0.f };
		dir = XMVector3Normalize(dir);
		float screenSpaceAngle = std::atan2f(XMVectorGetY(dir), XMVectorGetX(dir));

		// todo use scene.camera.getDirectionAngle() ?
		float angle = std::atan2f(XMVectorGetY(scene.camera.getDirectionVector()), XMVectorGetX(scene.camera.getDirectionVector()));
		XMVECTOR worldFromScreen = XMQuaternionRotationAxis({ 0,0,1,0 }, angle);
		dir = XMVector3Rotate(dir, worldFromScreen);
		assert(DirectX::Internal::XMVector3IsUnit(dir));

		return std::abs(cos(screenSpaceAngle));
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
		D2D1_SIZE_U size = { viewportWidth, viewportHeight };
		D2D1_BITMAP_PROPERTIES props = {
			{ DXGI_FORMAT_B8G8R8A8_UNORM, D2D1_ALPHA_MODE_PREMULTIPLIED }, // BGRA format required for Direct2D
			96.0f, 96.0f // DPI
		};

		drawBuffer = std::vector<uint32_t>(viewportWidth * viewportHeight, 0x000000FF);
		pLowResRenderTarget->CreateBitmap(size, nullptr, 0, &props, &pFloorGPUBitmap);

	}

	void updateDrawBuffer() {
		// Sky

		//float t_horizon = scene.zDir / scene.minZ;
		//horizon = std::clamp(horizon, 0, viewportHeight - 1); //< todo delete?
		int topBandHeight = 0.3f * float(viewportHeight);
		int r_top = 200;
		int g_top = 150;
		int b_top = 150;
		int r_min = 50;
		int g_min = 25;
		int b_min = 25;
		// Compute the geometric factor f so that (h0 - f) / (1 - f) == horizon.
		// Derived from:
		//   Sum S = (topBandHeight - f)/(1 - f)  must equal horizon.

		float f = (float(viewportHeight) - topBandHeight) / (float(viewportHeight) - 1.f);

		// Compute the (non-integer) number of bands so that last band = 1 pixel:
		float Nf = 1 + log(1.0f / topBandHeight) / log(f);
		int N = std::ceil(Nf);

		int yPos = std::clamp(getHorizon(scene.camera.pitch), 0, viewportHeight - 1);
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
				for (int x = 0; x < viewportWidth; ++x)
				{
					drawBuffer[y * viewportWidth + x] = color;
				}
			}
			yPos -= bandHeight;
		}


		//v Floor:
		float horTan = std::tan(scene.camera.getFov() / 2.f); //< horizontal tan
		float screenDist = HscrH<float>() / scene.camera.getVfov();
		float _x = 0.f;
		float _y = 0.f;
		//float perspective = 0; // 0 to 1

		int y = std::clamp(getHorizon(scene.camera.pitch) + 1, 0, viewportHeight - 1);
		for (; y < viewportHeight; ++y) {
			float zAngle = std::atan2f(screenDist, y - getHorizon(scene.camera.pitch) - 1); // hor.angle of vision for pix y. 
			float y_d = scene.camera.camHeight * std::tanf(zAngle); // distance along y-plane that scanline meets floor-plane

			//v   opposite = tan(a) * adj
			float horWidth = horTan * y_d; //< half horizontal width (how many units we can see horizontally to the left of center in this scanline)
			for (int x = 0; x < viewportWidth; ++x) {
				float x_t = (x - HscrW<float>()) / HscrW<float>(); // -1 (left) to 1 (right)
				//float screenAngle = (scene.getFov() / 2.f) * x_t; // results in slight perspective warp
				float screenAngle = std::atan2f(horWidth * x_t, y_d); //< hor.angle of vision for pix x. 
				float angle = scene.camera.getDirectionAngle() + screenAngle;

				//v   hyp		   = adj / cos(a)
				float perspCorrect = y_d / std::cosf(screenAngle);

				_x = std::cosf(angle) * perspCorrect;
				_y = std::sinf(angle) * perspCorrect;

				// translate
				_y += XMVectorGetY(scene.camera.position);
				_x += XMVectorGetX(scene.camera.position);

				//spdlog::info("x: {}, y: {}, _x: {}, _y: {}\n",x, y, _x, _y);
				uint32_t sample = sampleFloor(_x, _y);
				drawBuffer[y * viewportWidth + x] = sample;
			}

		}
	}

private:
	void checkFailed(HRESULT hr, HWND hwnd, const std::string& message) {
		assert(!FAILED(hr)); // break into debugger
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
	int viewportWidth; //< adjusted for upscaling
	int viewportHeight; //< adjusted for upscaling
	HWND hWnd;
	GJScene scene{};
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
	float MAXVIEWDIST = 90.f;
	const GameplayState* gameplayState;

};

