#include "Object.h"

CObject::CObject() {
	m_state = ST_FREE;
	m_active = false;
	m_id = -1;
	m_socket = 0;
	m_x = -1;
	m_y = -1;
	m_name[0] = 0;
	m_remain_size = 0;
	m_last_move_time = 0;
	m_sector_x = -1;
	m_sector_y = -1;
}

void CObject::recv() {
	DWORD recv_flag = 0;
	memset(&m_over_ex.m_overlapped, 0, sizeof(m_over_ex.m_overlapped));
	m_over_ex.m_wsabuf.len = BUF_SIZE - m_remain_size;
	m_over_ex.m_wsabuf.buf = m_over_ex.m_s_buf + m_remain_size;
	WSARecv(m_socket, &m_over_ex.m_wsabuf, 1, 0, &recv_flag,  &m_over_ex.m_overlapped, 0);
}

void CObject::send(void* packet) {
	if (true == is_NPC()) {
		return;
	}

	OVERLAPPED_EX* sdata = new OVERLAPPED_EX{ reinterpret_cast<char*>(packet) };
	WSASend(m_socket, &sdata->m_wsabuf, 1, 0, 0, &sdata->m_overlapped, 0);
}

void CObject::send_login_info_packet() {
	SC_LOGIN_INFO_PACKET packet;
	packet.size = sizeof(SC_LOGIN_INFO_PACKET);
	packet.type = SC_LOGIN_INFO;
	packet.id = m_id;
	packet.x = m_x;
	packet.y = m_y;
	strcpy_s(packet.name, m_name);

	packet.level = m_level;
	packet.max_hp = m_max_hp;
	packet.hp = m_hp;
	packet.exp = m_exp;
	packet.potion_s = m_potion_s;
	packet.potion_l = m_potion_l;
	send(&packet);
}

void CObject::send_login_ok_packet() {
}

void CObject::send_login_fail_packet() {
}

void CObject::send_add_object_packet(CObject& object) {
	if (true == is_NPC()) {
		return;
	}

	m_vl_lock.lock();
	m_view_list.insert(object.m_id);
	m_vl_lock.unlock();

	SC_ADD_OBJECT_PACKET packet;
	packet.size = sizeof(SC_ADD_OBJECT_PACKET);
	packet.type = SC_ADD_OBJECT;
	packet.id = object.m_id;
	packet.x = object.m_x;
	packet.y = object.m_y;
	packet.max_hp = object.m_max_hp;
	packet.hp = object.m_hp;
	strcpy_s(packet.name, object.m_name);
	send(&packet);
}

void CObject::send_remove_object_packet(CObject& object) {
	if (true == is_NPC()) {
		return;
	}

	m_vl_lock.lock();
	m_view_list.erase(object.m_id);
	m_vl_lock.unlock();

	SC_REMOVE_OBJECT_PACKET packet;
	packet.id = object.m_id;
	packet.size = sizeof(SC_REMOVE_OBJECT_PACKET);
	packet.type = SC_REMOVE_OBJECT;
	send(&packet);
}

void CObject::send_stat_change_packet(CObject& objcet) {
	if (true == is_NPC()) {
		return;
	}

	SC_STAT_CHANGE_PACKET packet;
	packet.id = objcet.m_id;
	packet.size = sizeof(SC_STAT_CHANGE_PACKET);
	packet.type = SC_STAT_CHANGE;
	packet.hp = objcet.m_hp;
	packet.exp = objcet.m_exp;
	packet.potion_s = objcet.m_potion_s;
	packet.potion_l = objcet.m_potion_l;
	send(&packet);
}

void CObject::send_move_packet(CObject& object) {
	if (true == is_NPC()) {
		return;
	}

	SC_MOVE_OBJECT_PACKET packet;
	packet.id = object.m_id;
	packet.size = sizeof(SC_MOVE_OBJECT_PACKET);
	packet.type = SC_MOVE_OBJECT;
	packet.x = object.m_x;
	packet.y = object.m_y;
	packet.move_time = object.m_last_move_time;
	send(&packet);
}

void CObject::send_chat_packet(CObject& object, const char* message) {
	if (true == is_NPC()) {
		return;
	}

	SC_CHAT_PACKET packet;
	packet.size = sizeof(SC_CHAT_PACKET);
	packet.type = SC_CHAT;
	packet.id = object.m_id;
	strcpy_s(packet.name, object.m_name);
	strcpy_s(packet.mess, message);
	send(&packet);
}

bool CObject::can_see(CObject& object) {

	if (std::abs(object.m_x - m_x) > VIEW_RANGE / 2) {
		return false;
	}

	if (std::abs(object.m_y - m_y) > VIEW_RANGE / 2) {
		return false;
	}

	return true;
}

void CObject::send_attack_packet(CObject& object, int degree) {
	if (true == is_NPC()) {
		return;
	}

	SC_ATTACK_PACKET packet;
	packet.size = sizeof(SC_ATTACK_PACKET);
	packet.type = SC_ATTACK;
	packet.id = m_id;
	packet.target_id = object.m_id;
	packet.degree = degree;
	packet.exp = object.damaged(degree);
	send(&packet);
}

void CObject::heal(int degree) {
	m_hp += degree;
	m_hp = m_hp > m_max_hp ? m_max_hp : m_hp;
}

int CObject::damaged(int degree) {
	m_hp -= degree;

	return m_hp < 0 ? m_exp : 0;
}

bool CObject::attack(CObject& object) {
	int target_x = object.m_x;
	int target_y = object.m_y;

	int x = m_x;
	int y = m_y;

	int new_x = x;
	int new_y = y;

	if (target_x == new_x && target_y == new_y) {
		return true;
	}

	for (int i = -1; i < 2; i += 2) {
		new_x = x;
		new_y = y;

		new_y += i;

		if (target_x == new_x && target_y == new_y) {
			return true;
		}

		new_x = x;
		new_y = y;

		new_x += i;

		if (target_x == new_x && target_y == new_y) {
			return true;
		}
	}

	return false;
}

bool CObject::attack_forward(CObject& object) {
	int target_x = object.m_x;
	int target_y = object.m_y;

	int x = m_x;
	int y = m_y;

	int cal_x = 0;
	int cal_y = 0;

	switch (m_direction) {
	case 0:
		cal_y--;
		break;
	case 1:
		cal_y++;
		break;
	case 2:
		cal_x--;
		break;
	case 3:
		cal_x++;
		break;
	default:
		break;
	}

	int new_x = x;
	int new_y = y;

	for (int i = 1; i <= 3; ++i) {
		new_x += cal_x;
		new_y += cal_y;

		if (target_x == new_x && target_y == new_y) {
			return true;
		}
	}

	return false;
}
