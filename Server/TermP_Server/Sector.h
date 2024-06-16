#pragma once
#include "common.h"

struct SECTOR {
	std::unordered_set<int> ids;
	std::mutex mtx;
};

class CSector {
	SECTOR m_Sectors[WORLD_HEIGHT / SECTOR_HEIGHT][WORLD_WIDTH / SECTOR_WIDTH];

public :
	CSector() {}
	~CSector() {}

	void add_id(int id, int x, int y);
	void remove_id(int id, int x, int y);
	void change_sector(int id, int old_x, int old_y, int new_x, int new_y);
	void calculate_around_sector(int x, int y, std::unordered_set<SECTOR*>& sector_arr);
};

