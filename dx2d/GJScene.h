#pragma once
#include <cstdint>
#include <vector>
#include <memory>
#include <bitset>
#include <random>
#include <fstream>
#include <cmath>
#include <numbers>

#include <DirectXMath.h>

#include "Animation.h"
#include "3d/common3d.h"

using namespace DirectX;

enum class EntityType {
	PlayerEntity1 = 0,
	PlayerEntity2,
	PlayerEntity3,
	PlayerEntity4,
	EnemyEntity1,
	EnemyEntity2,
	EnemyEntity3,
	EnemyEntity4
};

enum class Belonging {
	Player = 0,
	CPU
};

vec3 vec3op(const vec3& v, float(*op)(float)) {
	return vec3{
		op(v.e[0]),
		op(v.e[1]),
		op(v.e[2])
	};
}

struct Entity {
	uint16_t health = 1;
	uint16_t size = 2;
	vec3 momentum;
	vec3 getPos() const {
		vec3 val = vec3op(position, std::floor);
		return val;
	}
	void moveBy(const vec3& by) {
		position += by;
	}
	//const vec3& getRawPos() {
	//	return position;
	//}
	void setPos(const vec3& newPos) {
		position = newPos;
	}
	void setX(float x) {
		position.e[0] = x;
	}
	void setY(float y) {
		position.e[1] = y;
	}
private:
	vec3 position;
	//Belonging belonging = Belonging::CPU;
	//uint8_t factionId = 0;
	//uint32_t maxHealth = 1;
	//uint32_t power = 1;
	//uint32_t maxPower = 1;
	//std::bitset<32> powerUps;
	//std::vector<size_t> bitmapIds{};
	//Animation animation = Animation::Idle;
	//vec3 direction;
};

class Obstacle {

};

enum class State {
	MAINMENU = 0,
	PREGAME,
	INGAME,
	PAUSED,
	LOSS,
	WIN,
	size
};
struct GJScene {
	GJScene() {
		loadMap(mapFile);
	}
	void loadMap(const std::string& fileName) {
		std::string line;
		std::stringstream ss;
		std::ifstream f;
		f.open(fileName);
		if (!f.is_open()) {
			throw std::runtime_error("");
		}
		width = height = 0;
		while (f.peek() != EOF) {
			++height;
			getline(f, line);
			size_t findPlayer = line.find('@');
			if (findPlayer != std::string::npos) {
				position = XMVECTOR{ static_cast<float>(findPlayer) + 0.5f, static_cast<float>(height) + 0.5f, 0, 0 };
				line[findPlayer] = ' ';
			}
			width = std::max(width, line.size());
			ss << line;
		}
		map = ss.str();
	}


	const char& get(size_t x, size_t y) const {
		return map[y * width + x];
	}

	void resetEntities() {
		entities.fill(GJScene::emptyEntity);
		for (int i = 0; i < entities.size(); ++i) {
			entities[i].health = 1;
			entities[i].size = 2;
			entities[i].setPos(vec3{ 180.f, 240.f, 0.f });
		}
		//keybinds['Q'] = 0;
		//keybinds['A'] = 0;
		//keybinds['W'] = 0;
		//keybinds['S'] = 0;
	}

	void resetObstacles() {
		obstacles.fill(GJScene::emptyEntity);
	}


	//std::array<int, 255> keybinds;
	//Entity playerController;
	XMVECTOR lookAt{ 1,0,0,0 };
	XMVECTOR position{ 0,0,0,0 };
	float fov = 1.22f; //70deg
	//XMVECTOR viewAngleQuatL = XMQuaternionRotationAxis({0,0,1,0}, std::numbers::pi / 5);
	//XMVECTOR viewAngleQuatR = XMQuaternionInverse(viewAngleQuatL);
	std::string map;
	uint64_t width = 0;
	uint64_t height = 0;
	std::string mapFile = "assets/map1.txt";
	State state = State::MAINMENU;
	bool explodeCd = false;
	bool qLeapCd = false;
	bool qLeapActive = false;
	uint64_t hiScore = 0;
	uint64_t points = 100;
	//v entities move
	std::array<Entity, 4> entities{};
	std::array<Entity, 25> obstacles{};
	static inline Entity emptyEntity;
};
