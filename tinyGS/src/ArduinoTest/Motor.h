/*  AZ/EL Rotator controller for Arduino with servos
 */
#ifndef MOTOR_H
#define MOTOR_H
#include "../ConfigManager/ConfigManager.h"
#include "../../lib/ESP32_Servo/ESP32_Servo.h"
//#include <../lib/ESP32Servo/ESP32_Servo.h>
#include <Wire.h>

class Motor{
 public:
  static Motor& getInstance()
  {
    static Motor instance; 
    return instance;
  }
  Motor();
  void SerComm(double ComAzimuth, double ComElevation);
  Servo AzServo;  // create servo object to control the azimuth servo
  Servo ElServo;  // create servo object to control the elevation servo
  double ComAzimuth;
  double ComElevation;
};

#endif