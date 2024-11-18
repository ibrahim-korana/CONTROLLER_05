

void dali_callback(package_t *data, backword_t *backword)
{
    printf("Dali GELEN << Typ=%d Err=%d %02x %02X %02x\n",data->data.type,data->data.error,
    data->data.data0,data->data.data1,data->data.data2 );
}


uint8_t find_addr(void)
{
    gear_t kk={};
    for (int i=0;i<64;i++)
    {
        disk.read_file(GEAR_FILE,&kk,sizeof(gear_t),i);
        if (kk.short_addr>64) return i;
    }  
    return 200;
}

void adres_type(uint8_t adr, uint8_t gurup,address_t *bb)
{
   if (adr!=255) bb->short_adr = true;
   if (adr==255) bb->broadcast_adr = true;
   if (gurup!=0) {
    bb->group_adr=true;
    bb->broadcast_adr = false;
    bb->short_adr=false;
    bb->data = adr-1;
   } else bb->data = adr;
}

//-----------------------------------------
/*
    Dali networkünü tarayarak Otomatik adresleme yapar
*/
package_t special_command(uint8_t com, uint8_t dat)
{
    package_t pk = {};
    address_t bb = {};  
    bb.special = true;
    bb.data = com;
    pk.data.data0 = package_address(bb); //Special Broadcast
    pk.data.data1 = dat; //Initialize
    pk.data.type = BLOCK_16;
    return pk;
}

typedef enum {
    SND_H = 0x00,
    SND_M ,
    SND_L ,
    SND_A ,
} send_nible_t;

void double_send(package_t *pk)
{
    /*
      Konfiguresyon komutları ilk komutun bitiminden sonra 
      100ms içinde ikinci kez gönderilmelidir
    */
    dali.send(pk);
    vTaskDelay(50 / portTICK_PERIOD_MS);
    dali.send(pk);
    vTaskDelay(50 / portTICK_PERIOD_MS);
}


backword_tt send_compare(uint8_t h, uint8_t m, uint8_t l, send_nible_t snd)
{
    static int retry = 0;
    package_t pk;
     BEKLE
     //ESP_LOGI(TAG, "           Ara %02x%02x%02x",h,m,l);
    if (snd==SND_A || snd==SND_H)
    {
        BEKLE 
        pk = special_command(0x08,h);
        dali.send(&pk);
    }

    if (snd==SND_A || snd==SND_M)
    {
        BEKLE 
        pk = special_command(0x09,m);
        dali.send(&pk);
    }

    if (snd==SND_A || snd==SND_L)
    {
        BEKLE 
        pk = special_command(0x0A,l);
        dali.send(&pk);
    }

    BEKLE
    pk = special_command(0x04,0x00);
    backword_tt rr = dali.send_receive(&pk);
    if (rr==BACK_TIMEOUT)  rr=BACK_NO;
    if (rr==BACK_YES || rr==BACK_NO) {retry=0; return rr;}
    if (rr==BACK_TIMEOUT && snd==SND_A) {retry=0;return rr;}
    printf("BCK ERROR %02x\n",(int)rr);
    //if (rr==BACK_NONE) rr=BACK_NO;
    vTaskDelay(10 / portTICK_PERIOD_MS);
    if (++retry>5) {retry=0;return BACK_TIMEOUT;}
    return send_compare(h,m,l,snd);
}


uint8_t q_upH0(uint8_t *h,uint8_t *m,uint8_t *l, bool H, send_nible_t snd)
{
    uint8_t sayi=0x00;
    if (snd==SND_H) sayi = *h;
    if (snd==SND_M) sayi = *m;
    if (snd==SND_L) sayi = *l;

    uint8_t ust=sayi>>4, alt= sayi&0x0f ,count=3;
    backword_tt rr=BACK_BUSY;  
    uint8_t ek = 0x00;
    do {  
       ek = ek | (1<<count); 
       if (H) sayi = (ek<<4) | alt; 
       if (!H) sayi = ek | (ust<<4); 
       //printf("sayi=%02X\n",sayi);
       if (snd==SND_H) rr = send_compare(sayi,*m,*l, snd);
       if (snd==SND_M) rr = send_compare(*h,sayi,*l, snd);
       if (snd==SND_L) rr = send_compare(*h,*m,sayi, snd);
       if (rr==BACK_YES) {
            uint8_t ek1 = ek - (1<<(count+1));
            //printf("count=%d ek=%02X ek1=%02x\n",count, ek,ek1);
            uint8_t ss = 0;//, old_sayi = sayi;
            while (ek>=ek1)
            {
               // printf("        ARA sayi =%02X ss=%d OLD SAYI=%02X \n",sayi,ss,old_sayi);
                if (snd==SND_H) rr = send_compare(sayi,*m,*l, snd);
                if (snd==SND_M) rr = send_compare(*h,sayi,*l, snd);
                if (snd==SND_L) rr = send_compare(*h,*m,sayi, snd);
                if (rr==BACK_NO) {
                                //if (old_sayi==0xFF && sayi==0xEF) return 0xFF; 
                                //if (ss==0 || ss==1) return sayi; else return old_sayi;
                                return sayi;
                                 }
                ek--;
                ss++; 
                //old_sayi = sayi;                
                if (H) sayi = (ek<<4) | alt; 
                if (!H) sayi = ek | (ust<<4);                  
            }
           // printf("UP BULAMADI\n");
            if (H) return 0xF | alt; 
            if (!H) return (ust<<4) | 0x0F;   
                         }
       count--;                  
    } while(count<4);
    //printf("UP ANA While BULAMADI ust=%02x alt=%02x\n",ust,alt);
    if (H) return 0xF0 | alt; 
    if (!H) return (ust<<4) | 0x0F; 
    return 0xFF; 
}


uint8_t q_downH0(uint8_t *h,uint8_t *m,uint8_t *l, bool H, send_nible_t snd)
{
    uint8_t sayi=0x00;
    if (snd==SND_H) sayi = *h;
    if (snd==SND_M) sayi = *m;
    if (snd==SND_L) sayi = *l;
    uint8_t ust=sayi>>4, alt= sayi&0x0f ,count=3;
    //uint8_t ekle = 0x8; 
    backword_tt rr=BACK_BUSY;  
    uint8_t ek = 0x00;
    do {  
        ek = (1<<count); 
        if (H) sayi = (ek<<4) | alt; 
        if (!H) sayi = ek | (ust<<4); 
       // printf("DUS sayi=%02X\n",sayi);
        if (snd==SND_H) rr = send_compare(sayi,*m,*l, snd);
        if (snd==SND_M) rr = send_compare(*h,sayi,*l, snd);
        if (snd==SND_L) rr = send_compare(*h,*m,sayi, snd);
        if (rr==BACK_NO) {
            uint8_t ek1 = (1<<(count+1));
           // printf("count=%d ek=%02X ek1=%02x\n",count, ek,ek1);
            uint8_t ss = 0, old_sayi = sayi;
            while (ek<=ek1)
            {
               // printf("        ARA sayi =%02X ss=%d OLD SAYI=%02X \n",sayi,ss,old_sayi);
                if (snd==SND_H) rr = send_compare(sayi,*m,*l, snd);
                if (snd==SND_M) rr = send_compare(*h,sayi,*l, snd);
                if (snd==SND_L) rr = send_compare(*h,*m,sayi, snd);
                if (rr==BACK_YES) {
                                if (ss==0) return sayi; else return old_sayi;
                                return sayi;
                                 }
                ek++;
                ss++; 
                old_sayi = sayi;                
                if (H) sayi = (ek<<4) | alt; 
                if (!H) sayi = ek | (ust<<4); 
            }
           //printf("Down BULAMADI\n");
            if (H) return 0x00 | alt; 
            if (!H) return (ust<<4) | 0x00; 
                         }
        count--;                  
    } while(count<4);
    //printf("Down ANA While BULAMADI ust=%02x alt=%02x\n",ust,alt);
    if (H) return 0x00 | alt; 
    if (!H) return (ust<<4) | 0x00; 
    return 0x00;
}

uint8_t find_block(uint8_t *h,uint8_t *m,uint8_t *l,send_nible_t snd)
{
  if (snd==SND_H) *h=0x80;
  if (snd==SND_M) *m=0x80;
  if (snd==SND_L) *l=0x80; 

  backword_tt rr = send_compare(*h,*m,*l,SND_A);
  uint8_t sayi;
  if (rr==BACK_NO) {
     sayi=q_upH0(h,m,l, true, snd);
  } else {
     sayi=q_downH0(h,m,l, true,snd);
  }

  if (snd==SND_H) {*h=sayi; *h=*h|0x08;}
  if (snd==SND_M) {*m=sayi; *m=*m|0x08;}
  if (snd==SND_L) {*l=sayi; *l=*l|0x08;}
  printf("%01X",sayi>>4);
  fflush(stdout);
  rr = send_compare(*h,*m,*l,SND_A);
  if (rr==BACK_NO) {
     sayi=q_upH0(h,m,l, false, snd);
  } else {
     sayi=q_downH0(h,m,l, false, snd);
  }
  if (snd==SND_H) {*h=sayi;}
  if (snd==SND_M) {*m=sayi;}
  if (snd==SND_L) {*l=sayi;}
  printf("%01X",sayi&0x0F);
  fflush(stdout);
  return sayi;  
}


uint8_t find_min_number(uint8_t *h,uint8_t *m,uint8_t *l)
{
  find_block(h,m,l,SND_H);
  find_block(h,m,l,SND_M);
  find_block(h,m,l,SND_L);
  return 0xFF;  
}

backword_tt AtamaVeKontrol(uint8_t ats)
{
    //Atama yapılıyor
    package_t pk = special_command(0x0B,ats);
    dali.send(&pk);
    vTaskDelay(50 / portTICK_PERIOD_MS);
    dali.send(&pk);
    vTaskDelay(50 / portTICK_PERIOD_MS);
    pk = special_command(0x0C,ats);
    backword_tt rr=dali.send_receive(&pk);
    if (rr!=BACK_YES) rr=dali.send_receive(&pk);
    if (rr!=BACK_YES) ESP_LOGE(TAG,"Atama Yapıldı Kontrol BAŞARISIZ"); else ESP_LOGW(TAG,"Atama Yapıldı Kontrol BAŞARILI");
    return rr;
}

uint8_t AtamaYAP(uint8_t h, uint8_t m, uint8_t l)
{
    uint8_t abc = find_addr();
    if (abc>64) {
        ESP_LOGE(TAG,"GEAR FILE ERROR or Dali devive LIMIT ERROR");
        return 0x71;
    }   
    //Bulunan random adres h,m,l dedir. abc içinde atanabilecek
    //ilk boş adres var.  
    uint8_t ats = (abc<<1)|1;
    ESP_LOGI(TAG,"      %02X%02X%02X >> %d Atanacak",h,m,l,abc);
    backword_tt rr = AtamaVeKontrol(ats);
    if (rr!=BACK_YES) rr= AtamaVeKontrol(ats);
    if (rr==BACK_YES) {                    
        cJSON *root = cJSON_CreateObject();
        cJSON_AddStringToObject(root, "com", "init");
        cJSON_AddNumberToObject(root, "status", 1);
        cJSON_AddNumberToObject(root, "cihaz", abc);
        char *dat = cJSON_PrintUnformatted(root);                   
        tcpserver.Send(dat);
        cJSON_free(dat);
        cJSON_Delete(root);                   
        ESP_LOGI(TAG,"CIHAZ %02d ile numaralandı",abc);
        ESP_LOGI(TAG,"%02d Nolu Cihaz Devreden Çıkarılıyor",abc);
        
        char *mm=(char *)calloc(1,20);
        sprintf(mm,"Dev:%02d",abc);
        display_write(2,mm);
        free(mm);
        gear_t kk = {};
        kk.short_addr = abc;
        kk.type = 0x06;
        disk.write_file(GEAR_FILE,&kk,sizeof(gear_t),abc);
        vTaskDelay(50 / portTICK_PERIOD_MS);
        package_t pk = special_command(0x05,0x00);
        dali.send(&pk);
        vTaskDelay(100 / portTICK_PERIOD_MS);    
        return 0xFF;                       
    }
   return 0x00; 
}

uint8_t SonrakiniATA(uint8_t *h, uint8_t *m, uint8_t *l)
{
    uint8_t ret=0x00;
    if (*l==0xFF) {*m=*m+1;*l=0;} else *l=*l+1;
    vTaskDelay(50 / portTICK_PERIOD_MS);
    backword_tt rr = send_compare(*h,*m,*l,SND_A);
    if (rr==BACK_YES) 
      {
        vTaskDelay(50 / portTICK_PERIOD_MS);
        ret = AtamaYAP(*h,*m,*l);
        vTaskDelay(100 / portTICK_PERIOD_MS);
      }  
    return ret;
}

void Search_Device(uint8_t dev)
{
    ESP_LOGW(TAG,"CIHAZ ARAMA BASLATILIYOR");
    display_write(2,"ARANIYOR");
    /*Terminate*/
    ESP_LOGI(TAG,"     Durdurma komutu gidiyor");
    package_t pk = special_command(0x00,0x00);
    double_send(&pk);
    

    /*
       A5 Initialize komutunu gönderir
       Special command 2
       300ms aralıkla 2 kez gider
    */
        if (dev==0x00) {ESP_LOGI(TAG,"     Tüm cihazlara initialize gönderiliyor");GlobalConfig.atama_sirasi=1;}
        if (dev==0xFF) ESP_LOGI(TAG,"     Kısa adresi olmayan cihazlara initialize gönderiliyor");

    pk = special_command(0x02,dev);
    double_send(&pk);

    /*
       A7 Randomize komutunu gönderir
       Special command 3
    */

    ESP_LOGI(TAG,"     Randomize komutu gidiyor");  
    pk = special_command(0x03,0x00);
    double_send(&pk);
    vTaskDelay(20 / portTICK_PERIOD_MS);
    
    bool ara = true;
    while(ara)
    {
        uint8_t h=0xff, m=0xff, l=0xff;
        backword_tt rr = send_compare(h,m,l,SND_A);
        if (rr==BACK_NO || rr==BACK_TIMEOUT) ara=false;
        if (!ara) {
            vTaskDelay(100 / portTICK_PERIOD_MS);
            rr = send_compare(h,m,l,SND_A);
            if (rr==BACK_NO || rr==BACK_TIMEOUT) ara=false;
        }

          if (ara)
          {
            h=0x00; m=0x00; l=0x00; 
            BEKLE  
            find_min_number(&h,&m,&l); //Hattaki en küçük numaranın 1 eksigini bulur
            l++;
            rr = send_compare(h,m,l,SND_A);
            if (rr==BACK_YES) {

                vTaskDelay(20 / portTICK_PERIOD_MS);
                if (AtamaYAP(h,m,l)==0xFF) 
                {
                    if (SonrakiniATA(&h,&m,&l)==0xFF)
                    {
                       if (SonrakiniATA(&h,&m,&l)==0xFF) {
                          SonrakiniATA(&h,&m,&l); 
                       }; 
                    }; 
                } 
                            }
          }
    }
   
    vTaskDelay(10 / portTICK_PERIOD_MS);
    /*
       AB Exit Initialize
       Special command 5
    */ 
    ESP_LOGW(TAG,"CIHAZ ARAMA SONLANDIRILDI");
    pk = special_command(0x00,0x00);
    double_send(&pk); 

    display_write(2,"ARAMA BITTI");

   // ESP_LOGW(TAG,"%d Kayıtlı Adet cihaz bulunmaktadır.", GlobalConfig.atama_sirasi-1);
}



//-----------------------------------------
/*
    Dali networkünü tarayarak cevap veren cihazların 
    short adreslerini ve tiplerini bulur. 
    Aktif cihazlar diske kaydedilerek info da bilgi olarak gönderilir.
*/

void Device_Control_cable(bool kayit)
{
    ESP_LOGW(TAG,"Dali Kablolu Networkde Arama Baslatiliyor");
    uint8_t count =0;

    for (int i=0;i<64;i++)
    {
        package_t pk = {};
        address_t bb = {};  
        bb.short_adr = true;
        bb.arc_power = false;
        bb.data = i;
        pk.data.data0 = package_address(bb); //Special Broadcast
        pk.data.data1 = 0x99; //query device type
        pk.data.type = BLOCK_16;
        backword_t kk = dali.ysend_receive(&pk);
        if (kk.backword!=BACK_TIMEOUT) {
            
            gear_t zz = {};
            zz.short_addr = i;
            zz.type = kk.data;

            pk.data.data1 = 0xA7; //query device type   
            backword_t kk0 = dali.ysend_receive(&pk);
            if (kk0.backword!=BACK_TIMEOUT) {
                zz.ext_type = kk0.data;
            } else zz.ext_type = kk.data; 
 
            count++;
            
            if (kayit) disk.write_file(GEAR_FILE,&zz,sizeof(gear_t),i);
            char *mm;
            asprintf(&mm,"CDev:%2d [%02d-%02X]",count,zz.short_addr,zz.type);
            display_write(2,mm);
            free(mm);
            asprintf(&mm,"CDev:%2d [%02d-%02X:%02X]",count,zz.short_addr,zz.type,zz.ext_type);
            ESP_LOGI(TAG,"%s",mm); 
            free(mm);
            
            
            cJSON *root = cJSON_CreateObject();
            cJSON_AddStringToObject(root, "com", "search");
            cJSON_AddNumberToObject(root, "status", 2);
            cJSON_AddNumberToObject(root, "id", zz.short_addr);
            cJSON_AddNumberToObject(root, "type", zz.type);
            cJSON_AddNumberToObject(root, "ext_type", zz.ext_type);
            char *dat = cJSON_PrintUnformatted(root);
            tcpserver.Send(dat);
            cJSON_free(dat);
            cJSON_Delete(root);

        }
        // vTaskDelay(10 / portTICK_PERIOD_MS);       
    }

    ESP_LOGW(TAG,"Kablolu network taraması TAMAMLANDI");
    gpio_set_level(LED2,0);
    ESP_LOGI(TAG,"Dali Network Arama Sonlandirildı");
    char *mm = (char *)calloc(1,20);
    sprintf(mm,"Total Device:%d",count);
    ESP_LOGI(TAG,"%s",mm); 
    display_write(2,mm);
    free(mm);
    ESP_LOGI(TAG,"%d kayıtlı cihaz bulundu.",count);
}


void Device_Control_Wireless(bool kayit)
{
    uint8_t count =0;
    ESP_LOGW(TAG,"Dali Kablosuz Networkde Arama Baslatiliyor");
    for (int i=0;i<64;i++)
    {
        gpio_set_level(LED2,1);
        package_t pk = {};
        address_t bb = {};  
        bb.short_adr = true;
        bb.arc_power = false;
        bb.data = i;
        pk.data.data0 = package_address(bb); 
        pk.data.data1 = 0x99; //query device type
        pk.data.type = BLOCK_16;

        uint8_t kk = udpserver.send_and_receive_dali(BroadcastAdr,  &pk, true);
        if (kk==0x71) {kk=0;}
        if (kk>0) {
            count++;
            gear_t zz = {};
            zz.short_addr = i;
            zz.type = kk;

            pk.data.data1 = 0xA7; //query ext device type  
            uint8_t kk0 = udpserver.send_and_receive_dali(BroadcastAdr,  &pk, true);
            if (kk0!=0x71) {
                zz.ext_type = kk0;
            } else zz.ext_type = zz.type;

            if (kayit) disk.write_file(GEAR_FILE,&zz,sizeof(gear_t),i);
            char *mm ;
            asprintf(&mm,"WDev:%2d [%02d-%02X]",count,zz.short_addr,zz.type);
            display_write(2,mm);
            free(mm);
            asprintf(&mm,"WDev:%2d [%02d-%02X:%02X]",count,zz.short_addr,zz.type,zz.ext_type);
            ESP_LOGI(TAG,"%s",mm); 
            free(mm);
            
            cJSON *root = cJSON_CreateObject();
            cJSON_AddStringToObject(root, "com", "search");
            cJSON_AddNumberToObject(root, "status", 2);
            cJSON_AddNumberToObject(root, "id", zz.short_addr);
            cJSON_AddNumberToObject(root, "type", zz.type);
            cJSON_AddNumberToObject(root, "ext_type", zz.ext_type);
            char *dat = cJSON_PrintUnformatted(root);
            tcpserver.Send(dat);
            cJSON_free(dat);
            cJSON_Delete(root);
            gpio_set_level(LED2,0);

            vTaskDelay(300 / portTICK_PERIOD_MS);
        }
        gpio_set_level(LED2,0);
        vTaskDelay(100 / portTICK_PERIOD_MS);
    }
    gpio_set_level(LED2,0);
    ESP_LOGI(TAG,"Dali Network Arama Sonlandirildı");
    char *mm = (char *)calloc(1,20);
    sprintf(mm,"Total Device:%d",count);
    ESP_LOGI(TAG,"%s",mm); 
    display_write(2,mm);
    free(mm);

    ESP_LOGI(TAG,"%d kayıtlı cihaz bulundu.",count);
}

//-----------------------------------------
/*
    Belirtilen lamba üzerinde data ile gönderilen 
    query komutunu çalıştırarak sonucu döndürür.
*/
backword_t get_rapor(uint8_t adr, uint8_t data)
{
    printf("adres/data0 %02X %02x\n",adr,data);
    package_t pk = {};
    address_t bb = {};  
    bb.short_adr = true;
    bb.arc_power = false;
    bb.data = adr;
    pk.data.data0 = package_address(bb); 

    pk.data.data1 = 0x21;
    pk.data.type = BLOCK_16;
    double_send(&pk);
    BEKLE

    pk.data.data1 = data;
    pk.data.type = BLOCK_16;

    printf("adres/data1 %02X %02x\n",pk.data.data0,pk.data.data1);

    backword_t kk = dali.ysend_receive(&pk);
    printf("Rapor0 %02X %02x\n",adr,kk.data);
    
    return kk;
}

void DaliSend(package_t *pk, uint8_t type)
{
    if (type==0x99)
    {
        //Hepsi
        dali.busy_is_wait();
        dali.send(pk);
        if (!udpserver.send_dali(BroadcastAdr,pk)) 
          if (!udpserver.send_dali(BroadcastAdr,pk))
            if (!udpserver.send_dali(BroadcastAdr,pk)) ESP_LOGE(TAG,"Wireless paket GÖNDERİLEMEDİ");
        return;
    }
    if (type<0X0A)
    {
        //Kablolu
        dali.busy_is_wait();
        dali.send(pk);
    } else {
        //Kablosuz
        if (!udpserver.send_dali(BroadcastAdr,pk))
          if (!udpserver.send_dali(BroadcastAdr,pk)) ESP_LOGE(TAG,"Wireless paket GÖNDERİLEMEDİ");
    }
}

void dali_double_send(package_t *pk, uint8_t type)
{
    DaliSend(pk,type);
    vTaskDelay(50 / portTICK_PERIOD_MS);
    DaliSend(pk,type);
    vTaskDelay(50 / portTICK_PERIOD_MS);
}


backword_t DaliSendReceive(package_t *pk, uint8_t type)
{
    backword_t kk={};
    if(type<0x0A)
    {
        dali.busy_is_wait();
        kk = dali.ysend_receive(pk);
    } else {
        uint8_t vv = udpserver.send_and_receive_dali(BroadcastAdr,pk);
        if (vv==0x71) {
            kk.backword=BACK_TIMEOUT;
            kk.data=0;
        } else {
            kk.backword=BACK_DATA;
            kk.data=vv;
        }
    }
    return kk;
}

//-----------------------------------------
/*
    Lambayı grp ile belirlenen guruba alır veya gurubu kaldırır.  
    wr=1 ise gurubu ekler wr=0 ise gurubu siler 
    Tanımlar bit bazlıdır. 
    grp 1-16 arasındadır.
*/
void set_group(uint8_t grp, uint8_t adr, uint8_t wr, uint8_t type)
{
    package_t pk = {};
    address_t bb = {};  
    bb.short_adr = true;
    bb.arc_power = false;
    bb.broadcast_adr = false;
    bb.group_adr = false;
    bb.special = false;
    bb.data = adr;
    pk.data.data0 = package_address(bb); 
    pk.data.type = BLOCK_16;

    if (wr==1) pk.data.data1 = 0x60+grp-1; //guruba dahil et
    if (wr==0) pk.data.data1 = 0x70+grp-1; //guruptan çıkar
   
    dali_double_send(&pk,type); 
}

//-----------------------------------------
/*
    Lambanın hangi gurupta olduğunu döndürür. 
    HL=0 veya 1 olabilir. 
    HL=0 ise 0-7
    HL=1 ise 8-15 nolu gurupları tanımlar. 
    Tanımlar bit bazlıdır. 
*/
backword_t get_group(uint8_t adr, uint8_t HL, uint8_t type)
{
    package_t pk = {};
    address_t bb = {};  
    bb.short_adr = true;
    bb.arc_power = false;
    bb.data = adr;
    pk.data.data0 = package_address(bb); 
    if (HL==0) pk.data.data1 = 0xC0; //1 gurup
    if (HL==1) pk.data.data1 = 0xC1; //2 gurup
    pk.data.type = BLOCK_16;
    backword_t kk = DaliSendReceive(&pk,type);

    ESP_LOGI(TAG,"%02X Gurup %d %02x ",adr,HL,kk.data);
    return kk;
}

backword_t get_level_command(uint8_t adr, uint8_t comm, uint8_t type)
{
    package_t pk = {};
    address_t bb = {};  
    bb.short_adr = true;
    bb.arc_power = false;
    bb.data = adr;
    pk.data.data0 = package_address(bb); 
    pk.data.data1 = comm;
    pk.data.type = BLOCK_16;
  
    backword_t kk = DaliSendReceive(&pk,type);
    ESP_LOGI(TAG,"%02X %02x [ %02X %d]",adr,comm, kk.data, kk.data);
    return kk;
}

//-----------------------------------------
/*
    adr ile belirtilen lamba/lar a komutu uygular
*/
void command_action(uint8_t adr, uint8_t gurup, uint8_t comm)
{
  //  printf("Komut adr=%d gurup=%d comm=%d\n",adr,gurup,comm);
    package_t pk = {};
    address_t bb = {};
    adres_type(adr,gurup, &bb);   
    bb.arc_power = false;
    pk.data.data0 = package_address(bb);
    pk.data.data1 = 0x00; //kapat 
    if (comm==1) pk.data.data1 = 0x0A; //Son Durum
    if (comm==2) pk.data.data1 = 0x06; //En Düşük
    if (comm==3) pk.data.data1 = 0x05; //En Yüksek
    if (comm==4) pk.data.data1 = 0x08; //Yükselt
    if (comm==5) pk.data.data1 = 0x07; //Azalt

    if (comm==6) pk.data.data1 = 0x10; //go to scene 1 
    if (comm==7) pk.data.data1 = 0x11; //go to scene 2
    if (comm==8) pk.data.data1 = 0x12; //go to scene 3
    if (comm==9) pk.data.data1 = 0x13; //go to scene 4

    if (comm==10) pk.data.data1 = 0x14; //go to scene 5
    if (comm==11) pk.data.data1 = 0x15; //go to scene 6
    if (comm==12) pk.data.data1 = 0x16; //go to scene 7
    if (comm==13) pk.data.data1 = 0x17; //go to scene 8

    pk.data.type = BLOCK_16;
    DaliSend(&pk,0x99);
}

//-----------------------------------------
/*
    Lambanın senaryo degerini yazar. 
*/
/*
    CONFIG KOMUTLARI 2 kez GONDERILIR
*/

void set_fade(uint8_t adr,uint8_t val, uint8_t comm, uint8_t type)
{
    package_t pk0;
    pk0 = special_command(0x01,val);
    dali_double_send(&pk0,type); 
    BEKLE

    package_t pk = {};
    address_t bb = {};  
    bb.short_adr = true;
    bb.arc_power = false;
    bb.data = adr;
    pk.data.data0 = package_address(bb); 
    if (comm==0) pk.data.data1 = 0x2F; //Fade Rate
    if (comm==1) pk.data.data1 = 0x2E; //Fade Time
    if (comm==2) pk.data.data1 = 0x30; //Extended Fade
    if (comm==3) pk.data.data1 = 0x2C; //system_failure_level
    if (comm==4) pk.data.data1 = 0x2D; //power_on_level
    if (comm==5) pk.data.data1 = 0x2A; //Max_level
    if (comm==6) pk.data.data1 = 0x2B; //Min_level
    pk.data.type = BLOCK_16;
    dali_double_send(&pk,type); 
}

void set_scene(uint8_t adr,uint8_t scn,uint8_t val, uint8_t type)
{
    package_t pk0;
    pk0 = special_command(0x01,val);
    DaliSend(&pk0,type);
    BEKLE
    DaliSend(&pk0,type);
    BEKLE
    
    package_t pk = {};
    address_t bb = {};  
    bb.short_adr = true;
    bb.arc_power = false;
    bb.data = adr;
    pk.data.data0 = package_address(bb); 
    pk.data.data1 = 0x40+scn; //scene adresi
    pk.data.type = BLOCK_16;
    dali_double_send(&pk,type); 
}

//-----------------------------------------
/*
    Lambanın senaryo degerini yazar. 
*/
/*
    CONFIG KOMUTLARI 2 kez GONDERILIR
*/
void clear_scene(uint8_t adr,uint8_t scn, uint8_t type)
{   
    package_t pk = {};
    address_t bb = {};  
    bb.short_adr = true;
    bb.arc_power = false;
    bb.data = adr;
    pk.data.data0 = package_address(bb); 
    pk.data.data1 = 0x50+scn; //scene adresi
    pk.data.type = BLOCK_16;
    dali_double_send(&pk,type); 
}

//-----------------------------------------
/*
    Lambanın senaryo degerini döndürür. 
*/
backword_t get_scene(uint8_t adr,uint8_t scn, uint8_t type)
{
    package_t pk = {};
    address_t bb = {};  
    bb.short_adr = true;
    bb.arc_power = false;
    bb.data = adr;
    pk.data.data0 = package_address(bb); 
    pk.data.data1 = 0xB0+scn; //scene adresi
    pk.data.type = BLOCK_16;

    backword_t kk = DaliSendReceive(&pk,type);
    ESP_LOGI(TAG,"%02X scene %d %02x",adr,scn,kk.data);
    return kk;
}

//-----------------------------------------
void gear_reset(uint8_t adr, uint8_t type)
{
    package_t pk = {};
    address_t bb = {};  
    bb.short_adr = true;
    bb.arc_power = false;
    bb.data = adr;
    pk.data.type = BLOCK_16;
    pk.data.data0 = package_address(bb); 
    pk.data.data1 = 0x20; 
    dali_double_send(&pk,type); 
}

//-----------------------------------------
/*
    Kayıtlı tüm lambaları resetleyerek başlangıç 
    konumuna dönmesini sağlar. 
*/
void full_reset(void)
{
    for (int i=0;i<64;i++)
    {
        gear_t ff={};
        disk.read_file(GEAR_FILE,&ff,sizeof(gear_t),i);
        if (ff.short_addr<64) {
            gear_reset(ff.short_addr, ff.type);
            BEKLE
            vTaskDelay(100/portTICK_PERIOD_MS);
        }
    }
    ESP_LOGI(TAG,"Resetleme işlemi SONLANDIRILDI. Kayıtlı lambaların özellikleri default degerlere alındı");
}


//-----------------------------------------
/*
    Lambanın hangi güç ile yandığını döndürür. 
    Degere 0 ise lamba kapalıdır.
*/
backword_t get_level(uint8_t adr, uint8_t type)
{
    package_t pk = {};
    address_t bb = {};  
    bb.short_adr = true;
    bb.arc_power = false;
    bb.data = adr;
    pk.data.type = BLOCK_16;
    pk.data.data0 = package_address(bb); 
    pk.data.data1 = 0xA0; 
    backword_t kk = {}; 
    kk = DaliSendReceive(&pk,type); 
    if (kk.backword==BACK_TIMEOUT || (kk.backword==BACK_DATA && kk.data==255))
    {
        pk.data.data1 = 0x21;
        dali_double_send(&pk,type); 
        pk.data.data1 = 0x98;
        kk = DaliSendReceive(&pk,type); 
    }
   
    MBEKLE
    return kk;
}


//-----------------------------------------
/*
    adr ile belirtilen lamba/lar ı kapatır. 
    adr=255 ve gurup=0 ise tüm lambalar kapanır
    adr<64 ve gurup=0 ise belirtilen lamba kapanır
    adr<16 ve gurup=1 ise belirtilen gurup kapatılır.
*/
void off(uint8_t adr, uint8_t gurup)
{
    package_t pk = {};
    address_t bb = {};
    adres_type(adr,gurup, &bb);   
    bb.arc_power = false;
    pk.data.data0 = package_address(bb); 
    pk.data.data1 = 0x00; //kapat
    pk.data.type = BLOCK_16;
    DaliSend(&pk,0x99);
}

//-----------------------------------------
/*
    adr ile belirtilen lamba/ları yakar. 
    adr=255 ve gurup=0 ise tüm lambalar power gücü ile yakılır
    adr<64 ve gurup=0 ise belirtilen lamba power gücü ile yakılır
    adr<16 ve gurup=1 ise belirtilen gurup power gücü ile yakılır
    UDP gönderilir
*/
void arc_power(uint8_t adr, uint8_t gurup, uint8_t power)
{
    package_t pk = {};
    address_t bb = {};  
    adres_type(adr,gurup, &bb); 
    bb.arc_power = true;
    pk.data.data0 = package_address(bb); 
    pk.data.data1 = power; 
    pk.data.type = BLOCK_16;
    DaliSend(&pk,0x99);
}

/*
void all_on(void)
{
    package_t pk = {};
    address_t bb = {};  
    bb.broadcast_adr = true;
    bb.arc_power = true;
    bb.data = 0xFF;
    pk.data.data0 = package_address(bb); 
    pk.data.data1 = 254; 
    pk.data.type = BLOCK_16;
    dali.busy_is_wait();
    dali.send(&pk);
    ESP_LOGI(TAG,"All Lamp ON");
}

void all_off(void)
{
    package_t pk = {};
    address_t bb = {};  
    bb.broadcast_adr = true;
    bb.arc_power = false;
    bb.data = 0xFF;
    pk.data.data0 = package_address(bb); 
    pk.data.data1 = 0; 
    pk.data.type = BLOCK_16;
    dali.busy_is_wait();
    dali.send(&pk);
    ESP_LOGI(TAG,"All Lamp OFF");
}
*/

//-----------------------------------------
/*
    Kayıtlı lambaların statuslarını döndürür
*/
void refresh(void)
{
    ESP_LOGI(TAG,"Refresh");
    for (int i=0;i<64;i++)
    {
        gear_t ff={};
        disk.read_file(GEAR_FILE,&ff,sizeof(gear_t),i);
        if (ff.short_addr<64) {
            backword_t lvl = {};
            
            lvl = get_level(ff.short_addr, ff.type);
            if (lvl.backword==BACK_TIMEOUT) lvl = get_level(ff.short_addr, ff.type);
            if (lvl.backword!=BACK_TIMEOUT)
                ESP_LOGI(TAG,"     Addr %2d Power %03d",ff.short_addr, lvl.data); else
                ESP_LOGE(TAG,"     Addr %2d Timeout",ff.short_addr);
  
            cJSON *root = cJSON_CreateObject();
            cJSON_AddStringToObject(root, "com", "status");
            cJSON_AddNumberToObject(root,"adres",ff.short_addr);
            cJSON_AddNumberToObject(root,"gurup",0);
            cJSON_AddNumberToObject(root,"power",lvl.data);
            if (lvl.backword==BACK_TIMEOUT) cJSON_AddNumberToObject(root,"error",0x71);
            cJSON_AddBoolToObject(root,"stat",(lvl.data>0)?true:false);
            char *dat = cJSON_PrintUnformatted(root);
            tcpserver.Send(dat);
            cJSON_free(dat);
            cJSON_Delete(root);
            BEKLE
        }
    }
    ESP_LOGI(TAG,"Refresh SONLANDIRILDI");

}


void dali_out_test(void)
{
    package_t pk = {};
    address_t bb = {};  
    bb.broadcast_adr = true;
    bb.arc_power = true;
    bb.data = 0xFF;
    pk.data.data0 = package_address(bb); 
    pk.data.data1 = 254; 
    pk.data.type = BLOCK_16;

    while(1)
    {
        dali.busy_is_wait();
        dali.send(&pk);
        printf("ADR %02X DATA %02X\n",pk.data.data0,pk.data.data1);
        vTaskDelay(100 / portTICK_PERIOD_MS);
    }
}