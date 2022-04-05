
#pragma once

void initializeBoard();
void waitPowerButtonReleased();
void shutdownBoard();
int getBatteryVoltage();

enum class BatteryLevel
{
    B100,
    B75,
    B50,
    B25,
    B0
};

BatteryLevel batteryLevel(int voltage);
