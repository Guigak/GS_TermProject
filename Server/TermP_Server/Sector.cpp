#include "Sector.h"

void CSector::add_id(int id, int x, int y) {
	std::lock_guard<std::mutex> lock(m_Sectors[y][x].mtx);
	m_Sectors[y][x].ids.insert(id);
}

void CSector::remove_id(int id, int x, int y) {

	std::lock_guard<std::mutex> lock(m_Sectors[y][x].mtx);
	m_Sectors[y][x].ids.erase(id);
}

void CSector::change_sector(int id, int old_x, int old_y, int new_x, int new_y) {
	{
		std::lock_guard<std::mutex> lock(m_Sectors[new_y][new_x].mtx);
		m_Sectors[new_y][new_x].ids.insert(id);
	}
	{
		std::lock_guard<std::mutex> lock(m_Sectors[old_y][old_x].mtx);
		m_Sectors[old_y][old_x].ids.erase(id);
	}
}

void CSector::calculate_around_sector(int x, int y, std::unordered_set<SECTOR*>& sector_arr) {
	for (int i = -1; i < 2; i += 2) {
		int newx = x;

		newx += i * CLIENT_WIDTH / 2;

		newx = newx < 0 ? 0 : newx;
		newx = newx > W_WIDTH - 1 ? W_WIDTH - 1 : newx;

		for (int j = -1; j < 2; j += 2) {
			int newy = y;

			newy += j * CLIENT_HEIGHT / 2;
			newy = newy < 0 ? 0 : newy;
			newy = newy > W_WIDTH - 1 ? W_WIDTH - 1 : newy;

			int sctr_x = newx / SECTOR_WIDTH;
			int sctr_y = newy / SECTOR_HEIGHT;

			if (sector_arr.count(&(m_Sectors[sctr_y][sctr_x])) == 0) {
				std::lock_guard<std::mutex>(m_Sectors[sctr_y][sctr_x].mtx);
				sector_arr.insert(&(m_Sectors[sctr_y][sctr_x]));
			}
		}
	}
}
