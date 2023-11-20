///
//  TrustPlattform.h
//  TrustPlatform for ESP32 based on SPIFFS and encrypted file system
//  This Plafform provides a generic Trust Module for ESP32 
//  prerequesites: 
//      - SPIFFS min 4K Space 
//      - 
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


#ifndef TRUSTPLATFORM_H
#define TRUSTPLATFORM_H

#include "esp_system.h"
#include "esp_mac.h"
#include "esp_vfs.h"
#include "esp_spiffs.h"
#include "esp_log.h"
#include <string.h>
#include "esp_err.h"
#include "mbedtls/build_info.h"
#include "mbedtls/platform.h"
#include "mbedtls/aes.h"
#include "mbedtls/sha256.h" 


/***********      Defines        ************/
#define TP_NOT_INIT                     false
#define TP_INIT                         true



/***********      ERROR Codes    ************/
#define TP_OK                           0x0000                  // Everything ok      
#define TP_FAIL                         0x5300                  // Undefined Error; default Error
#define TP_ERR_INIT                     0x5301                  // Error during Init phase
#define TP_ERR_FILE_NOT_EXIST           0x5302                  // Error File doesn't exist
#define TP_ERR_COULD_NOT_OPEN_FILE      0x5303                  // Error while trying to open file
#define TP_ERR_READ_FILE                0x5304                  // Error during read file
#define TP_ERR_BUFFER_TO_SMALL          0x5305                  // Error given Buffer is to small     


//      Trust Platform definition
#define TP_BASE_PATH         "/TP"
#define TP_PARTITION_LABEL   "TrustStore"
#define TP_MASTER_KEY_NAME   "MasterKey.key"
#define TP_MAX_FILES         5



/***********      Type defintion        ************/

typedef struct
{
    bool init;
    esp_vfs_spiffs_conf_t tp_cnf;
} tp_store_conf_t;


/***********      function declatration        ************/


esp_err_t TPinit(void);
esp_err_t TPread(char* p_filename, unsigned char* p_buffer, uint16_t* p_len);
esp_err_t TPwrite(char* p_filename, unsigned char* p_buffer, uint16_t len);



#endif