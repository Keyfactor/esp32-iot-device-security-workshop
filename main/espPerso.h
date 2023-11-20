//
//  espPerso.h
//  esp Personalisation System for Campus Schwarzwald Showcase
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

#ifndef __ESPPERSO_H__
#define __ESPPERSO_H__



// State Definition for the Main Process Task 
enum STATE
{
    STATE_INIT,
    STATE_WAIT_WIFI,
    STATE_OTA_REQUEST,
    STATE_APP_LOOP,
    STATE_CONNECTION_IS_OK
};


// standard usage: Configuration made via menuconfig
#define CTRL_COMP_URL        CONFIG_OT_CTRL_URL
#define CTRL_COMP_PORT       CONFIG_OT_CTRL_PORT

#define NOT_INIT             0xFF
#define INIT                 0x00   



//
// Macro to check the error code.
// Prints the error code, error location, and the failed statement to serial output.
// Unlike to ESP_ERROR_CHECK() method this macros abort the application's execution if it was built as 'release'.
//
#define APP_ABORT_ON_ERROR(x)                                     \
    do                                                            \
    {                                                             \
        esp_err_t __err_rc = (x);                                 \
        if (__err_rc != ESP_OK)                                   \
        {                                                         \
            _esp_error_check_failed(__err_rc, __FILE__, __LINE__, \
                                    __ASSERT_FUNC, #x);           \
        }                                                         \
    } while (0);

// Global var definition
extern nvs_handle_t gPERSO;



/***********      function declaration       ************/
void espPerso(void);



#endif // __ESPPERSO_H__