#include "pch.h"
#include "MySqlMgr.h"
#include <sstream>
#include <functional>
#include <iostream>
#include "Utils/Utils.h"
#include "Const.h"

MySqlMgr::MySqlMgr() : _sqlSession(nullptr) {}

MySqlMgr& MySqlMgr::Instance()
{
	static MySqlMgr instance;
	return instance;
}

void MySqlMgr::EnsureConnection(bool forceUpdate)
{
	if (_sqlSession && !forceUpdate)
		return;

	try
	{
		auto setting = mysqlx::SessionSettings(
			mysqlx::SessionOption::USER, _user,
			mysqlx::SessionOption::PWD, _pass,
			mysqlx::SessionOption::HOST, _url,
			mysqlx::SessionOption::PORT, _port,
			mysqlx::SessionOption::DB, _schema,
			mysqlx::SessionOption::SSL_MODE, mysqlx::SSLMode::REQUIRED
		);
		//_sqlSession = std::make_unique<mysqlx::Session>(setting);
		_sqlSession = new mysqlx::Session(setting);
	}
	catch (sql::SQLException& e)
	{
		std::cout << "MYSQL ERROR: " << e.getErrorCode() << std::endl;
		return;
	}
	catch (std::exception& e)
	{
		std::cout << "STD ERROR: " << e.what() << std::endl;
		return;
	}
}
int MySqlMgr::DebugDatabaseInit()
{
	auto& db = Instance();
	db.EnsureConnection();

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
	{
		auto& db = Instance();
		auto lock = db._lock.OnWrite();
		db._url = url;
		db._port = port;
		db._user = user;
		db._pass = pass;
		db._schema = schema;
		db.EnsureConnection(true);
	}
	DebugDatabaseInit();
	return EXIT_SUCCESS;
}

void MySqlMgr::DoSql(const std::string& sqlCmd, std::function<void(mysqlx::SqlResult&&)> func)
{
#ifdef ENABLE_SQL_DEBUG
	std::cout << "SQL DEBUG MSG: DoSql - " << sqlCmd << std::endl;
#endif
	mysqlx::SqlResult result;
	try
	{
		auto& db = Instance();
		auto lock = db._lock.OnWrite();
		db.EnsureConnection();

		result = db._sqlSession->sql(sqlCmd).execute();
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

void MySqlMgr::DoSql(const std::vector<std::string>& sqlCmds, std::function<void(std::vector<mysqlx::SqlResult>&&)> func, bool enableLastIdReplace)
{
#ifdef ENABLE_SQL_DEBUG
	for (const auto& sqlCmd : sqlCmds)
		std::cout << "SQL DEBUG MSG: DoSql - " << sqlCmd << std::endl;
#endif
	std::vector<mysqlx::SqlResult> results;
	try
	{
		auto& db = Instance();
		auto lock = db._lock.OnWrite();
		db.EnsureConnection();

		db._sqlSession->startTransaction();

		int lastId = -1;
		for (const auto& sqlCmd : sqlCmds)
		{
			std::string finalCmd = sqlCmd;
			if (enableLastIdReplace)
				Utils::StringReplace(finalCmd, "LAST_INSERT_ID", std::to_string(lastId));
			results.push_back(db._sqlSession->sql(finalCmd).execute());
			lastId = static_cast<int>(results[results.size() - 1].getAutoIncrementValue());
		}

		db._sqlSession->commit();
	}
	catch (const mysqlx::Error& e)
	{
		std::cout << "MYSQL ERROR: " << e << std::endl;
		try
		{
			auto& db = Instance();
			db._sqlSession->rollback();
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
			db._sqlSession->rollback();
		}
		catch (const std::exception& rollbackEx)
		{
			std::cout << "ROLLBACK ERROR: " << rollbackEx.what() << std::endl;
		}
		results.clear();
	}
	func(std::move(results));
}

void MySqlMgr::Select(const std::string& table, const std::string& columns, const std::string& where, std::function<void(mysqlx::SqlResult&&)> func)
{
	mysqlx::SqlResult result;
	try
	{
		auto& db = Instance();
		auto lock = db._lock.OnWrite();
		db.EnsureConnection();

		std::ostringstream ss;
		ss << "SELECT " << (columns.empty() ? "*" : columns) << " FROM " << table;
		if (!where.empty())
			ss << " WHERE " << where;
#ifdef ENABLE_SQL_DEBUG
		std::cout << "SQL DEBUG MSG: Select - " << ss.str() << std::endl;
#endif
		result = db._sqlSession->sql(ss.str()).execute();
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
		auto lock = db._lock.OnWrite();
		db.EnsureConnection();

		std::ostringstream ss;
		ss << "UPDATE " << table << " SET " << setClause;
		if (!where.empty())
			ss << " WHERE " << where;
#ifdef ENABLE_SQL_DEBUG
		std::cout << "SQL DEBUG MSG: Update - " << ss.str() << std::endl;
#endif
		result = db._sqlSession->sql(ss.str()).execute();
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
		auto lock = db._lock.OnWrite();
		db.EnsureConnection();

		std::ostringstream ss;
		ss << "DELETE FROM " << table;
		if (!where.empty())
			ss << " WHERE " << where;
#ifdef ENABLE_SQL_DEBUG
		std::cout << "SQL DEBUG MSG: Delete - " << ss.str() << std::endl;
#endif
		result = db._sqlSession->sql(ss.str()).execute();
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
		auto lock = db._lock.OnWrite();
		db.EnsureConnection();

		std::ostringstream ss;
		ss << "INSERT INTO " << table << " (" << columns << ") VALUES (" << values << ")";
		if (!onDuplicateClause.empty())
			ss << " ON DUPLICATE KEY UPDATE " << onDuplicateClause;
#ifdef ENABLE_SQL_DEBUG
		std::cout << "SQL DEBUG MSG: Upsert - " << ss.str() << std::endl;
#endif
		result = db._sqlSession->sql(ss.str()).execute();
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
