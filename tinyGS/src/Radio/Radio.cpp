/*
  Radio.cpp - Class to handle radio communications
  
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

#include "Radio.h"
#include "correct/rs/ecc.h"
#include "correct/reed-solomon.h"
#include "correct/convolutional.h"
#include "ArduinoJson.h"
#if ARDUINOJSON_USE_LONG_LONG == 0 && !PLATFORMIO
#error "Using Arduino IDE is not recommended, please follow this guide https://github.com/G4lile0/tinyGS/wiki/Arduino-IDE or /ArduinoJson/src/ArduinoJson/Configuration.hpp and amend to #define ARDUINOJSON_USE_LONG_LONG 1 around line 68"
#endif
#include <base64.h>
#include "../Logger/Logger.h"
#include <chrono>
#include <sstream>

bool send_data = false;
bool send_config = false;
bool send_telemetry = false;
bool received = false;
bool eInterrupt = true;
bool noisyInterrupt = false;
int last_data_packet;

Radio::Radio()
    : spi(VSPI)
{
}

void Radio::init()
{
  Log::console(PSTR("[SX12xx] Initializing ... "));
  board_type board;

  if (strlen(ConfigManager::getInstance().getBoardTemplate()) > 0)
  {
    size_t size = 512;
    DynamicJsonDocument doc(size);
    DeserializationError error = deserializeJson(doc, ConfigManager::getInstance().getBoardTemplate());
    
    send_data = false;
    send_config = false;
    send_telemetry = false;
    last_data_packet = 0;
    std::chrono::high_resolution_clock::now();

    if (error.code() != DeserializationError::Ok || !doc.containsKey("radio"))
    {
      Log::console(PSTR("Error: Your Board template is not valid. Unable to init radio."));
      return;
    }

    board.OLED__address = doc["aADDR"];
    board.OLED__SDA = doc["oSDA"];
    board.OLED__SCL = doc["oSCL"];
    board.OLED__RST = doc["oRST"];
    board.PROG__BUTTON = doc["pBut"];
    board.BOARD_LED = doc["led"];
    board.L_SX127X = doc["radio"];
    board.L_NSS = doc["lNSS"];
    board.L_DI00 = doc["lDIO0"];
    board.L_DI01 = doc["lDIO1"];
    board.L_BUSSY = doc["lBUSSY"];
    board.L_RST = doc["lRST"];
    board.L_MISO = doc["lMISO"];
    board.L_MOSI = doc["lMOSI"];
    board.L_SCK = doc["lSCK"];
    board.L_TCXO_V = doc["lTCXOV"];
  }
  else
  {
    board = ConfigManager::getInstance().getBoardConfig();
  }

  spi.begin(board.L_SCK, board.L_MISO, board.L_MOSI, board.L_NSS);

  if (board.L_SX127X)
  {
    lora = new SX1278(new Module(board.L_NSS, board.L_DI00, board.L_DI01, spi, SPISettings(2000000, MSBFIRST, SPI_MODE0)));
  }
  else
  {
    lora = new SX1268(new Module(board.L_NSS, board.L_DI01, board.L_RST, board.L_BUSSY, spi, SPISettings(2000000, MSBFIRST, SPI_MODE0)));
  }

  begin();
}

int16_t Radio::begin()
{
  status.radio_ready = false;
  board_type board = ConfigManager::getInstance().getBoardConfig();
  ModemInfo &m = status.modeminfo;
  int16_t state = 0;

  if (m.modem_mode == "LoRa")
  {
    if (board.L_SX127X)
    {
      state = ((SX1278 *)lora)->begin(m.frequency + status.modeminfo.freqOffset, m.bw, m.sf, m.cr, m.sw, m.power, m.preambleLength, m.gain);
      if (m.fldro == 2)
        ((SX1278 *)lora)->autoLDRO();
      else
        ((SX1278 *)lora)->forceLDRO(m.fldro);

      ((SX1278 *)lora)->setCRC(m.crc);
    }
    else
    {
      state = ((SX1268 *)lora)->begin(m.frequency + status.modeminfo.freqOffset, m.bw, m.sf, m.cr, m.sw, m.power, m.preambleLength, board.L_TCXO_V);
      if (m.fldro == 2)
        ((SX1268 *)lora)->autoLDRO();
      else
        ((SX1268 *)lora)->forceLDRO(m.fldro);

      ((SX1268 *)lora)->setCRC(m.crc);
    }
  }
  else
  {
    if (ConfigManager::getInstance().getBoardConfig().L_SX127X)
    {
      state = ((SX1278 *)lora)->beginFSK(m.frequency + status.modeminfo.freqOffset, m.bitrate, m.freqDev, m.bw, m.power, m.preambleLength, (m.OOK != 255));
      ((SX1278 *)lora)->setDataShaping(m.OOK);
      ((SX1278 *)lora)->startReceive();
      ((SX1278 *)lora)->setDio0Action(setFlag);
      ((SX1278 *)lora)->setSyncWord(m.fsw, m.swSize);
    }
    else
    {
      state = ((SX1268 *)lora)->beginFSK(m.frequency + status.modeminfo.freqOffset, m.bitrate, m.freqDev, m.bw, m.power, m.preambleLength, ConfigManager::getInstance().getBoardConfig().L_TCXO_V);
      ((SX1268 *)lora)->setDataShaping(m.OOK);
      ((SX1268 *)lora)->startReceive();
      ((SX1268 *)lora)->setDio1Action(setFlag);
      state = ((SX1268 *)lora)->setSyncWord(m.fsw, m.swSize);
    }
  }

  // registers
  /*JsonArray regs = doc.as<JsonArray>();
  for (int i=0; i<regs.size(); i++) {
    JsonObject reg = regs[i];
    uint16_t regValue = reg["ref"];
    uint32_t regMask = reg["mask"];

    if (ConfigManager::getInstance().getBoardConfig().L_SX127X)
      state = ((SX1278*)lora)->_mod->SPIsetRegValue((regValue>>8)&0x0F, regValue&0x0F, (regMask>>16)&0x0F, (regMask>>8)&0x0F, regMask&0x0F);
   // else
   //   state = ((SX1268*)lora)->_mod->SPIsetRegValue((regValue>>8)&0x0F, regValue&0x0F, (regMask>>16)&0x0F, (regMask>>8)&0x0F, regMask&0x0F);
  }*/

  if (state == ERR_NONE)
  {
    //Log::console(PSTR("success!"));
  }
  else
  {
    Log::console(PSTR("failed, code %d"), state);
    return state;
  }

  // set the function that will be called
  // when new packet is received
  // attach the ISR to radio interrupt
  if (board.L_SX127X)
  {
    ((SX1278 *)lora)->setDio0Action(setFlag);
  }
  else
  {
    ((SX1268 *)lora)->setDio1Action(setFlag);
  }

  // start listening for LoRa packets
  Log::console(PSTR("[SX12x8] Starting to listen to %s"), m.satellite);
  if (board.L_SX127X)
    state = ((SX1278 *)lora)->startReceive();
  else
    state = ((SX1268 *)lora)->startReceive();

  if (state == ERR_NONE)
  {
    //Log::console(PSTR("success!"));
  }
  else
  {
    Log::console(PSTR("failed, code %d\nGo to the config panel (%s) and check if the board selected matches your hardware."), state, WiFi.localIP().toString());
    return state;
  }

  status.radio_ready = true;
  return state;
}

void Radio::setFlag()
{
  if (received || !eInterrupt)
    noisyInterrupt = true;

  if (!eInterrupt)
    return;

  received = true;
}

void Radio::enableInterrupt()
{
  eInterrupt = true;
}

void Radio::disableInterrupt()
{
  eInterrupt = false;
}

void Radio::startRx()
{
  // put module back to listen mode
  if (ConfigManager::getInstance().getBoardConfig().L_SX127X)
    ((SX1278 *)lora)->startReceive();
  else
    ((SX1268 *)lora)->startReceive();

  // we're ready to receive more packets,
  // enable interrupt service routine
  enableInterrupt();
}

int16_t Radio::sendTx(uint8_t *data, size_t length)
{
  if (!ConfigManager::getInstance().getAllowTx())
  {
    Log::error(PSTR("TX disabled by config"));
    return -1;
  }
  disableInterrupt();

  // send data
  int16_t state = 0;
  if (ConfigManager::getInstance().getBoardConfig().L_SX127X)
  {
    SX1278 *l = (SX1278 *)lora;
    state = l->transmit(data, length);
    l->setDio0Action(setFlag);
    l->startReceive();
  }
  else
  {
    SX1268 *l = (SX1268 *)lora;
    state = l->transmit(data, length);
    l->setDio1Action(setFlag);
    l->startReceive();
  }

  enableInterrupt();
  return state;
}

int16_t Radio::sendTestPacket()
{
  return sendTx((uint8_t *)TEST_STRING, strlen(TEST_STRING));
}

uint8_t Radio::listen()
{
  // check if the flag is set (received interruption)
  if (!received)
    return 1;

  // disable the interrupt service routine while
  // processing the data
  disableInterrupt();

  // reset flag
  received = false;

  size_t respLen = 0;
  uint8_t *respFrame = 0;
  int16_t state = 0;

  PacketInfo newPacketInfo;
  status.lastPacketInfo.crc_error = 0;
  // read received data
  if (ConfigManager::getInstance().getBoardConfig().L_SX127X)
  {
    SX1278 *l = (SX1278 *)lora;
    respLen = l->getPacketLength();
    respFrame = new uint8_t[respLen];
    state = l->readData(respFrame, respLen);
    newPacketInfo.rssi = l->getRSSI();
    newPacketInfo.snr = l->getSNR();
    newPacketInfo.frequencyerror = l->getFrequencyError();
  }
  else
  {
    SX1268 *l = (SX1268 *)lora;
    respLen = l->getPacketLength();
    respFrame = new uint8_t[respLen];
    state = l->readData(respFrame, respLen);
    newPacketInfo.rssi = l->getRSSI();
    newPacketInfo.snr = l->getSNR();
  }

  // check if the packet info is exactly the same as the last one
  if (newPacketInfo.rssi == status.lastPacketInfo.rssi &&
      newPacketInfo.snr == status.lastPacketInfo.snr &&
      newPacketInfo.frequencyerror == status.lastPacketInfo.frequencyerror)
  {
    Log::console(PSTR("Interrupt triggered but no new data available. Check wiring and electrical interferences."));
    delete[] respFrame;
    startRx();
    return 4;
  }

  status.lastPacketInfo.rssi = newPacketInfo.rssi;
  status.lastPacketInfo.snr = newPacketInfo.snr;
  status.lastPacketInfo.frequencyerror = newPacketInfo.frequencyerror;

  // print RSSI (Received Signal Strength Indicator)
  Log::console(PSTR("[SX12x8] RSSI:\t\t%f dBm\n[SX12x8] SNR:\t\t%f dB\n[SX12x8] Frequency error:\t%f Hz"), status.lastPacketInfo.rssi, status.lastPacketInfo.snr, status.lastPacketInfo.frequencyerror);
  
  
  // initialize static variable with current time
  static auto lastTime = std::chrono::high_resolution_clock::now();
  auto currentTime = std::chrono::high_resolution_clock::now();
  //compute time since last packet received
  auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(currentTime - lastTime);
  // update lastTime with current time for the next call
  lastTime = currentTime;
  

  if (state == ERR_NONE && respLen > 0)
  { 
    // read optional data
    Log::console(PSTR("Packet received (%u bytes):"), respLen);
    uint16_t buffSize = respLen * 3 + 1;
    if (buffSize > 255)
      buffSize = 255;
    char *rx_str = new char[buffSize];
    for (int i = 0; i < respLen; i++)
    {
      sprintf(rx_str + i * 3 % (buffSize - 1), "%02x ", respFrame[i]);
      if (i * 3 % buffSize == buffSize - 3 || i == respLen - 1)
        Log::console(PSTR("%s"), rx_str); // print before the buffer is going to loop back
    }

    /* 
    //read convolution decoded
    int index = ceil(respLen/RATE_CON);
    Log::console(PSTR("Packet convolution decoded (%u bytes):"), index);
    char *rx_str_deconv = new char[buffSize];
    uint8_t data_deconv[respLen];
    memcpy(data_deconv,respFrame,respLen);
    decode_conv(data_deconv,respLen);
    for (int i = 0; i < index; i++)
    {
      sprintf(rx_str_deconv + i * 3 % (buffSize - 1), "%02x ", data_deconv[i]);
      if (i * 3 % buffSize == buffSize - 3 || i == index - 1)
        Log::console(PSTR("%s"), rx_str_deconv); // print before the buffer is going to loop back
    }
    */


  
    //send_data ack  
    
    if (send_data){
      if (last_data_packet == 0){
        //add the correct received packets to the ack_data vector
        ACK_DATA_TC[0] = 0xC8;
        ACK_DATA_TC[1] = 0x9D;
        ACK_DATA_TC[2] = 0x18;
      }
      int received_index = respFrame[3];
      ACK_DATA_TC[received_index]=0x01;
      //ACK_DATA_TC[last_data_packet+3] = respFrame[3];
      if (((respFrame[3] == 0x13)||( duration > std::chrono::minutes(1)))&& last_data_packet>0){ 
        time_t currentUnixTime = time(NULL);
        uint32_t unixTime32 = (uint32_t)currentUnixTime;
        ACK_DATA_TC[23] = (unixTime32 >> 24) & 0xFF;
        ACK_DATA_TC[24] = (unixTime32 >> 16) & 0xFF;
        ACK_DATA_TC[25] = (unixTime32 >> 8) & 0xFF;
        ACK_DATA_TC[26] = unixTime32 & 0xFF;
        Log::console(PSTR("Received packets (%u bytes):"), sizeof(ACK_DATA_TC));
        char *ack_rx_data = new char[buffSize];
        uint8_t ack_data[sizeof(ACK_DATA_TC)];
        memcpy(ack_data,ACK_DATA_TC,sizeof(ACK_DATA_TC));
        for (int i = 0; i < sizeof(ACK_DATA_TC); i++)
        {
          sprintf(ack_rx_data + i * 3 % (buffSize - 1), "%02x ", ack_data[i]);
          if (i * 3 % buffSize == buffSize - 3 || i == sizeof(ACK_DATA_TC)-1)
            Log::console(PSTR("%s"), ack_rx_data); // print before the buffer is going to loop back
        }
        send_data = false;
        last_data_packet = 0;

        ConfigManager& configManager = ConfigManager::getInstance();
        uint8_t telecomand_encoded[256];
        int size = configManager.encode(ACK_DATA_TC,  sizeof(ACK_DATA_TC), telecomand_encoded);
        sendTx(telecomand_encoded, size);
        delay(500);
        sendTx(telecomand_encoded, size);
        Log::console(PSTR("Sending ACK DATA!"));

      }
      last_data_packet ++;
    }

    else{

    //read convolution decoded and deinterleaved
    uint8_t data_deinter[respLen];
    memcpy(data_deinter,respFrame,respLen);
    char *rx_str_deinter = new char[buffSize];
    deinterleave(data_deinter,respLen);
    //delete padding of interleaved
    int index = respLen;
    bool end = false;
    while(!end){
      if(data_deinter[index-1]!=0xFF)
        index--;
      else{
        index --;
        end = true;
      }
    }
    Log::console(PSTR("Packet deinterleaved (%u bytes):"), index);
    for (int i = 0; i < index; i++)
    {
      sprintf(rx_str_deinter + i * 3 % (buffSize - 1), "%02x ", data_deinter[i]);
      if (i * 3 % buffSize == buffSize - 3 || i == index - 1)
        Log::console(PSTR("%s"), rx_str_deinter); // print before the buffer is going to loop back
    }
    
    //read convolution decoded, deinterleaved and rs decoded
    Log::console(PSTR("Packet deinterleaved and rs decoded (%u bytes):"), index);
    char *rx_str_deinter_ders = new char[buffSize];
    uint8_t data_deinter_ders[index];
    memcpy(data_deinter_ders,data_deinter,index);
    decode_rs(data_deinter_ders,index);
    for (int i = 0; i < index; i++)
    {
      sprintf(rx_str_deinter_ders + i * 3 % (buffSize - 1), "%02x ", data_deinter_ders[i]);
      if (i * 3 % buffSize == buffSize - 3 || i == index - 1)
        Log::console(PSTR("%s"), rx_str_deinter_ders); // print before the buffer is going to loop back
    }
    
    //read data packet  
    Log::console(PSTR("Packet data (%u bytes):"), index - NPAR);
    char *rx_data = new char[buffSize];
    uint8_t packet_data[index - NPAR];
    memcpy(packet_data,data_deinter_ders,index - NPAR);
    for (int i = 0; i < index - NPAR; i++)
    {
      sprintf(rx_data + i * 3 % (buffSize - 1), "%02x ", packet_data[i]);
      if (i * 3 % buffSize == buffSize - 3 || i == index - NPAR - 1)
        Log::console(PSTR("%s"), rx_data); // print before the buffer is going to loop back
    }
    
    memcpy(respFrame,packet_data,index - NPAR);
    respLen = index - NPAR;
    }

    if(send_config){
      send_config = false;
    }
    if(send_telemetry){
      send_telemetry = false;
    }
  
    
    // if Filter enabled filter the received packet
    if (status.modeminfo.filter[0] != 0)
    {
      bool filter_flag = false;
      uint8_t filter_size = status.modeminfo.filter[0];
      uint8_t filter_ini = status.modeminfo.filter[1];

      for (uint8_t filter_pos = 0; filter_pos < filter_size; filter_pos++)
      {
        if (status.modeminfo.filter[2 + filter_pos] != respFrame[filter_ini + filter_pos])
          filter_flag = true;
      }

      // if the msg start with tiny (test packet) remove filter
      if (respFrame[0] == 0x54 && respFrame[1] == 0x69 && respFrame[2] == 0x6e && respFrame[3] == 0x79)
        filter_flag = false;

      if (filter_flag)
      {
        Log::console(PSTR("Filter enabled, doesn't look like the expected satellite packet"));
        delete[] respFrame;
        startRx();
        return 5;
      }
    }

    status.lastPacketInfo.crc_error = false;
    //String encoded = base64::encode(packet_data, index - NPAR);
    String encoded = base64::encode(respFrame, respLen);
    MQTT_Client::getInstance().sendRx(encoded, noisyInterrupt);
  }
  else if (state == ERR_CRC_MISMATCH)
  {
    // if filter is active, filter the CRC errors
    if (status.modeminfo.filter[0] == 0)
    {
      // packet was received, but is malformed
      status.lastPacketInfo.crc_error = true;
      String error_encoded = base64::encode("Error_CRC");
      MQTT_Client::getInstance().sendRx(error_encoded, noisyInterrupt);
    }
    else
    {
      Log::console(PSTR("Filter enabled, Error CRC filtered"));
      delete[] respFrame;
      startRx();
      return 5;
    }
  }

  delete[] respFrame;

  struct tm *timeinfo;
  time_t currenttime = time(NULL);
  if (currenttime < 0)
  {
    Log::error(PSTR("Failed to obtain time"));
    status.lastPacketInfo.time = "";
  }
  else
  {
    // store time of the last packet received:
    timeinfo = localtime(&currenttime);
    String thisTime = "";
    if (timeinfo->tm_hour < 10)
    {
      thisTime = thisTime + " ";
    } // add leading space if required
    thisTime = String(timeinfo->tm_hour) + ":";
    if (timeinfo->tm_min < 10)
    {
      thisTime = thisTime + "0";
    } // add leading zero if required
    thisTime = thisTime + String(timeinfo->tm_min) + ":";
    if (timeinfo->tm_sec < 10)
    {
      thisTime = thisTime + "0";
    } // add leading zero if required
    thisTime = thisTime + String(timeinfo->tm_sec);

    status.lastPacketInfo.time = thisTime;
  }

  noisyInterrupt = false;

  // put module back to listen mode
  startRx();

  if (state == ERR_NONE)
  {
    return 0;
  }
  else if (state == ERR_CRC_MISMATCH)
  {
    // packet was received, but is malformed
    Log::console(PSTR("[SX12x8] CRC error! Data cannot be retrieved"));
    int16_t buffSize = respLen * 3 + 1;

    if(send_config){

      Log::console(PSTR("Config not received, send NACK packet (%u bytes):"), sizeof(NACK_CONFIG_TC));
      char *nack_rx_data = new char[buffSize];
      uint8_t nack_data[sizeof(NACK_CONFIG_TC)];
      memcpy(nack_data,NACK_CONFIG_TC,sizeof(NACK_CONFIG_TC));
      for (int i = 0; i < sizeof(NACK_CONFIG_TC); i++)
      {
        sprintf(nack_rx_data + i * 3 % (buffSize - 1), "%02x ", nack_data[i]);
        if (i * 3 % buffSize == buffSize - 3 || i == sizeof(NACK_CONFIG_TC)-1)
          Log::console(PSTR("%s"), nack_rx_data); // print before the buffer is going to loop back
      }

      ConfigManager& configManager = ConfigManager::getInstance();
      uint8_t telecomand_encoded[256];
      int size = configManager.encode(NACK_CONFIG_TC,  sizeof(NACK_CONFIG_TC), telecomand_encoded);
      sendTx(telecomand_encoded, size);
      delay(500);
      sendTx(telecomand_encoded, size);
      Log::console(PSTR("Sending NACK CONFIG!"));
      send_config=false;

    }
    if(send_telemetry){

      Log::console(PSTR("Telemetry not received, send NACK packet (%u bytes):"), sizeof(NACK_TELEMETRY_TC));
      char *nack_rx_data = new char[buffSize];
      uint8_t nack_data[sizeof(NACK_TELEMETRY_TC)];
      memcpy(nack_data,NACK_TELEMETRY_TC,sizeof(NACK_TELEMETRY_TC));
      for (int i = 0; i < sizeof(NACK_TELEMETRY_TC); i++)
      {
        sprintf(nack_rx_data + i * 3 % (buffSize - 1), "%02x ", nack_data[i]);
        if (i * 3 % buffSize == buffSize - 3 || i == sizeof(NACK_TELEMETRY_TC)-1)
          Log::console(PSTR("%s"), nack_rx_data); // print before the buffer is going to loop back
      }

      ConfigManager& configManager = ConfigManager::getInstance();
      uint8_t telecomand_encoded[256];
      int size = configManager.encode(NACK_TELEMETRY_TC,  sizeof(NACK_TELEMETRY_TC), telecomand_encoded);
      sendTx(telecomand_encoded, size);
      delay(500);
      sendTx(telecomand_encoded, size);
      Log::console(PSTR("Sending NACK TELEMETRY!"));
      send_telemetry=false;

    }
    return 2;
  }
  else if (state == ERR_LORA_HEADER_DAMAGED)
  {
    // packet was received, but is malformed
    Log::console(PSTR("[SX12x8] Damaged header! Data cannot be retrieved"));
    return 2;
  }
  else
  {
    // some other error occurred
    Log::console(PSTR("[SX12x8] Failed, code %d"), state);
    return 3;
  }
}

void Radio::readState(int state)
{
  if (state == ERR_NONE)
  {
    Log::error(PSTR("success!"));
  }
  else
  {
    Log::error(PSTR("failed, code %d"), state);
    return;
  }
}

// remote
int16_t Radio::remote_freq(char *payload, size_t payload_len)
{
  float frequency = _atof(payload, payload_len);
  Log::console(PSTR("Set Frequency: %.3f MHz"), frequency);

  int16_t state = 0;
  if (ConfigManager::getInstance().getBoardConfig().L_SX127X)
  {
    ((SX1278 *)lora)->sleep(); // sleep mandatory if FastHop isn't ON.
    state = ((SX1278 *)lora)->setFrequency(frequency + status.modeminfo.freqOffset);
    ((SX1278 *)lora)->startReceive();
  }
  else
  {
    ((SX1268 *)lora)->sleep();
    state = ((SX1268 *)lora)->setFrequency(frequency + status.modeminfo.freqOffset);
    ((SX1268 *)lora)->startReceive();
  }

  readState(state);
  if (state == ERR_NONE)
    status.modeminfo.frequency = frequency;

  return state;
}

int16_t Radio::remote_bw(char *payload, size_t payload_len)
{
  float bw = _atof(payload, payload_len);
  Log::console(PSTR("Set bandwidth: %.3f MHz"), bw);

  int16_t state = 0;
  if (ConfigManager::getInstance().getBoardConfig().L_SX127X)
  {
    state = ((SX1278 *)lora)->setBandwidth(bw);
    ((SX1278 *)lora)->startReceive();
    ((SX1278 *)lora)->setDio0Action(setFlag);
  }
  else
  {
    state = ((SX1268 *)lora)->setBandwidth(bw);
    ((SX1268 *)lora)->startReceive();
    ((SX1268 *)lora)->setDio1Action(setFlag);
  }

  readState(state);
  if (state == ERR_NONE)
    status.modeminfo.bw = bw;

  return state;
}

int16_t Radio::remote_sf(char *payload, size_t payload_len)
{
  uint8_t sf = _atof(payload, payload_len);
  Log::console(PSTR("Set spreading factor: %u"), sf);

  int16_t state = 0;
  if (ConfigManager::getInstance().getBoardConfig().L_SX127X)
  {
    state = ((SX1278 *)lora)->setSpreadingFactor(sf);
    ((SX1278 *)lora)->startReceive();
    ((SX1278 *)lora)->setDio0Action(setFlag);
  }
  else
  {
    state = ((SX1268 *)lora)->setSpreadingFactor(sf);
    ((SX1268 *)lora)->startReceive();
    ((SX1268 *)lora)->setDio1Action(setFlag);
  }

  readState(state);

  if (state == ERR_NONE)
    status.modeminfo.sf = sf;

  return state;
}

int16_t Radio::remote_cr(char *payload, size_t payload_len)
{
  uint8_t cr = _atoi(payload, payload_len);
  Log::console(PSTR("Set coding rate: %u"), cr);

  int16_t state = 0;
  if (ConfigManager::getInstance().getBoardConfig().L_SX127X)
  {
    state = ((SX1278 *)lora)->setCodingRate(cr);
    ((SX1278 *)lora)->startReceive();
    ((SX1278 *)lora)->setDio0Action(setFlag);
  }
  else
  {
    state = ((SX1268 *)lora)->setCodingRate(cr);
    ((SX1268 *)lora)->startReceive();
    ((SX1268 *)lora)->setDio1Action(setFlag);
  }

  readState(state);

  if (state == ERR_NONE)
    status.modeminfo.cr = cr;

  return state;
}

int16_t Radio::remote_crc(char *payload, size_t payload_len)
{
  bool crc = _atoi(payload, payload_len);
  Log::console(PSTR("Set CRC: %s"), crc ? F("ON") : F("OFF"));
  int16_t state = 0;

  if (ConfigManager::getInstance().getBoardConfig().L_SX127X)
  {
    state = ((SX1278 *)lora)->setCRC(crc);
    ((SX1278 *)lora)->startReceive();
    ((SX1278 *)lora)->setDio0Action(setFlag);
  }
  else
  {
    state = ((SX1268 *)lora)->setCRC(crc);
    ((SX1268 *)lora)->startReceive();
    ((SX1268 *)lora)->setDio1Action(setFlag);
  }

  readState(state);
  return state;
}

int16_t Radio::remote_lsw(char *payload, size_t payload_len)
{
  uint8_t sw = _atoi(payload, payload_len);
  char strHex[2];
  sprintf(strHex, "%1x", sw);
  Log::console(PSTR("Set lsw: %s"), strHex);

  int16_t state = 0;
  if (ConfigManager::getInstance().getBoardConfig().L_SX127X)
    state = ((SX1278 *)lora)->setSyncWord(sw);
  else
    state = ((SX1268 *)lora)->setSyncWord(sw, 0x44);

  readState(state);
  return state;
}

int16_t Radio::remote_fldro(char *payload, size_t payload_len)
{
  bool ldro = _atoi(payload, payload_len);
  Log::console(PSTR("Set ForceLDRO: %s"), ldro ? F("ON") : F("OFF"));

  int16_t state = 0;
  if (ConfigManager::getInstance().getBoardConfig().L_SX127X)
  {
    state = ((SX1278 *)lora)->forceLDRO(ldro);
    ((SX1278 *)lora)->startReceive();
    ((SX1278 *)lora)->setDio0Action(setFlag);
  }
  else
  {
    state = ((SX1268 *)lora)->forceLDRO(ldro);
    ((SX1268 *)lora)->startReceive();
    ((SX1268 *)lora)->setDio1Action(setFlag);
  }

  readState(state);

  if (state == ERR_NONE)
  {
    if (ldro)
      status.modeminfo.fldro = true;
    else
      status.modeminfo.fldro = false;
  }

  return state;
}

int16_t Radio::remote_aldro(char *payload, size_t payload_len)
{
  Log::console(PSTR("Set AutoLDRO "));
  int16_t state = 0;

  if (ConfigManager::getInstance().getBoardConfig().L_SX127X)
  {
    state = ((SX1278 *)lora)->autoLDRO();
    ((SX1278 *)lora)->startReceive();
    ((SX1278 *)lora)->setDio0Action(setFlag);
  }
  else
  {
    state = ((SX1268 *)lora)->autoLDRO();
    ((SX1268 *)lora)->startReceive();
    ((SX1268 *)lora)->setDio1Action(setFlag);
  }

  readState(state);
  return state;
}

int16_t Radio::remote_pl(char *payload, size_t payload_len)
{
  uint16_t pl = _atoi(payload, payload_len);
  Log::console(PSTR("Set Preamble %u"), pl);
  int16_t state = 0;

  if (ConfigManager::getInstance().getBoardConfig().L_SX127X)
  {
    state = ((SX1278 *)lora)->setPreambleLength(pl);
    ((SX1278 *)lora)->startReceive();
    ((SX1278 *)lora)->setDio0Action(setFlag);
  }
  else
  {
    state = ((SX1268 *)lora)->setPreambleLength(pl);
    ((SX1268 *)lora)->startReceive();
    ((SX1268 *)lora)->setDio1Action(setFlag);
  }

  readState(state);
  if (state == ERR_NONE)
    status.modeminfo.preambleLength = pl;

  return state;
}

int16_t Radio::remote_begin_lora(char *payload, size_t payload_len)
{
  DynamicJsonDocument doc(256);
  deserializeJson(doc, payload, payload_len);
  float freq = doc[0];
  float bw = doc[1];
  uint8_t sf = doc[2];
  uint8_t cr = doc[3];
  uint8_t syncWord78 = doc[4];
  int8_t power = doc[5];
  uint8_t current_limit = doc[6];
  uint16_t preambleLength = doc[7];
  uint8_t gain = doc[8];
  uint16_t syncWord68 = doc[4];

  char sw78StrHex[2];
  char sw68StrHex[3];
  sprintf(sw78StrHex, "%1x", syncWord78);
  sprintf(sw68StrHex, "%2x", syncWord68);
  Log::console(PSTR("Set Frequency: %.3f MHz\nSet bandwidth: %.3f MHz\nSet spreading factor: %u\nSet coding rate: %u\nSet sync Word 127x: 0x%s\nSet sync Word 126x: 0x%s"), freq, bw, sf, cr, sw78StrHex, sw68StrHex);
  Log::console(PSTR("Set Power: %d\nSet C limit: %u\nSet Preamble: %u\nSet Gain: %u"), power, current_limit, preambleLength, gain);

  int16_t state = 0;
  if (ConfigManager::getInstance().getBoardConfig().L_SX127X)
  {
    ((SX1278 *)lora)->sleep(); // sleep mandatory if FastHop isn't ON.
    state = ((SX1278 *)lora)->begin(freq + status.modeminfo.freqOffset, bw, sf, cr, syncWord78, power, preambleLength, gain);
    ((SX1278 *)lora)->startReceive();
    ((SX1278 *)lora)->setDio0Action(setFlag);
  }
  else
  {
    state = ((SX1268 *)lora)->begin(freq + status.modeminfo.freqOffset, bw, sf, cr, syncWord68, power, preambleLength, ConfigManager::getInstance().getBoardConfig().L_TCXO_V);
    ((SX1268 *)lora)->startReceive();
    ((SX1268 *)lora)->setDio1Action(setFlag);
  }

  readState(state);
  if (state == ERR_NONE)
  {
    status.modeminfo.modem_mode = "LoRa";
    status.modeminfo.frequency = freq;
    status.modeminfo.bw = bw;
    status.modeminfo.power = power;
    status.modeminfo.preambleLength = preambleLength;
    status.modeminfo.sf = sf;
    status.modeminfo.cr = cr;
  }

  return state;
}

int16_t Radio::remote_begin_fsk(char *payload, size_t payload_len)
{
  DynamicJsonDocument doc(256);
  deserializeJson(doc, payload, payload_len);
  float freq = doc[0];
  float br = doc[1];
  float freqDev = doc[2];
  float rxBw = doc[3];
  int8_t power = doc[4];
  uint8_t currentlimit = doc[5];
  uint16_t preambleLength = doc[6];
  uint8_t ook = doc[7]; //

  Log::console(PSTR("Set Frequency: %.3f MHz\nSet bit rate: %.3f\nSet Frequency deviation: %.3f kHz\nSet receiver bandwidth: %.3f kHz\nSet Power: %d"), freq, br, freqDev, rxBw, power);
  Log::console(PSTR("Set Current limit: %u\nSet Preamble Length: %u\nOOK Modulation %s\nSet datashaping %u"), currentlimit, preambleLength, (ook != 255) ? F("ON") : F("OFF"), ook);

  int16_t state = 0;
  if (ConfigManager::getInstance().getBoardConfig().L_SX127X)
  {
    state = ((SX1278 *)lora)->beginFSK(freq + status.modeminfo.freqOffset, br, freqDev, rxBw, power, preambleLength, (ook != 255));
    ((SX1278 *)lora)->setDataShaping(ook);
    ((SX1278 *)lora)->startReceive();
    ((SX1278 *)lora)->setDio0Action(setFlag);
  }
  else
  {
    state = ((SX1268 *)lora)->beginFSK(freq + status.modeminfo.freqOffset, br, freqDev, rxBw, power, preambleLength, ConfigManager::getInstance().getBoardConfig().L_TCXO_V);
    ((SX1268 *)lora)->setDataShaping(ook);
    ((SX1268 *)lora)->startReceive();
    ((SX1268 *)lora)->setDio1Action(setFlag);
  }
  readState(state);

  if (state == ERR_NONE)
  {
    status.modeminfo.modem_mode = "FSK";
    status.modeminfo.frequency = freq;
    status.modeminfo.bw = rxBw;
    status.modeminfo.power = power;
    status.modeminfo.preambleLength = preambleLength;
    status.modeminfo.bitrate = br;
    status.modeminfo.freqDev = freqDev;
    status.modeminfo.OOK = ook;
  }

  return state;
}

int16_t Radio::remote_br(char *payload, size_t payload_len)
{
  uint8_t br = _atoi(payload, payload_len);
  Log::console(PSTR("Set FSK Bit rate: %u"), br);

  int16_t state = 0;
  if (ConfigManager::getInstance().getBoardConfig().L_SX127X)
    state = ((SX1278 *)lora)->setBitRate(br);
  else
    state = ((SX1268 *)lora)->setBitRate(br);

  readState(state);
  if (state == ERR_NONE)
    status.modeminfo.bitrate = br;

  return state;
}

int16_t Radio::remote_fd(char *payload, size_t payload_len)
{
  uint8_t fd = _atoi(payload, payload_len);
  Log::console(PSTR("Set FSK Frequency Dev.: %u"), fd);

  int16_t state = 0;
  if (ConfigManager::getInstance().getBoardConfig().L_SX127X)
    state = ((SX1278 *)lora)->setFrequencyDeviation(fd);
  else
    state = ((SX1268 *)lora)->setFrequencyDeviation(fd);

  readState(state);
  if (state == ERR_NONE)
    status.modeminfo.freqDev = fd;

  return state;
}

int16_t Radio::remote_fbw(char *payload, size_t payload_len)
{
  float frequency = _atof(payload, payload_len);
  Log::console(PSTR("Set FSK bandwidth: %.3f kHz"), frequency);

  int16_t state = 0;
  if (ConfigManager::getInstance().getBoardConfig().L_SX127X)
    state = ((SX1278 *)lora)->setRxBandwidth(frequency);
  else
    state = ((SX1268 *)lora)->setRxBandwidth(frequency);

  readState(state);
  if (state == ERR_NONE)
    status.modeminfo.bw = frequency;

  return state;
}

int16_t Radio::remote_fsw(char *payload, size_t payload_len)
{
  DynamicJsonDocument doc(256);
  deserializeJson(doc, payload, payload_len);
  uint8_t synnwordsize = doc.size();
  uint8_t syncWord[synnwordsize];

  Serial.println("");
  Serial.print(F("Set SyncWord Size "));
  Serial.print(synnwordsize);
  Serial.print(F("-> "));

  for (uint8_t words = 0; words < synnwordsize; words++)
  {
    syncWord[words] = doc[words + 1];
    Serial.print(F(" 0x"));
    Serial.print(syncWord[words], HEX);
    Serial.print(F(", "));
  }

  int16_t state = 0;
  if (ConfigManager::getInstance().getBoardConfig().L_SX127X)
    state = ((SX1278 *)lora)->setSyncWord(syncWord, synnwordsize);
  else
    state = ((SX1268 *)lora)->setSyncWord(syncWord, synnwordsize);

  readState(state);
  return state;
}

int16_t Radio::remote_fook(char *payload, size_t payload_len)
{
  DynamicJsonDocument doc(60);
  deserializeJson(doc, payload, payload_len);
  bool enableOOK = doc[0];
  uint8_t ook_shape = doc[1];

  Log::console(PSTR("OOK Modulation: %s"), enableOOK ? F("ON") : F("OFF"));
  Log::console(PSTR("Set OOK datashaping: %u"), ook_shape);

  int state = 0;
  if (ConfigManager::getInstance().getBoardConfig().L_SX127X)
  {
    state = ((SX1278 *)lora)->setOOK(enableOOK);
  }
  else
  {
    Log::error(PSTR("OOK not supported by the selected lora module!"));
    return -1;
  }

  readState(state);

  if (ConfigManager::getInstance().getBoardConfig().L_SX127X)
    state = ((SX1278 *)lora)->setDataShapingOOK(ook_shape);

  readState(state);
  return state;
}

void Radio::remote_SPIwriteRegister(char *payload, size_t payload_len)
{
  DynamicJsonDocument doc(60);
  deserializeJson(doc, payload, payload_len);
  uint8_t reg = doc[0];
  uint8_t data = doc[1];
  Log::console(PSTR("REG ID: 0x%x to 0x%x"), reg, data);

  if (ConfigManager::getInstance().getBoardConfig().L_SX127X)
    ((SX1278 *)lora)->_mod->SPIwriteRegister(reg, data);
  //  else
  //   ((SX1268*)lora)->_mod->SPIwriteRegister(reg,data);
}

int16_t Radio::remote_SPIreadRegister(char *payload, size_t payload_len)
{
  uint8_t reg = _atoi(payload, payload_len);
  uint8_t data = 0;

  int16_t state = 0;
  if (ConfigManager::getInstance().getBoardConfig().L_SX127X)
    data = ((SX1278 *)lora)->_mod->SPIreadRegister(reg);
  // else
  //   data = ((SX1268*)lora)->_mod->SPIreadRegister(reg);

  Log::console(PSTR("REG ID: 0x%x HEX : 0x%x BIN : %b"), reg, data, data);

  readState(state);
  return data;
}

int16_t Radio::remote_SPIsetRegValue(char *payload, size_t payload_len)
{
  DynamicJsonDocument doc(120);
  deserializeJson(doc, payload, payload_len);
  uint8_t reg = doc[0];
  uint8_t value = doc[1];
  uint8_t msb = doc[2];
  uint8_t lsb = doc[3];
  uint8_t checkinterval = doc[4];

  Serial.println("");
  Serial.print(F("REG ID: 0x"));
  Serial.println(reg, HEX);
  Serial.print(F("to HEX: 0x"));
  Serial.print(value, HEX);
  Serial.print(F("bin : 0x "));
  Serial.println(value, BIN);
  Serial.print(F("msb : "));
  Serial.println(msb);
  Serial.print(F("lsb : "));
  Serial.println(lsb);
  Serial.print(F("check_interval : "));
  Serial.println(checkinterval);

  int16_t state = 0;

  if (ConfigManager::getInstance().getBoardConfig().L_SX127X)
    state = ((SX1278 *)lora)->_mod->SPIsetRegValue(reg, value, msb, lsb, checkinterval);
  else
    //   state = ((SX1268*)lora)->_mod->SPIsetRegValue(reg, value, msb, lsb, checkinterval);

    readState(state);
  return state;
}

double Radio::_atof(const char *buff, size_t length)
{
  char str[length + 1];
  memcpy(str, buff, length);
  str[length] = '\0';
  return atof(str);
}

int Radio::_atoi(const char *buff, size_t length)
{
  char str[length + 1];
  memcpy(str, buff, length);
  str[length] = '\0';
  return atoi(str);
}

void  Radio::decode_conv(uint8_t* data, size_t length)
{
  correct_convolutional *conv = correct_convolutional_create(RATE_CON, ORDER_CON, correct_conv_r12_7_polynomial);
  int index = ceil(length/RATE_CON);
  uint8_t conv_decoded[index];
  ssize_t decoded_conv_size = correct_convolutional_decode(conv, data, length*8, conv_decoded);
  memcpy(data, conv_decoded,decoded_conv_size);
}

void  Radio::deinterleave(uint8_t* data, size_t length)
{
  bool end = false;
	int q = 0;
	int r = 0;
	int col;
	int row;
	uint8_t block[BLOCK_ROW_INTER][BLOCK_COL_INTER];
  uint8_t codeword_deinterleaved[length];

	while(q < length){
		for(col=0; col < BLOCK_COL_INTER && !end; col++){
			for(row=0; row < BLOCK_ROW_INTER && !end; row++){
				if (q < length){
					block[row][col] = data[q];
					q++;
				}
				else{
					end = true;
				}
			}
		}
		for(int t = 0; t < BLOCK_COL_INTER; t++){
			for(int p = 0; p < BLOCK_ROW_INTER; p++){
					codeword_deinterleaved[r] = block[t][p];
					r++;
			}
		}
	}
  memcpy(data,codeword_deinterleaved,length);
}

void  Radio::decode_rs(uint8_t* data, size_t length)
{
  
  
  correct_reed_solomon *rs = correct_reed_solomon_create(correct_rs_primitive_polynomial_ccsds, 1, 1, MIN_DISTANCE_RS);
  uint8_t rs_decoded[MESSAGE_LENGTH_RS];  
  ssize_t size_decode = correct_reed_solomon_decode(rs, data, length, rs_decoded); 

  
  initialize_ecc();
  srand(time(NULL));
  unsigned char *data_ch[length];
  memcpy(data_ch,data,length);
  decode_data(data, length);
  
  int erasures[16];
  int nerasures = 0;
  int syndrome = check_syndrome();
  
  // check if syndrome is all zeros 

  if (syndrome == 0) {
    // no errs detected, codeword payload should match message
    Log::console(PSTR("No errors detected, codeword payload should match message"));
  } else {
    //nonzero syndrome, attempting correcting errors
    int result = 0;//result 0 not able to correct, result 1 corrected
    Log::console(PSTR("Errors detected, proceeding to correct errors"));
    result =correct_errors_erasures (data,length,nerasures,erasures);
  }
  
}
