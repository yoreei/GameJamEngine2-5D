#pragma once
#include "common/cppUtil.h"

// Seconds since engine boot
Seconds GEngineTime{ 0 };
// How long the game has been going. Does not include Pause time and Menu time. Use this for game events
Seconds GGameTime;

/// Global pointer to engine to deliver input and gameplay events
class GameEngine;
GameEngine* GGameEnginePtr = nullptr;
