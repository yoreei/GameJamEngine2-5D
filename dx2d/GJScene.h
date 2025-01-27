#pragma once
#include <cstdint>
#include <vector>
#include <memory>
#include <bitset>
#include <random>

#include "Animation.h"
#include "3d/common3d.h"





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
