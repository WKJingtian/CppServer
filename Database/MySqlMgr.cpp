#include "pch.h"
#include "MySqlMgr.h"
#include <sstream>
#include <functional>
#include <iostream>

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
		auto schemas = db._sqlSession->getSchema(db._schema);
		auto table = schemas.getTable("user");
		auto result = table.select("_id", "_name").execute();
		while (auto resultElement = result.fetchOne()) {
			// Process the row data
			std::cout << "ID: " << resultElement.get(0) << ", Name: " << resultElement.get(1) << std::endl;
		}
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
	auto lock = db._lock.OnWrite();
	db._url = url;
	db._port = port;
	db._user = user;
	db._pass = pass;
	db._schema = schema;
	db.EnsureConnection(true);
	DebugDatabaseInit();
	return EXIT_SUCCESS;
}

void MySqlMgr::DoSql(const std::string& sqlCmd, std::function<void(mysqlx::SqlResult&&)> func)
{
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
