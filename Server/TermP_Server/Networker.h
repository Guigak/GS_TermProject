#pragma once
#include "common.h"
#include "Object.h"
#include "Sector.h"

class CNetworker {
	HANDLE m_h_iocp;
	SOCKET m_s_socket;	// s : server
	SOCKET m_c_socket;	// c : client
	OVERLAPPED_EX m_over_ex;

	std::array<CObject, MAX_NPC + MAX_USER> m_objects;

	CSector m_sectors;

public :
	CNetworker() {};
	~CNetworker() {};

	void init();
	void work();
	void clear();

	int get_new_client_id();
	void prcs_packet(int client_id, char* packet);
	void disconnect(int client_id);
};