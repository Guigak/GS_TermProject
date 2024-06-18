#include "common.h"

#include "Networker.h"

CNetworker g_NW;
CTimer g_timer;
CDatabase g_DB;

int main() {
	if (false == g_DB.connect()) {
		std::cout << "DB Connect ERROR" << std::endl;
		return 0;
	}
	g_DB.clear_Ingame();

	g_NW.set_database(&g_DB);

	g_NW.set_timer(&g_timer);
	g_NW.init();

	std::thread timer_thread{ &CTimer::work, &g_timer };

	std::thread database_thread{ &CDatabase::work, &g_DB };

	std::vector<std::thread> worker_threads;
	int num_threads = std::thread::hardware_concurrency();

	for (int i = 0; i < num_threads; ++i) {
		worker_threads.emplace_back(&CNetworker::work, &g_NW);
	}

	for (auto& th : worker_threads) {
		th.join();
	}

	timer_thread.join();

	database_thread.join();

	g_NW.clear();
	g_DB.disconnect();
}