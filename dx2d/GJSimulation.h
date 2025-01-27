#pragma once
#include <chrono>
#include <bitset>
#include <stdexcept>
#include <functional>
#include <fstream>

#include "GJScene.h"
#include "irrKlang.h"
#include "GJGlobals.h"

using DeltaTime = std::chrono::duration<double>;
class GJSimulation {
public:
	GJSimulation() {
		kbCallTable[static_cast<size_t>(State::INGAME)] = &GJSimulation::kbHandleINGAME;
		kbCallTable[static_cast<size_t>(State::PREGAME)] = &GJSimulation::kbHandlePREGAME;
		kbCallTable[static_cast<size_t>(State::WIN)] = &GJSimulation::kbHandleWIN;
		kbCallTable[static_cast<size_t>(State::LOSS)] = &GJSimulation::kbHandleLOSS;
		kbCallTable[static_cast<size_t>(State::PAUSED)] = &GJSimulation::kbHandlePAUSED;
		kbCallTable[static_cast<size_t>(State::MAINMENU)] = &GJSimulation::kbHandleMAINMENU;
		if (kbCallTable.size() != 6) {
			MessageBox(NULL, L"update kbCallTable.", L"Error", MB_OK);
			throw std::runtime_error("");
		}

		lastExplode = getTime();

		audioEngine = irrklang::createIrrKlangDevice();
		if (!audioEngine) {
			MessageBox(NULL, L"Could not initialize audio engine.", L"Error", MB_OK);
		}

		audioEngine->stopAllSounds();
		auto music = audioEngine->play2D("assets/ingame.mp3", true, false, true);
		if (!music) {
			MessageBox(NULL, L"Could not play ingame.mp3", L"Error", MB_OK);
		}
		enterMAINMENU();
	}

	const GJScene* getScene() {
		return &scene;
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
				loadNewGame();
			}
		}

	}
	void kbHandleINGAME(WPARAM wParam, bool keyDown) {
		if (keyDown) {
			if (wParam == VK_ESCAPE) {
				enterPAUSED();
				return;
			}

			if (wParam == 'Q') {
				castExplode(0);
			}
			else if (wParam == 'W') {
				castExplode(1);
			}
			else if (wParam == 'S') {
				castExplode(2);
			}
			else if (wParam == 'A') {
				castExplode(3);
			}
			else if (wParam == VK_SPACE) {
				castQLeap();
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
		if (!scene.qLeapCd) {
			scene.qLeapCd = true;
			scene.qLeapActive = true;
			lastQLeap = GNow;
		}

	}
	void castExplode(size_t target) {
		if (scene.explodeCd) { return; }
		if (scene.points < 75) { return; }
		if (scene.entities[target].health <= 0) { return; }

		scene.points -= 75;
		lastExplode = GNow;
		scene.explodeCd = true;
		for (int i = 0; i < scene.entities.size(); ++i) {
			scene.entities[i].health = 1;
			scene.entities[i].setPos(scene.entities[target].getPos());
		}
		static vec3 NW = unit_vector(vec3{ -1, -1, 0 });
		static vec3 NE = unit_vector(vec3{ 1, -1, 0 });
		static vec3 SE = unit_vector(vec3{ 1, 1, 0 });
		static vec3 SW = unit_vector(vec3{ -1, 1, 0 });
		scene.entities[0].momentum = NW; // Q
		scene.entities[1].momentum = NE; // W
		scene.entities[2].momentum = SE; // S
		scene.entities[3].momentum = SW; // A

	}

	void enterMAINMENU() {
		scene.state = State::MAINMENU;

		//audioEngine->stopAllSounds();
		//auto music = audioEngine->play2D("assets/mainmenu.mp3", true, false, true);
		//if (!music) {
		//	MessageBox(NULL, L"Could not play mainmenu.mp3", L"Error", MB_OK);
		//}
	}

	void enterWIN() {
		scene.state = State::WIN;
		//audioEngine->stopAllSounds();
		//auto music = audioEngine->play2D("assets/win.mp3", true, false, true);
		//if (!music) {
		//	MessageBox(NULL, L"Could not play win.mp3", L"Error", MB_OK);
		//}
	}

	void enterLOSS() {
		scene.state = State::LOSS;
	}

	void enterPAUSED() {
		scene.state = State::PAUSED;

		//audioEngine->stopAllSounds();
		//auto music = audioEngine->play2D("assets/mainmenu.mp3", true, false, true);
		//if (!music) {
		//	MessageBox(NULL, L"Could not play mainmenu.mp3", L"Error", MB_OK);
		//}

	}

	void enterPREGAME() {
		scene.state = State::PREGAME;
	}

	void enterINGAME() {
		scene.state = State::INGAME;

	}

	void tick(std::chrono::duration<double, std::milli> delta) {
		if (scene.state != State::INGAME) {
			return;
		}
		tickEvents(delta);
		tickStats(delta);
		tickMovement(delta);
		tickCollision(delta);
		++tickCounter;
	}

	void tickEvents(std::chrono::duration<double, std::milli> delta) {

		if (eventQueue.size() == 0) { return; }
		auto& tup = eventQueue.back();
		auto& tupDelta = std::get<DeltaTime>(tup);
		if (GNow - GGameStart > tupDelta) {
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

	void tickStats(std::chrono::duration<double, std::milli> delta) {
		if (scene.explodeCd && GNow - lastExplode > explodeCdSeconds) {
			scene.explodeCd = false;
		}
		if (GNow - lastPointsTime > pointsCdSeconds) {
			lastPointsTime = GNow;
			scene.points += 100;

		}

		if (scene.qLeapActive && GNow - lastQLeap > qLeapDuration) {
			scene.qLeapActive = false;

		}
		if (scene.qLeapCd && GNow - lastQLeap > qLeapCdSeconds) {
			scene.qLeapCd = false;
		}
	}

	void wrapAround(Entity& e, float padding = 0) {
		vec3 pos = e.getPos();
		if (pos.x() < 0.f - padding) { e.setX(359.f + padding); }
		else if (pos.x() > 360.f + padding) { e.setX(1.f - padding); }

		if (pos.y() < 0.f - padding) { e.setY(359.f + padding); }
		else if (pos.y() > 360.f + padding) { e.setY(1.f - padding); }

	}

	void reflectEntity(Entity& e, float padding = 0) {
		vec3 pos = e.getPos();
		if (pos.x() < 0.f - padding) { e.momentum = reflect(e.momentum, vec3{ 1, 0, 0 }); } // left
		else if (pos.x() > 360.f + padding) { e.momentum = reflect(e.momentum, vec3{ -1, 0, 0 }); } // right

		if (pos.y() < 0.f - padding) { e.momentum = reflect(e.momentum, vec3{ 0, 1, 0 }); } // top
		else if (pos.y() > 360.f + padding) { e.momentum = reflect(e.momentum, vec3{ 0, -1, 0 }); } // bottom

	}

	void tickMovement(std::chrono::duration<double, std::milli> delta) {
		if (!scene.qLeapActive) {
			for (int i = 0; i < scene.entities.size(); ++i) {
				Entity& e = scene.entities[i];
				if (e.health <= 0) {
					continue;
				}
				e.moveBy(e.momentum * globalSpeedUp);

				reflectEntity(e);
			}
		}
		for (int i = 0; i < scene.obstacles.size(); ++i) {
			Entity& e = scene.obstacles[i];
			if (e.health <= 0) {
				continue;
			}
			e.moveBy(e.momentum * globalSpeedUp);

			wrapAround(e, 15.f);

		}
	}

	void tickCollision(std::chrono::duration<double, std::milli> delta) {
		for (int i = 0; i < scene.obstacles.size(); ++i) {
			Entity& o1 = scene.obstacles[i];
			if (o1.health <= 0) {
				continue;
			}
			for (int j = i + 1; j < scene.obstacles.size(); ++j) {
				Entity& o2 = scene.obstacles[j];
				if (o2.health <= 0) {
					continue;
				}
				if (isCollision(o1, o2)) {
					ricochet(o1, o2);
				}

			}

			if (!scene.qLeapActive) {
				for (int j = 0; j < scene.entities.size(); ++j) {
					Entity& e = scene.entities[j];
					if (e.health <= 0) {
						continue;
					}
					if (isCollision(o1, e, cheatFactor)) {
						killEntity(j);
					}
				}
			}

		}
	}

	void ricochet(Entity& o1, Entity& o2) {
		//o1
		vec3 away = o1.getPos() - o2.getPos();
		// size is equivalent to weight
		o1.momentum = unit_vector(away + (o1.momentum * o1.size));

		away = -away;
		o2.momentum = unit_vector(away + (o2.momentum * o2.size));
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
		scene.hiScore = prevHiScore;
		if (scene.points > prevHiScore) {
			enterWIN();
			writeHiScore(scene.points);
		}
		else {
			enterLOSS();
		}
	}

	void handleInput(WPARAM wParam, bool keyDown) {

		(this->*kbCallTable[static_cast<size_t>(scene.state)])(wParam, keyDown);

	}

	void loadNewGame() {

		eventQueue = {
			{ DeltaTime{60.f}, [this]() { this->event6(); } },
			{ DeltaTime{38.f}, [this]() { this->event5(); } },
			{ DeltaTime{26.f}, [this]() { this->event4(); } },
			{ DeltaTime{18.f}, [this]() { this->event3(); } },
			{ DeltaTime{12.f}, [this]() { this->event2(); } },
			{ DeltaTime{6.f}, [this]() { this->event1(); } },
			{ DeltaTime{0.f}, [this]() { this->event0(); } },
		};

		scene = GJScene();
		scene.resetEntities();
		scene.resetObstacles();
		GGameStart = getTime();
		scene.points = 100;
		float globalSizeFactor = 0.f;
		enterINGAME();
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
			double pos = static_cast<double>(posDistr(GEN));
			if (side == 0) { // top
				vec3 newPos{ pos, 0, 0 };
				obstacles[id].setPos(newPos);
			}
			else if (side == 1) { // right
				vec3 newPos{ 380, pos, 0 };
				obstacles[id].setPos(newPos);
			}
			else if (side == 2) { // bottom
				vec3 newPos{ pos, 380.f, 0 };
				obstacles[id].setPos(newPos);
			}
			else { // left
				vec3 newPos{ 0, pos, 0 };
				obstacles[id].setPos(newPos);
			}

			unique = true;
			for (int i = 0; i < obstacles.size(); ++i) {
				Entity& o1 = scene.obstacles[i];
				if (o1.health <= 0 || i == id) {
					continue;
				}
				if (isCollision(obstacles[id], obstacles[i])) {
					unique = false;
					break;
				}
			}
		}

		vec3 center{ 180.f, 180.f, 0.f };
		obstacles[id].momentum = center - obstacles[id].getPos();
		obstacles[id].momentum = unit_vector(obstacles[id].momentum);

		static std::uniform_int_distribution<uint16_t> sizeDist(7, 11);
		obstacles[id].size = sizeDist(GEN) + globalSizeFactor;

	}

	bool isCollision(const Entity& e1, const Entity& e2, float cheatParam = 0.f) {
		vec3 delta = e1.getPos() - e2.getPos();
		//vec3 delta = e1.position - e2.position;
		float collisionLim = e1.size + e2.size - cheatParam;
		if (std::abs(delta.e[0]) < collisionLim) {
			if (std::abs(delta.e[1]) < collisionLim) {
				return true;
			}
		}
		return false;
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

	void writeHiScore(uint64_t score) {
		try {
			std::ofstream ofs(hiScoreFile, std::ofstream::out);
			ofs << std::to_string(scene.points);
			ofs.close();
		}
		catch (std::exception e) {
			MessageBox(NULL, L"Error writing HiScore to file", L"Error", MB_OK);
		}
	}

private:
	std::string hiScoreFile = "hiScore.txt";
	float cheatFactor = 1.f;
	float globalSpeedUp = 1.5f;
	float globalSizeFactor = 0.f;
	uint64_t tickCounter = 0;
	std::vector<std::tuple<DeltaTime, std::function<void()>>> eventQueue;
	std::chrono::duration<double> explodeCdSeconds{ 1.f };
	std::chrono::duration<double> qLeapCdSeconds{ 12.f };
	std::chrono::duration<double> qLeapDuration{ 2.f };
	std::chrono::duration<double> pointsCdSeconds{ 2.f };
	std::chrono::time_point<std::chrono::high_resolution_clock> lastExplode;
	std::chrono::time_point<std::chrono::high_resolution_clock> lastQLeap;
	std::chrono::time_point<std::chrono::high_resolution_clock> lastPointsTime;
	GJScene scene;
	std::bitset<254> kbMap;
	irrklang::ISoundEngine* audioEngine;
	using KeybindHandler = void(GJSimulation::*)(WPARAM, bool);
	std::array<KeybindHandler, static_cast<size_t>(State::size)> kbCallTable;

};
