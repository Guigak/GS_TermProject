#include "Networker.h"

std::array<CObject, MAX_NPC + MAX_USER> m_objects;

// random
std::random_device rd;
std::uniform_int_distribution<int> uid(0, WORLD_WIDTH - 1);

// lua
int API_get_x(lua_State* L)
{
	int user_id = (int)lua_tointeger(L, -1);
	lua_pop(L, 2);
	int x = m_objects[user_id].m_x;
	lua_pushnumber(L, x);
	return 1;
}

int API_get_y(lua_State* L)
{
	int user_id = (int)lua_tointeger(L, -1);
	lua_pop(L, 2);
	int y = m_objects[user_id].m_y;
	lua_pushnumber(L, y);
	return 1;
}

int API_SendMessage(lua_State* L)
{
	int my_id = (int)lua_tointeger(L, -3);
	int user_id = (int)lua_tointeger(L, -2);
	char* mess = (char*)lua_tostring(L, -1);

	lua_pop(L, 4);

	m_objects[user_id].send_chat_packet(m_objects[my_id], mess);
	return 0;
}

void CNetworker::set_timer(CTimer* p_timer) {
	m_p_timer = p_timer;
	m_p_timer->set_iocp(&m_h_iocp);
}

void CNetworker::set_database(CDatabase* p_database) {
	m_p_database = p_database;
	m_p_database->set_iocp(&m_h_iocp);
}

void CNetworker::init() {
	if (false == m_map.load_map()) {
		std::cout << "Map Load ERROR" << std::endl;
		exit(0);
	}

	// socket
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

	// npc
	std::cout << "NPC Initialize Start" << std::endl;

	for (int i = 0; i < MAX_NPC; ++i) {
		teleport(m_objects[i]);

		int new_sector_x = m_objects[i].m_sector_x = m_objects[i].m_x / SECTOR_WIDTH;
		int new_sector_y = m_objects[i].m_sector_y = m_objects[i].m_y / SECTOR_HEIGHT;

		m_sectors.add_id(i, new_sector_x, new_sector_y);

		m_objects[i].m_id = i;
		sprintf_s(m_objects[i].m_name, "N%d", i);
		m_objects[i].m_state = ST_INGAME;
		m_objects[i].m_active = false;
		m_objects[i].m_max_hp = 100;
		m_objects[i].m_hp = 100;
		m_objects[i].m_exp = 10;

		auto Lua = m_objects[i].m_Lua = luaL_newstate();
		luaL_openlibs(Lua);
		luaL_loadfile(Lua, "npc.lua");
		lua_pcall(Lua, 0, 0, 0);

		lua_getglobal(Lua, "set_uid");
		lua_pushnumber(Lua, i);
		lua_pcall(Lua, 1, 0, 0);

		lua_register(Lua, "API_SendMessage", API_SendMessage);
		lua_register(Lua, "API_get_x", API_get_x);
		lua_register(Lua, "API_get_y", API_get_y);
	}

	std::cout << "NPC Initialize End" << std::endl;
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
		case OP_ACCEPT:
		{
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
				m_objects[client_id].m_skill_mv = false;
				m_objects[client_id].m_last_move_time = 0;
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
		case OP_LOGIN_OK:
		{
			PLAYER_INFO* info = reinterpret_cast<PLAYER_INFO*>(over_ex->m_data);

			strcpy_s(m_objects[key].m_name, info->nickname);
			m_objects[key].m_level = info->level;
			m_objects[key].m_max_hp = info->max_hp;
			m_objects[key].m_hp = info->hp;
			m_objects[key].m_exp = info->exp;
			m_objects[key].m_potion_s = info->potion_s;
			m_objects[key].m_potion_l = info->potion_l;

			int new_x = m_objects[key].m_x = info->x;
			int new_y = m_objects[key].m_y = info->y;
			{
				std::lock_guard<std::mutex> ll(m_objects[key].m_s_lock);
				m_objects[key].m_state = ST_INGAME;
			}
			m_objects[key].send_login_info_packet();

			// Sector
			int new_sector_x = m_objects[key].m_sector_x = new_x / SECTOR_WIDTH;
			int new_sector_y = m_objects[key].m_sector_y = new_y / SECTOR_HEIGHT;

			m_sectors.add_id(key, new_sector_x, new_sector_y);

			std::unordered_set<SECTOR*> sectors_arr;

			m_sectors.calculate_around_sector(new_x, new_y, sectors_arr);

			// send
			for (auto& sector : sectors_arr) {
				std::lock_guard<std::mutex> lock((*sector).mtx);

				if ((*sector).ids.size() == 0) continue;

				for (auto& id : (*sector).ids) {
					{
						std::lock_guard<std::mutex> slock(m_objects[id].m_s_lock);

						if (ST_INGAME != m_objects[id].m_state) {
							continue;
						}
					}

					if (m_objects[id].m_id == key) {
						continue;
					}

					if (false == m_objects[key].can_see(m_objects[id])) {
						continue;
					}

					m_objects[id].send_add_object_packet(m_objects[key]);
					m_objects[key].send_add_object_packet(m_objects[id]);

					//
					if (true == m_objects[id].is_NPC()) {
						if (m_objects[id].m_active == false) {
							bool old_active = false;
							bool new_active = true;

							if (true == std::atomic_compare_exchange_strong(&m_objects[id].m_active, &old_active, new_active)) {
								m_p_timer->add_event(m_objects[id].m_id, 1000, EV_RANDOM_MOVE, -1);
							}
						}
						else {
							OVERLAPPED_EX* over_ex = new OVERLAPPED_EX;
							over_ex->m_option = OP_NPC_MOVE;
							over_ex->m_target_id = key;
							PostQueuedCompletionStatus(m_h_iocp, 1, m_objects[id].m_id, &over_ex->m_overlapped);
						}
					}
				}
			}

			delete info;
			break;
		}
		case OP_LOGIN_FAIL:
		{
			disconnect(key);

			break;
		}
		case OP_RECV:
		{
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
		case OP_RANDOM_MOVE:
			for (int i = MAX_NPC; i < MAX_NPC + MAX_USER; ++i) {
				if (m_objects[i].m_state != ST_INGAME) {
					continue;
				}

				if (true == m_objects[key].can_see(m_objects[i])) {
					random_move(m_objects[key]);
					m_p_timer->add_event(key, 1000, EV_RANDOM_MOVE, -1);
				}
				else {
					m_objects[key].m_active = false;
				}
			}

			delete over_ex;
			break;
		case OP_NPC_MOVE:
		{
			m_objects[key].m_lua_mtx.lock();
			auto L = m_objects[key].m_Lua;
			lua_getglobal(L, "event_player_move");
			lua_pushnumber(L, over_ex->m_target_id);
			lua_pcall(L, 1, 0, 0);
			//lua_pop(L, 1);
			m_objects[key].m_lua_mtx.unlock();
			delete over_ex;
		}
			break;
		case OP_SKILL_MOVEMENT:
			m_objects[key].m_skill_mv = false;
			break;
		default:
			break;
		}
	}
}

void CNetworker::clear() {
	closesocket(m_s_socket);
	WSACleanup();
}

void CNetworker::teleport(CObject& object) {
	int new_x = uid(rd) % WORLD_WIDTH;
	int new_y = uid(rd) % WORLD_HEIGHT;

	while (false == m_map.can_move(new_x, new_y)) {
		new_x = uid(rd) % WORLD_WIDTH;
		new_y = uid(rd) % WORLD_HEIGHT;
	}

	object.m_x = new_x;
	object.m_y = new_y;
}

void CNetworker::random_move(CObject& object) {
	short new_x = object.m_x;
	short new_y = object.m_y;

	if (false == m_map.can_move(new_y, new_x)) {
		return;
	}

	std::unordered_set<SECTOR*> old_sectors_arr;

	m_sectors.calculate_around_sector(new_x, new_y, old_sectors_arr);

	std::unordered_set<int> old_view_list;

	for (auto& sector : old_sectors_arr) {
		std::lock_guard<std::mutex> lock((*sector).mtx);

		if ((*sector).ids.size() == 0) continue;

		for (auto& id : (*sector).ids) {
			{
				std::lock_guard<std::mutex> slock(m_objects[id].m_s_lock);

				if (ST_INGAME != m_objects[id].m_state) {
					continue;
				}
			}

			if (m_objects[id].m_id == object.m_id) {
				continue;
			}

			if (false == object.can_see(m_objects[id])) {
				continue;
			}

			old_view_list.insert(id);
		}
	}

	// move
	do {
		new_x = object.m_x;
		new_y = object.m_y;

		switch (uid(rd) % 4) {
		case 0:
			if (new_y > 0) {
				new_y--;
			}
			break;
		case 1:
			if (new_y < W_HEIGHT - 1) {
				new_y++;
			}
			break;
		case 2:
			if (new_x > 0) {
				new_x--;
			}
			break;
		case 3:
			if (new_x < W_WIDTH - 1) {
				new_x++;
			}
			break;
		}
	} while (false == m_map.can_move(new_x, new_y));

	object.m_x = new_x;
	object.m_y = new_y;

	int old_sector_x = object.m_sector_x;
	int old_sector_y = object.m_sector_y;

	int new_sector_x = object.m_x / SECTOR_WIDTH;
	int new_sector_y = object.m_y / SECTOR_HEIGHT;

	if (old_sector_x != new_sector_x || old_sector_y != new_sector_y) {
		m_sectors.change_sector(object.m_id, old_sector_x, old_sector_y, new_sector_x, new_sector_y);
	}

	std::unordered_set<SECTOR*> new_sectors_arr;

	m_sectors.calculate_around_sector(new_x, new_y, new_sectors_arr);

	std::unordered_set<int> new_view_list;

	for (auto& sector : new_sectors_arr) {
		std::lock_guard<std::mutex> lock((*sector).mtx);

		if ((*sector).ids.size() == 0) continue;

		for (auto& id : (*sector).ids) {
			{
				std::lock_guard<std::mutex> slock(m_objects[id].m_s_lock);

				if (ST_INGAME != m_objects[id].m_state) {
					continue;
				}
			}

			if (m_objects[id].m_id == object.m_id) {
				continue;
			}

			if (false == object.can_see(m_objects[id])) {
				continue;
			}

			new_view_list.insert(id);
		}
	}

	// send
	for (int player_id : new_view_list) {
		if (0 == old_view_list.count(player_id)) {
			m_objects[player_id].send_add_object_packet(object);
		}
		else {
			m_objects[player_id].send_move_packet(object);
		}

		OVERLAPPED_EX* over_ex = new OVERLAPPED_EX;
		over_ex->m_option = OP_NPC_MOVE;
		over_ex->m_target_id = m_objects[player_id].m_id;
		PostQueuedCompletionStatus(m_h_iocp, 1, object.m_id, &over_ex->m_overlapped);
	}

	for (int player_id : old_view_list) {
		if (0 == new_view_list.count(player_id)) {
			m_objects[player_id].send_remove_object_packet(object);
		}
	}
}

void CNetworker::send_update_around(int client_id) {
	std::unordered_set<SECTOR*> sectors_arr;

	m_sectors.calculate_around_sector(m_objects[client_id].m_x, m_objects[client_id].m_y, sectors_arr);

	for (auto& sector : sectors_arr) {
		std::lock_guard<std::mutex> lock((*sector).mtx);

		if ((*sector).ids.size() == 0) continue;

		for (auto& id : (*sector).ids) {
			{
				std::lock_guard<std::mutex> slock(m_objects[id].m_s_lock);

				if (ST_INGAME != m_objects[id].m_state) {
					continue;
				}
			}

			if (m_objects[id].m_id == client_id) {
				continue;
			}

			if (false == m_objects[client_id].can_see(m_objects[id])) {
				continue;
			}

			if (true == m_objects[id].is_NPC()) {
				continue;
			}

			m_objects[id].send_stat_change_packet(m_objects[id]);
		}
	}
}

void CNetworker::check_dead(CObject& object) {
	if (object.m_hp <= 0) {
		object.m_hp = object.m_max_hp;
		object.m_exp /= 2;
		object.m_x = 4;
		object.m_y = 4;

		// Sector
		int old_sector_x = object.m_sector_x;
		int old_sector_y = object.m_sector_y;

		int new_sector_x = object.m_x / SECTOR_WIDTH;
		int new_sector_y = object.m_y / SECTOR_HEIGHT;

		if (old_sector_x != new_sector_x || old_sector_y != new_sector_y) {
			m_sectors.change_sector(object.m_id, old_sector_x, old_sector_y, new_sector_x, new_sector_y);
		}

		std::unordered_set<SECTOR*> sectors_arr;

		m_sectors.calculate_around_sector(object.m_x, object.m_y, sectors_arr);

		// view list
		std::unordered_set<int> new_view_list;
		object.m_vl_lock.lock();
		std::unordered_set<int> old_view_list = object.m_view_list;
		object.m_vl_lock.unlock();

		for (auto& sector : sectors_arr) {
			std::lock_guard<std::mutex> lock((*sector).mtx);

			if ((*sector).ids.size() == 0) continue;

			for (auto& id : (*sector).ids) {
				{
					std::lock_guard<std::mutex> slock(m_objects[id].m_s_lock);

					if (ST_INGAME != m_objects[id].m_state) {
						continue;
					}
				}

				if (m_objects[id].m_id == object.m_id) {
					continue;
				}

				if (false == object.can_see(m_objects[id])) {
					continue;
				}

				new_view_list.insert(id);

				//
				if (true == m_objects[id].is_NPC()) {
					if (m_objects[id].m_active == false) {
						bool old_active = false;
						bool new_active = true;

						if (true == std::atomic_compare_exchange_strong(&m_objects[id].m_active, &old_active, new_active)) {
							m_p_timer->add_event(m_objects[id].m_id, 1000, EV_RANDOM_MOVE, -1);
						}
					}
					else {
						OVERLAPPED_EX* over_ex = new OVERLAPPED_EX;
						over_ex->m_option = OP_NPC_MOVE;
						over_ex->m_target_id = object.m_id;
						PostQueuedCompletionStatus(m_h_iocp, 1, m_objects[id].m_id, &over_ex->m_overlapped);
					}
				}
			}
		}

		// send
		object.send_move_packet(object);

		for (int player_id : new_view_list) {
			if (0 == old_view_list.count(player_id)) {
				object.send_add_object_packet(m_objects[player_id]);
				m_objects[player_id].send_add_object_packet(object);
			}
			else {
				m_objects[player_id].send_move_packet(object);
			}
		}

		for (int player_id : old_view_list) {
			if (0 == new_view_list.count(player_id)) {
				object.send_remove_object_packet(m_objects[player_id]);
				m_objects[player_id].send_remove_object_packet(object);
			}
		}
	}
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
	case CS_LOGIN:
	{
		CS_LOGIN_PACKET* p = reinterpret_cast<CS_LOGIN_PACKET*>(packet);
		strcpy_s(m_objects[client_id].m_name, p->name);

		m_p_database->add_query(m_objects[client_id], Q_LOGIN, -1);
		break;
	}
	case CS_MOVE:
	{
		CS_MOVE_PACKET* p = reinterpret_cast<CS_MOVE_PACKET*>(packet);

		if (false == m_objects[client_id].m_skill_mv) {
			if (m_objects[client_id].m_last_move_time + 1000 > p->move_time) {
				return;
			}
		}

		m_objects[client_id].m_last_move_time = p->move_time;

		short new_x = m_objects[client_id].m_x;
		short new_y = m_objects[client_id].m_y;

		// move
		switch (p->direction) {
		case 0:
			if (new_y > 0) {
				new_y--;
			}
			break;
		case 1:
			if (new_y < W_HEIGHT - 1) {
				new_y++;
			}
			break;
		case 2:
			if (new_x > 0) {
				new_x--;
			}
			break;
		case 3:
			if (new_x < W_WIDTH - 1) {
				new_x++;
			}
			break;
		}
		m_objects[client_id].m_direction = p->direction;

		if (false == m_map.can_move(new_y, new_x)) {
			return;
		}

		m_objects[client_id].m_x = new_x;
		m_objects[client_id].m_y = new_y;

		// Sector
		int old_sector_x = m_objects[client_id].m_sector_x;
		int old_sector_y = m_objects[client_id].m_sector_y;

		int new_sector_x = m_objects[client_id].m_x / SECTOR_WIDTH;
		int new_sector_y = m_objects[client_id].m_y / SECTOR_HEIGHT;

		if (old_sector_x != new_sector_x || old_sector_y != new_sector_y) {
			m_sectors.change_sector(client_id, old_sector_x, old_sector_y, new_sector_x, new_sector_y);
		}

		std::unordered_set<SECTOR*> sectors_arr;

		m_sectors.calculate_around_sector(new_x, new_y, sectors_arr);

		// view list
		std::unordered_set<int> new_view_list;
		m_objects[client_id].m_vl_lock.lock();
		std::unordered_set<int> old_view_list = m_objects[client_id].m_view_list;
		m_objects[client_id].m_vl_lock.unlock();

		for (auto& sector : sectors_arr) {
			std::lock_guard<std::mutex> lock((*sector).mtx);

			if ((*sector).ids.size() == 0) continue;

			for (auto& id : (*sector).ids) {
				{
					std::lock_guard<std::mutex> slock(m_objects[id].m_s_lock);

					if (ST_INGAME != m_objects[id].m_state) {
						continue;
					}
				}

				if (m_objects[id].m_id == client_id) {
					continue;
				}

				if (false == m_objects[client_id].can_see(m_objects[id])) {
					continue;
				}

				new_view_list.insert(id);

				//
				if (true == m_objects[id].is_NPC()) {
					if (m_objects[id].m_active == false) {
						bool old_active = false;
						bool new_active = true;

						if (true == std::atomic_compare_exchange_strong(&m_objects[id].m_active, &old_active, new_active)) {
							m_p_timer->add_event(m_objects[id].m_id, 1000, EV_RANDOM_MOVE, -1);
						}
					}
					else {
						OVERLAPPED_EX* over_ex = new OVERLAPPED_EX;
						over_ex->m_option = OP_NPC_MOVE;
						over_ex->m_target_id = client_id;
						PostQueuedCompletionStatus(m_h_iocp, 1, m_objects[id].m_id, &over_ex->m_overlapped);
					}
				}
			}
		}

		// send
		m_objects[client_id].send_move_packet(m_objects[client_id]);

		for (int player_id : new_view_list) {
			if (0 == old_view_list.count(player_id)) {
				m_objects[client_id].send_add_object_packet(m_objects[player_id]);
				m_objects[player_id].send_add_object_packet(m_objects[client_id]);
			}
			else {
				m_objects[player_id].send_move_packet(m_objects[client_id]);
			}
		}

		for (int player_id : old_view_list) {
			if (0 == new_view_list.count(player_id)) {
				m_objects[client_id].send_remove_object_packet(m_objects[player_id]);
				m_objects[player_id].send_remove_object_packet(m_objects[client_id]);
			}
		}

		break;
	}
	case CS_TELEPORT:
	{
		teleport(m_objects[client_id]);

		// Sector
		int old_sector_x = m_objects[client_id].m_sector_x;
		int old_sector_y = m_objects[client_id].m_sector_y;

		int new_sector_x = m_objects[client_id].m_x / SECTOR_WIDTH;
		int new_sector_y = m_objects[client_id].m_y / SECTOR_HEIGHT;

		if (old_sector_x != new_sector_x || old_sector_y != new_sector_y) {
			m_sectors.change_sector(client_id, old_sector_x, old_sector_y, new_sector_x, new_sector_y);
		}

		std::unordered_set<SECTOR*> sectors_arr;

		m_sectors.calculate_around_sector(m_objects[client_id].m_x, m_objects[client_id].m_y, sectors_arr);

		// view list
		std::unordered_set<int> new_view_list;
		m_objects[client_id].m_vl_lock.lock();
		std::unordered_set<int> old_view_list = m_objects[client_id].m_view_list;
		m_objects[client_id].m_vl_lock.unlock();

		for (auto& sector : sectors_arr) {
			std::lock_guard<std::mutex> lock((*sector).mtx);

			if ((*sector).ids.size() == 0) continue;

			for (auto& id : (*sector).ids) {
				{
					std::lock_guard<std::mutex> slock(m_objects[id].m_s_lock);

					if (ST_INGAME != m_objects[id].m_state) {
						continue;
					}
				}

				if (m_objects[id].m_id == client_id) {
					continue;
				}

				if (false == m_objects[client_id].can_see(m_objects[id])) {
					continue;
				}

				new_view_list.insert(id);

				//
				if (true == m_objects[id].is_NPC()) {
					if (m_objects[id].m_active == false) {
						bool old_active = false;
						bool new_active = true;

						if (true == std::atomic_compare_exchange_strong(&m_objects[id].m_active, &old_active, new_active)) {
							m_p_timer->add_event(m_objects[id].m_id, 1000, EV_RANDOM_MOVE, -1);
						}
					}
					else {
						OVERLAPPED_EX* over_ex = new OVERLAPPED_EX;
						over_ex->m_option = OP_NPC_MOVE;
						over_ex->m_target_id = client_id;
						PostQueuedCompletionStatus(m_h_iocp, 1, m_objects[id].m_id, &over_ex->m_overlapped);
					}
				}
			}
		}

		// send
		m_objects[client_id].send_move_packet(m_objects[client_id]);

		for (int player_id : new_view_list) {
			if (0 == old_view_list.count(player_id)) {
				m_objects[client_id].send_add_object_packet(m_objects[player_id]);
				m_objects[player_id].send_add_object_packet(m_objects[client_id]);
			}
			else {
				m_objects[player_id].send_move_packet(m_objects[client_id]);
			}
		}

		for (int player_id : old_view_list) {
			if (0 == new_view_list.count(player_id)) {
				m_objects[client_id].send_remove_object_packet(m_objects[player_id]);
				m_objects[player_id].send_remove_object_packet(m_objects[client_id]);
			}
		}

		break;
	}
	case CS_SKILL_MOVEMENT:
		m_objects[client_id].m_skill_mv = true;

		m_p_timer->add_event(client_id, 2000, EV_SKILL_MOVEMENT, -1);
		break;
	case CS_ITEM_POSTION_S:
		if (m_objects[client_id].m_potion_s <= 0) {
			break;
		}
		else {
			m_objects[client_id].m_potion_s -= 1;
		}
	{
		m_objects[client_id].heal(10);
		
		m_objects[client_id].send_stat_change_packet(m_objects[client_id]);
		break;
	}
	case CS_ITEM_POSTION_L:
	{
		if (m_objects[client_id].m_potion_l <= 0) {
			break;
		}
		else {
			m_objects[client_id].m_potion_l -= 1;
		}

		m_objects[client_id].heal(20);

		m_objects[client_id].send_stat_change_packet(m_objects[client_id]);
		break;
	}
	case CS_ITEM_POISION:
	{
		m_objects[client_id].heal(-10);

		check_dead(m_objects[client_id]);

		m_objects[client_id].send_stat_change_packet(m_objects[client_id]);
		break;
	}
	case CS_CHAT:
	{
		CS_CHAT_PACKET* p = reinterpret_cast<CS_CHAT_PACKET*>(packet);

		std::unordered_set<SECTOR*> sectors_arr;

		m_sectors.calculate_around_sector(m_objects[client_id].m_x, m_objects[client_id].m_y, sectors_arr);

		for (auto& sector : sectors_arr) {
			std::lock_guard<std::mutex> lock((*sector).mtx);

			if ((*sector).ids.size() == 0) continue;

			for (auto& id : (*sector).ids) {
				{
					std::lock_guard<std::mutex> slock(m_objects[id].m_s_lock);

					if (ST_INGAME != m_objects[id].m_state) {
						continue;
					}
				}

				if (m_objects[id].m_id == m_objects[client_id].m_id) {
					continue;
				}

				m_objects[id].send_chat_packet(m_objects[client_id], p->mess);
			}
		}
		break;
	}
	case CS_ATTACK:
	{
		std::vector<int> ids;

		std::unordered_set<SECTOR*> sectors_arr;

		m_sectors.calculate_around_sector(m_objects[client_id].m_x, m_objects[client_id].m_y, sectors_arr);

		for (auto& sector : sectors_arr) {
			std::lock_guard<std::mutex> lock((*sector).mtx);

			if ((*sector).ids.size() == 0) continue;

			for (auto& id : (*sector).ids) {
				{
					std::lock_guard<std::mutex> slock(m_objects[id].m_s_lock);

					if (ST_INGAME != m_objects[id].m_state) {
						continue;
					}
				}

				if (m_objects[id].m_id == client_id) {
					continue;
				}

				if (false == m_objects[client_id].can_see(m_objects[id])) {
					continue;
				}

				if (true == m_objects[client_id].attack(m_objects[id])) {
					m_objects[client_id].send_attack_packet(m_objects[id], 10);
					ids.emplace_back(id);
				}
			}
		}

		for (auto& id : ids) {
			send_update_around(id);
		}
		break;
	}
	case CS_ATTACK_FORWARD:
	{
		std::vector<int> ids;

		std::unordered_set<SECTOR*> sectors_arr;

		m_sectors.calculate_around_sector(m_objects[client_id].m_x, m_objects[client_id].m_y, sectors_arr);

		for (auto& sector : sectors_arr) {
			std::lock_guard<std::mutex> lock((*sector).mtx);

			if ((*sector).ids.size() == 0) continue;

			for (auto& id : (*sector).ids) {
				{
					std::lock_guard<std::mutex> slock(m_objects[id].m_s_lock);

					if (ST_INGAME != m_objects[id].m_state) {
						continue;
					}
				}

				if (m_objects[id].m_id == client_id) {
					continue;
				}

				if (false == m_objects[client_id].can_see(m_objects[id])) {
					continue;
				}

				if (true == m_objects[client_id].attack_forward(m_objects[id])) {
					m_objects[client_id].send_attack_packet(m_objects[id], 20);
					ids.emplace_back(id);
				}
			}
		}

		for (auto& id : ids) {
			send_update_around(id);
		}
		break;
	}
	default:
		break;
	}

}

void CNetworker::disconnect(int client_id) {
	if (m_objects[client_id].m_state == ST_INGAME) {
		m_sectors.remove_id(client_id, m_objects[client_id].m_sector_x, m_objects[client_id].m_sector_y);

		m_p_database->add_query(m_objects[client_id], Q_LOGOUT, -1);
	}

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
