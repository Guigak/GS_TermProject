#include "common.h"

#include "Networker.h"

CNetworker g_NW;

int main() {
	g_NW.init();

	std::vector<std::thread> worker_threads;
	int num_threads = std::thread::hardware_concurrency();

	for (int i = 0; i < num_threads; ++i) {
		worker_threads.emplace_back(&CNetworker::work, &g_NW);
	}

	for (auto& th : worker_threads) {
		th.join();
	}

	g_NW.clear();
}