/*
 * ConnJDBC.cpp
 *
 *  Created on: Mar 19, 2020
 *      Author: ai_002
 */

#include "ConnJDBC.h"

ConnJDBC::ConnJDBC()
 : m_JdbcStat(false),
   m_pConn(nullptr),
   m_pStat(nullptr),
   m_pDriver(nullptr)
{
	// TODO Auto-generated constructor stub

	m_pDriver = sql::mysql::get_mysql_driver_instance();

	m_pConn = m_pDriver->connect("localhost", "admini", "123456");

	m_pConn->setAutoCommit(1);

	m_pStat = m_pConn->createStatement();

	//在默认配置不改变的情况下，如果连续8小时内都没有访问数据库的操作，再次访问mysql数据库的时候，mysql数据库会拒绝访问
	m_pStat->executeUpdate("set global wait_timeout=2814400;");
	m_pStat->executeUpdate("set global interactive_timeout=2814400;");

	m_JdbcStat = true;
}

ConnJDBC::~ConnJDBC()
{
	// TODO Auto-generated destructor stub
	if (m_pConn->isValid())
		m_pConn->close();
	delete m_pConn;
}


/*
 * Redis 类实现
 */
