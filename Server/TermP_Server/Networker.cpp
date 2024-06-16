#include "Networker.h"

void CNetworker::init() {
	WSADATA WSAData;
	WSAStartup(MAKEWORD(2, 2), &WSAData);

	m_s_socket = WSASocket(AF_INET, SOCK_STREAM, 0, NULL, 0, WSA_FLAG_OVERLAPPED);
	SOCKADDR_IN server_addr;
	memset(&server_addr, 0, sizeof(server_addr));
	server_addr.sin_family = AF_INET;
	server_addr.sin_port = htons(PORT_NUM);
	server_addr.sin_addr.S_un.S_addr = INADDR_ANY;
	bind(m_s_socket, reinterpret_cast<sockaddr*>(&server_addr), sizeof(server_addr));
	listen(m_s_socket, SOMAXCONN);

	SOCKADDR_IN client_addr;
	int addr_size = sizeof(client_addr);
	m_c_socket = WSASocket(AF_INET, SOCK_STREAM, 0, NULL, 0, WSA_FLAG_OVERLAPPED);

	m_h_iocp = CreateIoCompletionPort(INVALID_HANDLE_VALUE, 0, 0, 0);
	CreateIoCompletionPort(reinterpret_cast<HANDLE>(m_s_socket), m_h_iocp, 9999, 0);
	m_over_ex.m_option = OP_ACCEPT;
	AcceptEx(m_s_socket, m_c_socket, m_over_ex.m_s_buf, 0, addr_size + 16, addr_size + 16, 0, &m_over_ex.m_overlapped);
}

void CNetworker::work() {
	while (true) {
		// GQSC
		DWORD bytes;
		ULONG_PTR key;
		WSAOVERLAPPED* overlapped = nullptr;
		BOOL ret = GetQueuedCompletionStatus(m_h_iocp, &bytes, &key, &overlapped, INFINITE);

		// Casting
		OVERLAPPED_EX* over_ex = reinterpret_cast<OVERLAPPED_EX*>(overlapped);

		// Checking Errors
		if (FALSE == ret) {
			if (over_ex->m_option == OP_ACCEPT) {
				std::cout << "Accept Error";
			}
			else {
				std::cout << "GQCS Error on client[" << key << "]\n";
				disconnect(static_cast<int>(key));

				if (over_ex->m_option == OP_SEND) {
					delete over_ex;
				}

				continue;
			}
		}

		if ((0 == bytes) && ((over_ex->m_option == OP_RECV) || (over_ex->m_option == OP_SEND))) {
			disconnect(static_cast<int>(key));

			if (over_ex->m_option == OP_SEND) {
				delete over_ex;
			}

			continue;
		}

		// Processing
		switch (over_ex->m_option) {
		case OP_ACCEPT: {
			int client_id = get_new_client_id();

			if (client_id != -1) {
				{
					std::lock_guard<std::mutex> ll(m_objects[client_id].m_s_lock);
					m_objects[client_id].m_state = ST_ALLOC;
				}
				m_objects[client_id].m_x = 0;
				m_objects[client_id].m_y = 0;
				m_objects[client_id].m_id = client_id;
				m_objects[client_id].m_name[0] = 0;
				m_objects[client_id].m_remain_size = 0;
				m_objects[client_id].m_socket = m_c_socket;
				CreateIoCompletionPort(reinterpret_cast<HANDLE>(m_c_socket), m_h_iocp, client_id, 0);
				m_objects[client_id].recv();
				m_c_socket = WSASocket(AF_INET, SOCK_STREAM, 0, NULL, 0, WSA_FLAG_OVERLAPPED);
			}
			else {
				std::cout << "Max user exceeded.\n";
			}

			ZeroMemory(&m_over_ex.m_overlapped, sizeof(m_over_ex.m_overlapped));
			int addr_size = sizeof(SOCKADDR_IN);
			AcceptEx(m_s_socket, m_c_socket, m_over_ex.m_s_buf, 0, addr_size + 16, addr_size + 16, 0, &m_over_ex.m_overlapped);
			break;
		}
		case OP_RECV: {
			int remain_data = bytes + m_objects[key].m_remain_size;
			char* p = over_ex->m_s_buf;

			while (remain_data > 0) {
				int packet_size = p[0];
				if (packet_size <= remain_data) {
					prcs_packet(static_cast<int>(key), p);
					p = p + packet_size;
					remain_data = remain_data - packet_size;
				}
				else break;
			}

			m_objects[key].m_remain_size = remain_data;

			if (remain_data > 0) {
				memcpy(over_ex->m_s_buf, p, remain_data);
			}

			m_objects[key].recv();
			break;
		}
		case OP_SEND:
			delete over_ex;
			break;
		}
	}
}

void CNetworker::clear() {
	closesocket(m_s_socket);
	WSACleanup();
}

int CNetworker::get_new_client_id() {
	for (int i = MAX_NPC; i < MAX_NPC + MAX_USER; ++i) {
		std::lock_guard<std::mutex> lock(m_objects[i].m_s_lock);

		if (m_objects[i].m_state == ST_FREE) {
			return i;
		}
	}

	return -1;
}

void CNetworker::prcs_packet(int client_id, char* packet) {
	switch (packet[2]) {
	case CS_LOGIN: {
		CS_LOGIN_PACKET* p = reinterpret_cast<CS_LOGIN_PACKET*>(packet);
		strcpy_s(m_objects[client_id].m_name, p->name);
		{
			std::lock_guard<std::mutex> ll(m_objects[client_id].m_s_lock);
			m_objects[client_id].m_x = rand() % W_WIDTH;
			m_objects[client_id].m_y = rand() % W_HEIGHT;
			m_objects[client_id].m_state = ST_INGAME;
		}
		m_objects[client_id].send_login_info_packet();
		for (auto& pl : m_objects) {
			{
				std::lock_guard<std::mutex> ll(pl.m_s_lock);

				if (ST_INGAME != pl.m_state) {
					continue;
				}
			}

			if (pl.m_id == client_id) {
				continue;
			}

			if (false == m_objects[client_id].can_see(pl)) {
				continue;
			}

			m_objects[client_id].send_add_object_packet(pl);
		}
		break;
	}
	case CS_MOVE: {
		CS_MOVE_PACKET* p = reinterpret_cast<CS_MOVE_PACKET*>(packet);
		m_objects[client_id].m_last_move_time = p->move_time;
		short x = m_objects[client_id].m_x;
		short y = m_objects[client_id].m_y;

		switch (p->direction) {
		case 0:
			if (y > 0) {
				y--;
			}
			break;
		case 1:
			if (y < W_HEIGHT - 1) {
				y++;
			}
			break;
		case 2:
			if (x > 0) {
				x--;
			}
			break;
		case 3:
			if (x < W_WIDTH - 1) {
				x++;
			}
			break;
		}

		m_objects[client_id].m_x = x;
		m_objects[client_id].m_y = y;

		std::unordered_set<int> near_list;
		m_objects[client_id].m_vl_lock.lock();
		std::unordered_set<int> old_vlist = m_objects[client_id].m_view_list;
		m_objects[client_id].m_vl_lock.unlock();

		for (auto& cl : m_objects) {
			if (cl.m_state != ST_INGAME) {
				continue;
			}

			if (cl.m_id == client_id) {
				continue;
			}

			if (m_objects[client_id].can_see(cl)) {
				near_list.insert(cl.m_id);
			}
		}

		m_objects[client_id].send_move_packet(m_objects[client_id]);

		for (auto& pl : near_list) {
			auto& obj = m_objects[pl];

			if (obj.is_NPC()) {
				obj.m_vl_lock.lock();
				if (m_objects[pl].m_view_list.count(client_id)) {
					obj.m_vl_lock.unlock();
					m_objects[pl].send_move_packet(m_objects[client_id]);
				}
				else {
					obj.m_vl_lock.unlock();
					m_objects[pl].send_add_object_packet(m_objects[client_id]);
				}
			}

			if (old_vlist.count(pl) == 0)
				m_objects[client_id].send_add_object_packet(m_objects[pl]);
		}

		for (auto& pl : old_vlist) {
			if (0 == near_list.count(pl)) {
				m_objects[client_id].send_remove_object_packet(m_objects[pl]);

				if (m_objects[pl].is_NPC())
					m_objects[pl].send_remove_object_packet(m_objects[client_id]);
			}
			break;
		}
	}
	}

}

void CNetworker::disconnect(int client_id) {
	m_objects[client_id].m_vl_lock.lock();
	std::unordered_set <int> vl = m_objects[client_id].m_view_list;
	m_objects[client_id].m_vl_lock.unlock();

	for (auto& p_id : vl) {
		if (m_objects[p_id].is_NPC()) continue;
		auto& pl = m_objects[p_id];
		{
			std::lock_guard<std::mutex> ll(m_objects[p_id].m_s_lock);
			if (ST_INGAME != pl.m_state) continue;
		}
		if (pl.m_id == client_id) continue;
		pl.send_remove_object_packet(m_objects[client_id]);
	}

	closesocket(m_objects[client_id].m_socket);

	std::lock_guard<std::mutex> ll(m_objects[client_id].m_s_lock);
	m_objects[client_id].m_state = ST_FREE;
}
