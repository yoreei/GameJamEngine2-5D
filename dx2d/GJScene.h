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

struct Entity {
	void moveBy(const XMVECTOR& by) {
		position += by;
	}
	void setPos(const XMVECTOR& newPos) {
		position = newPos;
	}
	void setX(float x) {
		XMVectorSetX(position, x);
	}
	void setY(float y) {
		XMVectorSetY(position, y);
	}
	void interpolate(const Entity& e1, const Entity& e2, float alpha) {
		size = e1.size * (1.f - alpha) + e2.size * alpha;
		momentum = XMVector4Normalize(XMVectorLerp(e1.momentum, e2.momentum, alpha));
		position = XMVector4Normalize(XMVectorLerp(e1.position, e2.position, alpha));
	}
public:
	//v * Non-Interpolatable:
	uint16_t health = 1;
	//v * Interpolatable:
	uint16_t size = 2;
	XMVECTOR momentum{};
	XMVECTOR position{};
private:
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
		setPitch(0);
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
		entities.fill(Entity{});
		for (int i = 0; i < entities.size(); ++i) {
			entities[i].health = 1;
			entities[i].size = 2;
			entities[i].setPos({ 180.f, 240.f, 0.f, 0.f });
		}
	}

	void resetObstacles() {
		obstacles.fill(Entity{});
	}

	void setFov(float newFov) {
		fov = newFov;
		vfov = fov;
	}

	const XMVECTOR& getDir() const {
		return dir;
	}

	void setDir(const XMVECTOR& newDir) {
		dir = newDir;
	}
	//v radians
	const float getFov() const {
		return fov;
	}
	//v radians
	const float getVfov() const {
		return vfov;
	}

	void setPitch(float _pitch) {
		pitch = _pitch;
		//float t_horizon = pitch / -0.4f; // todo delete this
		horizon = HscrHf() + int(-pitch * HscrHf()) - 1; //< from -1 to (scene->ScrH - 1)
	}
	float getPitch() const {
		return pitch;
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

		bool wall = XMVector4Less(XMVectorAbs(position - XMVECTOR{ x,y + 3.f,0.f,0.f }), XMVECTOR{ 0.5f,0.5f,0.5f,0.5f });
		if (get(size_t(x), size_t(y)) == '#')
		{
			return 0xFFAAAAFF;
		}

		bool less = XMVector4Less(XMVectorAbs(position - XMVECTOR{ x,y + 3.f,0.f,0.f }), XMVECTOR{ 0.5f,0.5f,0.5f,0.5f });
		if (less)
		{
			return 0xFFAAFFAA;
		}

		less = XMVector4Less(XMVectorAbs(position - XMVECTOR{ x,y,0.f,0.f }), XMVECTOR{ 0.5f,0.5f,0.5f,0.5f });
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

	int getHorizon() const {
		return horizon;
	}

	void interpolate(const GJScene& s1, const GJScene& s2, float alpha) {
		//v * Public Interpolatable (1)
		for (int i = 0; i < entities.size(); ++i) {
			entities[i].interpolate(s1.entities[i], s2.entities[i], alpha);
		}
		//v * Public Interpolatable (2)
		for (int i = 0; i < obstacles.size(); ++i) {
			obstacles[i].interpolate(s1.obstacles[i], s2.obstacles[i], alpha);
		}
		//v * Public Interpolatable (3)
		position = XMVectorLerp(s1.position, s2.position, alpha);
		//v * Public Interpolatable (4)
		camHeight = s1.camHeight * (1.f - alpha) + s2.camHeight * alpha;

		//v * Private Interpolatable (1)
		dir = XMVector4Normalize(XMVectorLerp(s1.dir, s2.dir, alpha));
		//v * Private Interpolatable (2)
		angle = s1.angle * (1.f - alpha) + s2.angle * alpha;
		//v * Private Interpolatable (3)
		pitch = s1.pitch * (1.f - alpha) + s2.pitch * alpha;
		//v * Private Interpolatable (4)
		horizon = s1.horizon * (1.f - alpha) + s2.horizon * alpha;
	}
public:
	//v * Non-interpolatable:
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

	//v * Interpolatable (4):
	std::array<Entity, 4> entities{}; //< controlable
	std::array<Entity, 25> obstacles{};
	XMVECTOR position{ 0,0,0,0 }; //< only x and y are used
	float camHeight = 0.5f;

private:
	//v * Non-Interpolatable:
	float fov; //< radians
	float vfov; //< radians
	int scr_height;
	int hscr_height;
	int scr_width;
	int hscr_width;

	//v * Interpolatable (4):
	XMVECTOR dir;
	float angle; //< radians
	float pitch = 0; //< radians
	int horizon;
};
