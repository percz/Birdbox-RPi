//============================================================================

/*
 * Name        : Birdbox.cpp
 * Author      : Alan Childs
 * Version     :
 * Copyright   : Code is public domain
 * Description :
 */

//============================================================================

/*
 * Configuration
 */
//#define INI_PATH "/etc/birdbox" //Moved to SimpleIni.h
#define LOCK_FILE "/var/run/birdbox.pid"
//#define CURL_STATICLIB //Building on Raspbian 10 for Raspbian 9 requires static linking

/*
 * WiringPi
 */
#define LED_INFRARED 6
#define LED_POWER 3
#define LED_NETWORKGRN 4
#define LED_NETWORKRED 5
#define LED_OCCUPIED 0
#define DHTINTP 10
#define DHTEXTP 1

//============================================================================

/*
 * Public Libs
 */
#include <iostream>
#include <wiringPi.h> //WiringPi
//TODO: Replcae wiringPi with #include <pigpio.h>
#include "cronie.h" //Very simple Cron-like handling of tasks to be run at certain time intervals
#include "plog/Log.h"  //Error logger from https://github.com/SergiusTheBest/plog#consoleappender
#include <csignal> //?
#include "mysqlcon_wrapper/mysqlconn_wrapper.h" //Wrapper library to handle connecting to database, in turn uses MySQLConnector/C++
#include "simpleini/SimpleIni.h" //Loads and handles the configuration files, e.g. anything not stored in the MySQL or MariaDB database
#include <fstream> //For CPU load and hardware status stuff

/*
 * Private Code
 */
#include "database/databaseConf.h" //The default database structure; very specific to this application
#include "sensor/sensors.h" //For sensor readings as nice INT variables
#include "sensor/tests.h" //Test of sensors and returning results

using namespace std;

//============================================================================

void exithandler(int signum) { //Log if program exits.
	LOG_WARNING << "Exiting on user request";
	exit(signum);
};

void readArduino(MySQLConnWrapper& database) {
		LOG_DEBUG << "Getting Arduino reading";
		ArduinoOutput ArduinoRead;
		ArduinoRead = ArduinoCall(); //Do all the Arduino requests in one hit, to avoid lots of serial calls.
		LOG_DEBUG << "Raw Arduino Data: Trigger = " << ArduinoRead.Trigger << "  ScalesRead = " << ArduinoRead.ScalesRead;
		switch (ArduinoRead.error) {
		case 2: { //Successfull reading
					bool haveMotion = 0;
					static char filename[] = "/dev/shm/haveMotion";
				    ifstream istrm (filename); //Read In
				    if (!istrm.is_open() ) {
				        LOG_WARNING << "Failed to open file " << filename;
				    } else {
				        istrm >> haveMotion;	//Read file to bool haveMotion
				        istrm.close();
				    }
					bool occupiedLED = digitalRead(LED_OCCUPIED); //Takes about 0.00025 ms
					if ( !occupiedLED && (ArduinoRead.ScalesRead > 2) ) {
						if( digitalRead(LED_INFRARED) ) {
							LOG_DEBUG << "IR light on, depending on camera motion detect";
							if ( haveMotion && (ArduinoRead.ScalesRead > 2) )  { //Assume bird arrived, don't tare
									LOG_DEBUG << "Assuming a bird has entered based on camera motion and current status";
									database.execute("INSERT INTO `readings_arduino` (`readtime`, `birdin`, `birdout`, `birdweight`) VALUES (TIMESTAMP(CURRENT_TIMESTAMP), 1 , 0 , " + to_string(ArduinoRead.ScalesRead) + ");");
									digitalWrite(LED_OCCUPIED, HIGH);
								}
						} else { //IR Lights Off, depend on door trigger
							LOG_DEBUG << "IR light off, using RAW Arduino data";
							if ( ArduinoRead.Trigger && (ArduinoRead.ScalesRead > 2) ) {
									digitalWrite(LED_OCCUPIED, HIGH);
									database.execute("INSERT INTO `readings_arduino` (`readtime`, `birdin`, `birdout`, `birdweight`) VALUES (TIMESTAMP(CURRENT_TIMESTAMP), 1 , 0 , " + to_string(ArduinoRead.ScalesRead) + " );");
							}
						}
					} else if (ArduinoRead.ScalesRead <= 2) {
						LOG_DEBUG << "Assuming a bird has left based on camera motion and current status";
						database.execute("INSERT INTO `readings_arduino` (`readtime`, `birdin`, `birdout`, `birdweight`) VALUES (TIMESTAMP(CURRENT_TIMESTAMP), 0 , 1 , 0 );");
						digitalWrite(LED_OCCUPIED, LOW);
						ArduinoTare();
					}
					database.execute("UPDATE `live_status` SET `readtime` = (TIMESTAMP(CURRENT_TIMESTAMP)), `ScalesRead` = '" + to_string(ArduinoRead.ScalesRead) + "';");
					break;
				}

    	default: { //Could make this even more specific, with each case/error code, but the user would be expected to run in test mode if fault repeats and that does
    			LOG_WARNING << "Failed to get a reading from the Arduino. If this repeats re-run Nestbox Datalogger with --test option. Error Code: " << ArduinoRead.error;
    		}
		}
}


#include "datapoint/datapoint.h"
void readForcast(MySQLConnWrapper& database, datapoint::api& weather) {
	LOG_DEBUG << "Getting Weather Forecast from Met Office Datapoint";
	//TODO: Change to forecast as this is more local and gets a general picture and not an exact observation.
	try {
		datapoint::observation observation = weather.GetObservation();
		LOG_DEBUG << "Raw MetOffice / Datapoint data: Weather Type = " << to_string(observation.weatherType) << " Wind Speed =  " << observation.windSpeed << "  Wind Gust = " << observation.windGust ;
		//Dumb down the windSpeed to Beaufort Wind Scale (0 to 12 strength)
		database.execute("INSERT INTO `readings_weather` (`readtime`, `weather_code`,`wind_speed`) VALUES (TIMESTAMP(CURRENT_TIMESTAMP), " + to_string(observation.weatherType) + " , " + to_string((stoi(observation.windSpeed) + stoi(observation.windGust)) / 2) + " );");
		}
	catch (int e) {
		LOG_WARNING << "Unable to get weather report (Datapoint API) on this try. Retrying on next schedule. Error code: " << e;
	}
}

void readDHT22(MySQLConnWrapper& database, bool needDatalog) {
	LOG_DEBUG << "Getting DHT22 internal reading from GPIO Pin " << DHTINTP;
	DHTOutput DHTRead;
	DHTRead = DHT22Call(DHTINTP);
	switch (DHTRead.error) {
		case 0: {
			LOG_DEBUG << "Raw DHT22 (Internal) data:   Temp = " << to_string(DHTRead.Temperature) << "    Humidity = " << to_string(DHTRead.Humidity);
			database.execute("UPDATE `live_status` SET `readtime` = (TIMESTAMP(CURRENT_TIMESTAMP)), `DHTint_Temp` = " + to_string(DHTRead.Temperature) + ", `DHTint_Humi` = " + to_string(DHTRead.Humidity) +";");
			if (needDatalog) {
				database.execute("INSERT INTO `readings_dht22int` (`readtime`, `temperature`, `humidity`) VALUES (CURRENT_TIMESTAMP, " + to_string(DHTRead.Temperature) + " , " + to_string(DHTRead.Humidity) + " );");
			}
   			break;
   		}
		default: {
   			LOG_WARNING << "Reading of internal temperature and humidity sensor (DHT22) failed.";
   		}
	}

	LOG_DEBUG << "Getting DHT22 external reading from GPIO Pin " << DHTEXTP;
	DHTRead = DHT22Call(DHTEXTP);
	switch (DHTRead.error) {
		case 0: {
			LOG_DEBUG << "Raw DHT22 (External) data:   Temp = " << to_string(DHTRead.Temperature) << "    Humidity = " << to_string(DHTRead.Humidity);
			database.execute("UPDATE `live_status` SET `readtime` = (TIMESTAMP(CURRENT_TIMESTAMP)), `DHText_Temp` = " + to_string(DHTRead.Temperature) + ", `DHText_Humi` = " + to_string(DHTRead.Humidity) + ";");
			if (needDatalog) {
				database.execute("INSERT INTO `readings_dht22ext` (`readtime`, `temperature`, `humidity`) VALUES (CURRENT_TIMESTAMP, " + to_string(DHTRead.Temperature) + "," + to_string(DHTRead.Humidity) + " );");
			}
			break;
		}
		default: {
			LOG_WARNING << "Reading of external temperature and humidity sensor (DHT22) failed.";
		}
	}
}

int timerRead(string settingName, int timer_interval, MySQLConnWrapper& database ) {
		database.query("SELECT `value` FROM `settings_readings` WHERE `setting` = '" +  settingName + "' " );
	if (!(database.fetch())) {
		database.execute("INSERT INTO `settings_readings` (`setting`, `value`) VALUES ('" + settingName + "', '" + to_string(timer_interval) + "')");
		LOG_WARNING << "No or invalid interval time set for " << settingName << ", reset to default - " << timer_interval;
	}
	else timer_interval = stoi(database.print("value"));
	LOG_INFO << "Configuring " << settingName << " readings at " << timer_interval << " minute intervals"; //We can set this to seconds in the 'cronie.h' code, but that's not a production setting
	return timer_interval;
}

void readLight(MySQLConnWrapper& database, unsigned int reqLux, bool needDatalog) {
	LOG_DEBUG << "Getting light sensor reading";
	unsigned short int lux = LightCall();
	LOG_DEBUG << "RAW Lux: " << lux;
	if (lux > 400000 || lux < 0 ) {
		LOG_WARNING << "Reading of light Sensor test failed.";
	} else {

		database.execute("UPDATE `live_status` SET `readtime` = (TIMESTAMP(CURRENT_TIMESTAMP)), `lux` = " + to_string(lux) + ";");

		if (needDatalog) {
			LOG_DEBUG << "Logging new light reading" ;
			database.execute("INSERT INTO `readings_light` (`readtime`, `lux`) VALUES (CURRENT_TIMESTAMP, " + to_string(lux) + " );");
			//We might want to put the IR LED light off in it's own latch time limit. But for now just using 'when we log data' will do.
			if ( digitalRead(LED_INFRARED) && (lux > (reqLux + 2500)) ) {
				digitalWrite(LED_INFRARED, 0);
				LOG_DEBUG << "Turning off LED InfraRed" ;
			}
		} else if (!digitalRead(LED_INFRARED) && (lux < reqLux) ) { //Always turn on IR when it's dark.
			digitalWrite(LED_INFRARED, 1);
			LOG_DEBUG << "Turning on LED InfraRed" ;
		}
	}
}

void rebuildDatabase(const char * pDbschema) { //TODO: This

	//resetRecordsDatabase(database);

}

void resetRecordsDatabase(const char * pDbschema) { //TODO: This

	//resetRecordsDatabase(database);

}

int main( int argc, char *argv[] ) {


	signal (SIGINT, exithandler); //For handling CTRL-C to the log


	/**********************************************************************/


	/*
	 * Get some basic settings from a local configuration file.
	 */
	CSimpleIniA settings; settings.SetUnicode();
   	settings.LoadFile(INI_PATH); //The LoadFile function is surprisingly tolerant if the file doesn't exist, so just continue if not there and make what's missing...
   	{
   		const char * pLogLevel  = settings.GetValue("Log", "level", "INFO");
   		const char * pLogFile  = settings.GetValue("Log", "log_path", "/var/log/birdbox");
   		/*
   		 * Setup logging stuff.
   		 * none = 0, fatal = 1, error = 2, warning = 3, info = 4, debug = 5, verbose = 6
   		 */
   		if (	string(pLogLevel) != "NONE" &&
   				string(pLogLevel) != "FATAL" &&
				string(pLogLevel) != "ERROR" &&
				string(pLogLevel) != "WARNING" &&
				string(pLogLevel) != "INFO" &&
				string(pLogLevel) != "DEBUG" &&
				string(pLogLevel) != "VERBOSE") {
   			cout << "Invalid or incomplete log level \"" << pLogLevel << "\" in the " << pLogFile << " configuration file. Please check and correct before running again.\n\n";
   			exit(0); //C++ Shrugs.
   		}
   		try {
   			plog::init(plog::severityFromString(pLogLevel), pLogFile, 1000000, 2 );
   		}
   		catch(exception& e) {
   			cout << "Failed to generate the log file, " << pLogFile << ", and quit. \n" << e.what() <<"\n";
   			exit(0);
   		}
   		LOG_INFO << "Birdbox Daemon Started";
   		LOG_INFO << "Build Time: " << __DATE__ << " " << __TIME__ ;
   	}

   	LOG_DEBUG << "Loading and setting database settings.";
	MySQLConnWrapper database; //Need somewhere for the settings to go. Load settings early - needed for test and rebuild options not just main runtime.
	{
		database.host = string( settings.GetValue("Database", "address", "localhost") );
		database.addr = database.host + ":" + string( settings.GetValue("Database", "port", "3306") );
		database.user = string(settings.GetValue("Database", "user", "USERNAME"));
		database.password = string(settings.GetValue("Database", "pass", "PASSWORD"));
		database.table = settings.GetValue("Database", "table", "birdbox");
		LOG_INFO << "Database - Address:" << database.host << " User:" << database.user << " Password:******** Table:" << database.table ;
	}

   	LOG_DEBUG << "Loading and setting weather settings.";
   	datapoint::api weather;
   	{
   		string key = string(settings.GetValue("Datapoint", "key", "01234567-89ab-cdef-0123-456789abcdef")); //Default Key: "01234567-89ab-cdef-0123-456789abcdef"
   		double lat = stod(string(settings.GetValue("Datapoint", "lat", "0.000000"))),
   			   lon = stod(string(settings.GetValue("Datapoint", "lon", "0.000000")));
   		LOG_INFO << "Datapoint - Key:******** Latitude:" << lat << " Longitude:" << lon;
   		if ( key == "01234567-89ab-cdef-0123-456789abcdef" || lat == 0.000000 || lon == 0.000000 ) {
   			cout << "Invalid or incomplete Datapoint Weather settings in the " << INI_PATH <<  " configuration file. \n"
   				 "Please check and correct before running again.\n Check you are using your own unique Datapoit API Key and Latitude and Longitude are in North/East decimal format. \n\n";
   		}
   		weather.Setkey(key);
   		weather.Setlocation(lat, lon);
	}

	/*
	 * Setup WiringPi stuff
	 */
	LOG_DEBUG << "Configuring WiringPi Library";
	#define	ENV_CODES	"WIRINGPI_CODES"
	if(wiringPiSetup()==-1) {
		cout << "Unable to setup Raspberry Pi GPIO. Check WiringPi installed and program is run with required permissions. \n";
		exit (0);
	}
	pinMode (LED_POWER, OUTPUT) ; pinMode (LED_OCCUPIED, OUTPUT) ; pinMode (LED_NETWORKGRN, OUTPUT) ; pinMode (LED_NETWORKRED, OUTPUT) ;
	digitalWrite(LED_POWER, LOW); digitalWrite(LED_NETWORKGRN, LOW); digitalWrite(LED_NETWORKRED, LOW);

	/*
	 * Find out, from the command line, what it is we need to do...
	 */
	LOG_DEBUG << "Parsing command line varibles.";
	{
	   bool needtoversion = false, needtohelp = false, needtoreset = false, needtodaemon = false; //Allows multiple options
	   for(int i = 1; i < argc; i++) {
	      if ( string(argv[i]) == "--version" ) { //Display the version
	         needtoversion = true;
	          continue;
	      } else if ( string(argv[i]) == "--reset" ) {
	    	  needtoversion = true; needtoreset = true;
	    	  continue;
	      } else if (string(argv[i]) == "--help") {
	    	  needtoversion = true; needtohelp = true;
	    	  continue;
	      } else if ( string(argv[i]) == "--test" ) {
	    	  testhardware();
	    	  needtoversion = true;
	    	  break;
	      } else if ( string(argv[i]) == "--builddb" ) {
	    	  //Most stuff should be build-on-failure anyway though.
	    	  rebuildDatabase(database.table);
	    	  cout << "The SQL database has been successfully rebuilt.\n";
	    	  needtoversion = true;
	    	  continue;
	   	  } else if (string(argv[i]) == "--daemon") {
	    	  needtoversion = false; needtohelp = false; needtoreset = false;
	    	  needtodaemon = true;
	    	  break;
	      } else {
	    	  cout << "Invalid Option \"" << string(argv[i]) << "\"\nUse --help command for list of program options.\n";
	    	  needtoversion = true; // needtohelp = true;
	      }
	   }
	   if (needtoversion || needtohelp || needtoreset) {
		   if (needtoversion) {
			   cout << "\nBirdbox Datalogger\nProgram Build Time: " << __DATE__ << " " << __TIME__ << "\n"
					   "This is free software with ABSOLUTELY NO WARRANTY.\n\n";
		   }
		   if (needtohelp) {
			   cout << "\nValid command-line options : \n"
					   "\t--daemon\n"
					   "\t\tStart as a daemon process. You should ensure only a single daemon is running.\n"
					   "\t--version\n"
					   "\t\tDisplay the last program build time\n"
					   "\t--test\n"
					   "\t\tPerform a basic test of all the sensors, and print results.\n"
					   "\t--reset\n"
			   	   	   "\t\tDelete the entire database contents. You should ensure you backup first.\n"
					   "\t--builddb\n"
					   "\t\t(re)build the SQL database tables needed for Birdbox Datalogger, non-destructively.\n"
			           "\t--help\n"
			   	   	   "\t\tPrint this help message.\n"
					   "\nEnsure other settings are set in " << INI_PATH << "\n\n" ;
		   }
		   if (needtoreset) {
		    	resetRecordsDatabase(database.table);
		    	LOG_INFO << "Reset database on user request.";
		    	cout << "\nDatabase Reset\n";
		   }
		   LOG_INFO << "Exiting after completing command line options.";
		   exit(0);
	   }
	   /*INSERT INTO `settings_readings` (`KEY`, `setting`, `value`) VALUES (NULL, 'required_lux', '15500');
	    * Fork the Parent Process, and become a background process.
	    */
	   if (needtodaemon) {
		   LOG_DEBUG << "Forking to daemon.";
		   //First we fork...
		   pid_t pid, sid; pid = fork();
		   if (pid < 0) { exit(EXIT_FAILURE); }
		   if (pid > 0) { exit(EXIT_SUCCESS); } //We got a good pid, Close the Parent Process
		   umask(0); sid = setsid();
		   if (sid < 0 || (chdir("/")) < 0 ) { exit(EXIT_FAILURE); }
		   //...THEN try and open the lockfile as the child. 'Cause we kill the parent and keep child, we don't want to lock as parent.
		   	int lfp = open(LOCK_FILE,O_RDWR|O_CREAT,0640);
			if ( lfp<0 || lockf(lfp,F_TLOCK,0)<0 ) {
				LOG_FATAL << "Detected deamon already running, will exit.";
				cout << "Unable to start as Daemon. Another Birdbox Datalogger daemon is already running.\n";
				exit(0);
			}
			char str[10]; sprintf(str,"%d\n",getpid());
			write(lfp,str,strlen(str));
			close(STDIN_FILENO); close(STDOUT_FILENO); close(STDERR_FILENO); //Close Standard File Descriptors
	   }
	}

	/*
	 * Open a database, use settings imported from INI
	 */
	LOG_INFO << "Connecting database...";
   	if (database.user == "USERNAME" || database.password == "PASSWORD") { //Set by, and passed by, INI script as defaults if nothing set
   		CSimpleIniA settings; settings.SetUnicode();
   		cout << "\n\033[1;31mERROR: Invalid or incomplete username and password for the database in \033[0m" << INI_PATH << "\033[1;31m configuration file.\n Please check and correct before running again.\033[0m\n\n";
   		settings.SetValue("Database", "user", "USERNAME");
   		settings.SetValue("Database", "pass", "PASSWORD");
   		settings.SaveFile(INI_PATH);
   		exit(0); //Stop the program, or change to a Throw if that's your bag.
   	}
	database.connect();
	database.switchDb(database.table);

	LOG_DEBUG << "Configuring Required Lux";
	unsigned int reqLux = 15500;
	database.query("SELECT `value` FROM `settings_readings` WHERE `settings_readings`.`setting` = 'required_lux' ;" );
	if ( database.fetch() ) {
		LOG_DEBUG << "Reading required Lux from Database.";
		reqLux = stoi(database.print("value"));
	} else {
		database.query("INSERT INTO `settings_readings` (`KEY`, `setting`, `value`) VALUES (NULL, 'required_lux', '15500');");
		LOG_INFO << "Inserting default required Lux setting to Database.";
	}

	//TODO: Bring back Cronie!
	LOG_DEBUG << "Configuring Lux Timer";
	short unsigned int lighttimer = (timerRead( "time_lightTmr" , 15, database) * 60 ); //Convert minutes to seconds
	unsigned int lastlight = 0;
	database.query("SELECT unix_timestamp(readtime) FROM `readings_light` ORDER BY readtime DESC LIMIT 1 ;" );
	if ( database.fetch() ) {
		lastlight = stoi(database.print("unix_timestamp(readtime)"));
	}

	LOG_DEBUG << "Configuring DHT Timer";
	short unsigned int dhttimer = (timerRead( "time_dhtTmr" , 15, database) * 60 ); //Convert minutes to seconds
	unsigned int lastdhtLV = time(0), lastdhtDB = 0; //Last time we updated the Live status and the last update to the Database record
	database.query("SELECT unix_timestamp(readtime) FROM `readings_dht22int` ORDER BY readtime DESC LIMIT 1 ;" );
	if ( database.fetch() ) {
		lastdhtDB = stoi(database.print("unix_timestamp(readtime)"));
	}

	LOG_DEBUG << "Configuring Forecast Timer";
	short unsigned int forcastTmr = ( timerRead( "time_forcastTmr" , 60, database) );
	unsigned int lastForcast = time(0); //Always give the best gap to keep within T&C of API
	database.query("SELECT unix_timestamp(readtime) FROM `readings_weather` ORDER BY readtime DESC LIMIT 1 ;" );
	if ( database.fetch() ) {
		lastForcast = stoi(database.print("unix_timestamp(readtime)"));
	}



	/**********************************************************************/

	long previousMillis = 0, currentMillis = 0;
	static unsigned short int minimumTime = 1000; int requiredDelay = 100; //Time between blinks, and time checks. Milliseconds.
	//netstat network; //Network status object

	database.fin();
	delay( 10 );

	LOG_DEBUG << "Loop begins...";
	for ( ; ; ) { //Arduino much?

		currentMillis = millis(); //Arduino much?
		if (currentMillis - previousMillis >= minimumTime) {
			/*
			 *   Always flash the LED, regardless of what else needs doing.
			 */
	    	if (!digitalRead(LED_POWER)) digitalWrite (LED_POWER, HIGH) ;
	    	else digitalWrite (LED_POWER, LOW) ;
			previousMillis = currentMillis;

			/*
			 * Keep network test in a collapsing stack
			 *
			switch (network.test()) { //TODO: Two WIFIs, no Wired, for RPi3
				case 0:
					LOG_FATAL << "Network down.";
					digitalWrite(LED_NETWORKGRN, LOW); digitalWrite(LED_NETWORKRED, LOW);
					break;
				case 1:
					LOG_INFO << "Wireless network up.";
					digitalWrite(LED_NETWORKGRN, HIGH); digitalWrite(LED_NETWORKRED, LOW);
					break;
				case 2:
					LOG_INFO << "Wired network up.";
					digitalWrite(LED_NETWORKGRN, LOW); digitalWrite(LED_NETWORKRED, HIGH);
					break;
				default:
					break;
			}
			*/
		}

		database.connect();
		database.switchDb(database.table);

		readArduino(database);

		if ( (  time(0) - lastlight ) > lighttimer ) {
			LOG_DEBUG << "Timer called readLight() to Database.";
			readLight(database, reqLux, true);
			database.query("SELECT unix_timestamp(readtime) FROM `readings_light` ORDER BY readtime DESC LIMIT 1 ;" );
			if ( database.fetch() ) {
				lastlight = stoi(database.print("unix_timestamp(readtime)"));
			}
		} else {
			LOG_DEBUG << "Timer called readLight() for Status.";
			readLight(database, reqLux, false);
		}

		if ( ( time(0) - lastdhtDB ) > dhttimer ) {
    		LOG_DEBUG << "Timer called readDHT22() to Database.";
    		readDHT22(database, true);
    		lastdhtLV = time(0);
			database.query("SELECT unix_timestamp(readtime) FROM `readings_dht22int` ORDER BY readtime DESC LIMIT 1 ;" );
			if ( database.fetch() ) {
				lastdhtDB = stoi(database.print("unix_timestamp(readtime)"));
			}
    	} else if ( ( time(0) - lastdhtLV ) > 30 ) { //Hard coded rate limit of calls 30 seconds apart as the DHT22's are slow and hard-crash if pushed.
    		LOG_DEBUG << "Timer called readDHT22() for Status.";
    		readDHT22(database, false);
    		lastdhtLV = time(0);
    	}

    	if ( ( time(0) - lastForcast ) > forcastTmr ) {
    		LOG_DEBUG << "Timer called readForcast().";
    		readForcast(database, weather);
    		lastForcast = time(0);
    	}

    	database.fin();

    	requiredDelay = ((minimumTime - (currentMillis - previousMillis)) + 1);
    	if (requiredDelay > 1) {
    			delay( requiredDelay );
    	}


	}

    return 0;
}
