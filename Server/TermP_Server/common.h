#pragma once
#include <iostream>
#include <fstream>
#include <thread>
#include <mutex>
#include <array>
#include <vector>
#include <queue>
#include <random>
#include <unordered_set>;

// Socket
#include <WS2tcpip.h>
#include <MSWSock.h>

#pragma comment(lib, "WS2_32.lib")
#pragma comment(lib, "MSWSock.lib")

// Lua
#include "include/lua.hpp"

#pragma comment(lib, "lua54.lib")

// Protocol
#include "protocol.h"

// Database
#include <locale.h>
#include <sqlext.h>

// Class
enum OVERLAPPED_OPTION {
	OP_ACCEPT, OP_RECV, OP_SEND, OP_RANDOM_MOVE, OP_NPC_MOVE, OP_LOGIN_OK, OP_LOGIN_FAIL, OP_SKILL_MOVEMENT
};

class OVERLAPPED_EX {
public: 
	WSAOVERLAPPED m_overlapped;
	WSABUF m_wsabuf;
	char m_s_buf[BUF_SIZE];	// s : send
	OVERLAPPED_OPTION m_option;
	int m_target_id;
	long long m_data;

	OVERLAPPED_EX() {
		m_wsabuf.len = BUF_SIZE;
		m_wsabuf.buf = m_s_buf;
		m_option = OP_RECV;
		m_target_id = -1;
		m_data = -1;
		ZeroMemory(&m_overlapped, sizeof(m_overlapped));
	}
	OVERLAPPED_EX(char* packet) {
		m_wsabuf.len = packet[0];
		m_wsabuf.buf = m_s_buf;
		ZeroMemory(&m_overlapped, sizeof(m_overlapped));
		m_option = OP_SEND;
		memcpy(m_s_buf, packet, packet[0]);
	}
};