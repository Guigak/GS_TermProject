#include "Timer.h"

void CTimer::set_iocp(HANDLE* p_h_iocp) {
	m_p_h_iocp = p_h_iocp;
}

void CTimer::work() {
	while (true) {
		if (m_queue.empty()) {
			std::this_thread::sleep_for(std::chrono::milliseconds(1));
		}
		else {
			m_mtx.lock();
			EVENT event = m_queue.top();
			m_mtx.unlock();

			if (event.time < std::chrono::system_clock::now()) {
				m_mtx.lock();
				m_queue.pop();
				m_mtx.unlock();

				switch (event.type) {
				case EV_RANDOM_MOVE:
				{
					OVERLAPPED_EX* over_ex = new OVERLAPPED_EX;
					over_ex->m_option = OP_RANDOM_MOVE;
					PostQueuedCompletionStatus(*m_p_h_iocp, 1, event.id, &over_ex->m_overlapped);
				}
				break;
				case EV_SKILL_MOVEMENT:
				{
					OVERLAPPED_EX* over_ex = new OVERLAPPED_EX;
					over_ex->m_option = OP_SKILL_MOVEMENT;
					PostQueuedCompletionStatus(*m_p_h_iocp, 1, event.id, &over_ex->m_overlapped);
				}
					break;
				default:
					break;
				}
			}
		}
	}
}

void CTimer::add_event(int id, int delay_ms, EVENT_TYPE type, int target_id) {
	EVENT event;
	event.id = id;
	event.time = std::chrono::system_clock::now() + std::chrono::milliseconds(delay_ms);
	event.type = type;
	event.target_id = target_id;

	m_mtx.lock();
	m_queue.push(event);
	m_mtx.unlock();
}
