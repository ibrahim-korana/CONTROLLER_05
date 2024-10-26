

enum {
    BTN_ON = 0,
    BTN_OFF,
    BTN_ALL,
    BTN_EMPTY,
};

uint8_t btn1_status=BTN_EMPTY, btn2_status=BTN_EMPTY, btn3_status=BTN_EMPTY;

uint8_t btn_stat(uint8_t cfg, uint8_t btn)
{
    uint8_t kk = btn;
    if (cfg==13) kk=BTN_EMPTY;
    if (cfg<8 || cfg==12)
    {
        if (kk==BTN_ON) kk=BTN_OFF; else kk=BTN_ON;
    }
    if (cfg>7 && cfg<12) kk=BTN_ALL;
    return kk;
}

void btn_action(uint8_t cfg, uint8_t btn, uint8_t pow)
{
    if (cfg<8) {
      if (btn==BTN_ON) arc_power(cfg,1,pow); 
      if (btn==BTN_OFF) off(cfg,1);
    } 
    if (cfg==12) {
      if (btn==BTN_ON) arc_power(255,0,pow); 
      if (btn==BTN_OFF) off(255,0);
    } 
    if (cfg>7 && cfg<12) {
      if (btn==BTN_ALL) command_action(255,0,cfg-2); //Comm=6 ise sen1 7 sen2 vb
    } 


}
void btn_callback(void *arg, void *data)
{
    button_handle_t bt = (button_handle_t) arg;
    char *dd = (char *)data;
    button_event_t ev = iot_button_get_event(bt);
    printf("button data=%s  Event=%d\n", dd,ev);  
    
    if (strcmp(dd,"1")==0)
      {
        btn1_status = btn_stat(GlobalConfig.tus1,btn1_status);
        if (ev==BUTTON_SINGLE_CLICK) btn_action(GlobalConfig.tus1,btn1_status,GlobalConfig.pow1);
      }
    if (strcmp(dd,"2")==0)
      {
        btn2_status = btn_stat(GlobalConfig.tus2,btn2_status);
        if (ev==BUTTON_SINGLE_CLICK) btn_action(GlobalConfig.tus2,btn2_status,GlobalConfig.pow2);
      }
    if (strcmp(dd,"3")==0)
      {
        btn3_status = btn_stat(GlobalConfig.tus3,btn3_status);
        if (ev==BUTTON_SINGLE_CLICK) btn_action(GlobalConfig.tus3,btn3_status,GlobalConfig.pow3);
      }          
}

void Button_Config(gpio_num_t btno, const char *num, button_handle_t *bt)
{

    button_config_t *cfg;
    cfg = (button_config_t *)calloc(1,sizeof(button_config_t));
    cfg->type = BUTTON_TYPE_GPIO;
    cfg->gpio_button_config = {
        .gpio_num = btno,
        .active_level=0  
    };
    char *bt_num = (char*)calloc(1,3); strcpy(bt_num,num);
    *bt = iot_button_create(cfg);
    iot_button_register_cb(*bt,BUTTON_ALL_EVENT,btn_callback, bt_num);
}

