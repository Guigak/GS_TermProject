#include <SFML/Graphics.hpp>
#include <SFML/Network.hpp>
#include <iostream>
#include <fstream>
#include <unordered_map>
#include <Windows.h>
#include <vector>
#include <chrono>
using namespace std;

#include "../../Server/TermP_Server/protocol.h"

sf::TcpSocket s_socket;

constexpr float MINIMAP_WIDTH = 200.0f;
constexpr float MINIMAP_HEIGHT = 200.0f;

constexpr float MINIMAP_PLAYER_SIZE = 5.0f;

constexpr auto TILE_WIDTH = 40;
constexpr auto WINDOW_WIDTH = CLIENT_WIDTH * TILE_WIDTH;
constexpr auto WINDOW_HEIGHT = CLIENT_HEIGHT * TILE_WIDTH;

//
constexpr float CHATING_HEIGHT = 25.0f;
constexpr float CHATING_WIDTH = WINDOW_WIDTH * 2 / 3;

int g_left_x;
int g_top_y;
int g_myid;
int g_level = 1;
int g_hp = 100;
int g_max_hp = 100;
int g_exp = 0;
int g_potion_s = 0;
int g_potion_l = 0;

std::chrono::high_resolution_clock::time_point g_last_move_time;

sf::RenderWindow* g_window;
sf::Font g_font;

class OBJECT {
private:
	bool m_showing;

	sf::Text m_name;
	sf::Text m_chat;
	sf::RectangleShape m_hp_bar;
	chrono::system_clock::time_point m_mess_end_time;
public:
	sf::Sprite m_sprite;
	int m_x, m_y;
	int m_id;
	int m_max_hp;
	int m_hp;
	char name[NAME_SIZE];
	OBJECT(sf::Texture& t, int x, int y, int x2, int y2) {
		m_showing = false;
		m_sprite.setTexture(t);
		m_sprite.setTextureRect(sf::IntRect(x, y, x2, y2));
		m_hp_bar.setFillColor(sf::Color::Red);
	}
	OBJECT() {
		m_showing = false;
	}
	void show()
	{
		m_showing = true;
	}
	void hide()
	{
		m_showing = false;
	}

	void a_move(int x, int y) {
		m_sprite.setPosition((float)x, (float)y);
	}

	void a_draw() {
		g_window->draw(m_sprite);
	}

	void move(int x, int y) {
		m_x = x;
		m_y = y;
	}
	void draw() {
		if (false == m_showing) return;
		float rx = (m_x - g_left_x) * TILE_WIDTH + 1;
		float ry = (m_y - g_top_y) * TILE_WIDTH + 1;
		m_sprite.setPosition(rx, ry);
		g_window->draw(m_sprite);
		auto size = m_name.getGlobalBounds();
		if (m_mess_end_time < chrono::system_clock::now()) {
			m_name.setPosition(rx + TILE_WIDTH / 2 - size.width / 2, ry - 10);
			g_window->draw(m_name);
		}
		else {
			m_chat.setPosition(rx + TILE_WIDTH / 2 - size.width / 2, ry - 10);
			g_window->draw(m_chat);
		}

		if (g_myid != m_id) {
			m_hp_bar.setSize(sf::Vector2f((float)TILE_WIDTH * m_hp / m_max_hp, 5.0f));
			m_hp_bar.setPosition(rx + TILE_WIDTH / 2 - TILE_WIDTH / 2, ry - 15);
			g_window->draw(m_hp_bar);
		}
	}
	void set_name(const char str[]) {
		m_name.setFont(g_font);
		m_name.setString(str);
		m_name.setFillColor(sf::Color(255, 255, 0));
		m_name.setStyle(sf::Text::Bold);
	}
	void set_chat(const char str[]) {
		m_chat.setFont(g_font);
		m_chat.setString(str);
		m_chat.setFillColor(sf::Color(255, 255, 255));
		m_chat.setStyle(sf::Text::Bold);
		m_mess_end_time = chrono::system_clock::now() + chrono::seconds(3);
	}
};

OBJECT avatar;
unordered_map<int, OBJECT> players;
unordered_map<int, OBJECT> tiles;

sf::Texture* board;
sf::Texture* pieces;

// ui
sf::RectangleShape status_rect;

sf::RectangleShape minimap_map;
sf::RectangleShape minimap_plr;

sf::RectangleShape potions_rect;
sf::RectangleShape potion_s_rect;
sf::RectangleShape potion_l_rect;

sf::RectangleShape chating_rect;
sf::RectangleShape chating_log_rect;
std::vector<std::string> chat_logs;
std::string chat_input;
bool g_chating = false;

//
short tile_map[2000][2000]{0};

void client_initialize()
{
	board = new sf::Texture;
	pieces = new sf::Texture;
	board->loadFromFile("map.png");
	pieces->loadFromFile("chess2.png");
	if (false == g_font.loadFromFile("cour.ttf")) {
		cout << "Font Loading Error!\n";
		exit(-1);
	}

	// map tiles
	for (int i = 0; i < 48; ++i) {
		tiles[i] = OBJECT{ *board, 32 * (i % 8), 32 * (i / 8), 32, 32 };
		tiles[i].m_sprite.setScale(sf::Vector2f((float)TILE_WIDTH / 32.f, (float)TILE_WIDTH / 32.f));
	}

	avatar = OBJECT{ *pieces, 128, 0, 64, 64 };
	avatar.m_sprite.setScale(sf::Vector2f((float)TILE_WIDTH / 64.f, (float)TILE_WIDTH / 64.f));
	avatar.move(4, 4);

	//
	status_rect.setFillColor(sf::Color::Black);
	status_rect.setSize(sf::Vector2f((float)WINDOW_WIDTH / 3, 20.0f * 4));

	minimap_map.setFillColor(sf::Color::Black);
	minimap_map.setSize(sf::Vector2f(200.0f, 200.0f));
	minimap_plr.setFillColor(sf::Color::White);
	minimap_plr.setSize(sf::Vector2f(MINIMAP_PLAYER_SIZE, MINIMAP_PLAYER_SIZE));

	potions_rect.setFillColor(sf::Color::Black);
	potions_rect.setSize(sf::Vector2f(100.0f, 50.0f));

	potion_s_rect.setFillColor(sf::Color::White);
	potion_s_rect.setSize(sf::Vector2f(40.0f, 40.0f));

	potion_l_rect.setFillColor(sf::Color::White);
	potion_l_rect.setSize(sf::Vector2f(40.0f, 40.0f));

	//
	chating_rect.setFillColor(sf::Color::Black);
	chating_rect.setSize(sf::Vector2f(WINDOW_WIDTH * 2 / 3, CHATING_HEIGHT));

	chating_log_rect.setFillColor(sf::Color::White);
	chating_log_rect.setSize(sf::Vector2f(WINDOW_WIDTH * 2 / 3, CHATING_HEIGHT * 5));
}

void client_finish()
{
	players.clear();
	delete board;
	delete pieces;
}

void ProcessPacket(char* ptr)
{
	static bool first_time = true;
	switch (ptr[2])
	{
	case SC_LOGIN_INFO:
	{
		SC_LOGIN_INFO_PACKET* packet = reinterpret_cast<SC_LOGIN_INFO_PACKET*>(ptr);
		g_myid = packet->id;
		strcpy_s(avatar.name, packet->name);
		avatar.m_x = packet->x;
		avatar.m_y = packet->y;
		g_left_x = packet->x - CLIENT_WIDTH / 2;
		g_top_y = packet->y - CLIENT_HEIGHT / 2;
		avatar.set_name(packet->name);

		g_level = packet->level;
		g_max_hp = packet->max_hp;
		g_hp = packet->hp;
		g_exp = packet->exp;
		g_potion_s = packet->potion_s;
		g_potion_l = packet->potion_l;

		avatar.show();
	}
	break;

	case SC_ADD_OBJECT:
	{
		SC_ADD_OBJECT_PACKET* my_packet = reinterpret_cast<SC_ADD_OBJECT_PACKET*>(ptr);
		int id = my_packet->id;

		if (id == g_myid) {
			avatar.move(my_packet->x, my_packet->y);
			g_left_x = my_packet->x - CLIENT_WIDTH / 2;
			g_top_y = my_packet->y - CLIENT_HEIGHT / 2;
			avatar.show();
		}
		else {
			players[id] = OBJECT{ *pieces, 0, 0, 64, 64 };
			players[id].m_id = id;
			strcpy_s(players[id].name, my_packet->name);
			players[id].m_max_hp = my_packet->max_hp;
			players[id].m_hp = my_packet->hp;
			players[id].m_sprite.setScale(sf::Vector2f((float)TILE_WIDTH / 64.f, (float)TILE_WIDTH / 64.f));
			players[id].move(my_packet->x, my_packet->y);
			players[id].set_name(my_packet->name);
			players[id].show();
		}
		break;
	}
	case SC_MOVE_OBJECT:
	{
		SC_MOVE_OBJECT_PACKET* my_packet = reinterpret_cast<SC_MOVE_OBJECT_PACKET*>(ptr);
		int other_id = my_packet->id;
		if (other_id == g_myid) {
			avatar.move(my_packet->x, my_packet->y);
			g_left_x = my_packet->x - CLIENT_WIDTH / 2;
			g_top_y = my_packet->y - CLIENT_HEIGHT / 2;
		}
		else {
			players[other_id].move(my_packet->x, my_packet->y);
		}
		break;
	}

	case SC_REMOVE_OBJECT:
	{
		SC_REMOVE_OBJECT_PACKET* my_packet = reinterpret_cast<SC_REMOVE_OBJECT_PACKET*>(ptr);
		int other_id = my_packet->id;
		if (other_id == g_myid) {
			avatar.hide();
		}
		else {
			players.erase(other_id);
		}
		break;
	}
	case SC_CHAT:
	{
		SC_CHAT_PACKET* my_packet = reinterpret_cast<SC_CHAT_PACKET*>(ptr);
		int other_id = my_packet->id;
		 
		if (other_id == g_myid) {
			avatar.set_chat(my_packet->mess);
		}
		else if (other_id < MAX_NPC) {
			if (players.count(other_id) != 0) {
				players[other_id].set_chat(my_packet->mess);
			}
		}
		else {
			while (chat_logs.size() > 5) {
				chat_logs.erase(chat_logs.begin());
			}

			std::string temp_chat;
			temp_chat.append(my_packet->name).append(" : ").append(my_packet->mess);
			chat_logs.emplace_back(temp_chat);
		}
		break;
	}
	case SC_STAT_CHANGE:
	{
		SC_STAT_CHANGE_PACKET* my_packet = reinterpret_cast<SC_STAT_CHANGE_PACKET*>(ptr);
		int id = my_packet->id;

		if (id == g_myid) {
			g_hp = my_packet->hp;
			g_exp = my_packet->exp;
			g_potion_s = my_packet->potion_s;
			g_potion_l = my_packet->potion_l;
		}
		else {
			players[id].m_max_hp = my_packet->max_hp;
			players[id].m_hp = my_packet->hp;
		}

		break;
	}
	case SC_ATTACK:
	{
		SC_ATTACK_PACKET* my_packet = reinterpret_cast<SC_ATTACK_PACKET*>(ptr);
		int id = my_packet->id;
		int other_id = my_packet->target_id;

		if (id == g_myid) {
			while (chat_logs.size() > 5) {
				chat_logs.erase(chat_logs.begin());
			}

			if (my_packet->exp == 0) {
				std::string temp_chat;
				temp_chat.append("You attacked ").append(players[other_id].name).append(" with ").append(std::to_string(my_packet->degree)).append(" damage");
				chat_logs.emplace_back(temp_chat);
			}
			else {
				std::string temp_chat;
				temp_chat.append("You killed ").append(players[other_id].name).append(" and got ").append(std::to_string(my_packet->exp)).append(" exp");
				chat_logs.emplace_back(temp_chat);
			}
		}
		else {

		}

		break;
	}
	default:
		printf("Unknown PACKET type [%d]\n", ptr[1]);
	}
}

void process_data(char* net_buf, size_t io_byte)
{
	char* ptr = net_buf;
	static size_t in_packet_size = 0;
	static size_t saved_packet_size = 0;
	static char packet_buffer[BUF_SIZE];

	while (0 != io_byte) {
		if (0 == in_packet_size) in_packet_size = ptr[0];
		if (io_byte + saved_packet_size >= in_packet_size) {
			memcpy(packet_buffer + saved_packet_size, ptr, in_packet_size - saved_packet_size);
			ProcessPacket(packet_buffer);
			ptr += in_packet_size - saved_packet_size;
			io_byte -= in_packet_size - saved_packet_size;
			in_packet_size = 0;
			saved_packet_size = 0;
		}
		else {
			memcpy(packet_buffer + saved_packet_size, ptr, io_byte);
			saved_packet_size += io_byte;
			io_byte = 0;
		}
	}
}

void client_main()
{
	char net_buf[BUF_SIZE];
	size_t	received;

	auto recv_result = s_socket.receive(net_buf, BUF_SIZE, received);
	if (recv_result == sf::Socket::Error)
	{
		wcout << L"Recv 에러!";
		exit(-1);
	}
	if (recv_result == sf::Socket::Disconnected) {
		wcout << L"Disconnected\n";
		exit(-1);
	}
	if (recv_result != sf::Socket::NotReady)
		if (received > 0) process_data(net_buf, received);

	for (int i = 0; i < CLIENT_WIDTH; ++i)
		for (int j = 0; j < CLIENT_HEIGHT; ++j)
		{
			int tile_x = i + g_left_x;
			int tile_y = j + g_top_y;
			if ((tile_x < 0) || (tile_y < 0) || (tile_x > WORLD_WIDTH - 1) || (tile_y > WORLD_WIDTH - 1)) continue;

			tiles[tile_map[tile_y][tile_x]].a_move(TILE_WIDTH * i, TILE_WIDTH * j);
			tiles[tile_map[tile_y][tile_x]].a_draw();
		}
	avatar.draw();
	for (auto& pl : players) pl.second.draw();
	sf::Text text;
	text.setFont(g_font);
	text.setFillColor(sf::Color::White);
	char buf[100];
	sprintf_s(buf, "(%d, %d)", avatar.m_x, avatar.m_y);
	text.setString(buf);
	g_window->draw(text);

	//
	status_rect.setPosition((float)WINDOW_WIDTH / 2 - (float)WINDOW_WIDTH / 6, 0);
	g_window->draw(status_rect);

	// level
	sprintf_s(buf, "LEVEL : %d", g_level);
	text.setString(buf);
	auto size = text.getGlobalBounds();
	text.setPosition((float)WINDOW_WIDTH / 2 - size.width / 2, 0);
	g_window->draw(text);

	// hp
	sprintf_s(buf, "HP %d / %d", g_hp, g_max_hp);
	text.setString(buf);
	size = text.getGlobalBounds();
	text.setPosition((float)WINDOW_WIDTH / 2 - size.width / 2, size.height);
	g_window->draw(text);

	// exp
	int max_exp = std::pow(2, g_level - 1) * 100;
	sprintf_s(buf, "EXP %d / %d", g_exp, (int)max_exp);
	text.setString(buf);
	size = text.getGlobalBounds();
	text.setPosition((float)WINDOW_WIDTH / 2 - size.width / 2, size.height * 2);
	g_window->draw(text);

	//
	float offset_width = WINDOW_WIDTH - MINIMAP_WIDTH;
	minimap_map.setPosition(offset_width, 0.0f);
	g_window->draw(minimap_map);

	minimap_plr.setPosition(offset_width + MINIMAP_WIDTH / W_WIDTH * avatar.m_x - MINIMAP_PLAYER_SIZE / 2.0f, MINIMAP_HEIGHT / W_WIDTH * avatar.m_y - MINIMAP_PLAYER_SIZE / 2.0f);
	g_window->draw(minimap_plr);

	// potion
	potions_rect.setPosition(WINDOW_WIDTH - 100.0f, WINDOW_HEIGHT - 50.0f);
	g_window->draw(potions_rect);

	potion_s_rect.setPosition(WINDOW_WIDTH - 95.0f, WINDOW_HEIGHT - 45.0f);
	g_window->draw(potion_s_rect);

	potion_l_rect.setPosition(WINDOW_WIDTH -45.0f, WINDOW_HEIGHT - 45.0f);
	g_window->draw(potion_l_rect);

	sprintf_s(buf, "%d", g_potion_s);
	text.setString(buf);
	text.setFillColor(sf::Color::Black);
	size = text.getGlobalBounds();
	text.setPosition((float)WINDOW_WIDTH - 75.0f - size.width / 2.0f, (float)WINDOW_HEIGHT - 25.0f - size.height);
	g_window->draw(text);

	sprintf_s(buf, "%d", g_potion_l);
	text.setString(buf);
	size = text.getGlobalBounds();
	text.setPosition((float)WINDOW_WIDTH - 25.0f - size.width / 2.0f, (float)WINDOW_HEIGHT - 25.0f - size.height);
	g_window->draw(text);

	// chating
	chating_rect.setPosition(0, WINDOW_HEIGHT - CHATING_HEIGHT);
	g_window->draw(chating_rect);

	if (g_chating) {
		chating_log_rect.setPosition(0, WINDOW_HEIGHT - CHATING_HEIGHT * 6);
		g_window->draw(chating_log_rect);
	}

	while (chat_logs.size() > 5) {
		chat_logs.erase(chat_logs.begin());
	}

	sf::Text chat_text;
	chat_text.setFont(g_font);
	chat_text.setScale(0.5f, 0.5f);
	chat_text.setFillColor(sf::Color::White);

	if (g_chating) {
		int count = 1;

		chat_text.setString(chat_input);
		chat_text.setPosition(0, WINDOW_HEIGHT - CHATING_HEIGHT * count++);
		g_window->draw(chat_text);

		chat_text.setFillColor(sf::Color::Black);
		for (auto p = chat_logs.rbegin(); p != chat_logs.rend(); ++p) {
			chat_text.setString(*p);
			chat_text.setPosition(0, WINDOW_HEIGHT - CHATING_HEIGHT * count++);
			g_window->draw(chat_text);
		}
	}
	else {
		if (false == chat_logs.empty()) {
			chat_text.setString(chat_logs.back());
			chat_text.setPosition(0, WINDOW_HEIGHT - CHATING_HEIGHT);
			g_window->draw(chat_text);
		}
	}
}

void send_packet(void* packet)
{
	unsigned char* p = reinterpret_cast<unsigned char*>(packet);
	size_t sent = 0;
	s_socket.send(packet, p[0], sent);
}

int main()
{
	// read map
	std::cout << "Loading.." << std::endl;

	std::ifstream ifile("map_2000_edit.txt");

	if (!ifile.is_open()) {
		std::cout << "map load fail" << std::endl;
		exit(0);
	}

	int num;

	for (int i = 0; i < 2000; ++i) {
		for (int j = 0; j < 2000; ++j) {
			ifile >> num;
			tile_map[i][j] = num;
		}
	}

	ifile.close();

	//
	sf::IpAddress addr = "127.0.0.1";
	std::cout << "Server IP : ";
	std::cin >> addr;

	wcout.imbue(locale("korean"));
	sf::Socket::Status status = s_socket.connect(addr, PORT_NUM);
	s_socket.setBlocking(false);

	if (status != sf::Socket::Done) {
		wcout << L"서버와 연결할 수 없습니다.\n";
		exit(-1);
	}

	client_initialize();
	CS_LOGIN_PACKET p;
	p.size = sizeof(p);
	p.type = CS_LOGIN;

	char player_name[NAME_SIZE];
	std::cout << "ID : ";
	std::cin >> player_name;

	strcpy_s(p.name, player_name);
	send_packet(&p);
	avatar.set_name(p.name);

	sf::RenderWindow window(sf::VideoMode(WINDOW_WIDTH, WINDOW_HEIGHT), "2D CLIENT");
	g_window = &window;

	while (window.isOpen())
	{
		sf::Event event;
		while (window.pollEvent(event))
		{
			if (event.type == sf::Event::Closed)
				window.close();

			if (false == g_chating) {
				if (event.type == sf::Event::KeyPressed) {
					int direction = -1;
					switch (event.key.code) {
					case sf::Keyboard::Left:
						direction = 2;
						break;
					case sf::Keyboard::Right:
						direction = 3;
						break;
					case sf::Keyboard::Up:
						direction = 0;
						break;
					case sf::Keyboard::Down:
						direction = 1;
						break;
					case sf::Keyboard::Escape:
						window.close();
						break;
						//
					case sf::Keyboard::T:
					{
						CS_TELEPORT_PACKET p;
						p.size = sizeof(p);
						p.type = CS_TELEPORT;
						send_packet(&p);
						break;
					}
					case sf::Keyboard::A:
					{
						CS_ATTACK_PACKET p;
						p.size = sizeof(p);
						p.type = CS_ATTACK;
						send_packet(&p);
						break;
					}
					case sf::Keyboard::S:
					{
						CS_ATTACK_PACKET p;
						p.size = sizeof(p);
						p.type = CS_ATTACK_FORWARD;
						send_packet(&p);
						break;
					}
					case sf::Keyboard::F:
					{
						CS_SKILL_PACKET p;
						p.size = sizeof(p);
						p.type = CS_SKILL_MOVEMENT;
						send_packet(&p);
						break;
					}
					case sf::Keyboard::Num1:
					{
						CS_ITEM_PACKET p;
						p.size = sizeof(p);
						p.type = CS_ITEM_POSTION_S;
						send_packet(&p);
						break;
					}
					case sf::Keyboard::Num2:
					{
						CS_ITEM_PACKET p;
						p.size = sizeof(p);
						p.type = CS_ITEM_POSTION_L;
						send_packet(&p);
						break;
					}
					case sf::Keyboard::Num3:
					{
						CS_ITEM_PACKET p;
						p.size = sizeof(p);
						p.type = CS_ITEM_POISION;
						send_packet(&p);
						break;
					}
					case sf::Keyboard::Enter:
						g_chating = true;
						break;
					}
					if (-1 != direction) {
						CS_MOVE_PACKET p;
						p.size = sizeof(p);
						p.type = CS_MOVE;
						p.direction = direction;
						g_last_move_time = std::chrono::high_resolution_clock::now();
						p.move_time = static_cast<unsigned>(std::chrono::duration_cast<std::chrono::milliseconds>(g_last_move_time.time_since_epoch()).count());
						send_packet(&p);
					}
				}
			}
			else {
				if (event.type == sf::Event::KeyPressed && event.key.code == sf::Keyboard::Enter) {
					if (false == chat_input.empty()) {
						CS_CHAT_PACKET p;
						p.size = sizeof(CS_CHAT_PACKET);
						p.type = CS_CHAT;
						strcpy_s(p.mess, chat_input.c_str());
						send_packet(&p);

						std::string temp_chat;
						temp_chat.append(avatar.name).append(" : ").append(chat_input);
						chat_logs.emplace_back(temp_chat);
						chat_input.clear();
					}

					g_chating = false;
				}
			}

			if (g_chating && event.type == sf::Event::TextEntered) {
				if (event.text.unicode == '\b') {
					if (false == chat_input.empty()) {
						chat_input.pop_back();
					}
				}
				else if (event.text.unicode < 128 && event.text.unicode != '\r') {
					chat_input += (char)(event.text.unicode);
				}
			}
		}

		window.clear();
		client_main();
		window.display();
	}
	client_finish();

	return 0;
}