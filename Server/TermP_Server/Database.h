#pragma once
#include "common.h"

enum QUERY_TYPE {
	Q_LOGIN
};

struct QUERY {
	int id;
	char name[NAME_SIZE];
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

	void add_query(int id, char name[NAME_SIZE], QUERY_TYPE type, int target_id);

	bool login(int id, char name[NAME_SIZE]);

	void set_iocp(HANDLE* p_h_iocp);
};
