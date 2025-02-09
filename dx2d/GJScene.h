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
		setDir({ 0,-1,0,0 });
		setFov(1.f); //70deg
		setResolution(960, 1.f);
		setAngle(1);
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

	void setFov(float newFov) {
		fov = newFov;
		vfov = fov;
		//maxZ = sin(vfov / 2.f);
		//minZ = -sin(vfov / 2.f);
		maxZ = 1;
		minZ = -1;
	}

	//std::array<int, 255> keybinds;
	//Entity playerController;
	//float fov = 1.22f; //70deg
	float maxZ;
	float minZ;
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

	/* 1 up, -1 down, 0 center */
	float zDir = 0;
	//v radians
	//float zAngle = 0;
	// only x and y are used
	XMVECTOR position{ 0,0,0,0 };
	const XMVECTOR& getDir() const {
		return dir;
	}
	// TODO DELETE
	//const XMVECTOR& getLeft() const {
	//	return left;
	//}
	void setDir(const XMVECTOR& newDir) {
		dir = newDir;
		//left = XMVector3Cross(dir, { 0.f,0.f,1.f,0.f }); // todo delete
	}
	float camHeight = 0.5f;
	//v radians
	const float getFov() const {
		return fov;
	}
	//v radians
	const float getVfov() const {
		return vfov;
	}

	float intersect(const XMVECTOR& origin, const XMVECTOR& dir) const {
		BoundingBox b{ XMFLOAT3{0,0,0}, XMFLOAT3{0.5,0.5,0} };
		float dist = FLT_MAX;
		float bestDist = FLT_MAX;

		for (int y = 0; y < height; ++y) {
			for (int x = 0; x < width; ++x) {
				if (get(x, y) != '#') { continue; }
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

		if (x >= width || y >= height || x <= 0 || y <= 0) {
			return (0xFF << 24) | (c << 16) | (c << 8) | (c / 2);
		}

		bool wall = XMVector4Less(XMVectorAbs(position - XMVECTOR{ x,y + 3.f,0.f,0.f }), XMVECTOR{0.5f,0.5f,0.5f,0.5f});
		if (get(size_t(x), size_t(y)) == '#')
		{
			return 0xFFAAAAFF;
		}

		bool less = XMVector4Less(XMVectorAbs(position - XMVECTOR{ x,y + 3.f,0.f,0.f }), XMVECTOR{0.5f,0.5f,0.5f,0.5f});
		if (less)
		{
			return 0xFFAAFFAA;
		}

		less = XMVector4Less(XMVectorAbs(position - XMVECTOR{ x,y,0.f,0.f }), XMVECTOR{0.5f,0.5f,0.5f,0.5f});
		if (less)
		{
			return 0xFFFFAAAA;
		}
		else {
			return (0xFF << 24) | (c << 16) | (c << 8) | (c);
		}

	}
	int ScrH() const {
		return scr_height;
	}
	int ScrW() const {
		return scr_width;
	}
	int HscrH() const {
		return hscr_height;
	}
	int HscrW() const {
		return hscr_width;
	}
	float ScrHf() const {
		return scr_height;
	}
	float ScrWf() const {
		return scr_width;
	}
	float HscrHf() const {
		return hscr_height;
	}
	float HscrWf() const {
		return hscr_width;
	}
	//v pixels, aspect ratio
	void setResolution(int height, float ar) {
		scr_height = height;
		scr_width = int(std::round(height * ar));
		hscr_height = scr_height / 2;
		hscr_width = scr_width / 2;
	}

	//v radians
	void setAngle(float _angle) {
		angle = std::fmodf(_angle, 2 * std::numbers::pi);
		dir = XMVECTOR{ std::cosf(angle), std::sinf(angle), 0.f, 0.f };
		spdlog::info("angle: {}, dirx: {}, diry: {}\n", angle, XMVectorGetX(dir), XMVectorGetY(dir));
	}

	float getAngle() const {
		return angle;
	}

private:
	//v radians
	float angle;
	XMVECTOR dir;
	//XMVECTOR left;

	//v radians
	float fov;
	//v radians
	float vfov;
	int scr_height;
	int hscr_height;
	int scr_width;
	int hscr_width;
};
