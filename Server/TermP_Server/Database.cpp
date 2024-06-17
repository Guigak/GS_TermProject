#include "Database.h"

void disp_error(SQLHANDLE hHandle, SQLSMALLINT hType, RETCODE RetCode)
{
	SQLSMALLINT iRec = 0;
	SQLINTEGER iError;
	WCHAR wszMessage[1000];
	WCHAR wszState[SQL_SQLSTATE_SIZE + 1];
	if (RetCode == SQL_INVALID_HANDLE) {
		fwprintf(stderr, L"Invalid handle!\n");
		return;
	}
	while (SQLGetDiagRec(hType, hHandle, ++iRec, wszState, &iError, wszMessage,
		(SQLSMALLINT)(sizeof(wszMessage) / sizeof(WCHAR)), (SQLSMALLINT*)NULL) == SQL_SUCCESS) {
		// Hide data truncated..
		if (wcsncmp(wszState, L"01004", 5)) {
			fwprintf(stderr, L"[%5.5s] %s (%d)\n", wszState, wszMessage, iError);
		}
	}
}

void CDatabase::work() {
	while (true) {
		if (m_queue.empty()) {
			std::this_thread::sleep_for(std::chrono::milliseconds(1));
		}
		else {
			m_q_mtx.lock();
			QUERY query = m_queue.front();
			m_queue.pop();
			m_q_mtx.unlock();

			switch (query.type) {
			case Q_LOGIN:
				if (false == login(query.p_object)) {
					OVERLAPPED_EX* over_ex = new OVERLAPPED_EX;
					over_ex->m_option = OP_LOGIN_FAIL;
					PostQueuedCompletionStatus(*m_p_h_iocp, 1, query.p_object->m_id, &over_ex->m_overlapped);
				}
				break;
			case Q_LOGOUT:
				if (false == logout(query.p_object)) {
					std::cout << "LOGOUT ERROR" << std::endl;
				}
				break;
			default:
				break;
			}
		}
	}
}

bool CDatabase::connect() {
	SQLRETURN retcode;

	setlocale(LC_ALL, "korean");
	// Allocate environment handle  
	retcode = SQLAllocHandle(SQL_HANDLE_ENV, SQL_NULL_HANDLE, &m_henv);

	// Set the ODBC version environment attribute  
	if (retcode == SQL_SUCCESS || retcode == SQL_SUCCESS_WITH_INFO) {
		retcode = SQLSetEnvAttr(m_henv, SQL_ATTR_ODBC_VERSION, (SQLPOINTER*)SQL_OV_ODBC3, 0);

		// Allocate connection handle  
		if (retcode == SQL_SUCCESS || retcode == SQL_SUCCESS_WITH_INFO) {
			retcode = SQLAllocHandle(SQL_HANDLE_DBC, m_henv, &m_hdbc);

			// Set login timeout to 5 seconds  
			if (retcode == SQL_SUCCESS || retcode == SQL_SUCCESS_WITH_INFO) {
				SQLSetConnectAttr(m_hdbc, SQL_LOGIN_TIMEOUT, (SQLPOINTER)5, 0);

				// Connect to data source  
				retcode = SQLConnect(m_hdbc, (SQLWCHAR*)L"TP_2019182022", SQL_NTS, (SQLWCHAR*)NULL, SQL_NTS, (SQLWCHAR*)NULL, SQL_NTS);

				disp_error(m_hstmt, SQL_HANDLE_STMT, retcode);

				// Allocate statement handle  
				if (retcode == SQL_SUCCESS || retcode == SQL_SUCCESS_WITH_INFO) {
					std::cout << "SQL Connected" << std::endl;
					return true;
				}
				else
					return false;
			}
			else
				return false;
		}
		else
			return false;
	}
	else
		return false;
}

void CDatabase::disconnect() {
	SQLDisconnect(m_hdbc);
	SQLFreeHandle(SQL_HANDLE_DBC, m_hdbc);
	SQLFreeHandle(SQL_HANDLE_ENV, m_henv);
}

void CDatabase::add_query(CObject& object, QUERY_TYPE type, int target_id) {
	QUERY query;
	query.p_object = &object;
	query.type = type;
	query.target_id = target_id;

	m_q_mtx.lock();
	m_queue.push(query);
	m_q_mtx.unlock();
}

bool CDatabase::login(CObject* p_object) {
	SQLRETURN retcode;

	SQLWCHAR nickname[NAME_SIZE];
	SQLSMALLINT visual, max_hp, hp, level, exp, x, y, potion_s, potion_l;

	SQLLEN l_nickname, l_visual, l_max_hp, l_hp, l_level, l_exp, l_x, l_y, l_potion_s, l_potion_l;

	SQLWCHAR szId[NAME_SIZE];
	SQLSMALLINT dPosx, dPosy;
	SQLLEN cbId = 0, cbPosx = 0, cbPosy = 0;

	m_s_mtx.lock();

	retcode = SQLAllocHandle(SQL_HANDLE_STMT, m_hdbc, &m_hstmt);

	WCHAR cmd[1024];
	WCHAR wname[NAME_SIZE];

	mbstowcs_s(nullptr, wname, NAME_SIZE, p_object->m_name, NAME_SIZE);
	wsprintf(cmd, L"select u_Nickname, u_Visual, u_Max_Hp, u_Hp, u_Level, u_Exp, u_X, u_Y, u_Potion_S, u_Potion_L from user_table where u_Id = '%s'", wname);
	retcode = SQLExecDirect(m_hstmt, (SQLWCHAR*)cmd, SQL_NTS);
	if (retcode == SQL_SUCCESS || retcode == SQL_SUCCESS_WITH_INFO) {

		// Bind columns 1, 2, and 3  
		retcode = SQLBindCol(m_hstmt, 1, SQL_C_WCHAR, nickname, 20, &l_nickname);
		retcode = SQLBindCol(m_hstmt, 2, SQL_C_SHORT, &visual, 10, &l_visual);
		retcode = SQLBindCol(m_hstmt, 3, SQL_C_SHORT, &max_hp, 10, &l_max_hp);
		retcode = SQLBindCol(m_hstmt, 4, SQL_C_SHORT, &hp, 10, &l_hp);
		retcode = SQLBindCol(m_hstmt, 5, SQL_C_SHORT, &level, 10, &l_level);
		retcode = SQLBindCol(m_hstmt, 6, SQL_C_SHORT, &exp, 10, &l_exp);
		retcode = SQLBindCol(m_hstmt, 7, SQL_C_SHORT, &x, 10, &l_x);
		retcode = SQLBindCol(m_hstmt, 8, SQL_C_SHORT, &y, 10, &l_y);
		retcode = SQLBindCol(m_hstmt, 9, SQL_C_SHORT, &potion_s, 10, &l_potion_s);
		retcode = SQLBindCol(m_hstmt, 10, SQL_C_SHORT, &potion_l, 10, &l_potion_l);

		// Fetch and print each row of data. On an error, display a message and exit.  
		retcode = SQLFetch(m_hstmt);

		if (retcode == SQL_ERROR) {
			std::cout << "Error" << std::endl;
		}

		if (retcode == SQL_SUCCESS || retcode == SQL_SUCCESS_WITH_INFO)
		{
			wprintf(L"%20s %6d %6d %6d %6d %6d %6d %6d %6d %6d", nickname, visual, max_hp, hp, level, exp, x, y, potion_s, potion_l);

			PLAYER_INFO* info = new PLAYER_INFO;
			WideCharToMultiByte(CP_ACP, 0, nickname, -1, info->nickname, NAME_SIZE, NULL, NULL);

			while (info->nickname[strlen(info->nickname) - 1] == ' ') {
				info->nickname[strlen(info->nickname) - 1] = info->nickname[strlen(info->nickname)];
			}

			info->visual = visual;
			info->max_hp = max_hp;
			info->hp = hp;
			info->level = level;
			info->exp = exp;
			info->x = x;
			info->y = y;
			info->potion_s = potion_s;
			info->potion_l = potion_l;

			OVERLAPPED_EX* over_ex = new OVERLAPPED_EX;
			over_ex->m_option = OP_LOGIN_OK;
			over_ex->m_data = reinterpret_cast<long long>(info);
			PostQueuedCompletionStatus(*m_p_h_iocp, 1, p_object->m_id, &over_ex->m_overlapped);

			m_s_mtx.unlock();
			return true;
		}
		else {
			m_s_mtx.unlock();
		}

		return false;
	}
	else {
		disp_error(m_hstmt, SQL_HANDLE_STMT, retcode);
		m_s_mtx.unlock();
		return false;
	}
}

bool CDatabase::logout(CObject* p_object) {

	SQLRETURN retcode;
	SQLWCHAR szId[NAME_SIZE];
	SQLSMALLINT dPosx, dPosy;
	SQLLEN cbId = 0, cbPosx = 0, cbPosy = 0;

	m_s_mtx.lock();

	retcode = SQLAllocHandle(SQL_HANDLE_STMT, m_hdbc, &m_hstmt);

	WCHAR cmd[200];
	WCHAR wname[NAME_SIZE];

	mbstowcs_s(nullptr, wname, NAME_SIZE, p_object->m_name, NAME_SIZE);
	wsprintf(cmd, L"update user_table set u_X = %d, u_Y = %d where u_Nickname = '%s'", p_object->m_x, p_object->m_y, wname);
	retcode = SQLExecDirect(m_hstmt, (SQLWCHAR*)cmd, SQL_NTS);
	if (retcode == SQL_SUCCESS || retcode == SQL_SUCCESS_WITH_INFO) {
		//wprintf(L"Logout [%s] %d %d\n", wname, objects[c_id].x, objects[c_id].y);
		m_s_mtx.unlock();
		return true;
	}
	else {
		disp_error(m_hstmt, SQL_HANDLE_STMT, retcode);
		m_s_mtx.unlock();
		return false;
	}
}

void CDatabase::set_iocp(HANDLE* p_h_iocp) {
    m_p_h_iocp = p_h_iocp;
}