int DeviceID_genKey(char* p_seed)
{
  int ret = DEVID_FAIL;
  char filename[] = "Keyfile01.key";

  
  ESP_LOGI(TAG,"Seeding the random number generator...      Watermark: %d bytes", uxTaskGetStackHighWaterMark(NULL));
  mbedtls_entropy_init(&gEntropy);
  gDevIDKeyGen = true;
  if ((ret = mbedtls_ctr_drbg_seed(&gCtr_drbg, mbedtls_entropy_func, &gEntropy,
                                     (const unsigned char *) p_seed,
                                     strlen(p_seed))) != 0) {
        ESP_LOGE(TAG, " failed\n  !  mbedtls_ctr_drbg_seed returned %d", ret);
  }
  else
  {
     ESP_LOGI(TAG,"Generating the keypair  ...                 Watermark: %d bytes", uxTaskGetStackHighWaterMark(NULL));
    if ((ret = mbedtls_pk_setup(&pk, mbedtls_pk_info_from_type((mbedtls_pk_type_t) gOpt.type))) != 0) 
    {
      ESP_LOGE(TAG," failed\n  !  mbedtls_pk_setup returned -0x%04x", (unsigned int) -ret);
    }
    else
    {
      ret = mbedtls_rsa_gen_key(mbedtls_pk_rsa(pk),mbedtls_ctr_drbg_random,&gCtr_drbg,gOpt.rsa_keysize,DEVID_EXPONENT);
      if (ret != 0)
      {
        ESP_LOGE(TAG," failed  !  mbedtls_rsa_gen_key returned -0x%04x", (unsigned int) -ret);
      }
      else
      {
        unsigned char *output_buf = (unsigned char *)malloc(sizeof(unsigned char) * 16000);  
        memset(output_buf,0x00,16000);
        if ((ret = mbedtls_pk_write_key_pem(&pk, output_buf, 16000)) != 0) 
        {
          ESP_LOGE(TAG," failed  !  mbedtls_pk_write_key_pem returned -0x%04x", (unsigned int) -ret);
        }
        else
        {
          char keyfile[]="/TP/DemoKey.key";
          unsigned char *c = output_buf;
          FILE *f;
          size_t len = 0;
          
          esp_vfs_spiffs_conf_t config = {
            .base_path = "/TP",
            .partition_label = "TrustStore",
            .max_files = 5,
            .format_if_mount_failed = true,
          };

          ret = esp_vfs_spiffs_register(&config);
          ESP_LOGI(TAG,"SPIFFS register :%d",ret);


          
          len = strlen((char*)output_buf);
          ESP_LOGI(TAG,"Keyfile len:%d",len);

          if ((f = fopen(keyfile, "w")) == NULL) 
          {
            ESP_LOGI(TAG,"ERROR open File:%s",keyfile);
            return ret;
          }
          ESP_LOGI(TAG,"write file: %d",len);
          if (fwrite(c, 1, len, f) != len) 
          {
              ESP_LOGI(TAG,"ERROR Keyfile len:%d",len);
              fclose(f);
              return ret;
          }

          fclose(f);

        
          mbedtls_pk_context tmp;
          mbedtls_pk_init(&tmp);   
          ret = mbedtls_pk_parse_keyfile(&tmp,keyfile,NULL,mbedtls_ctr_drbg_random, &gCtr_drbg);
          if (ret !=0)
          {
             ESP_LOGI(TAG,"faild to parse keyfile: -0x%04x\n",(unsigned int) -ret);            
          }
          else
          {
             ESP_LOGI(TAG,"SUCCESS: -0x%04x\n",(unsigned int) -ret);             
          }

          TPwrite(filename,output_buf,len+1);
          uint16_t buflen = len +1;
          TPread(filename,output_buf,&buflen);
          //ESP_LOGI(TAG,"Keyfile read :\n%s",output_buf);
          //esp_err_t TPwrite(char* p_filename, unsigned char* p_buffer, uint16_t len)
          //ESP_LOGI(TAG,"Keyfile:\n%s",output_buf);
          //ESP_LOGI(TAG,"Keyfile:\n%s",p_buffer);
          mbedtls_pk_free(&tmp);
          ret = DEVID_OK;
        }
        free(output_buf);
        
      }
    }
  }
  return ret;
}