#pragma once
#include "common.h"
#include "Object.h"

enum QUERY_TYPE {
	Q_LOGIN, Q_LOGOUT
};

struct QUERY {
	CObject* p_object;
	QUERY_TYPE type;
	int target_id;
};

struct PLAYER_INFO {
	char nickname[NAME_SIZE];
	int visual;
	int hp;
	int	max_hp;
	int exp;
	int level;
	short x, y;
	int potion_s, potion_l;
};

class CDatabase {
	std::queue<QUERY> m_queue;
	std::mutex m_q_mtx;	// q : queue

	SQLHENV m_henv;
	SQLHDBC m_hdbc;
	SQLHSTMT m_hstmt = 0;

	std::mutex m_s_mtx;	// s : sql

	HANDLE* m_p_h_iocp;

public :
	CDatabase() {}
	~CDatabase() {};

	void work();

	bool connect();
	void disconnect();
	void add_query(CObject& object, QUERY_TYPE type, int target_id);

	int login(CObject* p_object);
	bool logout(CObject* p_object);
	bool set_ingame(CObject* p_object);
	bool clear_Ingame();

	bool create_data(CObject* p_object);

	void set_iocp(HANDLE* p_h_iocp);
};

