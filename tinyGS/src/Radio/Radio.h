/*
  Radio.h - Class to handle radio communications
  
  Copyright (C) 2020 -2021 @G4lile0, @gmag12 and @dev_4m1g0

  This program is free software: you can redistribute it and/or modify
  it under the terms of the GNU General Public License as published by
  the Free Software Foundation, either version 3 of the License, or
  (at your option) any later version.

  This program is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
  GNU General Public License for more details.

  You should have received a copy of the GNU General Public License
  along with this program.  If not, see <https://www.gnu.org/licenses/>.
*/

#ifndef RADIO_H
#define RADIO_H
#define RADIOLIB_EXCLUDE_HTTP

#ifndef RADIOLIB_GODMODE
#define RADIOLIB_GODMODE
#endif
#include <RadioLib.h>
#include "../ConfigManager/ConfigManager.h"
#include "../Status.h"
#include "../Mqtt/MQTT_Client.h"

extern Status status;

class Radio {
public:
  static Radio& getInstance()
  {
    static Radio instance; 
    return instance;
  }

  void init();
  int16_t begin();
  void enableInterrupt();
  void disableInterrupt();
  void startRx();
  uint8_t listen();
  bool isReady() { return status.radio_ready; }
  int16_t remote_freq(char* payload, size_t payload_len);
  int16_t remote_bw(char* payload, size_t payload_len);
  int16_t remote_sf(char* payload, size_t payload_len);
  int16_t remote_cr(char* payload, size_t payload_len);
  int16_t remote_crc(char* payload, size_t payload_len);
  int16_t remote_lsw(char* payload, size_t payload_len);
  int16_t remote_fldro(char* payload, size_t payload_len);
  int16_t remote_aldro(char* payload, size_t payload_len);
  int16_t remote_pl(char* payload, size_t payload_len);
  int16_t remote_begin_lora(char* payload, size_t payload_len);
  int16_t remote_begin_fsk(char* payload, size_t payload_len);
  int16_t remote_br(char* payload, size_t payload_len);
  int16_t remote_fd(char* payload, size_t payload_len);
  int16_t remote_fbw(char* payload, size_t payload_len);
  int16_t remote_fsw(char* payload, size_t payload_len);
  int16_t remote_fook(char* payload, size_t payload_len);
  int16_t remote_SPIsetRegValue(char* payload, size_t payload_len);
  void remote_SPIwriteRegister(char* payload, size_t payload_len);
  int16_t remote_SPIreadRegister(char* payload, size_t payload_len);
  int16_t sendTx(uint8_t* data, size_t length);
  int16_t sendTestPacket();
  void decode_conv(uint8_t* data, size_t length);
  void deinterleave(uint8_t* data, size_t length);
  void decode_rs(uint8_t* data, size_t length);
  int hexByteToDecimalInt(uint8_t hexByte);
  void isSendData(bool send);
  byte RESET_TC[3] = {0xC8, 0x9D, 0x01};
  byte NOMINAL_TC[4] = {0xC8, 0x9D, 0x02, 0x5A}; // 90%
  byte LOW_TC[4] = {0xC8, 0x9D, 0x03, 0x50}; // 80%
  byte CRITICAL_TC[4] = {0xC8, 0x9D, 0x04, 0x41}; // 65%
  byte EXIT_LOW_POWER_TC[3] = {0xC8, 0x9D, 0x05};
  byte EXIT_CONTINGENCY_TC[3] = {0xC8, 0x9D, 0x06};
  byte EXIT_SUNSAFE_TC[3] = {0xC8, 0x9D, 0x07};
  byte SET_TIME_TC[3] = {0xC8, 0x9D, 0x08};
  byte SET_CONSTANT_KP_TC[3] = {0xC8, 0x9D, 0x0A};
  byte TLE_TC_1[69] = {0xC8, 0x9D, 0x0B, 0x31, 0x20, 0x34, 0x31, 0x37, 0x33, 0x32, 0x55, 0x20, 0x31, 0x36, 0x30,   //aqui hay 15, incluye el pin y id
                    0x35, 0x31, 0x42, 0x20, 0x31, 0x36, 0x32, 0x36, 0x36, 0x2e, 0x33, 0x30, 0x31, 0x39, 0x39, 0x31, 0x34, 0x34,   //18 por linea
                    0x20, 0x2e, 0x30, 0x30, 0x30, 0x30, 0x32, 0x35, 0x33, 0x31, 0x20, 0x30, 0x30, 0x30, 0x30, 0x30, 0x2d, 0x30,
                    0x20, 0x31, 0x30, 0x37, 0x39, 0x35, 0x2d, 0x33, 0x20, 0x30, 0x20, 0x39, 0x39, 0x39, 0x38, 0x0a, 0x32, 0x20};   //Total primer paquete => 69 bytes (3 + 66)
  byte TLE_TC_2[69] = {0xC8, 0x9D, 0x0B, 0x34, 0x31, 0x37, 0x33, 0x32, 0x20, 0x39, 0x37, 0x2e, 0x33, 0x37, 0x33, 0x34, 0x20, 0x31, 0x38, 0x31, 0x2e,
                      0x34, 0x35, 0x32, 0x30, 0x20, 0x30, 0x30, 0x31, 0x35, 0x35, 0x33, 0x37, 0x20, 0x31, 0x33, 0x31, 0x2e, 0x32, //21 bytes (3 + 18)
                      0x32, 0x39, 0x34, 0x20, 0x33, 0x34, 0x39, 0x2e, 0x32, 0x37, 0x32, 0x32, 0x20, 0x31, 0x35, 0x2e, 0x32, 0x33, //18 bytes
                      0x37, 0x33, 0x31, 0x30, 0x37, 0x30, 0x20, 0x35, 0x37, 0x32, 0x38, 0xFF}; //11 bytes. TOTAL 69 (3 + 65 + ff)
  byte SET_GYRO_RES_TC[3] = {0xC8, 0x9D, 0x0C};
  byte SEND_DATA_TC[4] = {0xC8, 0x9D, 0x14, 0x00};
  byte SEND_TELEMETRY_TC[3] = {0xC8, 0x9D, 0x15};
  byte STOP_SENDING_DATA_TC[3] = {0xC8, 0x9D, 0x16};
  byte ACK_DATA_TC[23];
  byte SET_SF_CR_TC[3] = {0xC8, 0x9D, 0x18};
  byte SEND_CALIBRATION_TC[3] = {0xC8, 0x9D, 0x19};
  byte CHANGE_TIMEOUT_TC[3] = {0xC8, 0x9D, 0x1A};
  byte TAKE_PHOTO_TC[3] = {0xC8, 0x9D, 0x1E};
  byte TAKE_RF_TC[3] = {0xC8, 0x9D, 0x28};
  byte SEND_CONFIG_TC[3] = {0xC8, 0x9D, 0x32};

 //byte NACK_DATA[23] = {0xC8, 0x9D, 0x33};

  
private:
  Radio();
  PhysicalLayer* lora;
  void readState(int state);
  static void setFlag();
  SPIClass spi;
  const char* TEST_STRING = "TinyGS-test "; // make sure this always start with "TinyGS-test"!!!

  double _atof(const char* buff, size_t length);
  int _atoi(const char* buff, size_t length);

};


#endif