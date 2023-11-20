///
//  DeviceID.c
//  Support the DeviveID within the ESP32. Within this package 
//  all relevant function arround the DeviceID for are covers. 
//  The Default X.509 Credentials could be configured with in the 
//  menuconfig tool. 
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
// License.    #include "freertos/FreeRTOS.h"


#define LOG_LOCAL_LEVEL ESP_LOG_INFO
#include "DeviceID.h"
#include "TrustPlatform.h"
#include "esp_vfs.h"
#include "esp_spiffs.h"


#include "esp_heap_caps.h"


static const char *TAG = "DeviceID";

/***********      Global type definitio       ************/

struct options
{
    int type;                     /* the type of key to generate          */
    int rsa_keysize;              /* length of key in bits                */
    char *subject_name;           /* subject name for certificate request */
    char *subjectalt_name;        /* subject alternative name */
    int format;                   /* the output format to use             */
    mbedtls_md_type_t md_alg;     /* Hash algorithm used for signature.   */
} gOpt;



// global crypto Context var 
mbedtls_pk_context pk;
mbedtls_entropy_context gEntropy;
mbedtls_ctr_drbg_context gCtr_drbg;
mbedtls_mpi N,P,Q,D,E,DP,DQ,QP;
mbedtls_x509write_csr req;

bool gDevIDopen = false;
bool gDevIDKeyGen = false;



//
//      DeviceID_open()
//      inital the Device itself, prepair the crypt Context , fetches the default paramter the structure. 
//      @return:    success: DEVID_OK
//                  failure: Error
//
//int DeviceID_open(void)
int DeviceID_open(void)
{
 
  int ret = DEVID_FAIL;
  ESP_LOGI(TAG,"init TrustStore Context.......");
  if(TPinit() != TP_OK)
  {
      ret = DEVID_ERR_INIT;
  }
  else
  {
    
    ESP_LOGI(TAG,"init DevID Context.......");
    mbedtls_ctr_drbg_init(&gCtr_drbg);
    mbedtls_pk_init(&pk);
    mbedtls_mpi_init(&N);mbedtls_mpi_init(&P);mbedtls_mpi_init(&Q);
    mbedtls_mpi_init(&D);mbedtls_mpi_init(&E);mbedtls_mpi_init(&DP);
    mbedtls_mpi_init(&DQ);mbedtls_mpi_init(&QP);
    mbedtls_x509write_csr_init(&req);
    gOpt.type = DEVID_TYPE;
    gOpt.rsa_keysize = DEVID_RSA_KEYSIZE;
    gOpt.format = DEVID_FORMAT;
    gOpt.md_alg = DEVID_MD_ALG;
    gOpt.subject_name = DEVID_SUBJECT_NAME;
    gDevIDopen = true;
    ret = DEVID_OK;
  }
  return ret;
}
//
//      DeviceID_close()
//      close the Device ID conext and free all resources
//      @return:    success: DEVID_OK
//                  failure: Error
//
int DeviceID_close(void)
{
  int ret = DEVID_FAIL;

  if(gDevIDKeyGen)
  {
    mbedtls_entropy_free(&gEntropy);
    gDevIDKeyGen = false;
  }
  if(gDevIDopen)
  {
    mbedtls_x509write_csr_free(&req);
    mbedtls_pk_free(&pk);
    mbedtls_ctr_drbg_free(&gCtr_drbg);
    gDevIDopen = false;
  }
  
  ret = DEVID_OK;
  return ret;
}



//
//      DeviceID_genKey()
//      seed the Random Number generator. 
//      Generate Key Pair and store it to the TrustStore
//      @param  - [Input] p_seed = the seed for the Random 
//
//      @return:    success: DEVID_OK
//                  failure: error Message
//
int DeviceID_genKey(char* p_seed)
{
  int ret = DEVID_FAIL;
  char filename[] = DEVID_KEY_FILENAME;
  

  
  ESP_LOGI(TAG,"Seeding the random number generator...      Watermark: %d bytes", uxTaskGetStackHighWaterMark(NULL));
  mbedtls_entropy_init(&gEntropy);
  gDevIDKeyGen = true;
  if ((ret = mbedtls_ctr_drbg_seed(&gCtr_drbg, mbedtls_entropy_func, &gEntropy,
                                     (const unsigned char *) p_seed,
                                     strlen(p_seed))) != 0) 
  {
       ESP_LOGE(TAG, " failed\n  !  mbedtls_ctr_drbg_seed returned %d", ret);
      ret = DEVID_ERR_KEYGEN;
  }
  else
  {
     ESP_LOGI(TAG,"Generating the keypair  ...                 Watermark: %d bytes", uxTaskGetStackHighWaterMark(NULL));
    if ((ret = mbedtls_pk_setup(&pk, mbedtls_pk_info_from_type((mbedtls_pk_type_t) gOpt.type))) != 0) 
    {
      ESP_LOGE(TAG," failed\n  !  mbedtls_pk_setup returned -0x%04x", (unsigned int) -ret);
      ret = DEVID_ERR_KEYGEN;
    }
    else
    {
      ret = mbedtls_rsa_gen_key(mbedtls_pk_rsa(pk),mbedtls_ctr_drbg_random,&gCtr_drbg,gOpt.rsa_keysize,DEVID_EXPONENT);
      if (ret != 0)
      {
        ESP_LOGE(TAG," failed  !  mbedtls_rsa_gen_key returned -0x%04x", (unsigned int) -ret);
        ret = DEVID_ERR_KEYGEN;
      }
      else
      {
        unsigned char *output_buf = (unsigned char *)malloc(sizeof(unsigned char) * 16000);  
        memset(output_buf,0x00,16000);
        if ((ret = mbedtls_pk_write_key_pem(&pk, output_buf, 16000)) != 0) 
        {
          ESP_LOGE(TAG," failed  !  mbedtls_pk_write_key_pem returned -0x%04x", (unsigned int) -ret);
          ret = DEVID_ERR_KEYGEN;
        }
        else
        {
          size_t len = 0;

          len = strlen((char*)output_buf);
          if(TPwrite(filename,output_buf,len) != TP_OK)
          {
            ESP_LOGI(TAG,"Error write file : ");  
            ret = DEVID_ERR_KEYGEN;
          }
          else
          {
            ESP_LOGI(TAG,"Keyfile saved .....  ");  
            ret = DEVID_OK;
          }
        }
        free(output_buf);
      }
    }
  }
  return ret;
}
  
//
//      DeviceID_genCSR()
//      generate the DevID CSR 
//      Default value for Algorithm, key usage, are set in header file
//      @param  - [Input] p_csrbuf  = pointer to the buffer where the csr is stored
//      @param  - [Input] csrbuflen = size of of the csrbuffer    
//      @param  - [Input] p_subname = the subject name buffer string
//      @param  - [Input] p_subaltname = the subject alternative name buffer
//      @return:    success: DEVID_OK
//                  failure: error Message
//


int DeviceID_genCSR(unsigned char* p_csrbuf, uint16_t csrbuflen, char* p_subname, char* p_subaltname)
{
  int ret = DEVID_FAIL;
  char filename[] = DEVID_KEY_FILENAME;
  mbedtls_pk_context tmp;
  unsigned char *output_buf = (unsigned char *)malloc(sizeof(unsigned char) * 16000);  

  
  ESP_LOGI(TAG,"generate CSR........                        Watermark: %d bytes", uxTaskGetStackHighWaterMark(NULL));
  gOpt.type = DEVID_TYPE;
  gOpt.rsa_keysize = DEVID_RSA_KEYSIZE;
  gOpt.format = DEVID_FORMAT;
  gOpt.md_alg = DEVID_MD_ALG;
  //gOpt.subject_name = DEVID_SUBJECT_NAME;
  gOpt.subject_name = p_subname;

  gOpt.subjectalt_name = p_subaltname;
  ESP_LOGI(TAG, "output_buf len: %d",sizeof(output_buf));
  memset(output_buf,0x00,16000);
  uint16_t buflen = 16000;

  if (TPread(filename,output_buf,&buflen) == TP_OK)
  {
    mbedtls_pk_init(&tmp);   
    ret = mbedtls_pk_parse_key(&tmp,output_buf,buflen+1,NULL,0,mbedtls_ctr_drbg_random, &gCtr_drbg);
    if (ret !=0)
    {
       ESP_LOGI(TAG,"faild to parse keyfile: -0x%04x\n",(unsigned int) -ret);
       ret = DEVID_ERR_CSRGEN;            
    }
    else
    {
      ESP_LOGI(TAG,"Keyfile read ");             
      memset(output_buf,0x00,16000);
      mbedtls_x509write_csr_set_md_alg( &req, MBEDTLS_MD_SHA256);
      mbedtls_x509write_csr_set_key_usage( &req, MBEDTLS_X509_KU_DIGITAL_SIGNATURE );
      ESP_LOGI(TAG,"Set sub name:   %s",gOpt.subject_name);
      mbedtls_x509write_csr_set_subject_name(&req,gOpt.subject_name);
      mbedtls_x509write_csr_set_key( &req, &tmp );
      ESP_LOGI(TAG,"after mbedtls_x509write_csr_set_key...        Watermark: %d bytes", uxTaskGetStackHighWaterMark(NULL));
      int mbedResult;
      mbedResult = mbedtls_x509write_csr_pem(&req, p_csrbuf, csrbuflen, mbedtls_ctr_drbg_random, &gCtr_drbg);
      if ( mbedResult != 0){
          ESP_LOGE(TAG," failed\n  !  mbedtls_x509write_csr_pem returned -0x%04x", (unsigned int) -ret);
      }
      ESP_LOGI(TAG,"after mbedtls_x509write_csr_pem...            Watermark: %d bytes", uxTaskGetStackHighWaterMark(NULL));
    }
    mbedtls_pk_free(&tmp);   
  }
  else
  {
    ESP_LOGI(TAG,"Error read keyfile ");  
    ret = DEVID_ERR_CSRGEN;
  }
  
  free(output_buf);
  return ret;
}
        


//
//      DeviceID_storeCert()
//      store the DeviID in the TrustStore area 
//      @param  - [Input] p_devID  = pointer to the buffer for the DevID 
//      @param  - [Input] devIDlen = size of of the p_devID buffer    
//      @return:    success: DEVID_OK
//                  failure: error Message
//


int DeviceID_storeCert(unsigned char* p_devID, uint16_t devIDlen)
{
  int ret = DEVID_FAIL;
  char filename[] = DEVID_CERT_FILENAME;
  size_t olen = 0;

  unsigned char *output_buf = (unsigned char *)malloc(sizeof(unsigned char) * 16000);  
  memset(output_buf,0x00,16000);
  if ((ret = mbedtls_pem_write_buffer(DEVID_CERTHEADER,DEVID_CERTFOOTER,p_devID,devIDlen,output_buf,16000,&olen)) != 0)
  {
    ESP_LOGE(TAG," convert Cert failed\n  !  mbedtls_pem_write_buffer returned -0x%04x", (unsigned int) -ret);
    ret = DEVID_ERR_WRITE_CERT;
  }
  else
  {
    if(TPwrite(filename,output_buf,olen) != TP_OK)
    {
      ESP_LOGI(TAG,"Error write file : ");  
      ret = DEVID_ERR_KEYGEN;
    }
    else
    {
      
      ESP_LOGI(TAG,"DevID Stored Name: %s ",filename);
      ESP_LOGI(TAG,"DevID Stored Name: \n%s ",output_buf);
      ret = DEVID_OK;
    }
  }
  free(output_buf);
  return ret;
}




