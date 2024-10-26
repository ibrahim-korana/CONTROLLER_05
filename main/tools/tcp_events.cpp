
void Add_Gurup(cJSON *gurup)
{
    cJSON *grp;
    char *mm = (char *)calloc(1,15);

    for (int i=1;i<9;i++)
    {
        cJSON_AddItemToArray(gurup, grp = cJSON_CreateObject());
        cJSON_AddItemToObject(grp, "gr", cJSON_CreateNumber(i));
        sprintf(mm,"Gurup %d",i);
        cJSON_AddItemToObject(grp, "nm", cJSON_CreateString(mm));
    }
    free(mm);    
}

void Add_Gear(cJSON *gear)
{
    cJSON *Lm;
    char *mm = (char *)calloc(1,15);
    for (int i=0;i<64;i++)
    {
        gear_t ff={};
        disk.read_file(GEAR_FILE,&ff,sizeof(gear_t),i);
        if (ff.short_addr<64) {
           cJSON_AddItemToArray(gear, Lm = cJSON_CreateObject());
           cJSON_AddItemToObject(Lm, "gr", cJSON_CreateNumber(ff.short_addr));           
           sprintf(mm,"Lamba %d",i);
           cJSON_AddItemToObject(Lm, "nm", cJSON_CreateString(mm)); 
           cJSON_AddItemToObject(Lm, "tp", cJSON_CreateNumber(ff.type));         
        }
    }
    
    free(mm);
}

void dali_port(tcp_events_data_t *ev) 
{
    if (ev->data[0]!='{') return;
    char *data = (char *)calloc(1,ev->data_len+1);
    strcpy(data,ev->data);


    printf("DALI DATA %s\n",data);

    free(data); 
}


void data_parse(char *data)
{
    //data convert edilip system event olarak yayınlanacak 
    cJSON *rcv_json = cJSON_Parse(data);
    char *command = (char *)calloc(1,15);
    JSON_getstring(rcv_json,"com", command,15);

    //------------------------------------
    if (strcmp(command,"ping")==0) {
        cJSON *root = cJSON_CreateObject();
        cJSON_AddStringToObject(root, "com", "pong");
        char *dat = cJSON_PrintUnformatted(root);
        tcpserver.Send(dat);
        cJSON_free(dat);
        cJSON_Delete(root);
     } else ESP_LOGI("TCP_SERVER","%s",data);
    //------------------------------------ 
    //------------------------------------ 
    if (strcmp(command,"reset")==0) {
        esp_restart();
    } 
    //------------------------------------
    if (strcmp(command,"clear_number")==0) {
        disk.file_empty(GEAR_FILE);
        gear_t ff = {};
        ff.short_addr = 200;
        ff.type=0;
        for (int i=0;i<64;i++)
        {
            disk.write_file(GEAR_FILE,&ff,sizeof(gear_t),i);
        }  
        
        cJSON *root = cJSON_CreateObject();
        cJSON_AddStringToObject(root, "com", "search");
        cJSON_AddNumberToObject(root, "status", 0);
        char *dat = cJSON_PrintUnformatted(root);
        tcpserver.Send(dat);
        cJSON_free(dat);
        cJSON_Delete(root);
        ESP_LOGW(TAG,"Clear OK");
        display_write(2,"Dev Cleared");
    }
    //------------------------------------
    if (strcmp(command,"list_number")==0) {
        gear_t ff = {};
        uint8_t count=0;
        for (int i=0;i<64;i++)
        {
            disk.read_file(GEAR_FILE,&ff,sizeof(gear_t),i);
            if (ff.short_addr<64) {
                ESP_LOGI(TAG,"Short Address : %02d  Type : %02X",ff.short_addr, ff.type);
                count++;
            }
        } 
        
        cJSON *root = cJSON_CreateObject();
        cJSON_AddStringToObject(root, "com", "search");
        cJSON_AddNumberToObject(root, "status", 0);
        char *dat = cJSON_PrintUnformatted(root);
        tcpserver.Send(dat);
        cJSON_free(dat);
        cJSON_Delete(root);
        ESP_LOGW(TAG,"List OK"); 
        char *mm = (char *)calloc(1,20);
        sprintf(mm,"Dev Count:%d",count);
        ESP_LOGI(TAG,"%s",mm); 
        display_write(2,mm);
        free(mm);
    }
    //------------------------------------ 
    if (strcmp(command,"refresh")==0) {
        char *ss = (char*)calloc(1,100);
        sprintf(ss,"Refresh");
        display_write(2,ss);
        free(ss);
        refresh();
    } 
    //------------------------------------
    if (strcmp(command,"intro")==0) {
        char *ss = (char*)calloc(1,100);
        sprintf(ss,"Intro");
        display_write(2,ss);
        free(ss);

        cJSON *root = cJSON_CreateObject();
        root = cJSON_CreateObject();
        cJSON_AddStringToObject(root, "com", "intro");

        //cJSON *grp, *Lm;
        cJSON *gurup = cJSON_CreateArray();
        Add_Gurup(gurup);
        cJSON_AddItemToObject(root, "gurup", gurup);

        cJSON *gear = cJSON_CreateArray();
        Add_Gear(gear);
        cJSON_AddItemToObject(root, "gear", gear);

        char *dat = cJSON_PrintUnformatted(root);
        tcpserver.Send(dat);
        cJSON_free(dat);
        cJSON_Delete(root);
    } 
    //------------------------------------ 
    if (strcmp(command,"search")==0) {
        uint8_t kayit=0, cable=0;
        JSON_getint(rcv_json,"kayit", &kayit);
        JSON_getint(rcv_json,"cable", &cable);
        char *ss = (char*)calloc(1,50);
        sprintf(ss,"Search %d",kayit);
        display_write(2,ss);
        free(ss);
        cJSON *root = cJSON_CreateObject();
        cJSON_AddStringToObject(root, "com", "search");
        cJSON_AddNumberToObject(root, "status", 1);
        char *dat = cJSON_PrintUnformatted(root);
        tcpserver.Send(dat);
        cJSON_free(dat);
        cJSON_Delete(root);

        if (cable==1) Device_Control_cable(kayit);
        if (cable==0) Device_Control_Wireless(kayit);
        if (cable==2) {
            Device_Control_cable(kayit);
            Device_Control_Wireless(kayit);
        } 

        root = cJSON_CreateObject();
        cJSON_AddStringToObject(root, "com", "search");
        cJSON_AddNumberToObject(root, "status", 0);
        dat = cJSON_PrintUnformatted(root);
        tcpserver.Send(dat);
        cJSON_free(dat);
        cJSON_Delete(root);
    } 
    //------------------------------------ 
    if (strcmp(command,"arc_power")==0) {
      
        uint8_t power=0, adr=0, grp=255;
        JSON_getint(rcv_json,"adres", &adr);
        JSON_getint(rcv_json,"gurup", &grp);
        JSON_getint(rcv_json,"power", &power);
        if (power==255) power=254;
        arc_power(adr, grp, power);
        vTaskDelay(100 / portTICK_PERIOD_MS);
   
        cJSON *root = cJSON_CreateObject();
        cJSON_AddStringToObject(root, "com", "status");
        cJSON_AddNumberToObject(root,"adres",adr);
        cJSON_AddNumberToObject(root,"gurup",grp);
        cJSON_AddNumberToObject(root,"power",power);
        cJSON_AddBoolToObject(root,"stat",(power>0)?true:false);
        char *dat = cJSON_PrintUnformatted(root);
        tcpserver.Send(dat);
        cJSON_free(dat);
        cJSON_Delete(root);
    }
    //------------------------------------ 
    if (strcmp(command,"off")==0) {
        uint8_t adr=0, grp=255;
        JSON_getint(rcv_json,"adres", &adr);
        JSON_getint(rcv_json,"gurup", &grp);
        off(adr,grp);
        vTaskDelay(100 / portTICK_PERIOD_MS);
         
        cJSON *root = cJSON_CreateObject();
        cJSON_AddStringToObject(root, "com", "status");
        cJSON_AddNumberToObject(root,"adres",adr);
        cJSON_AddNumberToObject(root,"gurup",grp);
        cJSON_AddNumberToObject(root,"power",0);
        cJSON_AddBoolToObject(root,"stat",false);
        char *dat = cJSON_PrintUnformatted(root);
        tcpserver.Send(dat);
        cJSON_free(dat);
        cJSON_Delete(root);
    }

    //------------------------------------
    if (strcmp(command,"set_zaman")==0) {
      uint8_t adr=0, grp=0;
        JSON_getint(rcv_json,"adres", &adr);
        JSON_getint(rcv_json,"gurup", &grp);
        zaman_t kk={};
        JSON_getint(rcv_json,"on1s", &kk.on1saat);
        JSON_getint(rcv_json,"on1d", &kk.on1dakika);
        JSON_getint(rcv_json,"on2s", &kk.on2saat);
        JSON_getint(rcv_json,"on2d", &kk.on2dakika);
        JSON_getint(rcv_json,"off1s", &kk.off1saat);
        JSON_getint(rcv_json,"off1d", &kk.off1dakika);
        JSON_getint(rcv_json,"off2s", &kk.off2saat);
        JSON_getint(rcv_json,"off2d", &kk.off2dakika);
        JSON_getint(rcv_json,"pow1", &kk.power1);
        JSON_getint(rcv_json,"pow2", &kk.power2);
        if (kk.power1<10) kk.power1= 150;
        if (kk.power2<10) kk.power2= 150;

        if (kk.on1saat>0 && kk.on1dakika>0) kk.active=1;
        if (kk.on2saat>0 && kk.on2dakika>0) kk.active=1;
        if (kk.off1saat>0 && kk.off1dakika>0) kk.active=1;
        if (kk.off2saat>0 && kk.off2dakika>0) kk.active=1;
        kk.gurup = grp;
        if (grp==1)
        {        
            disk.write_file(ZAMAN_FILE,&kk,sizeof(zaman_t),adr);
            init_cron();
        }   
        if (grp==0) {
            disk.write_file(ZAMAN_FILE,&kk,sizeof(zaman_t),adr+16);
            init_cron();
        }
    }
    //------------------------------------
    if (strcmp(command,"get_zaman")==0) {
      uint8_t adr=0, grp=0;
        JSON_getint(rcv_json,"adres", &adr);
        JSON_getint(rcv_json,"gurup", &grp);

        zaman_t kk={};
        if (grp==1)
        {        
            disk.read_file(ZAMAN_FILE,&kk,sizeof(zaman_t),adr);
        }

        if (grp==0)
        {        
            disk.read_file(ZAMAN_FILE,&kk,sizeof(zaman_t),adr+16);
        }

        cJSON *root = cJSON_CreateObject();
        cJSON_AddStringToObject(root, "com", "get_zaman");
        cJSON_AddNumberToObject(root,"adres",adr);
        cJSON_AddNumberToObject(root,"gurup",grp);
        cJSON_AddNumberToObject(root,"on1s",kk.on1saat);
        cJSON_AddNumberToObject(root,"on1d",kk.on1dakika);
        cJSON_AddNumberToObject(root,"on2s",kk.on2saat);
        cJSON_AddNumberToObject(root,"on2d",kk.on2dakika);

        cJSON_AddNumberToObject(root,"off1s",kk.off1saat);
        cJSON_AddNumberToObject(root,"off1d",kk.off1dakika);
        cJSON_AddNumberToObject(root,"off2s",kk.off2saat);
        cJSON_AddNumberToObject(root,"off2d",kk.off2dakika);
        cJSON_AddNumberToObject(root,"pow1",kk.power1);
        cJSON_AddNumberToObject(root,"pow2",kk.power2);

        char *dat = cJSON_PrintUnformatted(root);
        tcpserver.Send(dat);
        cJSON_free(dat);
        cJSON_Delete(root);   
    }
    //------------------------------------
    if (strcmp(command,"action")==0) {
        uint8_t adr=0, grp=0, com=0;
        JSON_getint(rcv_json,"adres", &adr);
        JSON_getint(rcv_json,"gurup", &grp);
        JSON_getint(rcv_json,"komut", &com);
        command_action(adr,grp,com); 
    }

    //------------------------------------
    if (strcmp(command,"set_frate")==0) {
        uint8_t adr=0, val=0,typ=0;
        JSON_getint(rcv_json,"adres", &adr);
        JSON_getint(rcv_json,"value", &val);
        JSON_getint(rcv_json,"type", &typ);
        set_fade(adr,val,0,typ);
    }
    //------------------------------------
    if (strcmp(command,"set_ftime")==0) {
        uint8_t adr=0, val=0,typ=0;
        JSON_getint(rcv_json,"adres", &adr);
        JSON_getint(rcv_json,"value", &val);
        JSON_getint(rcv_json,"type", &typ);
        set_fade(adr,val,1,typ);
    }
    //------------------------------------
    if (strcmp(command,"set_efade")==0) {
        uint8_t adr=0, val=0,typ=0;
        JSON_getint(rcv_json,"adres", &adr);
        JSON_getint(rcv_json,"value", &val);
        JSON_getint(rcv_json,"type", &typ);
        set_fade(adr,val,2,typ);
    }
    //------------------------------------
    if (strcmp(command,"set_fail")==0) {
        uint8_t adr=0, val=0,typ=0;
        JSON_getint(rcv_json,"adres", &adr);
        JSON_getint(rcv_json,"value", &val);
        JSON_getint(rcv_json,"type", &typ);
        set_fade(adr,val,3,typ);
    }
    //------------------------------------
    if (strcmp(command,"set_pow")==0) {
        uint8_t adr=0, val=0,typ=0;
        JSON_getint(rcv_json,"adres", &adr);
        JSON_getint(rcv_json,"value", &val);
        JSON_getint(rcv_json,"type", &typ);
        set_fade(adr,val,4,typ);
    }
    //------------------------------------
    if (strcmp(command,"get_scene")==0) {
      uint8_t adr=0, scn=0, val=0, typ=0;
        JSON_getint(rcv_json,"adres", &adr);
        JSON_getint(rcv_json,"scene", &scn);
        JSON_getint(rcv_json,"type", &typ);
        val=get_scene(adr,scn,typ);
        cJSON *root = cJSON_CreateObject();
        cJSON_AddStringToObject(root, "com", "scene_status");
        cJSON_AddNumberToObject(root,"adres",adr);
        cJSON_AddNumberToObject(root,"scene",scn);
        cJSON_AddNumberToObject(root,"value",val);
        char *dat = cJSON_PrintUnformatted(root);
        tcpserver.Send(dat);
        cJSON_free(dat);
        cJSON_Delete(root);   
    }
    //------------------------------------
    if (strcmp(command,"set_scene")==0) {
        uint8_t adr=0, scn=0, val=0, typ=0;
        JSON_getint(rcv_json,"adres", &adr);
        JSON_getint(rcv_json,"scene", &scn);
        JSON_getint(rcv_json,"value", &val);
        JSON_getint(rcv_json,"type", &typ);
        
        set_scene(adr,scn,val, typ);
        vTaskDelay(100 / portTICK_PERIOD_MS);
        val=get_scene(adr,scn,typ);
        cJSON *root = cJSON_CreateObject();
        cJSON_AddStringToObject(root, "com", "scene_status");
        cJSON_AddNumberToObject(root,"adres",adr);
        cJSON_AddNumberToObject(root,"scene",scn);
        cJSON_AddNumberToObject(root,"value",val);
        char *dat = cJSON_PrintUnformatted(root);
        tcpserver.Send(dat);
        cJSON_free(dat);
        cJSON_Delete(root);   

    }
    //------------------------------------
    if (strcmp(command,"clear_scene")==0) {
        uint8_t adr=0, scn=0, val=0, typ=0;
        JSON_getint(rcv_json,"adres", &adr);
        JSON_getint(rcv_json,"scene", &scn);
        JSON_getint(rcv_json,"type", &typ);
        clear_scene(adr,scn,typ);
        vTaskDelay(100 / portTICK_PERIOD_MS);
        val=get_scene(adr,scn,typ);
        cJSON *root = cJSON_CreateObject();
        cJSON_AddStringToObject(root, "com", "scene_status");
        cJSON_AddNumberToObject(root,"adres",adr);
        cJSON_AddNumberToObject(root,"scene",scn);
        cJSON_AddNumberToObject(root,"value",val);
        char *dat = cJSON_PrintUnformatted(root);
        tcpserver.Send(dat);
        cJSON_free(dat);
        cJSON_Delete(root);   
    }
    //------------------------------------ 
    if (strcmp(command,"get_level")==0) {
        uint8_t adr=0, rap=0;

        JSON_getint(rcv_json,"adres", &adr);
        gear_t kk = {};
        disk.read_file(GEAR_FILE,&kk,sizeof(gear_t),adr);
        rap = get_level(adr,kk.type);
        
        cJSON *root = cJSON_CreateObject();
        cJSON_AddStringToObject(root, "com", "status");
        cJSON_AddNumberToObject(root,"adres",adr);
        cJSON_AddNumberToObject(root,"gurup",0);
        cJSON_AddNumberToObject(root,"power",rap);

           cJSON_AddBoolToObject(root,"stat",(rap>0)?true:false);

        char *dat = cJSON_PrintUnformatted(root);
        tcpserver.Send(dat);
        cJSON_free(dat);
        cJSON_Delete(root);
    } 
    //------------------------------------ 
    if (strcmp(command,"get_rapor")==0) {
        uint8_t adr=0, rap=0, dat0=0;
        JSON_getint(rcv_json,"adres", &adr);
        JSON_getint(rcv_json,"data", &dat0);
        rap = get_rapor(adr,dat0);
        
        cJSON *root = cJSON_CreateObject();
        cJSON_AddStringToObject(root, "com", "get_rapor");
        cJSON_AddNumberToObject(root,"raw",rap);
        char *dat = cJSON_PrintUnformatted(root);
        tcpserver.Send(dat);
        cJSON_free(dat);
        cJSON_Delete(root);
    } 
    //------------------------------------ 
    if (strcmp(command,"get_gurup")==0) {
        uint8_t grp0=0,grp1=0, adr=0, min=0, max=0, delay = 50, fail=0, fade=0, efade=0,ponl=0, glob = 0, typ=0;
        JSON_getint(rcv_json,"adres", &adr);
        JSON_getint(rcv_json,"type", &typ);
        grp0 = get_group(adr,0,typ);
        vTaskDelay(delay / portTICK_PERIOD_MS);
        grp1 = get_group(adr,1,typ);
        vTaskDelay(delay / portTICK_PERIOD_MS);
        min = get_level_command(adr,0xA2,typ);
        vTaskDelay(delay / portTICK_PERIOD_MS);
        max = get_level_command(adr, 0xA1,typ);
        vTaskDelay(delay / portTICK_PERIOD_MS);
        fail = get_level_command(adr,0xA4,typ);
        vTaskDelay(delay / portTICK_PERIOD_MS);
        fade = get_level_command(adr,0xA5,typ);
        vTaskDelay(delay / portTICK_PERIOD_MS);
        efade = get_level_command(adr,0xA8,typ);
        vTaskDelay(delay / portTICK_PERIOD_MS);
        ponl = get_level_command(adr,0xA3,typ);
        vTaskDelay(delay / portTICK_PERIOD_MS);
        glob = get_level_command(adr,0x90,typ);


        cJSON *root = cJSON_CreateObject();
        cJSON_AddStringToObject(root, "com", "group");
        cJSON_AddNumberToObject(root,"adres",adr);
        cJSON_AddNumberToObject(root,"low",grp0);
        cJSON_AddNumberToObject(root,"high",grp1);
        cJSON_AddNumberToObject(root,"min",min);
        cJSON_AddNumberToObject(root,"max",max);
        cJSON_AddNumberToObject(root,"fail",fail);
        cJSON_AddNumberToObject(root,"fade",fade);
        cJSON_AddNumberToObject(root,"efade",efade);
        cJSON_AddNumberToObject(root,"powon",ponl);
        cJSON_AddNumberToObject(root,"global",glob);
        char *dat = cJSON_PrintUnformatted(root);
        tcpserver.Send(dat);
        cJSON_free(dat);
        cJSON_Delete(root);
    } 
    //------------------------------------ 
    if (strcmp(command,"set_gurup")==0) {
        uint8_t grp=0,adr=0, yaz=0, grp0=0, grp1=0, typ = 0;;
        JSON_getint(rcv_json,"gurup", &grp); //1-16 arası
        JSON_getint(rcv_json,"adres", &adr); 
        JSON_getint(rcv_json,"yaz", &yaz); //1 yaz 0 sil
        JSON_getint(rcv_json,"type", &typ); //cihaz tipi
        
        set_group(grp,adr,yaz, typ);

        vTaskDelay(100 / portTICK_PERIOD_MS);
        grp0 = get_group(adr,0, typ);
        vTaskDelay(100 / portTICK_PERIOD_MS);
        grp1 = get_group(adr,1, typ);

        cJSON *root = cJSON_CreateObject();
        cJSON_AddStringToObject(root, "com", "group");
        cJSON_AddNumberToObject(root,"adres",adr);
        cJSON_AddNumberToObject(root,"low",grp0);
        cJSON_AddNumberToObject(root,"high",grp1);
        char *dat = cJSON_PrintUnformatted(root);
        tcpserver.Send(dat);
        cJSON_free(dat);
        cJSON_Delete(root);        
    } 
    //------------------------------------ 
    if (strcmp(command,"init")==0) {
        uint8_t type=0;
        JSON_getint(rcv_json,"type", &type);
        cJSON *root = cJSON_CreateObject();
        cJSON_AddStringToObject(root, "com", "init");
        cJSON_AddNumberToObject(root, "status", 1);
        char *dat = cJSON_PrintUnformatted(root);
        tcpserver.Send(dat);
        cJSON_free(dat);
        cJSON_Delete(root);

        Search_Device(type);
        
        root = cJSON_CreateObject();
        cJSON_AddStringToObject(root, "com", "init");
        cJSON_AddNumberToObject(root, "status", 0);
        dat = cJSON_PrintUnformatted(root);
        tcpserver.Send(dat);
        cJSON_free(dat);
        cJSON_Delete(root);
     } 
    //------------------------------------ 
    //------------------------------------ 
    //------------------------------------ 
    free(command);
    cJSON_Delete(rcv_json); 
}

void tcp_handler(void* handler_args, esp_event_base_t base, int32_t id, void* event_data)
{
    tcp_events_data_t *ev = (tcp_events_data_t *)event_data;

    if (ev->port==5718)
      {
          dali_port(ev);
          return;
      }
    if (ev->data[0]!='{') return;
    char *data = (char *)calloc(1,ev->data_len+1);
    strcpy(data,ev->data);

    char *token = strtok(data, "&");
    while (token != NULL)
    {
        //printf("token : %s\n", token);
        data_parse(token);
        token = strtok(NULL, "&");
    }

    
   // data_parse(data);
    free(data);
} 