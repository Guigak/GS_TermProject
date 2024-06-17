#include "Map.h"

bool CMap::load_map() {
	// load map
	std::cout << "Loading.." << std::endl;

	std::ifstream ifile("map_2000_edit.txt");

	if (!ifile.is_open()) {
		std::cout << "map load fail" << std::endl;
		return false;
	}

	int num;

	for (int i = 0; i < 2000; ++i) {
		for (int j = 0; j < 2000; ++j) {
			ifile >> num;

			if (num == 29 || num == 30 ||
				num == 3 || num == 4 || num == 5 || num == 6 || num == 7 ||
				num == 11 || num == 12 || num == 13 || num == 14 || num == 15 ||
				num == 21 || num == 22 || num == 23) {
				tile_map[i][j] = true;
			}
			else {
				tile_map[i][j] = false;
			}
		}
	}

	ifile.close();
	return true;
}

bool CMap::can_move(int x, int y) {
	return tile_map[y][x];
}
