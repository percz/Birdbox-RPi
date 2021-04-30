//============================================================================
// Name        : cronie.cpp
// Author      : Alan Childs
// Version     :
// Copyright   : Code is public domain
// Description :
//============================================================================

#include <iostream>
#include "cronie.h"
#include "plog/Log.h"  //Error logger from https://github.com/SergiusTheBest/plog#consoleappender


/*
 * TODO: Allow better handling of Seconds / Minutes / Hours / Days using another, externally set, variable
 */

int timeHandle::curTime(int debugTimeScale) { //Not an entirely needed function, I just wanted to clean the code a bit.
	time_t rawtime; struct tm * timeinfo; time (&rawtime); timeinfo = localtime (&rawtime);
	if (debugTimeScale == 1) { //Seconds
		return timeinfo->tm_sec;
	}
	else { //Minutes
		return timeinfo->tm_min;
	}
}

void timeHandle::setItvl(unsigned int itvlNew)
{
	itvlTime = itvlNew;
	nxtTime = 0;
	loopTime = false;
	while ( nxtTime <= curTime() ) {
		nxtTime += itvlTime;
		if (nxtTime >=  60) {
			nxtTime -= 60;
			break;
		}
	}
	//Log a good setting
}

bool timeHandle::needRead()
{
	  if ( curTime() >= nxtTime ) {
		  if ( loopTime && ( curTime() >= (60 - itvlTime) )  ) {
			  return false; //Because nxtTime is in the next minute/second, not this one
		  }
		  else {
			  return true;
		  }
	  }
	  else return false;
}

void timeHandle::addItvl() {
	//nxtTime = 0;
	while ( nxtTime <= curTime() ) {
		nxtTime += itvlTime;
		if (nxtTime >=  60) {
			nxtTime -= 60;
			loopTime = true;
			break;
		}
		else if (loopTime == true) {
			loopTime = false;
		}
	}
}
