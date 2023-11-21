# ESP32 -  IoT Device Security Workshop

This ESP32 Firmware project is the completet package that will be loaded upfront into the Device. 
It basically consists of four function modules

* IoTMate.c
The main IoT Device Application 

* espPerso.c
The personalisation workflow, which is triggert during first Time Boot Process. 

* DeviceID.c
All function to generate  Key Material, generate CSR and to store the Device ID in the Trustet area

* TrustPlatform.c
The Trusted Storage area that is part of the SPLIFFS file. It is encrypted with a "on-demand" generated AES Key. 

** Note **
The whole configuration fo the default paramter are made within the idf.py menuconfig. 

## Prerequisite
Due to the fact that the main IoT Device is working with an OLED Display the Component folder with the sdd1306 package has to be included into the makefile of the Project. 
In addition you have to add mbedtls to you project. This is the core crypto-library that is used. 
 
## How does it work
Please follow the Tutorial about the Workshop at: [KeyFactor Community Youtube](https://www.youtube.com/@KeyfactorCommunity)

## Technical support and feedback

Please use the following feedback channels:

* For a feature request or bug report, create a [KeyFactor Community Youtube](https://www.youtube.com/@KeyfactorCommunity) [GitHub issue](../../issues/new).

We will get back to you as soon as possible.

