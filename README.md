# ESP32 -  IoT Device Security Workshop
This ESP32 Firmware project is the complete package that will be loaded upfront into the device. 

It consists of four function modules: 
* _IoTMate.c_ – The main IoT device application 

* _espPerso.c_ – The personalization workflow, which is triggered during the first Time Boot Process 

* _DeviceID.c_ – All functions to generate key material, generate CSR, and store the Device ID in the trusted area

* _TrustPlatform.c_ – The trusted storage area that is part of the _SPLIFFS_ file. It is encrypted with an on-demand generated AES Key 

**Note:** Configuration of the default parameters is done in the _idf.py menuconfig_. 

## Prerequisites
The following prerequisites apply: 
* Since the main IoT device uses an OLED Display, the _Component_ folder with the _sdd1306_ package has to be included in the _makefile_. 
* _mbedtls_ must be added to the project. This is the core crypto library that is used. 
 
## How does it work
Please follow the workshop tutorial on: [KeyFactor Community Youtube](https://www.youtube.com/@KeyfactorCommunity)

## Technical support and feedback
For a feature request or bug report, create a [GitHub issue](../../issues/new). We will get back to you as soon as possible.
