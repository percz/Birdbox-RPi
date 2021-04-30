#include "../mysqlcon_wrapper/mysqlconn_wrapper.h"

#ifndef DATABASECONF_H
#define	DATABASECONF_H

void rebuildDatabase(MySQLConnWrapper& database); //Pass the class as a reference

void resetRecordsDatabase(MySQLConnWrapper& database);
void resetSettingsDatabase(MySQLConnWrapper& database);

void testDatabase(MySQLConnWrapper& database);

#endif	/* DATABASECONF_H */
