#pragma once
#include <chrono>
#include <bitset>
#include <stdexcept>
#include <functional>
#include <fstream>
#include <numbers>
#include <filesystem>

#include <DirectXMath.h>

#include "GJScene.h"
#include "irrklang/irrKlang.h"
#include "GJGlobals.h"
#include "GJRenderer.h"

using namespace DirectX;

/// a.k.a the game engine. handles input, sound, and simulation. Delegates rendering. 
class GameEngine {
public:
	GameEngine(HWND hWnd, const std::string& fileName) :
		renderer{ hWnd, &gameplayState, &rendererScene }
	{
		GGameTime = Seconds{ 0 };
		enterMAINMENU();

		// loadMapFile:
		[this, &fileName]() -> void {
			// Load map into simulationScene, then advance rendererScene to simulationScene
			gameplayState.fileName = fileName;
			std::string line;
			std::stringstream ss;

			namespace fs = std::filesystem;
			fs::path p{ fileName };

			if (!fs::exists(p)) {
				throw std::runtime_error(
					"Cannot open map file '" + fileName + "': file does not exist");
			}
			if (!fs::is_regular_file(p)) {
				throw std::runtime_error(
					"Cannot open map file '" + fileName + "': not a regular file");
			}

			std::ifstream f;
			f.open(fileName, std::ios::in | std::ios::binary);
			if (!f.is_open()) {
				int err = errno;
				throw std::system_error(err, std::generic_category(), "Failed to open map file " + fileName);
			}

			gameplayState.width = gameplayState.height = 0;
			while (f.peek() != EOF) {
				++gameplayState.height;
				getline(f, line);
				// remove any '\r' left by Windows line endings
				if (!line.empty() && line.back() == '\r') {
					line.pop_back();
				}

				size_t findPlayer = line.find('@');
				if (findPlayer != std::string::npos) {
					scene.camera.position = XMVECTOR{ static_cast<float>(findPlayer) + 0.5f, static_cast<float>(gameplayState.height) + 0.5f, 0, 0 };
					line[findPlayer] = ' ';
				}
				gameplayState.width = std::max(gameplayState.width, line.size());
				ss << line;
			}
			gameplayState.map = ss.str();

			advanceRendererToSimulation();
			}();

		// loadGameplay
		eventQueue = {
			// { DeltaTime{60.f}, [this]() { this->event6(); } },
			//{ DeltaTime{38.f}, [this]() { this->event5(); } },
			//{ DeltaTime{26.f}, [this]() { this->event4(); } },
			//{ DeltaTime{18.f}, [this]() { this->event3(); } },
			//{ DeltaTime{12.f}, [this]() { this->event2(); } },
			//{ DeltaTime{6.f}, [this]() { this->event1(); } },
			//{ DeltaTime{0.f}, [this]() { this->event0(); } },
		};

		gameplayState.points = 100;

		// input handling
		kbCallTable[static_cast<size_t>(State::INGAME)] = &GameEngine::kbHandleINGAME;
		kbCallTable[static_cast<size_t>(State::PREGAME)] = &GameEngine::kbHandlePREGAME;
		kbCallTable[static_cast<size_t>(State::WIN)] = &GameEngine::kbHandleWIN;
		kbCallTable[static_cast<size_t>(State::LOSS)] = &GameEngine::kbHandleLOSS;
		kbCallTable[static_cast<size_t>(State::PAUSED)] = &GameEngine::kbHandlePAUSED;
		kbCallTable[static_cast<size_t>(State::MAINMENU)] = &GameEngine::kbHandleMAINMENU;
		if (kbCallTable.size() != 6) {
			MessageBox(NULL, L"update kbCallTable.", L"Error", MB_OK);
			throw std::runtime_error("");
		}


		// sound:
		audioEngine = irrklang::createIrrKlangDevice();
		if (!audioEngine) {
			MessageBox(NULL, L"Could not initialize audio engine.", L"Error", MB_OK);
		}

		// disabling sound for debug because annoying
		if (!DEBUG) {
			audioEngine->stopAllSounds();
			auto music = audioEngine->play2D("assets/ingame.mp3", true, false, true);
			if (!music) {
				MessageBox(NULL, L"Could not play ingame.mp3", L"Error", MB_OK);
			}
		}
	}

	void loadNewGame(const std::string& fileName) {
		// We can't use *this = {..} because that will create a temporary GameEngine and that is problematic:
		// The temporary's destructor will release resources whose handles will then be copied to *this. Goes both for:
		// 1. the winapi handles released via ComPtr
		// 2. the pointers to Scene and GameplayState that both Simulation and Renderer hold internally.
		// Finally, (Not a bug, but an inconvenience) if GameEngine has a large memory footprint, we might run out of memory when the temporary
		// gets created

		HWND hWndCopy = renderer.hWnd;
		const std::string fileNameCopy = fileName;
		std::destroy_at(this);
		std::construct_at(this, hWndCopy, fileNameCopy);
	}

	/// \param alpha [0..1] interpolation factor between simulation and renderer scenes
	void draw(float alpha) {
		interpolateRendererToSimulation(alpha);
		renderer.draw();
	}

	void kbHandlePREGAME(WPARAM wParam, bool keyDown) {
		if (keyDown) {
			switch (wParam) {
			case VK_ESCAPE:
			case 'Q':
			case 'W':
			case 'S':
			case 'A':
			case VK_SPACE:
			case VK_RETURN:
				enterINGAME();
			}
		}

	}
	void kbHandleINGAME(WPARAM wParam, bool keyDown) {
		if (keyDown) {
			if (wParam == VK_ESCAPE) {
				enterPAUSED();
				return;
			}

			auto& cam = scene.camera;
			if (wParam == VK_LEFT) {
				cam.setDirectionAngle(cam.getDirectionAngle() - 0.3f);
			}
			else if (wParam == VK_RIGHT) {
				cam.setDirectionAngle(cam.getDirectionAngle() + 0.3f);
			}
			else if (wParam == 'W') {
				cam.position = cam.position + scene.camera.getDirectionVector() * 0.3f;
			}
			else if (wParam == 'S') {
				cam.position = cam.position - cam.getDirectionVector() * 0.3f;
			}
			else if (wParam == 'Q') {
				cam.camHeight += 0.2f;
			}
			else if (wParam == 'E') {
				cam.camHeight -= 0.2f;
			}
			else if (wParam == VK_SPACE) {
				castQLeap();
			}
			else if (wParam == VK_UP) {
				cam.setPitch(cam.getPitch() + 0.1f);
			}
			else if (wParam == VK_DOWN) {
				cam.setPitch(cam.getPitch() - 0.1f);
			}


		}
	}

	void kbHandleWIN(WPARAM wParam, bool keyDown) {
		kbHandlePAUSED(wParam, keyDown);
	}

	void kbHandleLOSS(WPARAM wParam, bool keyDown) {
		kbHandlePAUSED(wParam, keyDown);
	}

	void kbHandlePAUSED(WPARAM wParam, bool keyDown) {
		if (keyDown) {
			if (wParam == VK_ESCAPE) {
				enterINGAME();
			}
			else if (wParam == 'R') {
				enterPREGAME();
			}

			else if (wParam == VK_BACK) {
				exit(0);
			}
		}
	}

	void kbHandleMAINMENU(WPARAM wParam, bool keyDown) {
		if (keyDown) {
			if (wParam == VK_RETURN) {
				enterPREGAME();
			}
			else if (wParam == VK_BACK) {
				exit(0);
			}
		}

	}

	void castQLeap() {
		if (!gameplayState.qLeapCd) {
			gameplayState.qLeapCd = true;
			gameplayState.qLeapActive = true;
			lastQLeap = GGameTime;
		}

	}

	void enterMAINMENU() {
		gameplayState.state = State::MAINMENU;

		//audioEngine->stopAllSounds();
		//auto music = audioEngine->play2D("assets/mainmenu.mp3", true, false, true);
		//if (!music) {
		//	MessageBox(NULL, L"Could not play mainmenu.mp3", L"Error", MB_OK);
		//}
	}

	void enterWIN() {
		gameplayState.state = State::WIN;
		//audioEngine->stopAllSounds();
		//auto music = audioEngine->play2D("assets/win.mp3", true, false, true);
		//if (!music) {
		//	MessageBox(NULL, L"Could not play win.mp3", L"Error", MB_OK);
		//}
	}

	void enterLOSS() {
		gameplayState.state = State::LOSS;
	}

	void enterPAUSED() {
		gameplayState.state = State::PAUSED;

		//audioEngine->stopAllSounds();
		//auto music = audioEngine->play2D("assets/mainmenu.mp3", true, false, true);
		//if (!music) {
		//	MessageBox(NULL, L"Could not play mainmenu.mp3", L"Error", MB_OK);
		//}

	}

	void enterPREGAME() {
		gameplayState.state = State::PREGAME;
	}

	void enterINGAME() {
		gameplayState.state = State::INGAME;

	}

	void tick(Seconds delta) {
		GEngineTime += delta;
		if (gameplayState.state == State::INGAME) {
			GGameTime += delta;
			advanceRendererToSimulation();

			tickEvents(delta);
			tickStats(delta);
			tickMovement(delta);
			tickCollision(delta);
		}

	}

	void tickEvents(Seconds UNUSED(delta)) {

		if (eventQueue.size() == 0) { return; }
		auto& tup = eventQueue.back();
		auto& tupDelta = std::get<Seconds>(tup);
		if (GGameTime > tupDelta) {
			std::get<1>(tup)();
			eventQueue.pop_back();
		}
	}

	void event0() {
		globalSpeedUp = 1.5f;
		for (int i = 0; i < 5; ++i) {
			initRandObstacle(i);
		}

	}
	void event1() {
		globalSpeedUp = 1.6f;
		for (int i = 5; i < 9; ++i) {
			initRandObstacle(i);
		}
	}
	void event2() {
		globalSpeedUp = 1.7f;
		for (int i = 10; i < 15; ++i) {
			initRandObstacle(i);
		}
	}
	void event3() {
		globalSpeedUp = 1.8f;
		for (int i = 15; i < 17; ++i) {
			initRandObstacle(i);
		}
	}
	void event4() {
		globalSpeedUp = 2.f;
		//increaseGlobalSize(1);
		for (int i = 17; i < 19; ++i) {
			initRandObstacle(i);
		}
	}
	void event5() {
		//increaseGlobalSize(1);
		for (int i = 23; i < scene.obstacles.size(); ++i) {
			initRandObstacle(i);
		}
	}
	void event6() {
		globalSpeedUp = 2.3f;
		//increaseGlobalSize(1);
		MessageBox(NULL, L"OMG You're Hardcore!", L"Error", MB_OK);
		for (int i = 0; i < scene.obstacles.size(); ++i) {
		}
	}

	void increaseGlobalSize(int add) {
		for (Entity& o : scene.obstacles) {
			if (o.health == 0) { continue; }
			o.size += add;
		}
		globalSizeFactor += add;
	}

	void tickStats(Seconds UNUSED(delta)) {
		if (GGameTime - lastPointsTime > pointsCdSeconds) {
			lastPointsTime = GGameTime;
			gameplayState.points += 100;
		}

		if (gameplayState.qLeapActive && GGameTime - lastQLeap > qLeapDuration) {
			gameplayState.qLeapActive = false;

		}
		if (gameplayState.qLeapCd && GGameTime - lastQLeap > qLeapCdSeconds) {
			gameplayState.qLeapCd = false;
		}
	}

	void killEntity(size_t id) {
		scene.entities[id].health = 0;
		for (const Entity& e : scene.entities) {
			if (e.health != 0) {
				return;
			}
		}
		// here all entities have health 0
		scene.entities[id].health = 1; //< so the player can see their entity on the loss screen
		endGame();
	}

	void endGame() {
		uint64_t prevHiScore = readHiScore();
		gameplayState.hiScore = prevHiScore;
		if (gameplayState.points > prevHiScore) {
			enterWIN();
			writeHiScore();
		}
		else {
			enterLOSS();
		}
	}

	void handleInput(WPARAM wParam, bool keyDown) {
		(this->*kbCallTable[static_cast<size_t>(gameplayState.state)])(wParam, keyDown);
	}

	void initRandObstacle(size_t id) {
		static std::mt19937 GEN(46);
		static std::uniform_int_distribution<int> posDistr(-20, 380);
		static std::uniform_int_distribution<int> sideDistr(0, 3);

		auto& obstacles = scene.obstacles;
		obstacles[id].health = 1;
		bool unique = false;
		while (!unique) {

			int side = sideDistr(GEN);
			float pos = toF(posDistr(GEN));
			if (side == 0) { // top
				XMVECTOR newPos{ pos, 0.f, 0.f };
				obstacles[id].position = newPos;
			}
			else if (side == 1) { // right
				XMVECTOR newPos{ 380, pos, 0 };
				obstacles[id].position = newPos;
			}
			else if (side == 2) { // bottom
				XMVECTOR newPos{ pos, 380.f, 0 };
				obstacles[id].position = newPos;
			}
			else { // left
				XMVECTOR newPos{ 0, pos, 0 };
				obstacles[id].position = newPos;
			}

			unique = true;
			for (int i = 0; i < obstacles.size(); ++i) {
				Entity& o1 = scene.obstacles[i];
				if (o1.health <= 0 || i == id) {
					continue;
				}
				if (obstacles[id].collides(obstacles[i])) {
					unique = false;
					break;
				}
			}
		}

		XMVECTOR center{ 180.f, 180.f, 0.f, 0.f };
		obstacles[id].momentum = center - obstacles[id].position;
		obstacles[id].momentum = XMVector3Normalize(obstacles[id].momentum);

		static std::uniform_int_distribution<uint16_t> sizeDist(7, 11);
		obstacles[id].size = sizeDist(GEN) + globalSizeFactor;

	}


	uint64_t readHiScore() {
		std::string line;
		std::ifstream myfile(hiScoreFile);
		if (myfile.is_open())
		{
			getline(myfile, line);
			myfile.close();
		}
		else {
			MessageBox(NULL, L"Could not read hiScore file", L"Error", MB_OK);
			return 0;
		}
		uint64_t toInt;
		try {
			toInt = std::stoll(line);
		}
		catch (std::exception e) {
			MessageBox(NULL, L"Could not interpret hiScore data", L"Error", MB_OK);
			return 0;
		}
		return toInt;
	}

	void writeHiScore() {
		try {
			std::ofstream ofs(hiScoreFile, std::ofstream::out);
			ofs << std::to_string(gameplayState.points);
			ofs.close();
		}
		catch (std::exception e) {
			MessageBox(NULL, L"Error writing HiScore to file", L"Error", MB_OK);
		}
	}

	/// advance
	void interpolateRendererToSimulation(float alpha) {
		rendererScene.interpolate(rendererScene, alpha);
	}

	void advanceRendererToSimulation() {
		rendererScene = scene; // full copy
	}

	void tickMovement(Seconds UNUSED(delta)) {
		//if (!scene.qLeapActive) {
		//	for (int i = 0; i < scene.entities.size(); ++i) {
		//		Entity& e = scene.entities[i];
		//		if (e.health <= 0) {
		//			continue;
		//		}
		//		e.moveBy(e.momentum * globalSpeedUp);

		//		reflectEntity(e);
		//	}
		//}
		//for (int i = 0; i < scene.obstacles.size(); ++i) {
		//	Entity& e = scene.obstacles[i];
		//	if (e.health <= 0) {
		//		continue;
		//	}
		//	e.moveBy(e.momentum * globalSpeedUp);

		//	wrapAround(e, 15.f);

		//}
	}

	void tickCollision(Seconds UNUSED(delta)) {
		//for (int i = 0; i < scene.obstacles.size(); ++i) {
		//	Entity& o1 = scene.obstacles[i];
		//	if (o1.health <= 0) {
		//		continue;
		//	}
		//	for (int j = i + 1; j < scene.obstacles.size(); ++j) {
		//		Entity& o2 = scene.obstacles[j];
		//		if (o2.health <= 0) {
		//			continue;
		//		}
		//		if (isCollision(o1, o2)) {
		//			ricochet(o1, o2);
		//		}

		//	}

		//	if (!scene.qLeapActive) {
		//		for (int j = 0; j < scene.entities.size(); ++j) {
		//			Entity& e = scene.entities[j];
		//			if (e.health <= 0) {
		//				continue;
		//			}
		//			if (isCollision(o1, e, cheatFactor)) {
		//				killEntity(j);
		//			}
		//		}
		//	}

		//}
	}

	void ricochet(Entity& o1, Entity& o2) {
		auto away = o1.position - o2.position;
		// size is equivalent to weight
		o1.momentum = XMVector3Normalize(away + (o1.momentum * o1.size));
		o1.momentum = XMVector3Normalize(o1.momentum);

		away = -away;
		o2.momentum = XMVector3Normalize(away + (o2.momentum * o2.size));
	}

	GJRenderer renderer;
private:
	float cheatFactor = 1.f;
	float globalSpeedUp = 1.5f;

	GameplayState gameplayState{};

	/// simulation scene
	GJScene scene{};
	GJScene rendererScene{};

	std::string hiScoreFile = "hiScore.txt";
	float globalSizeFactor = 0.f;
	std::vector<std::tuple<Seconds, std::function<void()>>> eventQueue;

	Seconds qLeapCdSeconds{ 12.f };
	Seconds qLeapDuration{ 2.f };
	Seconds pointsCdSeconds{ 2.f };

	Seconds lastQLeap;
	Seconds lastPointsTime;
	std::bitset<254> kbMap;
	irrklang::ISoundEngine* audioEngine;
	using KeybindHandler = void(GameEngine::*)(WPARAM, bool);
	std::array<KeybindHandler, static_cast<size_t>(State::size)> kbCallTable;
};

