//============================================================================
// Name        : BirdboxTestSubs.cpp
// Created	   : In the past
// Author      : Alan Childs
// Version     : 0.3
// Copyright   : None really
// Description : Handle Sensor Readings for Birdbox
//============================================================================

#include "sensors.h"

#include <cstdio>
#include <cstring>
#include <iostream>
#include <ctime> //Control things with a timer
#include <sstream> //Gives:   stringstream
#include <inttypes.h>
#include <wiringPi.h> //WiringPi
#include <wiringPiI2C.h>
#include <wiringSerial.h>
#include <curl/curl.h>
#include <rapidjson/reader.h>
#include <rapidjson/document.h>

using namespace std;


int fdLight = 0, fdSerial = 0 ;
const char device[] = "/dev/ttyAMA0" ;    //Hardwired
const unsigned long baud = 2000000 ;       //XXX: Slim chance this needs reducing if we ever have Birdbox in a electrically noisy environment.



int LightCall (void)
{

	// ALL COMMAND TSL2561
	// Default I2C RPI address in (0x39) = FLOAT ADDR (Slave) Other [(0x49) = VCC ADDR / (0x29) = GROUND ADDR]
	#define TSL2561_ADDR_LOW                   (0x29)
	#define TSL2561_ADDR_FLOAT                 (0x39)
	#define TSL2561_ADDR_HIGH                   (0x49)
	#define TSL2561_CONTROL_POWERON             (0x03)
	#define TSL2561_CONTROL_POWEROFF          (0x00)
	#define TSL2561_GAIN_0X                        (0x00)   //No gain
	#define TSL2561_GAIN_AUTO                (0x01)
	#define TSL2561_GAIN_1X                 (0x02)
	#define TSL2561_GAIN_16X                  (0x12) // (0x10)
	#define TSL2561_INTEGRATIONTIME_13MS          (0x00)   // 13.7ms
	#define TSL2561_INTEGRATIONTIME_101MS          (0x01) // 101ms
	#define TSL2561_INTEGRATIONTIME_402MS         (0x02) // 402ms
	#define TSL2561_READBIT                   (0x01)
	#define TSL2561_COMMAND_BIT                (0x80)   //Must be 1
	#define TSL2561_CLEAR_BIT                (0x40)   //Clears any pending interrupt (write 1 to clear)
	#define TSL2561_WORD_BIT                   (0x20)   // 1 = read/write word (rather than byte)
	#define TSL2561_BLOCK_BIT                  (0x10)   // 1 = using block read/write
	#define TSL2561_REGISTER_CONTROL           (0x00)
	#define TSL2561_REGISTER_TIMING            (0x81)
	#define TSL2561_REGISTER_THRESHHOLDL_LOW      (0x02)
	#define TSL2561_REGISTER_THRESHHOLDL_HIGH     (0x03)
	#define TSL2561_REGISTER_THRESHHOLDH_LOW      (0x04)
	#define TSL2561_REGISTER_THRESHHOLDH_HIGH     (0x05)
	#define TSL2561_REGISTER_INTERRUPT            (0x06)
	#define TSL2561_REGISTER_CRC                  (0x08)
	#define TSL2561_REGISTER_ID                   (0x0A)
	#define TSL2561_REGISTER_CHAN0_LOW            (0x8C)
	#define TSL2561_REGISTER_CHAN0_HIGH           (0x8D)
	#define TSL2561_REGISTER_CHAN1_LOW            (0x8E)
	#define TSL2561_REGISTER_CHAN1_HIGH           (0x8F)
	#define TSL2561_ADDR_LOW                   (0x29)
	#define TSL2561_ADDR_FLOAT                 (0x39)
	#define TSL2561_ADDR_HIGH                   (0x49)
	//Delay getLux function
	#define LUXDELAY 500

	   //int fd = 0;
	   if (fdLight == 0) fdLight = wiringPiI2CSetup(TSL2561_ADDR_HIGH); //I set mine. I don't like the idea of floating. Just personal pref.
	   wiringPiI2CWriteReg8(fdLight, TSL2561_COMMAND_BIT, TSL2561_CONTROL_POWERON); //enable the device
	   wiringPiI2CWriteReg8(fdLight, TSL2561_REGISTER_TIMING, TSL2561_GAIN_AUTO); //auto gain and timing = 101 mSec
	   //Wait for the conversion to complete
	   delay(LUXDELAY);
	   //Reads visible + IR diode from the I2C device auto
	   uint16_t visible_and_ir = wiringPiI2CReadReg16(fdLight, TSL2561_REGISTER_CHAN0_LOW);
	   wiringPiI2CWriteReg8(fdLight, TSL2561_COMMAND_BIT, TSL2561_CONTROL_POWEROFF); //disable the device
   return visible_and_ir*2;

}

DHTOutput DHT22Call (int DHTPIN) {

	DHTOutput DHT22return = {0,0,1}; //Default to blank return (with unknown error)

	for (int readAttempts = 0 ; readAttempts < 5 ; readAttempts++ ) { //loop until we get a success read
	  uint8_t laststate = HIGH;
	  uint8_t counter = 0;
	  uint8_t j = 0, i;
	  static int dht22_dat[5] = {0,0,0,0,0};
	  #define MAXTIMINGS 85

	  dht22_dat[0] = dht22_dat[1] = dht22_dat[2] = dht22_dat[3] = dht22_dat[4] = 0;

	  // pull pin down for 18 milliseconds
	  pinMode(DHTPIN, OUTPUT);
	  digitalWrite(DHTPIN, HIGH);
	  delay(10);
	  digitalWrite(DHTPIN, LOW);
	  delay(18);
	  // then pull it up for 40 microseconds
	  digitalWrite(DHTPIN, HIGH);
	  delayMicroseconds(40);
	  // prepare to read the pin
	  pinMode(DHTPIN, INPUT);

	  // detect change and read data
	  for ( i=0; i< MAXTIMINGS; i++) {
	    counter = 0;
	    while ((digitalRead(DHTPIN)) == laststate) {
	      counter++;
	      delayMicroseconds(2);
	      if (counter == 255) {
	        break;
	      }
	    }
	    laststate = (digitalRead(DHTPIN));

	    if (counter == 255) break;

	    // ignore first 3 transitions
	    if ((i >= 4) && (i%2 == 0)) {
	      // shove each bit into the storage bytes
	      dht22_dat[j/8] <<= 1;
	      if (counter > 16)
	        dht22_dat[j/8] |= 1;
	      j++;
	    }
	  }

	  // check we read 40 bits (8bit x 5 ) + verify checksum in the last byte
	  // print it out if data is good
	  if ((j >= 40) &&
	      (dht22_dat[4] == ((dht22_dat[0] + dht22_dat[1] + dht22_dat[2] + dht22_dat[3]) & 0xFF)) ) {
	        float t, h;
	        h = (float)dht22_dat[0] * 256 + (float)dht22_dat[1];
	        h /= 10;
	        t = (float)(dht22_dat[2] & 0x7F)* 256 + (float)dht22_dat[3];
	        t /= 10.0;
	        if ((dht22_dat[2] & 0x80) != 0)  t *= -1;

	        //DHTOutput DHT22return = {0,0,0};
	        DHT22return.Humidity  = h ;
	        DHT22return.Temperature = t ;
	        DHT22return.error = 0 ;

	      	return DHT22return;
	      	break; //Break once we get a successfull return.
	  }
	  delay(3000); //Guide says to keep probes at least two seconds apart. Go for three and be sure.
}
//printf("Probe Error \n");

//DHT22return.error = 1 ; //Unknown error
return DHT22return;
}


int serialConnected = 0;
void ArduinoTare () {
	if (!serialConnected && (fdSerial = serialOpen (device, baud)) < 0)
	{
		serialConnected = 0; serialClose (fdSerial); fdSerial = 0; //reset connection
		throw 1;
	}
	else if (!serialConnected)
	{
		serialConnected = 1;
	}
	serialPutchar (fdSerial, 'T') ;
}

ArduinoOutput ArduinoCall () {

	ArduinoOutput ArduinoReturn = { 0, 0 , 1 } ; //Default to blank return, with error 1 (meaning unknown error)
	//ArduinoReturn.error = 1; //Unknown error defaults if all else fails

	if (!serialConnected && (fdSerial = serialOpen (device, baud)) < 0)
	{
		ArduinoReturn.error = 0; serialConnected = 0; serialClose (fdSerial); fdSerial = 0; //reset connection
		return ArduinoReturn;
	}
	else if (!serialConnected)
	{
		serialConnected = 1;
	}

	serialFlush (fdSerial) ;
	char incomingString[10]; int strngIndex = 0, varNum = 0; memset(incomingString,0,10);
	float parsedInValue[4] = {-1,-1,-1,-2}; //Remember to set this to the number of arrays we might get! -2 Allows for scales drift.

	serialPutchar (fdSerial, 'S') ; //Send 'S' as a send/ping code on the Arduino.
	delay(50); //Use some wiringPi code so we can wait for serial buffer to fill...

	for ( unsigned int lastSerialReadTime = millis() ; (lastSerialReadTime + 1000 ) > millis() ; ) //Loop until we get to the end of the line, then break. Timeout if nothing after 1 second.
	{
		if(serialDataAvail (fdSerial)){    //Incoming data will be in bytes at a time.. "1" ... "2" ... "3" but the overall dump of info will be comma separated, such "12,12,12"

			lastSerialReadTime = millis(); //Reset timeout timer
			char newChar = serialGetchar (fdSerial);
			//*** Catch errors from serialGetchar and 'continue' the for loop.
			if (newChar != '\n' && newChar != ',') //Make sure we haven't reached the end of the information dump
			{
				//cout << "Debug: Good char?... Looks like a >>" << newChar << "<< but is also a int(" << int(newChar) << ") " << endl;
				if( !isdigit(newChar) &&  (newChar != '-') && (newChar != '.') && (newChar != '\0') && (newChar != '\r') ) { // The input is corrupted or invalid, so don't bother taking in anymore.
					ArduinoReturn.error = 0; serialConnected = 0; serialClose (fdSerial); fdSerial = 0; //reset connection
					return ArduinoReturn;
				}

				incomingString[strngIndex] = newChar;
				strngIndex++;

			}
			else if (newChar == ',' ) //Then move on to the next variable
			{
				stringstream(incomingString) >> parsedInValue[varNum]; //Change the string into an int
				varNum++; strngIndex = 0; memset(incomingString,0, 10);
			}
			else if (newChar == '\n' ) //Pass the array into the variables, then reset everything, the dump is complete
			{
				stringstream(incomingString) >> parsedInValue[varNum];
				//if (parsedInValue[0] == 0) { cout << "Corrupted input, retrying. \n"  ; varNum = 0; continue; } //Sometimes the data gets messed up on first loop.
				//It's at this point we can do something with the pasedInValue array, if we like...
				serialPutchar (fdSerial, 'R') ; //Send 'R' as a reset/pong code on the Arduino.
				break; //Because we got everything
			}
		}
	}

	//No need to check [0] (Trigger,) as it's bool
    //if  ( parsedInValue[1] > -1 && parsedInValue[1] < 100  && parsedInValue[2] > -1 && parsedInValue[2] < 100 ) ArduinoReturn.error += 1; //Check the count in and count out values are acceptable, 100 is arbitrary
	if  ( parsedInValue[1] > -100 && parsedInValue[1] < 1000 ) ArduinoReturn.error += 1; //Assuming we are using 1Kg rated scales we should never return greater than 1000g; although the bird should be rather less!
    // ERROR BINARY VALUES:   0 = Coms Error, 1 = Unknown Error, 2 = Good Scales

	ArduinoReturn.Trigger = parsedInValue[0];
	ArduinoReturn.ScalesRead = parsedInValue[1];
//	ArduinoReturn.DoorRead.CountedIn = parsedInValue[??];
//	ArduinoReturn.DoorRead.CountedOut = parsedInValue[??];


	//close fdSerial
	return ArduinoReturn;
}


/*
 * I've written this for Birdbox, but tried to make it portable.
 */
/*
int netstat::test() {
	  if (getifaddrs(&ifaddr) == -1) return 0; //No interface
	  now = 0; //reset
	  for (ifa = ifaddr; ifa != NULL; ifa = ifa->ifa_next) {

	    if (ifa->ifa_addr == NULL ||
	    	(ifa->ifa_flags & IFF_LOOPBACK)	||
	        ifa->ifa_addr->sa_family != AF_INET) continue;

		  strncpy(pwrq.ifr_name, ifa->ifa_name, 16);

		  sock = socket(AF_INET, SOCK_STREAM, 0);
		  if (ioctl(sock, SIOCGIWNAME, &pwrq) != -1) {
			  if (!(now == 1) && (now < 3) ) now += 1; //Wifi
		  }  else {
			  if (now < 2) now += 2; //Cable
		  }
		  close(sock);
	  }
	  freeifaddrs(ifaddr);
		if (now > 0) {
			if (now == 1) {
				if ( last != 1 ) {
					last = 1; return 1; //Wireless up
				}
			} else if ( last != 2 ) {
				last = 2; return 2;  //Wired up
			}
		} else if ( last != 0 ) {
			last = 0; return 0; //All networks down
		}
	  delete ifaddr;
	  return -1; //No change
}
*/

//TODO: ...others? // camera // power supply

