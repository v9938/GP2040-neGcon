#include "peripheralmanager.h"

void PeripheralManager::initUSB() {
    // XIAO D4=GPIO6(D+)、D5=GPIO7(D-)。保存値に依存せず固定配線を使用する。
    blockUSB0.setConfig(0, 6, -1, 0);
}

PeripheralUSB* PeripheralManager::getUSB(uint8_t block) {
    return block == 0 ? &blockUSB0 : nullptr;
}

bool PeripheralManager::isUSBEnabled(uint8_t block) {
    return block == 0 && blockUSB0.configured;
}
