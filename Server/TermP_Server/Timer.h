#pragma once
#include "common.h"

enum EVENT_TYPE {
	EV_RANDOM_MOVE, EV_SKILL_MOVEMENT
};

struct EVENT {
	int id;
	std::chrono::system_clock::time_point time;
	EVENT_TYPE type;
	int target_id;

	bool operator<(const EVENT& rhs) const {
		return time > rhs.time;
	}
};

class CTimer {
	std::priority_queue<EVENT> m_queue;
	std::mutex m_mtx;

	HANDLE* m_p_h_iocp;

public :
	CTimer() {}
	CTimer(HANDLE* p_h_iocp) : m_p_h_iocp(p_h_iocp) {}
	~CTimer() {}

	void set_iocp(HANDLE* p_h_iocp);

	void work();

	void add_event(int id, int delay_ms, EVENT_TYPE type, int target_id);
};