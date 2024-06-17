#pragma once
#include "common.h"
#include "Object.h"
#include "Sector.h"
#include "Timer.h"

class CNetworker {
	HANDLE m_h_iocp;
	SOCKET m_s_socket;	// s : server
	SOCKET m_c_socket;	// c : client
	OVERLAPPED_EX m_over_ex;

	CSector m_sectors;

	CTimer* m_p_timer;

public :
	CNetworker() {};
	~CNetworker() {};

	void set_timer(CTimer* p_timer);

	void init();
	void work();
	void clear();

	void random_move(CObject& object);

	int get_new_client_id();
	void prcs_packet(int client_id, char* packet);
	void disconnect(int client_id);
};