#pragma once
#include "common.h"
#include "Object.h"
#include "Sector.h"
#include "Timer.h"
#include "Database.h"
#include "Map.h"

class CNetworker {
	HANDLE m_h_iocp;
	SOCKET m_s_socket;	// s : server
	SOCKET m_c_socket;	// c : client
	OVERLAPPED_EX m_over_ex;

	CSector m_sectors;

	CTimer* m_p_timer;

	CDatabase* m_p_database;

	CMap m_map;

public :
	CNetworker() {};
	~CNetworker() {};

	void set_timer(CTimer* p_timer);
	void set_database(CDatabase* p_database);

	void init();
	void work();
	void clear();

	void teleport(CObject& object);
	void random_move(CObject& object);

	void check_dead(CObject& object);

	int get_new_client_id();
	void prcs_packet(int client_id, char* packet);
	void disconnect(int client_id);
};