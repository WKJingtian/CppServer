#include "pch.h"
#include "MySqlMgr.h"
#include <sstream>
#include <functional>
#include <iostream>
#include "Utils/Utils.h"
#include "Const.h"
#include "DbThreadPool.h"
#include "Utils/MainThreadDispatcher.h"

MySqlMgr::MySqlMgr() = default;

MySqlMgr& MySqlMgr::Instance()
{
	static MySqlMgr instance;
	return instance;
}

mysqlx::SqlResult MySqlMgr::ExecuteSqlInternal(const std::string& sqlCmd)
{
#ifdef ENABLE_SQL_DEBUG
	std::cout << "SQL DEBUG MSG: DoSql - " << sqlCmd << std::endl;
#endif
	mysqlx::SqlResult result;
	try
	{
		auto& db = Instance();
		auto* session = db.EnsureConnection();
		if (!session)
			return result;

		result = session->sql(sqlCmd).execute();
	}
	catch (const mysqlx::Error& e)
	{
		std::cout << "MYSQL ERROR: " << e << std::endl;
	}
	catch (const std::exception& e)
	{
		std::cout << "STD ERROR: " << e.what() << std::endl;
	}
	return result;
}

std::vector<mysqlx::SqlResult> MySqlMgr::ExecuteSqlInternal(const std::vector<std::string>& sqlCmds, bool enableLastIdReplace)
{
#ifdef ENABLE_SQL_DEBUG
	for (const auto& sqlCmd : sqlCmds)
		std::cout << "SQL DEBUG MSG: DoSql - " << sqlCmd << std::endl;
#endif
	std::vector<mysqlx::SqlResult> results;
	try
	{
		auto& db = Instance();
		auto* session = db.EnsureConnection();
		if (!session)
			return results;

		session->startTransaction();

		int lastId = -1;
		for (const auto& sqlCmd : sqlCmds)
		{
			std::string finalCmd = sqlCmd;
			if (enableLastIdReplace)
				Utils::StringReplace(finalCmd, "LAST_INSERT_ID", std::to_string(lastId));
			results.push_back(session->sql(finalCmd).execute());
			lastId = static_cast<int>(results[results.size() - 1].getAutoIncrementValue());
		}

		session->commit();
	}
	catch (const mysqlx::Error& e)
	{
		std::cout << "MYSQL ERROR: " << e << std::endl;
		try
		{
			auto& db = Instance();
			auto* session = db.EnsureConnection();
			if (session)
				session->rollback();
		}
		catch (const std::exception& rollbackEx)
		{
			std::cout << "ROLLBACK ERROR: " << rollbackEx.what() << std::endl;
		}
		results.clear();
	}
	catch (const std::exception& e)
	{
		std::cout << "STD ERROR: " << e.what() << std::endl;
		try
		{
			auto& db = Instance();
			auto* session = db.EnsureConnection();
			if (session)
				session->rollback();
		}
		catch (const std::exception& rollbackEx)
		{
			std::cout << "ROLLBACK ERROR: " << rollbackEx.what() << std::endl;
		}
		results.clear();
	}
	return results;
}

mysqlx::Session* MySqlMgr::EnsureConnection(bool forceUpdate)
{
	struct ThreadSession
	{
		std::unique_ptr<mysqlx::Session> session;
		uint64_t version = 0;
	};

	thread_local ThreadSession tls;

	if (forceUpdate)
	{
		tls.session.reset();
		tls.version = 0;
	}

	const uint64_t globalVersion = _configVersion.load(std::memory_order_acquire);
	if (tls.session && tls.version == globalVersion)
		return tls.session.get();

	std::string url;
	std::string user;
	std::string pass;
	std::string schema;
	unsigned int port = 0;
	{
		auto rLock = _lock.OnRead();
		url = _url;
		user = _user;
		pass = _pass;
		schema = _schema;
		port = _port;
	}

	if (url.empty() || user.empty() || schema.empty())
		return nullptr;

	try
	{
		auto setting = mysqlx::SessionSettings(
			mysqlx::SessionOption::USER, user,
			mysqlx::SessionOption::PWD, pass,
			mysqlx::SessionOption::HOST, url,
			mysqlx::SessionOption::PORT, port,
			mysqlx::SessionOption::DB, schema,
			mysqlx::SessionOption::SSL_MODE, mysqlx::SSLMode::REQUIRED
		);
		tls.session = std::make_unique<mysqlx::Session>(setting);
		tls.version = globalVersion;
	}
	catch (sql::SQLException& e)
	{
		std::cout << "MYSQL ERROR: " << e.getErrorCode() << std::endl;
		tls.session.reset();
		return nullptr;
	}
	catch (std::exception& e)
	{
		std::cout << "STD ERROR: " << e.what() << std::endl;
		tls.session.reset();
		return nullptr;
	}
	return tls.session.get();
}
int MySqlMgr::DebugDatabaseInit()
{
	auto& db = Instance();
	if (!db.EnsureConnection())
		return EXIT_FAILURE;

	try
	{
		std::string selectCols = "*";
		MySqlMgr::Select("`wkr_server_schema`.`v_user_info_with_asset`", selectCols, "", [](mysqlx::SqlResult&& selectRes)
			{
				while (auto resultElement = selectRes.fetchOne()) {
					// Process the row data
					std::cout << "ID: " << resultElement.get(2) << ", Name: " << resultElement.get(0) << ", Chips: " << resultElement.get(3) << std::endl;
				}
			});
		std::cout << "Sql Module Init Check Passed!" << std::endl;
	}
	catch (sql::SQLException& e)
	{
		std::cout << "MYSQL ERROR: " << e.getErrorCode() << std::endl;
		return EXIT_FAILURE;
	}
	catch (std::exception& e)
	{
		std::cout << "STD ERROR: " << e.what() << std::endl;
		return EXIT_FAILURE;
	}
	return EXIT_SUCCESS;
}
int MySqlMgr::Init(
	const std::string& url,
	const unsigned int port,
	const std::string& user,
	const std::string& pass,
	const std::string& schema)
{
	auto& db = Instance();
	{
		auto lock = db._lock.OnWrite();
		db._url = url;
		db._port = port;
		db._user = user;
		db._pass = pass;
		db._schema = schema;
		db._configVersion.fetch_add(1, std::memory_order_release);
	}
	db.EnsureConnection(true);

#ifdef ENABLE_SQL_DEBUG
	DebugDatabaseInit();
#endif
	return EXIT_SUCCESS;
}

void MySqlMgr::DoSql(const std::string& sqlCmd, std::function<void(mysqlx::SqlResult&&)> func)
{
	auto result = ExecuteSqlInternal(sqlCmd);
	if (func) func(std::move(result));
}

void MySqlMgr::DoSql(const std::vector<std::string>& sqlCmds, std::function<void(std::vector<mysqlx::SqlResult>&&)> func, bool enableLastIdReplace)
{
	auto results = ExecuteSqlInternal(sqlCmds, enableLastIdReplace);
	if (func) func(std::move(results));
}

void MySqlMgr::Select(const std::string& table, const std::string& columns, const std::string& where, std::function<void(mysqlx::SqlResult&&)> func)
{
	mysqlx::SqlResult result;
	try
	{
		auto& db = Instance();
		std::ostringstream ss;
		ss << "SELECT " << (columns.empty() ? "*" : columns) << " FROM " << table;
		if (!where.empty())
			ss << " WHERE " << where;
#ifdef ENABLE_SQL_DEBUG
		std::cout << "SQL DEBUG MSG: Select - " << ss.str() << std::endl;
#endif
		auto* session = db.EnsureConnection();
		if (session)
			result = session->sql(ss.str()).execute();
	}
	catch (const mysqlx::Error& e)
	{
		std::cout << "MYSQL ERROR: " << e << std::endl;
	}
	catch (const std::exception& e)
	{
		std::cout << "STD ERROR: " << e.what() << std::endl;
	}
	func(std::move(result));
}

void MySqlMgr::Update(const std::string& table, const std::string& setClause, const std::string& where, std::function<void(mysqlx::SqlResult&&)> func)
{
	mysqlx::SqlResult result;
	try
	{
		auto& db = Instance();
		std::ostringstream ss;
		ss << "UPDATE " << table << " SET " << setClause;
		if (!where.empty())
			ss << " WHERE " << where;
#ifdef ENABLE_SQL_DEBUG
		std::cout << "SQL DEBUG MSG: Update - " << ss.str() << std::endl;
#endif
		auto* session = db.EnsureConnection();
		if (session)
			result = session->sql(ss.str()).execute();
	}
	catch (const mysqlx::Error& e)
	{
		std::cout << "MYSQL ERROR: " << e << std::endl;
	}
	catch (const std::exception& e)
	{
		std::cout << "STD ERROR: " << e.what() << std::endl;
	}
	func(std::move(result));
}

void MySqlMgr::Delete(const std::string& table, const std::string& where, std::function<void(mysqlx::SqlResult&&)> func)
{
	mysqlx::SqlResult result;
	try
	{
		auto& db = Instance();
		std::ostringstream ss;
		ss << "DELETE FROM " << table;
		if (!where.empty())
			ss << " WHERE " << where;
#ifdef ENABLE_SQL_DEBUG
		std::cout << "SQL DEBUG MSG: Delete - " << ss.str() << std::endl;
#endif
		auto* session = db.EnsureConnection();
		if (session)
			result = session->sql(ss.str()).execute();
	}
	catch (const mysqlx::Error& e)
	{
		std::cout << "MYSQL ERROR: " << e << std::endl;
	}
	catch (const std::exception& e)
	{
		std::cout << "STD ERROR: " << e.what() << std::endl;
	}
	func(std::move(result));
}

void MySqlMgr::Upsert(const std::string& table, const std::string& columns, const std::string& values, const std::string& onDuplicateClause, std::function<void(mysqlx::SqlResult&&)> func)
{
	mysqlx::SqlResult result;
	try
	{
		auto& db = Instance();
		std::ostringstream ss;
		ss << "INSERT INTO " << table << " (" << columns << ") VALUES (" << values << ")";
		if (!onDuplicateClause.empty())
			ss << " ON DUPLICATE KEY UPDATE " << onDuplicateClause;
#ifdef ENABLE_SQL_DEBUG
		std::cout << "SQL DEBUG MSG: Upsert - " << ss.str() << std::endl;
#endif
		auto* session = db.EnsureConnection();
		if (session)
			result = session->sql(ss.str()).execute();
	}
	catch (const mysqlx::Error& e)
	{
		std::cout << "MYSQL ERROR: " << e << std::endl;
	}
	catch (const std::exception& e)
	{
		std::cout << "STD ERROR: " << e.what() << std::endl;
	}
	func(std::move(result));
}

std::future<mysqlx::SqlResult> MySqlMgr::DoSqlFuture(const std::string& sqlCmd)
{
	return DbThreadPool::Inst().EnqueueTask([sqlCmd]()
		{
			return ExecuteSqlInternal(sqlCmd);
		});
}

std::future<std::vector<mysqlx::SqlResult>> MySqlMgr::DoSqlFuture(const std::vector<std::string>& sqlCmds, bool enableLastIdReplace)
{
	return DbThreadPool::Inst().EnqueueTask([sqlCmds, enableLastIdReplace]()
		{
			return ExecuteSqlInternal(sqlCmds, enableLastIdReplace);
		});
}

void MySqlMgr::DoSqlAsync(const std::string& sqlCmd, std::function<void(mysqlx::SqlResult&&)> func)
{
	auto fut = DoSqlFuture(sqlCmd);
	if (func)
		MainThreadDispatcher::Watch(std::move(fut), std::move(func));
}

void MySqlMgr::DoSqlAsync(const std::vector<std::string>& sqlCmds, std::function<void(std::vector<mysqlx::SqlResult>&&)> func, bool enableLastIdReplace)
{
	auto fut = DoSqlFuture(sqlCmds, enableLastIdReplace);
	if (func)
		MainThreadDispatcher::Watch(std::move(fut), std::move(func));
}

void MySqlMgr::SelectAsync(const std::string& table, const std::string& columns, const std::string& where, std::function<void(mysqlx::SqlResult&&)> func)
{
	std::ostringstream ss;
	ss << "SELECT " << (columns.empty() ? "*" : columns) << " FROM " << table;
	if (!where.empty())
		ss << " WHERE " << where;
#ifdef ENABLE_SQL_DEBUG
	std::cout << "SQL DEBUG MSG: Select - " << ss.str() << std::endl;
#endif
	DoSqlAsync(ss.str(), std::move(func));
}

void MySqlMgr::UpdateAsync(const std::string& table, const std::string& setClause, const std::string& where, std::function<void(mysqlx::SqlResult&&)> func)
{
	std::ostringstream ss;
	ss << "UPDATE " << table << " SET " << setClause;
	if (!where.empty())
		ss << " WHERE " << where;
#ifdef ENABLE_SQL_DEBUG
	std::cout << "SQL DEBUG MSG: Update - " << ss.str() << std::endl;
#endif
	DoSqlAsync(ss.str(), std::move(func));
}

void MySqlMgr::DeleteAsync(const std::string& table, const std::string& where, std::function<void(mysqlx::SqlResult&&)> func)
{
	std::ostringstream ss;
	ss << "DELETE FROM " << table;
	if (!where.empty())
		ss << " WHERE " << where;
#ifdef ENABLE_SQL_DEBUG
	std::cout << "SQL DEBUG MSG: Delete - " << ss.str() << std::endl;
#endif
	DoSqlAsync(ss.str(), std::move(func));
}

void MySqlMgr::UpsertAsync(const std::string& table, const std::string& columns, const std::string& values, const std::string& onDuplicateClause, std::function<void(mysqlx::SqlResult&&)> func)
{
	std::ostringstream ss;
	ss << "INSERT INTO " << table << " (" << columns << ") VALUES (" << values << ")";
	if (!onDuplicateClause.empty())
		ss << " ON DUPLICATE KEY UPDATE " << onDuplicateClause;
#ifdef ENABLE_SQL_DEBUG
	std::cout << "SQL DEBUG MSG: Upsert - " << ss.str() << std::endl;
#endif
	DoSqlAsync(ss.str(), std::move(func));
}
