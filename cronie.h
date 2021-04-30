/*
 * cronie.h
 *
 *  Created on: 8 Oct 2015
 *      Author: Alan
 */

#ifndef CONTROLLERS_H_
#define CONTROLLERS_H_

class timeHandle {
	private:
		int itvlTime; //Have to use signed as the tm_mins, oddly, are.
		int nxtTime;
		bool loopTime; //Handle nxtTime rolling into the next minute
		int curTime(int debugTimeScale = 0); //Get the current time scale, 1 = seconds (for debugging code), anything else minutes (production)
	public:
		bool needRead();
		void addItvl();
		void setItvl(unsigned int itvlNew);
};

#endif /* CONTROLLERS_H_ */
