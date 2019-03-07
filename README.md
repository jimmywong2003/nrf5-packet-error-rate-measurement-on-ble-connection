# BLE_Throughput_Test_with_PER

Example on BLE Throughput Test with Packet Error Rate

This demo is to show the relationship between packet error rate and throughput of BLE.
It is based on two examples together.
* [Multilink] (https://github.com/NordicPlayground/nrf52-ble-multi-link-multi-role) 
* [Image transfer demo] (https://github.com/NordicPlayground/nrf52-ble-image-transfer-demo)

It uses the Adafruit ILI9341 2.8 320x240 [LCD](https://learn.adafruit.com/adafruit-2-8-tft-touch-shield-v2/graphics-test) to show the packet error rate.

## Packet Error Rate

The packet error ratio (PER) is the number of incorrectly received data packets divided by the total number of received packets. A packet is declared incorrect if at least one bit is erroneous.
In this example, it counts the difference between number of TX packets send and number of RX (CRC OK) packets to estimate the PER.

![Image of PER](https://github.com/jimmywong2003/BLE_Throughput_Test_with_PER/blob/master/picture/how_to_get_packet_eror_rate.png)

## Requirements
* nRF52832 DK / nRF52840 DK
* Adafruit ILI9341 2.8 320x240
* Segger Embedded Studio
* nRF5_SDK_v15.3.0


