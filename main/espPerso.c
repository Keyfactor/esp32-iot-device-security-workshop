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


#include "deviceID.h"


#include "espPerso.h"

#include "cJSON.h"



#include "esp_heap_caps.h"


static EventGroupHandle_t s_wifi_event_group;

#define WIFI_CONNECTED_BIT BIT0
#define WIFI_FAIL_BIT      BIT1
#define WIFI_SSID   CONFIG_OT_WIFI_SSID
#define WIFI_PW     CONFIG_OT_WIFI_PASSWORD
#define WIFI_RETRY  CONFIG_OT_MAXIMUM_RETRY


#define MAX_HTTP_RECV_BUFFER 512
#define MAX_HTTP_OUTPUT_BUFFER 2048


// Global var definition section:
static const char *TAG = "PERSO";

static int s_retry_num = 0;
unsigned char gCertBuf[4096];
nvs_handle_t gPERSO;




static void _set_static_ip(esp_netif_t *netif)
{
    if (esp_netif_dhcpc_stop(netif) != ESP_OK) {
        ESP_LOGE(TAG, "Failed to stop dhcp client");
        return;
    }
    esp_netif_ip_info_t ip;
    memset(&ip, 0 , sizeof(esp_netif_ip_info_t));
    ip.ip.addr = ipaddr_addr(CONFIG_OT_STATIC_IP_ADDR);
    ip.netmask.addr = ipaddr_addr(CONFIG_OT_STATIC_NETMASK_ADDR);
    ip.gw.addr = ipaddr_addr(CONFIG_OT_STATIC_GW_ADDR);
    if (esp_netif_set_ip_info(netif, &ip) != ESP_OK) {
        ESP_LOGE(TAG, "Failed to set ip info");
        return;
    }
    ESP_LOGD(TAG, "Success to set static ip");
}


static void wifi_event_handler(void *p_arg, esp_event_base_t event_base, int32_t event_id, void *event_data)
{

    if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_START)
    {
        esp_wifi_connect();
    } else if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_CONNECTED) {
        _set_static_ip(p_arg);
    } else if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_DISCONNECTED) {
        if (s_retry_num < WIFI_RETRY) {
            esp_wifi_connect();
            s_retry_num++;
            ESP_LOGI(TAG, "retry to connect to the AP");
        } else {
            xEventGroupSetBits(s_wifi_event_group, WIFI_FAIL_BIT);
        }
        ESP_LOGI(TAG,"connect to the AP fail");
    } else if (event_base == IP_EVENT && event_id == IP_EVENT_STA_GOT_IP) {
        ip_event_got_ip_t* event = (ip_event_got_ip_t*) event_data;
        ESP_LOGI(TAG, "got ip:" IPSTR, IP2STR(&event->ip_info.ip));
        s_retry_num = 0;
        xEventGroupSetBits(s_wifi_event_group, WIFI_CONNECTED_BIT);
    }



}


void wifi_connection()
{
    
    s_wifi_event_group = xEventGroupCreate();

    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());
    esp_netif_t *sta_netif = esp_netif_create_default_wifi_sta();
    assert(sta_netif);

    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));

    esp_event_handler_instance_t instance_any_id;
    esp_event_handler_instance_t instance_got_ip;
    ESP_ERROR_CHECK(esp_event_handler_instance_register(WIFI_EVENT,
                                                        ESP_EVENT_ANY_ID,
                                                        &wifi_event_handler,
                                                        sta_netif,
                                                        &instance_any_id));
    ESP_ERROR_CHECK(esp_event_handler_instance_register(IP_EVENT,
                                                        IP_EVENT_STA_GOT_IP,
                                                        &wifi_event_handler,
                                                        sta_netif,
                                                        &instance_got_ip));

    wifi_config_t wifi_config = {
        .sta = {
            .ssid = WIFI_SSID,
            .password = WIFI_PW,
        },
    };
    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA) );
    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &wifi_config) );
    ESP_ERROR_CHECK(esp_wifi_start() );
    ESP_LOGI(TAG, "wifi_init_sta finished.");

    /* Waiting until either the connection is established (WIFI_CONNECTED_BIT) or connection failed for the maximum
     * number of re-tries (WIFI_FAIL_BIT). The bits are set by event_handler() (see above) */
    EventBits_t bits = xEventGroupWaitBits(s_wifi_event_group,
            WIFI_CONNECTED_BIT | WIFI_FAIL_BIT,
            pdFALSE,
            pdFALSE,
            portMAX_DELAY);

    /* xEventGroupWaitBits() returns the bits before the call returned, hence we can test which event actually
     * happened. */
    if (bits & WIFI_CONNECTED_BIT) 
    {
        ESP_LOGI(TAG, "connected to ap SSID:%s", WIFI_SSID);
    } else if (bits & WIFI_FAIL_BIT) 
    {
        ESP_LOGI(TAG, "Failed to connect to SSID:%s",WIFI_SSID);
    } else 
    {
        ESP_LOGE(TAG, "UNEXPECTED EVENT");
    }
}



/* Our URI handler function to be called during GET /uri request */

esp_err_t status_handler(httpd_req_t *req)
{
    int ret = ESP_FAIL;
    cJSON *rspJSN = NULL;
    
    ESP_LOGI(TAG, "\n==================================================================");
    ESP_LOGI(TAG, "GET: v1/DevID/status");
    rspJSN = cJSON_CreateObject();
    cJSON_AddStringToObject(rspJSN,"status","Ready");
    char *rsp_json_string = cJSON_Print(rspJSN);
    ESP_LOGI(TAG," GET response: %s",rsp_json_string);
    if(httpd_resp_send(req, rsp_json_string  , HTTPD_RESP_USE_STRLEN)!= ESP_OK)
        ESP_LOGI(TAG," ERRRO Sending response ");
    else
        ret = ESP_OK;
    free(rsp_json_string);
    cJSON_Delete(rspJSN);
    return ret;
}

/* Our URI handler function to be called during POST /uri request */
esp_err_t genCSR_handler(httpd_req_t *req)
{
    /* Destination buffer for content of HTTP POST request.
     * httpd_req_recv() accepts char* only, but content could
     * as well be any binary data (needs type casting).
     * In case of string data, null termination will be absent, and
     * content length would give length of string */
    char content[100];
    char subname[300];

    /* Truncate if content length larger than the buffer */
    size_t recv_size = MIN(req->content_len, sizeof(content));
    ESP_LOGI(TAG, "\n==================================================================");
    ESP_LOGI(TAG, "POST: v1/DevID/genCSR");

    int ret = httpd_req_recv(req, content, recv_size);
    if (ret <= 0) 
    {   /* 0 return value indicates connection closed */
        /* Check if timeout occurred */
        if (ret == HTTPD_SOCK_ERR_TIMEOUT) 
        {
            /* In case of timeout one can choose to retry calling
             * httpd_req_recv(), but to keep it simple, here we
             * respond with an HTTP 408 (Request Timeout) error */
            httpd_resp_send_408(req);
        }
        /* In case of error, returning ESP_FAIL will
         * ensure that the underlying socket is closed */
        return ESP_FAIL;
    }
    else 
    {
        cJSON *msgBuf = cJSON_Parse(content);
        subname[0] = 0x00;
     
        if (cJSON_GetObjectItem(msgBuf, "CN")) 
        {
            char *cn = cJSON_GetObjectItem(msgBuf,"CN")->valuestring;
            strcat (subname,"CN=");
            strcat (subname,cn);
            strcat (subname, ",");
            ESP_LOGI(TAG, "subname= %s",subname);
            free(cn);
        }
        if (cJSON_GetObjectItem(msgBuf, "O")) 
        {
            char *o = cJSON_GetObjectItem(msgBuf,"O")->valuestring;
            strcat (subname,"O=");
            strcat (subname,o);
            strcat (subname, ",");
            ESP_LOGI(TAG, "subname= %s",subname);
            free(o);   
        }
        if (cJSON_GetObjectItem(msgBuf, "SN")) 
        {
            char *sn = cJSON_GetObjectItem(msgBuf,"SN")->valuestring;
            strcat (subname,"serialNumber=");
            strcat (subname,sn);
            //strcat (subname, ",");
            ESP_LOGI(TAG, "subname= %s",subname);
            free(sn);   
        }
        ret = DeviceID_genCSR(gCertBuf, 4096, subname, subname);
        //cJSON_Delete(msgBuf);
    }

    ESP_LOGI(TAG,"CSR Buffer: %s",gCertBuf);
    httpd_resp_send(req,(char*)gCertBuf, HTTPD_RESP_USE_STRLEN);
    return ESP_OK;
}



esp_err_t devIDfinal_handler(httpd_req_t *req)
{
    
    cJSON *rspJSN = NULL;
    char* content = (char*)malloc(2000);
        
    /* Truncate if content length larger than the buffer */
    size_t recv_size = MIN(req->content_len, 2000);
    ESP_LOGI(TAG, "\n==================================================================");
        ESP_LOGI(TAG, "POST: v1/DevID/DevIDfinal: recv_size len: %d",recv_size);

    int ret = httpd_req_recv(req, content, recv_size);
    if (ret <= 0) 
    {   /* 0 return value indicates connection closed */
        /* Check if timeout occurred */
        if (ret == HTTPD_SOCK_ERR_TIMEOUT) 
        {
            /* In case of timeout one can choose to retry calling
             * httpd_req_recv(), but to keep it simple, here we
             * respond with an HTTP 408 (Request Timeout) error */
            httpd_resp_send_408(req);
        }
        /* In case of error, returning ESP_FAIL will
         * ensure that the underlying socket is closed */
        return ESP_FAIL;
    }
    else 
    {
        ESP_LOGI(TAG, "Message recieved");
        cJSON *msgBuf = cJSON_Parse(content);
     
        if (cJSON_GetObjectItem(msgBuf, "DevID")) 
        {
            char *devIDder = cJSON_GetObjectItem(msgBuf,"DevID")->valuestring;
            ESP_LOGI(TAG, "DevID : %s",devIDder); 
            if(DeviceID_storeCert((unsigned char*) devIDder, strlen(devIDder))!= 0)
                return ESP_FAIL;
        }
        else
        {
            ESP_LOGI(TAG, "No value DevID found"); 
            return ESP_FAIL;
        }
        cJSON_Delete(msgBuf);
        ESP_LOGI(TAG, "Ready generate final response");

        rspJSN = cJSON_CreateObject();
        cJSON_AddStringToObject(rspJSN,"status","Init");
        char *rsp_json_string = cJSON_Print(rspJSN);
        ESP_LOGI(TAG," POST response: %s",rsp_json_string);
        if(httpd_resp_send(req, rsp_json_string  , HTTPD_RESP_USE_STRLEN)!= ESP_OK)
        {
            ESP_LOGI(TAG," ERRRO Sending response ");
            return ESP_FAIL;
        }
        uint8_t state = INIT;
        if( nvs_set_u8(gPERSO, "status", state) != ESP_OK)
        {
            while(1)
            {
              ESP_LOGI(TAG, "\n############ ERROR DURING PERSO STOP WORKING \n");
            }

        }
        nvs_close(gPERSO);
        free(rsp_json_string);
        cJSON_Delete(rspJSN);
    }
    return ESP_OK;
}


/* URI handler structure for GET /uri */
httpd_uri_t uri_status = {
    .uri      = "/v1/DevID/status", 
    .method   = HTTP_GET,
    .handler  = status_handler,
    .user_ctx = NULL
};

/* URI handler structure for POST /uri */
httpd_uri_t uri_gencsr = {
    .uri      = "/v1/DevID/genCSR", 
    .method   = HTTP_POST,
    .handler  = genCSR_handler,
    .user_ctx = NULL
};
/* URI handler structure for POST /uri */
httpd_uri_t uri_devIDfinal = {
    .uri      = "/v1/DevID/DevIDfinal", 
    .method   = HTTP_POST,
    .handler  = devIDfinal_handler,
    .user_ctx = NULL
};

/* Function for starting the webserver */
httpd_handle_t start_webserver(void)
{
    /* Generate default configuration */
    httpd_config_t config = HTTPD_DEFAULT_CONFIG();
    /* Empty handle to esp_http_server */
    httpd_handle_t server = NULL;
    /* Start the httpd server */
    if (httpd_start(&server, &config) == ESP_OK) 
    {
        /* Register URI handlers */
        httpd_register_uri_handler(server, &uri_status);
        httpd_register_uri_handler(server, &uri_gencsr);
        httpd_register_uri_handler(server, &uri_devIDfinal);
    }
    /* If server failed to start, handle will be NULL */
    if (server == NULL){
        ESP_LOGE(TAG, "failed to start server");
    }
    else
    {
        ESP_LOGI(TAG, "\n==================================================================\n");        
        ESP_LOGI(TAG, "REST Endpoint started ");
    }
    return server;
}




void espPerso(void)
{
    int ret = DEVID_ERR_INIT;
    const char *p_init_seed = "gen_key";
    esp_err_t err; 
    bool do_perso = false;
    

    err = nvs_open("status",NVS_READWRITE,&gPERSO);
    if (err != ESP_OK)
    {
        while(1)
        {
          ESP_LOGI(TAG, "\n############ ERROR DURING Open Status !! STOP WORKING \n");
        }
    }
    uint8_t state = NOT_INIT;

    err= nvs_get_u8(gPERSO,"status",&state);
    switch (err) 
    {
            case ESP_OK:
                ESP_LOGI(TAG,"Done\n");
                ESP_LOGI(TAG,"Restart counter = %d", state);
                if (state ==NOT_INIT)
                    do_perso = true;
                else
                    do_perso = false;
                
                break;
            case ESP_ERR_NVS_NOT_FOUND:
                ESP_LOGI(TAG,"The value is not initialized yet!\n");
                state = NOT_INIT;
                err = nvs_set_u8(gPERSO, "status", state);    
                do_perso = true;
                break;
            default :
                ESP_LOGI(TAG,"Error (%s) reading!\n", esp_err_to_name(err));
                while(1)
                {
                  ESP_LOGI(TAG, "\n############ ERROR DURING read status PERSO STOP WORKING \n");
                }
    }


// Dummy to run everytime into the Perso routing

    do_perso = true;

// Dummy to run everytime into the Perso routing    

    if (do_perso == true)
    {
        ret = DeviceID_open();
        if( ret == DEVID_OK)
        {
            ESP_LOGI(TAG, "\n==================================================================\n");
            ret = DeviceID_genKey(p_init_seed);
            if( ret == DEVID_OK)
            {
                ESP_LOGI(TAG, "\n==================================================================\n");
                wifi_connection();
                start_webserver();
                while(1);
                DeviceID_close();
            }
        }
        if (ret != DEVID_OK)
        {
            while(1)
            {
              ESP_LOGI(TAG, "\n############ ERROR DURING  PERSO STOP WORKING \n");
            }
        
        }
    }

}
