#if !defined(_RADIOLIB_RFM95_H)
#define _RADIOLIB_RFM95_H

#include "../../TypeDef.h"

#if !defined(RADIOLIB_EXCLUDE_RFM9X)

#include "../../Module.h"
#include "../SX127x/SX127x.h"
#include "../SX127x/SX1278.h"

// SX127X_REG_VERSION
#define RFM9X_CHIP_VERSION_OFFICIAL                   0x11
#define RFM9X_CHIP_VERSION_UNOFFICIAL                 0x12  // according to datasheet, only 0x11 should be possible, but some modules seem to have 0x12

/*!
  \class RFM95

  \brief Derived class for %RFM95 modules. Overrides some methods from SX1278 due to different parameter ranges.
*/
class RFM95: public SX1278 {
  public:

    // constructor

    /*!
      \brief Default constructor. Called from Arduino sketch when creating new LoRa instance.

      \param mod Instance of Module that will be used to communicate with the %LoRa chip.
    */
    RFM95(Module* mod);

    // basic methods

    /*!
      \brief %LoRa modem initialization method. Must be called at least once from Arduino sketch to initialize the module.

      \param freq Carrier frequency in MHz. Allowed values range from 868.0 MHz to 915.0 MHz.

      \param bw %LoRa link bandwidth in kHz. Allowed values are 10.4, 15.6, 20.8, 31.25, 41.7, 62.5, 125, 250 and 500 kHz.

      \param sf %LoRa link spreading factor. Allowed values range from 6 to 12.

      \param cr %LoRa link coding rate denominator. Allowed values range from 5 to 8.

      \param syncWord %LoRa sync word. Can be used to distinguish different networks. Note that value 0x34 is reserved for LoRaWAN networks.

      \param power Transmission output power in dBm. Allowed values range from 2 to 17 dBm.

      \param preambleLength Length of %LoRa transmission preamble in symbols. The actual preamble length is 4.25 symbols longer than the set number.
      Allowed values range from 6 to 65535.

      \param gain Gain of receiver LNA (low-noise amplifier). Can be set to any integer in range 1 to 6 where 1 is the highest gain.
      Set to 0 to enable automatic gain control (recommended).

      \returns \ref status_codes
    */
    int16_t begin(float freq = 915.0, float bw = 125.0, uint8_t sf = 9, uint8_t cr = 7, uint8_t syncWord = SX127X_SYNC_WORD, int8_t power = 10, uint16_t preambleLength = 8, uint8_t gain = 0);

    // configuration methods

    /*!
      \brief Sets carrier frequency. Allowed values range from 868.0 MHz to 915.0 MHz.

      \param freq Carrier frequency to be set in MHz.

      \returns \ref status_codes
    */
    int16_t setFrequency(float freq);

#ifndef RADIOLIB_GODMODE
  private:
#endif

};

#endif

#endif
