/*
 * ConnJDBC.h
 *
 *  Created on: Mar 19, 2020
 *      Author: ai_002
 */

#ifndef CONNJDBC_H_
#define CONNJDBC_H_

#include <iostream>
#include <map>
#include <string>
#include <memory>
#include <sys/time.h>

#include <jdbc/mysql_driver.h>
#include <jdbc/mysql_connection.h>
#include <jdbc/cppconn/driver.h>
#include <jdbc/cppconn/statement.h>
#include <jdbc/cppconn/prepared_statement.h>
#include <jdbc/cppconn/metadata.h>
#include <jdbc/cppconn/exception.h>

using namespace sql;

class ConnJDBC {
public:
	ConnJDBC();
	~ConnJDBC();

	inline bool initStatus() { return m_JdbcStat; }

	inline void selectDataBase(const std::string& command)
	{
		m_pStat->execute(command);
	}

	inline ResultSet* setQueryCommand(const std::string& command)
	{
		return m_pStat->executeQuery(command);
	}

	inline int setUpdateCommand(const std::string& command)
	{
		if (m_pConn->isClosed())
	    {   
			m_pStat->close();
			m_pConn->reconnect();
			m_pConn->setAutoCommit(1);
		    m_pStat = m_pConn->createStatement();
	    }
		//m_pStatU = m_pConn->createStatement();	
		int updateCount = m_pStat->executeUpdate(command);
		return updateCount;
	}
	inline void setRollBack()
	{
		m_pConn->rollback();
		//m_pStat->close();
		//m_pStatU->close();
		//m_pStat = m_pConn->createStatement();	
	}

private:
	bool m_JdbcStat;
	Connection *m_pConn;
	Statement *m_pStat;
	//Statement *m_pStatU;
	mysql::MySQL_Driver *m_pDriver;
};

#endif /* CONNJDBC_H_ */
