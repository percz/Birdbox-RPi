//============================================================================
// Name        : BirdboxTestSubs.cpp
// Created	   : In the past
// Author      : Alan Childs
// Version     : 0.1
// Copyright   : None really
// Description : Run test routines, calling sensors.cpp, and return results
//============================================================================

#include "tests.h"
#include "sensors.h"

#include <iostream>
#include <wiringPi.h> //WiringPi
#include "../plog/Log.h"  //Error logger from https://github.com/SergiusTheBest/plog#consoleappender
#include "../mysqlcon_wrapper/mysqlconn_wrapper.h" //Wrapper library to handle connecting to database, in turn uses MySQLConnector/C++
#include <fstream> //For CPU load and hardware status stuff
#include "../database/databaseConf.h" //The default database structure; very specific to this application

int BlinkTestLEDS (int fast) //Make sure we can blink the LEDs on and off...
{
  int LEDfaults = 0 ;
  // wiringPiSetup () ;
  pinMode (0, OUTPUT) ; //Mid Red
  pinMode (3, OUTPUT) ; //Right Toggle (1=Red,0=Green)
  pinMode (4, OUTPUT) ; //Left Green
  pinMode (5, OUTPUT) ; //Left Red

  int LEDdelay = 2 ; if (fast) { LEDdelay = 1 ;}
  clock_t highDelay = (LEDdelay * 100000); //thousandth of a second
  clock_t lowDelay = (LEDdelay * 100000); //thousandth of a second

  clock_t start_time = clock();
  int trigger_high = start_time; //start high
  int trigger_low = ( start_time + lowDelay );

  for ( int i = 0 ; i < (LEDdelay * 10) ; ) //This loop and clock example is heavy on the CPU load, but non-blocking and fairly accurate!
  {

	clock_t start_time = clock();

	if ( start_time > trigger_high ) {
		digitalWrite (0, HIGH) ;
		digitalWrite (3, HIGH) ;
		digitalWrite (4, LOW) ;
		digitalWrite (5, HIGH) ;
		if (trigger_low < trigger_high ) { trigger_low = start_time + lowDelay; i++ ; }
		if ( !digitalRead(0) && !digitalRead(3) && digitalRead(4) && !digitalRead(5)  ) LEDfaults = 1; //Really basic error check; we can't check the circuit but just confirm RPi/WiringPi settings accepted
	}
	if ( start_time > trigger_low ) {
		digitalWrite (0, LOW) ;
		digitalWrite (3, LOW) ;
		digitalWrite (4, HIGH) ;
		digitalWrite (5, LOW) ;
		if (trigger_high < trigger_low ) { trigger_high = start_time + highDelay; }
		if ( digitalRead(0) && digitalRead(3) && !digitalRead(4) && digitalRead(5)  ) LEDfaults = 1;
	}
  }
  return LEDfaults ;
}

void testhardware() {
  cout << string( 2, '\n' );
  string errorMsg = "";
  /*
   * Test the LEDs
   */
  cout << "Beginning LED test, please visually check all LED's blink.\n"; sleep(2);
  pinMode(6, OUTPUT) ; //Infrared on the birdbox should be an output anyway's, so no need to check it - just set it.
  bool InfraStart;
  if (!digitalRead(6)) {
	  digitalWrite(6, HIGH) ;
	  InfraStart = false;
  }
  else InfraStart = true;
  if ( BlinkTestLEDS(0) || !digitalRead(6) ) { //We don't get an actual feedback from the LED's, just check WiringPi can set the pin high.
	  cout << setw(26) << left << "LED Test : "         << setw(20)  << "Cycle Failed" << setw(26) << left << ""                         <<"\033[31m[FAIL]\033[0m \n" ;
	  errorMsg += "LED test failed! Please check the ULN2003 IC and GPIO pins for shorts.\n" ;
  }
  else { cout << setw(26) << left << "LED Test : "         << setw(20)  << "Cycle Completed" << setw(26) << left << ""                         <<"\033[32m[PASS]\033[0m  \n" ;}
  if (!InfraStart) digitalWrite(6, LOW) ; //Restore to original state

  /*
   * Test the TSL2561 Light Sensor
   */
  int lux;
  lux = LightCall();
  if (lux > 40000 || lux < 0 ) {
	  cout << setw(26) << left << "Light Sensor : "        << setw(20)  << " "             << setw(26) << left << " "                           <<"\033[31m[FAIL]\033[0m \n";
	  errorMsg += "Light Sensor test failed! Please check connection and TSL2561 device.\n";
  }
  else {
	  cout << setw(26) << left << "Light Sensor : "        << setw(20)  << "Lux = "        << setw(26) << left << lux                           <<"\033[32m[PASS]\033[0m \n";
  }

  /*
   * Test DHT22/AM2302 temperature and moisture sensors
   * Internal sensor is on pin 10, external sensor on pin 1, as per WiringPi convention.
   */
   DHTOutput DHTRead;
   DHTRead = DHT22Call(10);
   switch (DHTRead.error) {
   case 0:
   	{
   		cout << setw(26) << left << "Internal Sensor : "   << setw(20) << "Temperature = " << setw(26) << left << DHTRead.Temperature              << "\n";
   		cout << setw(26) << left << " "                    << setw(20) << "Humidity = "    << setw(26) << left << DHTRead.Humidity               << "\033[32m[PASS]\033[0m \n";
   		break;
   	}
   default:
   	{
   		cout << setw(26)  << left << "Internal Sensor : "  << setw(20) << " "             << setw(26)  << left << " "                             << "\033[31m[FAIL]\033[0m \n";
   		errorMsg += "Internal temperature and humidity sensor (DHT22) failed! Please check connection to device.\n";
   	}
   }
   DHTRead = DHT22Call(1);
   switch (DHTRead.error) {
   case 0:
   	{
   		cout << setw(26) << left << "External Sensor : "   << setw(20) << "Temperature = " << setw(26) << left << DHTRead.Temperature              << "\n";
   		cout << setw(26) << left << " "                    << setw(20) << "Humidity = "    << setw(26) << left << DHTRead.Humidity               << "\033[32m[PASS]\033[0m \n";
   		break;
   	}
   default:
   	{
   		cout << setw(26)  << left << "External Sensor : "  << setw(20) << " "             << setw(26)  << left << " "                             << "\033[31m[FAIL]\033[0m \n";
   		errorMsg += "External temperature and humidity sensor (DHT22) failed! Please check connection to device.\n";
   	}
   }

   /*
    * Test the Arduino and it's devices
    */
    ArduinoOutput ArduinoRead;
    ArduinoRead = ArduinoCall(); //Do all the Arduino requests in one hit, to avoid lots of serial calls.
    //cout << "DEBUG -ERROR CODE:    " << ArduinoRead.error << "\n" ;
    switch (ArduinoRead.error) {
    case 0: //Serial coms wouldn't open
    	{
        	cout << setw(26) << left << "Scales : "      << setw(20)   << " "         << setw(26) << left << " "                             <<  "\033[31m[FAIL]\033[0m \n";
    		errorMsg += "Failure opening serial communication with the Arduino. Check cable between the Pi and PCB.\n";
    		break;
    	}
    case 2: //Skip case 1, as it's an unknown error
    	{
    		//	cout << "Arduino tested and connected devices read successfully.\n";
        	cout << setw(26) << left << "Scales : "        << setw(20) << "Weight = " << setw(26) << left << ArduinoRead.ScalesRead           << "\033[32m[PASS]\033[0m \n";
        	break;
    	}
    default: //And thus case 1, or something unexpectedly and stupidly wrong.
    	{
        	cout << setw(26) << left << "Scales : "      << setw(20)   << " "         << setw(26) << left << " "                             <<  "\033[31m[FAIL]\033[0m \n";
    		errorMsg += "An unknown error developed trying to get the Scale readings from the Arduino. Check it's running correct firmware.\n";
    	}
    }

    /*
     * Check the CPU temp
     */
    int cpuTemp;
    ifstream cpuFile("/sys/class/thermal/thermal_zone0/temp");
    cpuFile >> cpuTemp;
    cpuFile.close();
    cpuTemp /= 1000;
    if (cpuTemp > 10 && cpuTemp < 70) {
    	cout << setw(26) << left << "CPU : "                << setw(20) << "Temperature = "    << setw(26) << left << cpuTemp << "\033[32m[PASS]\033[0m \n";
    }
    else {
    	cout << setw(26) << left << "CPU : "                << setw(20) << "Temperature = "    << setw(26) << left << cpuTemp << "\033[31m[FAIL]\033[0m \n";
    	if (cpuTemp > 70 ) errorMsg += "The CPU is current running too hot! Check the location of the box and it's internal wiring for shorts. \n";
    	if (cpuTemp < 10 ) errorMsg += "The CPU is current running very cold! Check the location of the box. During extreme winter weather, the Birdbox may need powering off to avoid damage. \n";
    }


    /*
     * Check memory allocated for GPU, not really a Datalogging issue but put it in anyway.
     */
    /*
    int gpuMem;
    ifstream bootFile("");
    bootFile >> gpuMem;
    bootFile.close();
    if (gpuMem >= 128) {
    	cout << setw(26) << left << "GPU : "                << setw(20) << "Set Memory = "    << setw(26) << left << gpuMem << "\033[32m[PASS]\033[0m \n";
    }
    else {
    	cout << setw(26) << left << "CPU : "                << setw(20) << "Set Memory = "    << setw(26) << left << gpuMem << "\033[31m[FAIL]\033[0m \n";
    	errorMsg += "The GPU needs 128Mb memory allocated in /boot/config.txt in order for the video streamer to function. \n";
    }
    */

    /*
     * Finally, dump any error messages we got during the testing for the user.
     */
    if (errorMsg != "") cout << "\033[1;31m\n\n" << errorMsg << "\033[0m \n" ;


    /*
     * Check the Network
     */

}
