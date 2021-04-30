//============================================================================
// Name        : Birdbox.cpp
// Author      : Alan Childs
// Version     :
// Copyright   : Code is public domain
// Description :
//============================================================================

#include <iostream>
#include "../mysqlcon_wrapper/mysqlconn_wrapper.h"


void rebuildDatabase(MySQLConnWrapper& database) {

//Main database
	database.execute("CREATE DATABASE IF NOT EXISTS `birdbox` DEFAULT CHARACTER SET latin1 COLLATE latin1_swedish_ci;");

//Readings
	database.execute("CREATE TABLE IF NOT EXISTS `readings_arduino` ("
	  	  	  	  	 "`readtime` timestamp NOT NULL DEFAULT CURRENT_TIMESTAMP COMMENT 'Time of reading',"
	  	  	  	  	 "`birdin` tinyint(3) UNSIGNED DEFAULT NULL COMMENT 'Number of entries',"
	  	  	  	  	 "`birdout` tinyint(3) UNSIGNED DEFAULT NULL COMMENT 'Number of exits',"
	  	  	  	  	 "`birdweight` decimal(10,0) DEFAULT NULL COMMENT 'Last bird weight ',"
	  	  	  	  	 "PRIMARY KEY (`readtime`)"
					 ") ENGINE=InnoDB DEFAULT CHARSET=latin1 COMMENT='Arduino readings of bird entries and weight';");

	database.execute("CREATE TABLE IF NOT EXISTS `readings_dht22ext` ("
					 "`readtime` timestamp NOT NULL DEFAULT CURRENT_TIMESTAMP,"
					 "`tempature` decimal(4,1) UNSIGNED NOT NULL COMMENT 'Always positive and read as centigrade',"
					 "`humidity` decimal(5,2) UNSIGNED NOT NULL COMMENT 'Always positive and read as a percentage',"
					 "PRIMARY KEY (`readtime`)"
					 ") ENGINE=InnoDB DEFAULT CHARSET=latin1;");

	database.execute("CREATE TABLE IF NOT EXISTS `readings_dht22int` ("
					 "`readtime` timestamp NOT NULL DEFAULT CURRENT_TIMESTAMP,"
					 "`tempature` decimal(4,1) UNSIGNED NOT NULL COMMENT 'Always positive and read as centigrade',"
					 "`humidity` decimal(4,1) UNSIGNED NOT NULL COMMENT 'Always positive and read as a percentage',"
					 "PRIMARY KEY (`readtime`)"
					 ") ENGINE=InnoDB DEFAULT CHARSET=latin1;");

	database.execute("CREATE TABLE IF NOT EXISTS `readings_light` ("
				     "`readtime` timestamp NOT NULL DEFAULT CURRENT_TIMESTAMP,"
					 "`lux` smallint(5) UNSIGNED NOT NULL"
					 ") ENGINE=InnoDB DEFAULT CHARSET=latin1;");

//Settings
	database.execute("CREATE TABLE IF NOT EXISTS `settings_readings` ("
					 "`setting` varchar(60) NOT NULL COMMENT 'Setting Name',"
					 "`value` tinyint(3) UNSIGNED NOT NULL DEFAULT '5',"
					 "UNIQUE KEY `time_arduino` (`setting`,`value`)"
					 ") ENGINE=InnoDB DEFAULT CHARSET=latin1;");

}

void resetRecordsDatabase(MySQLConnWrapper& database) {
	database.execute("TRUNCATE `readings_arduino`; ");
	database.execute("TRUNCATE `readings_dht22int`; ");
	database.execute("TRUNCATE `readings_dht22ext`; ");
	database.execute("TRUNCATE `readings_light`; ");
}

void resetSettingsDatabase(MySQLConnWrapper& database) {
	database.execute("CREATE TABLE IF NOT EXISTS `settings_readings` ("
					 "`setting` varchar(60) NOT NULL COMMENT 'Setting Name',"
					 "`value` tinyint(3) UNSIGNED NOT NULL DEFAULT '5',"
					 ") ENGINE=InnoDB DEFAULT CHARSET=latin1;");

	database.execute("DELETE FROM `settings_readings` WHERE 1 = 1;");
	//Everything else is (currently) set by the main function anyway.
}

void testDatabase(MySQLConnWrapper& database) {

	//Check database structure is valid

}
