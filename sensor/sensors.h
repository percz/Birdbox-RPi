/*
 * BirdboxTestSubs.h
 *
 *  Created on: 6 Oct 2015
 *      Author: Alan
 */

//Class Defs

#ifndef BIRDBOX_SENSOR_CLASSES_H_
#define BIRDBOX_SENSOR_CLASSES_H_

struct DHTOutput {
	float Humidity;
	float Temperature;
	int error;
};

/*
struct DoorOutput {
	int CountedIn;
	int CountedOut;
};
*/

void ArduinoTare();
struct ArduinoOutput {
	bool Trigger;
	float ScalesRead;
	int error;
};
ArduinoOutput ArduinoCall();
//TODO: Class/OOP the Arduino

//The rest is just for the network status!
/*
#include <sys/ioctl.h>
#include <sys/socket.h>
#include <linux/wireless.h>
#include <unistd.h> // 'close'
#include <ifaddrs.h> //for netTest
#include <arpa/inet.h> //for localhost check

class netstat {
	public:
		int test();
	//	void on();
	//	void off();
	private:
		int sock = -1;
		unsigned short int last, now;
		bool display = 1;
		struct ifaddrs *ifaddr, *ifa;
		struct iwreq pwrq;
};
*/

#endif /* BIRDBOX_SENSOR_CLASSES_H_ */

//Functions

#ifndef BIRDBOX_SENSOR_SUBS_H_
#define BIRDBOX_SENSOR_SUBS_H_

int LightCall();
DHTOutput DHT22Call(int DHTPIN);

#endif /* BIRDBOX_SENSOR_SUBS_H_ */
