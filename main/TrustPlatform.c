///
//  TrustPlattform.c
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


#include "TrustPlatform.h"


/***********      Global definitions       ************/

static const char *TAG = "TrustPlatform";

uint8_t gSYS_KEY[32];
bool gINT; 
mbedtls_aes_context aes;








/***********      Global definitions       ************/

// TrustPlatform Definiton 
tp_store_conf_t  _vTPstore = {
    false,
    {
        .base_path = TP_BASE_PATH, 
        .partition_label = TP_PARTITION_LABEL, 
        .max_files = TP_MAX_FILES, 
        .format_if_mount_failed = true
    },
};

/***********      Local function definitions       ************/

esp_err_t _derive_sys_Key(uint8_t* p_syskey)
{
    esp_err_t ret = TP_FAIL; 
    uint8_t base_mac_addr[10] = {0};
    
    ESP_LOGI(TAG, "generate System Master Key ");
    // Read default Factory MAC Address from efuse 

    ret = esp_read_mac(base_mac_addr, ESP_MAC_EFUSE_FACTORY);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to get base MAC address from EFUSE BLK0. (%s)", esp_err_to_name(ret));
        ESP_LOGE(TAG, "Aborting");
        abort();
    }
    // generate 256 bit AES Key 
    mbedtls_sha256(base_mac_addr, 6, p_syskey, 0);
    ret = TP_OK;
    return ret;
}


static void directoryTP(char* p_path)
{
    DIR* p_dir = opendir(p_path);
    assert(p_dir != NULL);
    while(true)
    {
        struct dirent* pe = readdir(p_dir);
        if (!pe) break;
        ESP_LOGI(TAG,"d_name=%s/%s d_ino=%d d_type=%x", p_path, pe->d_name,pe->d_ino, pe->d_type);
    }
    closedir(p_dir);
}


static void init_aes()
{
    mbedtls_aes_init(&aes);
    mbedtls_aes_setkey_enc(&aes,gSYS_KEY,256);
    mbedtls_aes_setkey_dec(&aes,gSYS_KEY,256);
}

static void deinit_aes()
{
    mbedtls_aes_free(&aes);
}

static size_t get_output_size(size_t input_size)
{
    if (input_size %16 == 0)
    {
        return input_size;
    } else {
        return (16* (input_size/16 +1));
    }
}

static int get_qtd_blocks(size_t input_size)
{
    if (input_size %16 == 0)
        return input_size/16;
    else
        return input_size /16 +1;
}

static void print_hex(unsigned char *buf, int len){
    for(int i = 0; i < len; i++){
        printf("%02x ", buf[i]);
    }
    printf("\n");
}



static void aes_encrypt(unsigned char* p_in, size_t input_size, unsigned char* p_out)
{
    unsigned char iv[16];
    int qtd_blocks = get_qtd_blocks(input_size);
    unsigned char input_block[16];
    unsigned char output_block[16];
    

    memset (iv,0,16);
    for (int i = 0; i < qtd_blocks; i++)
    {
        memset(input_block, 0, 16);
        memset(output_block, 0, 16);
        if (input_size -(i*16)<16)
        {
            memcpy(input_block,p_in + (i*16),input_size-(i*16));
        } else
        {
            memcpy(input_block,p_in + (i*16),16);
        }
        mbedtls_aes_crypt_cbc(&aes,MBEDTLS_AES_ENCRYPT,16,iv,input_block,output_block);
        memcpy(p_out +(i*16),output_block,16);
    }
}

static void aes_decrypt(unsigned char* p_in, size_t input_size, unsigned char* p_out)
{
    unsigned char iv[16];
    int qtd_blocks = get_qtd_blocks(input_size);
    unsigned char input_block[16];
    unsigned char output_block[16];    

    memset (iv,0,16);
    for (int i =0;i<qtd_blocks;i++)
    {
        memset(input_block, 0, 16);
        memset(output_block, 0, 16);
        if(input_size - (i*16)<16)
            memcpy(input_block,p_in +(i*16),input_size-(i*16));
        else
            memcpy(input_block,p_in +(i*16),16);

        mbedtls_aes_crypt_cbc(&aes,MBEDTLS_AES_DECRYPT,16,iv,input_block,output_block);
        memcpy(p_out +(i*16),output_block,16);
    }
}



//
//      TPinit()
//      inital init function. Device Master System Key. Initialize the Keystore
//      If no Keystore sturcture found foramt SPIFFS and setup the structure 
//      Read Index 
//      @param  - [Input] sessionToken = the GUID for the curl session             */
//      @param  - [Input] jodId = the platform GUID reference for the job          */
//      @param  - [Input] endpoint = the                                           */
//
//      @return:    success: TP_OK
//                  failure: error Message
//
esp_err_t TPinit(void)
{

    esp_err_t ret = TP_OK;
    size_t total = 0, used = 0;
    char KeyName[]= TP_MASTER_KEY_NAME;
    unsigned char input[]="-----BEGIN CERTIFICATE REQUEST-----MIICmjCCAYICAQAwNzEWMBQGA1UEAwwNd3JvdmVyLWRwcy05OTEQMA4GA1UECgwHRHljb2RlWDELMAkGA1UEBhMCSUQwggEiMA0GCSqGSIb3==================================================================ENDE"; 
    unsigned char output[400];

    ESP_LOGI(TAG, "\n==================================================================");
    ESP_LOGI(TAG, "Start TPinit");
    
    gINT = TP_NOT_INIT;
    // Start to register SPIFFS partition if not found format SPIFFS and generatate struct
    ret = esp_vfs_spiffs_register(&_vTPstore.tp_cnf);
    if (ret != TP_OK) 
    {
        ESP_LOGE(TAG, "Failed to initialize SPIFFS (%s)", esp_err_to_name(ret));
        ret = esp_spiffs_info(_vTPstore.tp_cnf.partition_label, &total, &used);
        if (ret != ESP_OK) {
            ESP_LOGE(TAG, "Failed to get SPIFFS partition information (%s). Formatting...", esp_err_to_name(ret));
            esp_spiffs_format(_vTPstore.tp_cnf.partition_label);
        } 
        esp_spiffs_info(_vTPstore.tp_cnf.partition_label, &total, &used);
        ESP_LOGD(TAG, "SPIFFS partition: Name: %s, free bytes: %d, used bytes: %d",_vTPstore.tp_cnf.partition_label,total,used);
    }
    esp_spiffs_info(_vTPstore.tp_cnf.partition_label, &total, &used);
    ESP_LOGI(TAG, "SPIFFS partition: Name: %s, free bytes: %zd, used bytes: %zd",_vTPstore.tp_cnf.partition_label,total,used);
    directoryTP(_vTPstore.tp_cnf.base_path);
    
    esp_spiffs_format(_vTPstore.tp_cnf.partition_label);
    ESP_LOGI(TAG, "SPIFFS partition after format: Name: %s, free bytes: %zd, used bytes: %zd",_vTPstore.tp_cnf.partition_label,total,used);
    directoryTP(_vTPstore.tp_cnf.base_path);
    
    // Derive Sytem AES Key and store in Temp Buffer
    // memset buffer
    ret =  _derive_sys_Key(gSYS_KEY);
    if (ret == TP_OK)
    {
        init_aes();
        gINT = TP_INIT;
    }
    ESP_LOGI(TAG, " TP init ready with status: %s; Code: %#04X",esp_err_to_name(ret), ret);
    return ret; 
}

//
//      TPreadKey()
//      read keyfile bei the given key name. 
//       
//      @param  - [Input] p_filename = the name of the file to be read
//      @param  - [Output] p_buffer = the buffer where the data hat to be written
//      @param  - [PutPut] p_len =  input : the max buffer size; Output =the lenght of the data in p_buffer                                         
//
//      @return:    success: TP_OK
//                  failure: error Message
//

esp_err_t TPread(char* p_filename, unsigned char* p_buffer, uint16_t* p_len)
{
    esp_err_t ret = TP_FAIL;
    char tmbuffer[30];
    size_t output_len = get_output_size(*p_len);
    unsigned char *p_readbuf;
    int filesize = 0;
  
    if (TP_INIT)
    {
        sprintf(tmbuffer,"%s/%s",TP_BASE_PATH,p_filename);
        ESP_LOGI (TAG," Read File:%s LEN buffer %d",tmbuffer, output_len);
        FILE* file = fopen(tmbuffer,"r");
        if (file == NULL)
        {
            ESP_LOGE(TAG,"File at given path does't exist: %s",tmbuffer);
            ret = TP_ERR_FILE_NOT_EXIST;
        } else
        {
            fseek(file, 0,SEEK_END);
            filesize = ftell(file);
            ESP_LOGI (TAG," Read File LEN:  %d",filesize);
            rewind(file);
            if (*p_len >= filesize)
            {
                p_readbuf = (unsigned char*)malloc(filesize);
                size_t readlen = fread(p_readbuf,1,filesize,file);
                if (ferror(file)!=0)
                {
                  ESP_LOGE(TAG,"Error reading file");
                  ret = TP_ERR_READ_FILE;
                } else
                {
                    aes_decrypt(p_readbuf,filesize,p_buffer);
                    *p_len = filesize;
                    ret = TP_OK;
                 
                }
                free(p_readbuf);
            }
            else
            {
                ret = TP_ERR_BUFFER_TO_SMALL;

            }        
            fclose(file);    
        }
    }
    return(ret);
}


//
//      TPwrite()
//      write file bei the given name. 
//      - if file doesn't exist the will be generated. 
//      - if file exist file will be overwritten       
//
//      @param  - [Input] p_filename = the name of the file to be written
//      @param  - [Output] p_buffer = the buffer to be written to the file
//      @param  - [PutPut] p_len = the lenght of the data in p_buffer                                           
//
//      @return:    success: TP_OK
//                  failure: error Message
//

esp_err_t TPwrite(char* p_filename, unsigned char* p_buffer, uint16_t len)
{
    esp_err_t ret = TP_FAIL;
    char tmbuffer[30];
    size_t output_len = get_output_size(len);
    unsigned char *p_writebuf;
    
  
    if (TP_INIT)
    {
     
        p_writebuf = (unsigned char*)malloc(output_len);
        sprintf(tmbuffer,"%s/%s",TP_BASE_PATH,p_filename);
        aes_encrypt(p_buffer,len,p_writebuf);
        ESP_LOGI (TAG," Write File:%s LEN buffer %d",tmbuffer, output_len);
        FILE* file = fopen(tmbuffer,"w");
        if (file == NULL)
        {
            ESP_LOGE(TAG,"Failed to open file for writing: %s",tmbuffer);
            ret = TP_ERR_COULD_NOT_OPEN_FILE;
        } else
        {   
            fwrite(p_writebuf,output_len,1,file);
            fclose(file);
            ret = TP_OK;
        }
        free(p_writebuf);
    }
    return(ret);
}







