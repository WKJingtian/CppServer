#pragma once
#include "CppServerAPI.h"
#include "mysql/include/jdbc/mysql_connection.h"
#include "mysql/include/mysql/jdbc.h"
#include "mysql/include/mysqlx/xdevapi.h"
#include "Utils/ReadWriteLock.h"
#include <memory>
#include <string>
#include <functional>
#include <sstream>

class CPPSERVER_API MySqlMgr
{
	static MySqlMgr& Instance();
	static int DebugDatabaseInit();

	//std::unique_ptr<mysqlx::Session> _sqlSession;
	mysqlx::Session* _sqlSession;
	std::string _url{};
	std::string _user{};
	std::string _pass{};
	std::string _schema{};
	unsigned int _port;
	ReadWriteLock _lock;

	void EnsureConnection(bool forceUpdate = false);

	MySqlMgr();
	MySqlMgr(const MySqlMgr&) = delete;
	MySqlMgr& operator=(const MySqlMgr&) = delete;

public:

	~MySqlMgr() = default;

	// if you want to debug, use the following parameters:
	// "127.0.0.1"
	// 3306
	// "root"
	// "1QAZ2wsx"
	// "wkr_server_schema"
	static int Init(
		const std::string& url,
		const unsigned int port,
		const std::string& user,
		const std::string& pass,
		const std::string& schema);

	static void Select(const std::string& table, const std::string& columns, const std::string& where, std::function<void(mysqlx::SqlResult&&)> func);
	static void Update(const std::string& table, const std::string& setClause, const std::string& where, std::function<void(mysqlx::SqlResult&&)> func);
	static void Delete(const std::string& table, const std::string& where, std::function<void(mysqlx::SqlResult&&)> func);
	static void Upsert(const std::string& table, const std::string& columns, const std::string& values, const std::string& onDuplicateClause, std::function<void(mysqlx::SqlResult&&)> func);

private:

};
