#ifndef _PERIPHERALMANAGER_H_
#define _PERIPHERALMANAGER_H_

#include "peripheral_usb.h"

#define PMGR PeripheralManager::getInstance()

class PeripheralManager {
public:
	PeripheralManager(PeripheralManager const&) = delete;
	void operator=(PeripheralManager const&)  = delete;
	static PeripheralManager& getInstance() // Thread-safe storage ensures cross-thread talk
	{
		static PeripheralManager instance;
		return instance;
	}

    PeripheralUSB* getUSB(uint8_t block);

    void initUSB();
    bool isUSBEnabled(uint8_t block);
private:
    PeripheralManager(){}

    PeripheralUSB blockUSB0;
};

#endif
