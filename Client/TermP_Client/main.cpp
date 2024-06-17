#include <SFML/Graphics.hpp>
#include <SFML/Network.hpp>
#include <iostream>
#include <fstream>
#include <unordered_map>
#include <Windows.h>
#include <chrono>
using namespace std;

//#pragma comment (lib, "opengl32.lib")
//#pragma comment (lib, "winmm.lib")
//#pragma comment (lib, "ws2_32.lib")

#include "../../Server/TermP_Server/protocol.h"

sf::TcpSocket s_socket;

constexpr float MINIMAP_WIDTH = 200.0f;
constexpr float MINIMAP_HEIGHT = 200.0f;

constexpr float MINIMAP_PLAYER_SIZE = 5.0f;

constexpr auto TILE_WIDTH = 40;
constexpr auto WINDOW_WIDTH = CLIENT_WIDTH * TILE_WIDTH;   // size of window
constexpr auto WINDOW_HEIGHT = CLIENT_HEIGHT * TILE_WIDTH;

int g_left_x;
int g_top_y;
int g_myid;
int g_level = 1;
int g_hp = 100;
int g_max_hp = 100;
int g_exp = 0;

sf::RenderWindow* g_window;
sf::Font g_font;

class OBJECT {
private:
	bool m_showing;

	sf::Text m_name;
	sf::Text m_chat;
	chrono::system_clock::time_point m_mess_end_time;
public:
	sf::Sprite m_sprite;
	int m_x, m_y;
	char name[NAME_SIZE];
	OBJECT(sf::Texture& t, int x, int y, int x2, int y2) {
		m_showing = false;
		m_sprite.setTexture(t);
		m_sprite.setTextureRect(sf::IntRect(x, y, x2, y2));
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

OBJECT white_tile;
OBJECT black_tile;

sf::Texture* board;
sf::Texture* pieces;

//
sf::RectangleShape status_rect;

sf::RectangleShape minimap_map;
sf::RectangleShape minimap_plr;

//
short tile_map[2000][2000]{0};

void client_initialize()
{
	board = new sf::Texture;
	pieces = new sf::Texture;
	//board->loadFromFile("chessmap.bmp");
	board->loadFromFile("map.png");
	pieces->loadFromFile("chess2.png");
	if (false == g_font.loadFromFile("cour.ttf")) {
		cout << "Font Loading Error!\n";
		exit(-1);
	}

	// wall 1
	tiles[0] = OBJECT{ *board, 0, 0, 32, 32 };
	tiles[0].m_sprite.setScale(sf::Vector2f((float)TILE_WIDTH / 32.f, (float)TILE_WIDTH / 32.f));
	tiles[1] = OBJECT{ *board, 32, 0, 32, 32 };
	tiles[1].m_sprite.setScale(sf::Vector2f((float)TILE_WIDTH / 32.f, (float)TILE_WIDTH / 32.f));
	tiles[2] = OBJECT{ *board, 64, 0, 32, 32 };
	tiles[2].m_sprite.setScale(sf::Vector2f((float)TILE_WIDTH / 32.f, (float)TILE_WIDTH / 32.f));

	tiles[8] = OBJECT{ *board, 0, 32, 32, 32 };
	tiles[8].m_sprite.setScale(sf::Vector2f((float)TILE_WIDTH / 32.f, (float)TILE_WIDTH / 32.f));
	tiles[9] = OBJECT{ *board, 32, 32, 32, 32 };
	tiles[9].m_sprite.setScale(sf::Vector2f((float)TILE_WIDTH / 32.f, (float)TILE_WIDTH / 32.f));
	tiles[10] = OBJECT{ *board, 64, 32, 32, 32 };
	tiles[10].m_sprite.setScale(sf::Vector2f((float)TILE_WIDTH / 32.f, (float)TILE_WIDTH / 32.f));

	tiles[16] = OBJECT{ *board, 0, 64, 32, 32 };
	tiles[16].m_sprite.setScale(sf::Vector2f((float)TILE_WIDTH / 32.f, (float)TILE_WIDTH / 32.f));
	tiles[17] = OBJECT{ *board, 32, 64, 32, 32 };
	tiles[17].m_sprite.setScale(sf::Vector2f((float)TILE_WIDTH / 32.f, (float)TILE_WIDTH / 32.f));
	tiles[18] = OBJECT{ *board, 64, 64, 32, 32 };
	tiles[18].m_sprite.setScale(sf::Vector2f((float)TILE_WIDTH / 32.f, (float)TILE_WIDTH / 32.f));

	// wall 2
	tiles[24] = OBJECT{ *board, 0, 96, 32, 32 };
	tiles[24].m_sprite.setScale(sf::Vector2f((float)TILE_WIDTH / 32.f, (float)TILE_WIDTH / 32.f));
	tiles[25] = OBJECT{ *board, 32, 96, 32, 32 };
	tiles[25].m_sprite.setScale(sf::Vector2f((float)TILE_WIDTH / 32.f, (float)TILE_WIDTH / 32.f));
	tiles[26] = OBJECT{ *board, 64, 96, 32, 32 };
	tiles[26].m_sprite.setScale(sf::Vector2f((float)TILE_WIDTH / 32.f, (float)TILE_WIDTH / 32.f));

	tiles[32] = OBJECT{ *board, 0, 128, 32, 32 };
	tiles[32].m_sprite.setScale(sf::Vector2f((float)TILE_WIDTH / 32.f, (float)TILE_WIDTH / 32.f));
	tiles[33] = OBJECT{ *board, 32, 128, 32, 32 };
	tiles[33].m_sprite.setScale(sf::Vector2f((float)TILE_WIDTH / 32.f, (float)TILE_WIDTH / 32.f));
	tiles[34] = OBJECT{ *board, 64, 128, 32, 32 };
	tiles[34].m_sprite.setScale(sf::Vector2f((float)TILE_WIDTH / 32.f, (float)TILE_WIDTH / 32.f));

	tiles[40] = OBJECT{ *board, 0, 160, 32, 32 };
	tiles[40].m_sprite.setScale(sf::Vector2f((float)TILE_WIDTH / 32.f, (float)TILE_WIDTH / 32.f));
	tiles[41] = OBJECT{ *board, 32, 160, 32, 32 };
	tiles[41].m_sprite.setScale(sf::Vector2f((float)TILE_WIDTH / 32.f, (float)TILE_WIDTH / 32.f));
	tiles[42] = OBJECT{ *board, 64, 160, 32, 32 };
	tiles[42].m_sprite.setScale(sf::Vector2f((float)TILE_WIDTH / 32.f, (float)TILE_WIDTH / 32.f));

	// road
	tiles[5] = OBJECT{ *board, 160, 0, 32, 32 };
	tiles[5].m_sprite.setScale(sf::Vector2f((float)TILE_WIDTH / 32.f, (float)TILE_WIDTH / 32.f));
	tiles[6] = OBJECT{ *board, 192, 0, 32, 32 };
	tiles[6].m_sprite.setScale(sf::Vector2f((float)TILE_WIDTH / 32.f, (float)TILE_WIDTH / 32.f));
	tiles[7] = OBJECT{ *board, 224, 0, 32, 32 };
	tiles[7].m_sprite.setScale(sf::Vector2f((float)TILE_WIDTH / 32.f, (float)TILE_WIDTH / 32.f));

	tiles[13] = OBJECT{ *board, 160, 32, 32, 32 };
	tiles[13].m_sprite.setScale(sf::Vector2f((float)TILE_WIDTH / 32.f, (float)TILE_WIDTH / 32.f));
	tiles[14] = OBJECT{ *board, 192, 32, 32, 32 };
	tiles[14].m_sprite.setScale(sf::Vector2f((float)TILE_WIDTH / 32.f, (float)TILE_WIDTH / 32.f));
	tiles[15] = OBJECT{ *board, 224, 32, 32, 32 };
	tiles[15].m_sprite.setScale(sf::Vector2f((float)TILE_WIDTH / 32.f, (float)TILE_WIDTH / 32.f));

	tiles[21] = OBJECT{ *board, 160, 64, 32, 32 };
	tiles[21].m_sprite.setScale(sf::Vector2f((float)TILE_WIDTH / 32.f, (float)TILE_WIDTH / 32.f));
	tiles[22] = OBJECT{ *board, 192, 64, 32, 32 };
	tiles[22].m_sprite.setScale(sf::Vector2f((float)TILE_WIDTH / 32.f, (float)TILE_WIDTH / 32.f));
	tiles[23] = OBJECT{ *board, 224, 64, 32, 32 };
	tiles[23].m_sprite.setScale(sf::Vector2f((float)TILE_WIDTH / 32.f, (float)TILE_WIDTH / 32.f));

	// wall 1
	tiles[19] = OBJECT{ *board, 96, 64, 32, 32 };
	tiles[19].m_sprite.setScale(sf::Vector2f((float)TILE_WIDTH / 32.f, (float)TILE_WIDTH / 32.f));
	tiles[20] = OBJECT{ *board, 128, 64, 32, 32 };
	tiles[20].m_sprite.setScale(sf::Vector2f((float)TILE_WIDTH / 32.f, (float)TILE_WIDTH / 32.f));

	tiles[27] = OBJECT{ *board, 96, 96, 32, 32 };
	tiles[27].m_sprite.setScale(sf::Vector2f((float)TILE_WIDTH / 32.f, (float)TILE_WIDTH / 32.f));
	tiles[28] = OBJECT{ *board, 128, 96, 32, 32 };
	tiles[28].m_sprite.setScale(sf::Vector2f((float)TILE_WIDTH / 32.f, (float)TILE_WIDTH / 32.f));

	// wall 2
	tiles[35] = OBJECT{ *board, 96, 128, 32, 32 };
	tiles[35].m_sprite.setScale(sf::Vector2f((float)TILE_WIDTH / 32.f, (float)TILE_WIDTH / 32.f));
	tiles[36] = OBJECT{ *board, 128, 128, 32, 32 };
	tiles[36].m_sprite.setScale(sf::Vector2f((float)TILE_WIDTH / 32.f, (float)TILE_WIDTH / 32.f));

	tiles[43] = OBJECT{ *board, 96, 160, 32, 32 };
	tiles[43].m_sprite.setScale(sf::Vector2f((float)TILE_WIDTH / 32.f, (float)TILE_WIDTH / 32.f));
	tiles[44] = OBJECT{ *board, 128, 160, 32, 32 };
	tiles[44].m_sprite.setScale(sf::Vector2f((float)TILE_WIDTH / 32.f, (float)TILE_WIDTH / 32.f));

	// road
	tiles[3] = OBJECT{ *board, 96, 0, 32, 32 };
	tiles[3].m_sprite.setScale(sf::Vector2f((float)TILE_WIDTH / 32.f, (float)TILE_WIDTH / 32.f));
	tiles[4] = OBJECT{ *board, 128, 0, 32, 32 };
	tiles[4].m_sprite.setScale(sf::Vector2f((float)TILE_WIDTH / 32.f, (float)TILE_WIDTH / 32.f));

	tiles[11] = OBJECT{ *board, 96, 32, 32, 32 };
	tiles[11].m_sprite.setScale(sf::Vector2f((float)TILE_WIDTH / 32.f, (float)TILE_WIDTH / 32.f));
	tiles[12] = OBJECT{ *board, 128, 32, 32, 32 };
	tiles[12].m_sprite.setScale(sf::Vector2f((float)TILE_WIDTH / 32.f, (float)TILE_WIDTH / 32.f));

	// 1
	tiles[29] = OBJECT{ *board, 160, 96, 32, 32 };
	tiles[29].m_sprite.setScale(sf::Vector2f((float)TILE_WIDTH / 32.f, (float)TILE_WIDTH / 32.f));

	// 2
	tiles[30] = OBJECT{ *board, 192, 96, 32, 32 };
	tiles[30].m_sprite.setScale(sf::Vector2f((float)TILE_WIDTH / 32.f, (float)TILE_WIDTH / 32.f));

	white_tile = OBJECT{ *board, 32, 160, 32, 32 };
	white_tile.m_sprite.setScale(sf::Vector2f((float)TILE_WIDTH / 32.f, (float)TILE_WIDTH / 32.f));
	black_tile = OBJECT{ *board, 32, 160, 32, 32 };
	black_tile.m_sprite.setScale(sf::Vector2f((float)TILE_WIDTH / 32.f, (float)TILE_WIDTH / 32.f));

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
		avatar.m_x = packet->x;
		avatar.m_y = packet->y;
		g_left_x = packet->x - CLIENT_WIDTH / 2;
		g_top_y = packet->y - CLIENT_HEIGHT / 2;
		avatar.set_name(packet->name);

		//while (packet->name[strlen(packet->name) - 1] == ' ') {
		//	packet->name[strlen(packet->name) - 1] = packet->name[strlen(packet->name)];
		//}

		g_level = packet->level;
		g_max_hp = packet->max_hp;
		g_hp = packet->hp;
		g_exp = packet->exp;

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
		else /*if (id < MAX_USER)*/ {
			players[id] = OBJECT{ *pieces, 0, 0, 64, 64 };
			players[id].m_sprite.setScale(sf::Vector2f((float)TILE_WIDTH / 64.f, (float)TILE_WIDTH / 64.f));
			players[id].move(my_packet->x, my_packet->y);
			players[id].set_name(my_packet->name);
			players[id].show();
		}
		//else {
		//	//npc[id - NPC_START].x = my_packet->x;
		//	//npc[id - NPC_START].y = my_packet->y;
		//	//npc[id - NPC_START].attr |= BOB_ATTR_VISIBLE;
		//}
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
		else /*if (other_id < MAX_USER)*/ {
			players[other_id].move(my_packet->x, my_packet->y);
		}
		//else {
		//	//npc[other_id - NPC_START].x = my_packet->x;
		//	//npc[other_id - NPC_START].y = my_packet->y;
		//}
		break;
	}

	case SC_REMOVE_OBJECT:
	{
		SC_REMOVE_OBJECT_PACKET* my_packet = reinterpret_cast<SC_REMOVE_OBJECT_PACKET*>(ptr);
		int other_id = my_packet->id;
		if (other_id == g_myid) {
			avatar.hide();
		}
		else /*if (other_id < MAX_USER)*/ {
			players.erase(other_id);
		}
		//else {
		//	//		npc[other_id - NPC_START].attr &= ~BOB_ATTR_VISIBLE;
		//}
		break;
	}
	case SC_CHAT:
	{
		SC_CHAT_PACKET* my_packet = reinterpret_cast<SC_CHAT_PACKET*>(ptr);
		int other_id = my_packet->id;

		if (other_id == g_myid) {
			avatar.set_chat(my_packet->mess);
		}
		else {
			players[other_id].set_chat(my_packet->mess);
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

			//if (0 == (tile_x / 3 + tile_y / 3) % 2) {
			//	white_tile.a_move(TILE_WIDTH * i, TILE_WIDTH * j);
			//	white_tile.a_draw();
			//}
			//else
			//{
			//	black_tile.a_move(TILE_WIDTH * i, TILE_WIDTH * j);
			//	black_tile.a_draw();
			//}
		}
	avatar.draw();
	for (auto& pl : players) pl.second.draw();
	sf::Text text;
	text.setFont(g_font);
	char buf[100];
	sprintf_s(buf, "(%d, %d)", avatar.m_x, avatar.m_y);
	text.setString(buf);
	g_window->draw(text);

	//
	status_rect.setPosition((float)WINDOW_WIDTH / 2 - (float)WINDOW_WIDTH / 6, 0);
	g_window->draw(status_rect);

	// level
	sprintf_s(buf, "Level : %d", g_level);
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

			//if (num >= 48) {
			//	tile_map[j][i] = 29;
			//}
			//else { 
			//	tile_map[j][i] = num;
			//}
		}
	}

	ifile.close();

	//
	//std::cout << "re-writing.." << std::endl;

	//std::ofstream ofile("map_2000_edit.txt");

	//if (!ofile.is_open()) {
	//	std::cout << "map write fail" << std::endl;
	//	exit(0);
	//}

	//for (int i = 0; i < 2000; ++i) {
	//	for (int j = 0; j < 2000; ++j) {
	//		ofile << tile_map[j][i];
	//		ofile << " ";
	//	}
	//}

	//ofile.close();

	//
	wcout.imbue(locale("korean"));
	sf::Socket::Status status = s_socket.connect("127.0.0.1", PORT_NUM);
	s_socket.setBlocking(false);

	if (status != sf::Socket::Done) {
		wcout << L"서버와 연결할 수 없습니다.\n";
		exit(-1);
	}

	client_initialize();
	CS_LOGIN_PACKET p;
	p.size = sizeof(p);
	p.type = CS_LOGIN;

	//string player_name{ "P" };
	//player_name += to_string(GetCurrentProcessId());
	char player_name[NAME_SIZE];
	std::cout << "ID : ";
	std::cin >> player_name;

	//strcpy_s(p.name, player_name.c_str());
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
				}
				if (-1 != direction) {
					CS_MOVE_PACKET p;
					p.size = sizeof(p);
					p.type = CS_MOVE;
					p.direction = direction;
					send_packet(&p);
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