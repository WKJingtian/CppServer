#include "pch.h"
#include "MySqlMgr.h"
#include <sstream>
#include <functional>
#include <iostream>
#include "Utils/Utils.h"
#include "Const.h"

MySqlMgr::MySqlMgr() = default;

MySqlMgr& MySqlMgr::Instance()
{
	static MySqlMgr instance;
	return instance;
}

std::shared_ptr<mysqlx::Client> MySqlMgr::EnsureClient(bool forceUpdate)
{
	if (!forceUpdate)
	{
		auto rLock = _lock.OnRead();
		if (_client)
			return _client;
	}

	auto wLock = _lock.OnWrite();
	if (!forceUpdate && _client)
		return _client;

	if (_url.empty() || _user.empty() || _schema.empty())
	{
		_client.reset();
		return nullptr;
	}

	try
	{
		_client = std::make_shared<mysqlx::Client>(
			mysqlx::SessionOption::USER, _user,
			mysqlx::SessionOption::PWD, _pass,
			mysqlx::SessionOption::HOST, _url,
			mysqlx::SessionOption::PORT, _port,
			mysqlx::SessionOption::DB, _schema,
			mysqlx::SessionOption::SSL_MODE, mysqlx::SSLMode::REQUIRED,
			mysqlx::ClientOption::POOLING, true
		);
	}
	catch (sql::SQLException& e)
	{
		std::cout << "MYSQL ERROR: " << e.getErrorCode() << std::endl;
		_client.reset();
		return nullptr;
	}
	catch (std::exception& e)
	{
		std::cout << "STD ERROR: " << e.what() << std::endl;
		_client.reset();
		return nullptr;
	}
	return _client;
}
int MySqlMgr::DebugDatabaseInit()
{
	auto& db = Instance();
	auto client = db.EnsureClient();
	if (!client)
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
	}
	db.EnsureClient(true);

#ifdef ENABLE_SQL_DEBUG
	DebugDatabaseInit();
#endif
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
		auto client = db.EnsureClient();
		if (!client)
		{
			func(std::move(result));
			return;
		}

		mysqlx::Session session(*client);
		result = session.sql(sqlCmd).execute();
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
	auto& db = Instance();
	auto client = db.EnsureClient();
	if (!client)
	{
		func(std::move(results));
		return;
	}

	std::unique_ptr<mysqlx::Session> session;
	try
	{
		session = std::make_unique<mysqlx::Session>(*client);
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
			if (session)
				session->rollback();
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
		auto client = db.EnsureClient();
		if (!client)
		{
			func(std::move(result));
			return;
		}

		std::ostringstream ss;
		ss << "SELECT " << (columns.empty() ? "*" : columns) << " FROM " << table;
		if (!where.empty())
			ss << " WHERE " << where;
#ifdef ENABLE_SQL_DEBUG
		std::cout << "SQL DEBUG MSG: Select - " << ss.str() << std::endl;
#endif
		mysqlx::Session session(*client);
		result = session.sql(ss.str()).execute();
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
		auto client = db.EnsureClient();
		if (!client)
		{
			func(std::move(result));
			return;
		}

		std::ostringstream ss;
		ss << "UPDATE " << table << " SET " << setClause;
		if (!where.empty())
			ss << " WHERE " << where;
#ifdef ENABLE_SQL_DEBUG
		std::cout << "SQL DEBUG MSG: Update - " << ss.str() << std::endl;
#endif
		mysqlx::Session session(*client);
		result = session.sql(ss.str()).execute();
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
		auto client = db.EnsureClient();
		if (!client)
		{
			func(std::move(result));
			return;
		}

		std::ostringstream ss;
		ss << "DELETE FROM " << table;
		if (!where.empty())
			ss << " WHERE " << where;
#ifdef ENABLE_SQL_DEBUG
		std::cout << "SQL DEBUG MSG: Delete - " << ss.str() << std::endl;
#endif
		mysqlx::Session session(*client);
		result = session.sql(ss.str()).execute();
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
		auto client = db.EnsureClient();
		if (!client)
		{
			func(std::move(result));
			return;
		}

		std::ostringstream ss;
		ss << "INSERT INTO " << table << " (" << columns << ") VALUES (" << values << ")";
		if (!onDuplicateClause.empty())
			ss << " ON DUPLICATE KEY UPDATE " << onDuplicateClause;
#ifdef ENABLE_SQL_DEBUG
		std::cout << "SQL DEBUG MSG: Upsert - " << ss.str() << std::endl;
#endif
		mysqlx::Session session(*client);
		result = session.sql(ss.str()).execute();
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
