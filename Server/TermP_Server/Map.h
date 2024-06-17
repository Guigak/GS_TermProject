#pragma once
#include "common.h"

class CMap {
	bool tile_map[WORLD_WIDTH][WORLD_HEIGHT];

public :
	CMap() {}
	~CMap() {}

	bool load_map();

	bool can_move(int x, int y);
};

