#include "common.h"

#include "Networker.h"

CNetworker g_NW;
CTimer g_timer;

int main() {
	g_NW.set_timer(&g_timer);
	g_NW.init();

	std::thread timer_thread{ &CTimer::work, &g_timer };

	std::vector<std::thread> worker_threads;
	int num_threads = std::thread::hardware_concurrency();

	for (int i = 0; i < num_threads; ++i) {
		worker_threads.emplace_back(&CNetworker::work, &g_NW);
	}

	for (auto& th : worker_threads) {
		th.join();
	}

	timer_thread.join();

	g_NW.clear();
}