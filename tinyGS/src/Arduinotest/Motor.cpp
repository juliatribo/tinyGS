#include "Motor.h"

Motor:: Motor(): AzServo(),ElServo() {
//Pins for motor
  AzServo.attach(2);  // Assigned port for azimuth servo
  ElServo.attach(25);  // Assigned port for elevation servo
  ComAzimuth = 0;
  ComElevation = 0;
}

void Motor::SerComm(double ComAzimuth, double ComElevation) {
  this->ComAzimuth = ComAzimuth;
  this->ComElevation = ComElevation;
if (ComElevation >= 0) // positive elevation, the rotor is able to point the satellite
  {
    ComElevation = 130 - ComElevation;
    ComAzimuth = ComAzimuth/2;

    AzServo.write(ComAzimuth);  // move the servos
    ElServo.write(ComElevation);
  } 
  else // negative elevation, the rotor is NOT able to point the satellite
  {  

  }
}