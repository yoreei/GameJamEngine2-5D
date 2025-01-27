#pragma once
#include <vector>
#include <array>
#include <algorithm>
#include <cassert>
#include <iostream>
#include <limits>

constexpr float M_PI = 3.14159265358979323846f;
constexpr float INF = std::numeric_limits<float>::infinity();

struct vec2 {
	float x = 0.f;
	float y = 0.f;

	vec2() = default;
	vec2(float x, float y) : x(x), y(y) {};
	vec2(float val) : x(val), y(val) {};

	float& operator[](size_t index) {
		if (index == 0) return x;
		else if (index == 1) return y;
		else throw std::out_of_range("Index out of range for vec2");
	}

	const float& operator[](size_t index) const {
		if (index == 0) return x;
		else if (index == 1) return y;
		else throw std::out_of_range("Index out of range for vec2");
	}

	vec2 operator*(float scalar) const {
		return { x * scalar, y * scalar };
	}

	vec2 operator/(float scalar) const {
		return { x / scalar, y / scalar };
	}

	friend vec2 operator*(float scalar, const vec2& v) {
		return { v.x * scalar, v.y * scalar };
	}

	vec2 operator+(const vec2& v) const {
		return { x + v.x, y + v.y };
	}

	vec2 operator-(const vec2& v) const {
		return { x - v.x, y - v.y };
	}
	float cross(const vec2& other) const {
		return x * other.y - y * other.x;
	}
};

struct line {
	vec2 A{};
	vec2 B{};
};

struct ColorF {
	ColorF() = default;
	ColorF(float value) {
		c[0] = value;
		c[1] = value;
		c[2] = value;
	}

	ColorF(float r, float g, float b) {
		c[0] = r;
		c[1] = g;
		c[2] = b;
	}

	float c[3] = { 0.f, 0.f, 0.f };

};

struct Polygon {
	Polygon(std::vector<vec2>&& _vertices) : vertices(std::move(_vertices)) {
		edges.reserve(this->vertices.size());
		size_t n = vertices.size();
		aabbMin = { INF };
		aabbMax = { -INF };

		for (size_t i = 0; i < n; ++i) {
			const vec2& A = vertices[i];
			const vec2& B = vertices[(i + 1) % n];

			edges.push_back(B - A);

			aabbMax.x = std::max(aabbMax.x, A.x);
			aabbMax.y = std::max(aabbMax.y, A.y);
			aabbMin.x = std::min(aabbMin.x, A.x);
			aabbMin.y = std::min(aabbMin.y, A.y);
		}
		centroid = (aabbMax + aabbMin) * 0.5f;
	}
	std::vector<vec2> vertices{};
	std::vector<vec2> edges{};
	vec2 centroid{};
	vec2 aabbMin{};
	vec2 aabbMax{};

	bool isPointInside(const vec2& Q) const {
		bool has_positive = false;
		bool has_negative = false;

		size_t n = vertices.size();
		for (size_t i = 0; i < n; ++i) {
			const vec2& A = vertices[i];
			const vec2& edge = edges[i];

			vec2 to_point = Q - A;

			float cross_product = edge.cross(to_point);

			if (cross_product > 0.f) has_positive = true;
			if (cross_product < 0.f) has_negative = true;

			// If we have both positive and negative signs, Q is outside
			if (has_positive && has_negative) return false;
		}

		// Q is inside if all cross products had the same sign
		return true;
	}
};

struct Ray {
	vec2 origin;
	vec2 dir;
};

struct Light {
	// lightSize: Our spotlights are small circles that emit light in sectors.
	// The lights do not illuminate themselves (the small circles). This option controls
	// the size of the small circles. (This is for Q=P task requirement)
	static float lightSize;

	float Px; float Py; float r; float s; float i;
	float illuminate(const vec2& Q) const {
		vec2 QP = { Px - Q.x, Py - Q.y };
		float distanceSquared = QP.x * QP.x + QP.y * QP.y;
		if (distanceSquared < lightSize) { return 0.f; }

		return i / distanceSquared;
	}
};

// To be controlled by includer
float Light::lightSize = 0.00001f;

vec2 fminf(const vec2& a, const vec2& b) {
	return vec2(fminf(a.x, b.x), fminf(a.y, b.y));
}

vec2 fmaxf(const vec2& a, const vec2& b) {
	return vec2(fmaxf(a.x, b.x), fmaxf(a.y, b.y));
}

float degreesFromRadians(float radians) {
	return radians * 180.0f / M_PI;
}
float radiansFromDegrees(float degrees) {
	return degrees * (M_PI / 180.0f);
}

/* Input: any number, Output: [0.f, 360)*/
float normalizeAngle(float angle) {
	while (angle < 0.f) angle += 360.0f;
	while (angle >= 360.f) angle -= 360.0f;
	return angle;
}

/* For Debugging */
void dumpVect(const std::vector<size_t>& v) {
	for (const auto& value : v) {
		std::cout << value << ", ";
	}
}

// return: is point inside AABB?
bool bPointInAABB(const vec2& Q, const vec2& aabbMin, const vec2& aabbMax) {
	return (Q.x >= aabbMin.x && Q.x <= aabbMax.x) &&
		(Q.y >= aabbMin.y && Q.y <= aabbMax.y);
}

// return: which side of the line (p,q) lies point r
int orientation(const vec2& p, const vec2& q, const vec2& r) {
	float val = (q - p).cross(r - p);
	if (val > 0) return 1;  // Clockwise
	if (val < 0) return -1; // Counterclockwise
	return 0;               // Collinear
}

// return: do two line segments intersect?
bool bLinesIntersect(const vec2& p1, const vec2& q1, const vec2& p2, const vec2& q2) {
	int o1 = orientation(p1, q1, p2);
	int o2 = orientation(p1, q1, q2);
	int o3 = orientation(p2, q2, p1);
	int o4 = orientation(p2, q2, q1);

	// General case
	if (o1 != o2 && o3 != o4) return true;
	// Special cases: check if they are collinear and overlap
	vec2 aabbMin = fminf(p1, q1);
	vec2 aabbMax = fmaxf(p1, q1);
	if (o1 == 0 && bPointInAABB(p2, aabbMin, aabbMax)) return true;
	if (o2 == 0 && bPointInAABB(q2, aabbMin, aabbMax)) return true;
	aabbMin = fminf(p2, q2);
	aabbMax = fmaxf(p2, q2);
	if (o3 == 0 && bPointInAABB(p1, aabbMin, aabbMax)) return true;
	if (o4 == 0 && bPointInAABB(q1, aabbMin, aabbMax)) return true;

	return false;
}

bool bLinePolygonIntersect(const line& l, const Polygon& obj) {
	size_t n = obj.vertices.size();
	for (size_t i = 0; i < n; ++i) {
		vec2 objA = obj.vertices[i];
		vec2 objB = obj.vertices[(i + 1) % n]; // Wrap around to first vertex

		if (bLinesIntersect(l.A, l.B, objA, objB)) {
			return true;
		}
	}
	return false;
}

bool sectorPointIntersect(const Light& light, const vec2& Q) {
	//vec2 unitSqCenter = vec2{ 0.5f, 0.5f };
	vec2 P = vec2{ light.Px, light.Py };// - unitSqCenter;
	vec2 PQ = P - Q;
	bool inside = false;
	if (light.s >= 360.f) {
		inside = true;
	}
	else {
		float halfSpread = light.s / 2.f;
		float sectBound1 = light.r - halfSpread; // [-180; 540]
		sectBound1 = normalizeAngle(sectBound1); // [0; 360]
		float sectBound2 = light.r + halfSpread; // [-180; 540]
		sectBound2 = normalizeAngle(sectBound2); // [0; 360]
		float QPAngle = degreesFromRadians(std::atan2(PQ.y, PQ.x)) - 180.f; // [-180; 180]
		QPAngle = normalizeAngle(QPAngle); // [0; 360]

		if (sectBound1 <= sectBound2) {
			inside = (QPAngle >= sectBound1 && QPAngle <= sectBound2);
		}
		else {
			inside = (QPAngle >= sectBound1 || QPAngle <= sectBound2);
		}
	}
	return inside;
}

bool rayAABSOcclusion(const Ray& ray, const vec2& squarePos, float squareSize) {
	float xMin = squarePos.x;
	float xMax = squarePos.x + squareSize;
	float yMin = squarePos.y;
	float yMax = squarePos.y - squareSize;

	// Direction inverses to handle division efficiently
	float invDirX = 1.0f / ray.dir.x;
	float invDirY = 1.0f / ray.dir.y;

	// Calculate t for the x-axis intersections
	float xt1 = (xMin - ray.origin.x) * invDirX;
	float xt2 = (xMax - ray.origin.x) * invDirX;

	// Calculate t for the y-axis intersections
	float yt1 = (yMin - ray.origin.y) * invDirY;
	float yt2 = (yMax - ray.origin.y) * invDirY;

	// Ensure t1 <= t2 and t3 <= t4 by swapping if necessary
	if (xt1 > xt2) std::swap(xt1, xt2);
	if (yt1 > yt2) std::swap(yt1, yt2);

	// Find the largest t entry and the smallest t exit
	float tEntry = std::max(xt1, yt1);
	float tExit = std::min(xt2, yt2);

	// If tEntry > tExit or the ray exits behind the origin, no intersection occurs
	if (tEntry - 0.0001f > tExit || tExit < -0.0001f) {
		return false;  // No intersection
	}

	return true;
}

// Region codes for Cohen-Sutherland algorithm
const int INSIDE = 0; // 0000
const int LEFT = 1;   // 0001
const int RIGHT = 2;  // 0010
const int BOTTOM = 4; // 0100
const int TOP = 8;    // 1000

// Region codes for Cohen-Sutherland algorithm
int computeRegionCode(const vec2& p, float x_min, float x_max, float y_min, float y_max) {
	int code = INSIDE;

	if (p.x < x_min)      code |= LEFT;
	else if (p.x > x_max) code |= RIGHT;
	if (p.y < y_min)      code |= BOTTOM;
	else if (p.y > y_max) code |= TOP;

	return code;
}

/* Cohen-Sutherland algorithm. return whether a line passes through or is inside an Axis Aligned Bounding Square.
This algorithm is supposedly faster in 2D for lineAABB intersection compared to slab method */
bool bLineAABBIntersect(line l, const vec2& aabbMin, const vec2& aabbMax) {
	// Define the square's bounds
	const float& x_min = aabbMin.x;
	const float& x_max = aabbMax.x;
	const float& y_min = aabbMin.y;
	const float& y_max = aabbMax.y;

	// Compute region codes for both endpoints of the line
	int codeA = computeRegionCode(l.A, x_min, x_max, y_min, y_max);
	int codeB = computeRegionCode(l.B, x_min, x_max, y_min, y_max);

	while (true) {
		if ((codeA | codeB) == 0) {
			// Both endpoints are inside the square
			return true;
		}
		else if (codeA & codeB) {
			// Both endpoints are outside the square in the same region
			return false;
		}
		else {
			// Some part of the line segment potentially intersects the square
			float x = INF, y = INF;
			int codeOut = codeA ? codeA : codeB;

			// Find the intersection point with the box boundary
			if (codeOut & TOP) {           // Point is above the box
				x = l.A.x + (l.B.x - l.A.x) * (y_max - l.A.y) / (l.B.y - l.A.y);
				y = y_max;
			}
			else if (codeOut & BOTTOM) { // Point is below the box
				x = l.A.x + (l.B.x - l.A.x) * (y_min - l.A.y) / (l.B.y - l.A.y);
				y = y_min;
			}
			else if (codeOut & RIGHT) {  // Point is to the right of the box
				y = l.A.y + (l.B.y - l.A.y) * (x_max - l.A.x) / (l.B.x - l.A.x);
				x = x_max;
			}
			else if (codeOut & LEFT) {   // Point is to the left of the box
				y = l.A.y + (l.B.y - l.A.y) * (x_min - l.A.x) / (l.B.x - l.A.x);
				x = x_min;
			}

			// Replace the outside point with the intersection point and update the region code
			if (codeOut == codeA) {
				l.A.x = x;
				l.A.y = y;
				codeA = computeRegionCode(l.A, x_min, x_max, y_min, y_max);
			}
			else {
				l.B.x = x;
				l.B.y = y;
				codeB = computeRegionCode(l.B, x_min, x_max, y_min, y_max);
			}
		}
	}
}

