#pragma once
#include "CppServerAPI.h"
#include "mysql/include/jdbc/mysql_connection.h"
#include "mysql/include/mysql/jdbc.h"
#include "mysql/include/mysqlx/xdevapi.h"
#include "Utils/ReadWriteLock.h"
#include <memory>
#include <atomic>
#include <string>
#include <functional>
#include <sstream>
#include <vector>
#include <future>

class CPPSERVER_API MySqlMgr
{
	static MySqlMgr& Instance();
	static int DebugDatabaseInit();

	std::string _url{};
	std::string _user{};
	std::string _pass{};
	std::string _schema{};
	unsigned int _port;
	std::atomic<uint64_t> _configVersion{0};
	ReadWriteLock _lock;

	mysqlx::Session* EnsureConnection(bool forceUpdate = false);

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

	static void DoSql(const std::string& sqlCmd, std::function<void(mysqlx::SqlResult&&)> func);
	static void DoSql(const std::vector<std::string>& sqlCmds, std::function<void(std::vector<mysqlx::SqlResult>&&)> func, bool enableLastIdReplace);
	static void Select(const std::string& table, const std::string& columns, const std::string& where, std::function<void(mysqlx::SqlResult&&)> func);
	static void Update(const std::string& table, const std::string& setClause, const std::string& where, std::function<void(mysqlx::SqlResult&&)> func);
	static void Delete(const std::string& table, const std::string& where, std::function<void(mysqlx::SqlResult&&)> func);
	static void Upsert(const std::string& table, const std::string& columns, const std::string& values, const std::string& onDuplicateClause, std::function<void(mysqlx::SqlResult&&)> func);

	// Async API (non-blocking, runs on DB thread pool, callback runs on main thread)
	static std::future<mysqlx::SqlResult> DoSqlFuture(const std::string& sqlCmd);
	static std::future<std::vector<mysqlx::SqlResult>> DoSqlFuture(const std::vector<std::string>& sqlCmds, bool enableLastIdReplace);
	static void DoSqlAsync(const std::string& sqlCmd, std::function<void(mysqlx::SqlResult&&)> func);
	static void DoSqlAsync(const std::vector<std::string>& sqlCmds, std::function<void(std::vector<mysqlx::SqlResult>&&)> func, bool enableLastIdReplace);
	static void SelectAsync(const std::string& table, const std::string& columns, const std::string& where, std::function<void(mysqlx::SqlResult&&)> func);
	static void UpdateAsync(const std::string& table, const std::string& setClause, const std::string& where, std::function<void(mysqlx::SqlResult&&)> func);
	static void DeleteAsync(const std::string& table, const std::string& where, std::function<void(mysqlx::SqlResult&&)> func);
	static void UpsertAsync(const std::string& table, const std::string& columns, const std::string& values, const std::string& onDuplicateClause, std::function<void(mysqlx::SqlResult&&)> func);

private:
	static mysqlx::SqlResult ExecuteSqlInternal(const std::string& sqlCmd);
	static std::vector<mysqlx::SqlResult> ExecuteSqlInternal(const std::vector<std::string>& sqlCmds, bool enableLastIdReplace);
};
