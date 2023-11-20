///
//  DeviceID.h
//  Support the DeviveID within the ESP32
//
//  
//  Created by Andreas Philipp on 11.07.2023
//  Copyright © 2023 Keyfactor
//
// Licensed under the Apache License, Version 2.0 (the "License"); you may   
// not use this file except in compliance with the License.  You may obtain a 
// copy of the License at http://www.apache.org/licenses/LICENSE-2.0.  Unless 
// required by applicable law or agreed to in writing, software distributed   
// under the License is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES   
// OR CONDITIONS OF ANY KIND, either express or implied. See the License for  
// thespecific language governing permissions and limitations under the       
// License.   


#ifndef DEVICEID_H
#define DEVICEID_H

#include "esp_log.h"
#include <string.h>
#include "esp_task_wdt.h"
#include "esp_err.h"
#include "mbedtls/rsa.h"
#include "mbedtls/pk.h"
#include "mbedtls/sha1.h"
#include "mbedtls/platform.h"
#include "mbedtls/build_info.h"
#include "mbedtls/oid.h"
#include "mbedtls/ctr_drbg.h"
#include "mbedtls/x509.h"
#include "mbedtls/pem.h"
#include "mbedtls/x509_csr.h"
#include "mbedtls/error.h"
#include "mbedtls/md.h"
#include "mbedtls/entropy.h"
#include "mbedtls/bignum.h"

/***********      Global definition       ************/
#define DEVID_FORMAT_PEM        0
#define DEVID_FORMAT_DER        1
#define DEVID_TYPE              MBEDTLS_PK_RSA
#define DEVID_RSA_KEYSIZE       2048
#define DEVID_EXPONENT          65537
#define DEVID_KEY_FILENAME      "DevID.key"
#define DEVID_CERT_FILENAME		"DevID.crt"
#define DEVID_FORMAT            DEVID_FORMAT_PEM
#define DEVID_MD_ALG            MBEDTLS_MD_SHA256
//#define DEVID_SUBJECT_NAME      "CN=wrover-dps-99,O=DycodeX,C=ID,serialNumber=0123456"
//#define DEVID_SUBJECT_NAME      "CN=de-fault,O=default,C=DE,serialNumber=0"
#define DEVID_SUBJECT_NAME      "CN=1-ev8DpE0WaJ,O=Campus Schwarzwald,serialNumber=1-ev8DpE0WaJ"
#define DEVID_CERTHEADER		"-----BEGIN CERTIFICATE-----\n"
#define DEVID_CERTFOOTER		"-----END CERTIFICATE-----\n"




/***********      ERROR Codes    ************/
#define DEVID_OK                           	0x0000                  // Everything ok      
#define DEVID_FAIL                         	0x4300                  // Undefined Error; default Error
#define DEVID_ERR_INIT						0x4301					// Error during the init phase
#define DEVID_ERR_KEYGEN					0x4302					// Error during key generation phase
#define DEVID_ERR_CSRGEN					0x4303					// Error durign CSR generation 
#define DEVID_ERR_WRITE_CERT				0x4304					// Error during conversion



/***********      DevID X.509  definition       ************/
// standard usage: Configuration made via menuconfig
#define DEVID_C         CONFIG_OT_DEVID_C



// alternative section please remove the commands.
// #define DEVID_C         "DE"



/***********      function declaration       ************/


//int DeviceID_open(void);
int DeviceID_open(void);
int DeviceID_genKey(char* p_seed);
int DeviceID_genCSR(unsigned char* p_csrbuf, uint16_t csrbuflen, char* p_subname, char* p_subaltname);
int DeviceID_storeCert(unsigned char* p_devID, uint16_t devIDlen);
int DeviceID_close(void);


#endif
