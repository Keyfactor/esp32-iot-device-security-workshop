//
//  espPerso.c
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

#define LOG_LOCAL_LEVEL ESP_LOG_DEBUG

#include "esp_log.h"
#include <stdio.h>
#include <sys/param.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/timers.h"
#include "freertos/event_groups.h"
#include "esp_wifi.h"
#include "esp_log.h"
#include "nvs_flash.h"
#include "esp_netif.h"
#include <netdb.h>
#include "esp_http_server.h"
#include "esp_chip_info.h"
#include "wifi_manager.h"
#include "TrustPlatform.h"
#include "espPerso.h"


static const char *TAG = "IOT-MATE";



void cb_connection_ok(void *pvParameter){
    ip_event_got_ip_t* param = (ip_event_got_ip_t*)pvParameter;

    /* transform IP to human readable string */
    char str_ip[16];
    esp_ip4addr_ntoa(&param->ip_info.ip, str_ip, IP4ADDR_STRLEN_MAX);
    ESP_LOGI(TAG, "\n==================================================================\n");
    ESP_LOGI(TAG, "Connected !  My IP is %s!", str_ip);
    TPinit();
}



void app_main(void)
{


    esp_chip_info_t chip_info;

    esp_err_t ret = nvs_flash_init();
    ESP_ERROR_CHECK(ret);  
    
    esp_log_level_set("TrustPlatform",ESP_LOG_INFO);
    esp_log_level_set("wifi",ESP_LOG_ERROR);
    ESP_LOGI(TAG, "\n==================================================================\n");
    ESP_LOGI(TAG, "Starting up the personalization ...... ");
    // get system enviroments and log data
    esp_chip_info(&chip_info);
    ESP_LOGI(TAG, "SYSTEM INFORMATION:   ChipType: %s",CONFIG_IDF_TARGET);
    ESP_LOGI(TAG, "                      Core(s) : %d",chip_info.cores);
    ESP_LOGI(TAG, "                      COM     : Wifi%s%s",(chip_info.features & CHIP_FEATURE_BT) ? "/BT" : "", 
                                                               (chip_info.features & CHIP_FEATURE_BLE) ? "/BLE" : "");
    ESP_LOGI(TAG, "                      Rev     : %d", chip_info.revision);
    
    ESP_LOGI(TAG, "\n==================================================================\n");
    espPerso();
    ESP_LOGI(TAG, " Personalizsation done start IOT-MATE appe ");
    wifi_manager_start();
    wifi_manager_set_callback(WM_EVENT_STA_GOT_IP, &cb_connection_ok);
      
}
