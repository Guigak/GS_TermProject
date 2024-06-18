#pragma once
#include "common.h"

enum OBJECT_STATE {
	ST_FREE, ST_ALLOC, ST_INGAME
};

class CObject {
	OVERLAPPED_EX m_over_ex;	// used only in recv

public :
	std::mutex m_s_lock;	// s : state
	OBJECT_STATE m_state;
	int m_id;
	SOCKET m_socket;
	short m_x;
	short m_y;
	char m_name[NAME_SIZE];
	std::unordered_set<int> m_view_list;
	std::mutex m_vl_lock;	// vl : view list
	int m_remain_size;
	int m_last_move_time;
	int m_sector_x;
	int m_sector_y;

	//
	int m_level;
	int m_max_hp;
	int m_hp;
	int m_exp;

	//
	int m_potion_s;
	int m_potion_l;

	//
	int m_direction = 0;	// 0 : UP, 1 : DOWN, 2 : LEFT, 3 : RIGHT

	bool m_skill_mv;

	std::atomic_bool m_active;

	// Lua
	lua_State* m_Lua;
	std::mutex m_lua_mtx;

	//
	CObject();
	~CObject() {};

	void recv();
	void send(void* packet);

	void send_login_info_packet();
	void send_login_ok_packet();
	void send_login_fail_packet();
	
	void send_add_object_packet(CObject& object);
	void send_remove_object_packet(CObject& object);
	void send_stat_change_packet(CObject& objcet);
	void send_attack_packet(CObject& object, int degree);

	void send_move_packet(CObject& object);

	void send_chat_packet(CObject& object, const char* message);

	//
	bool is_NPC() { return m_id < MAX_NPC; }
	bool can_see(CObject& object);

	void heal(int degree);
	int damaged(int degree);

	//
	bool attack(CObject& object);
	bool attack_forward(CObject& object);
};

