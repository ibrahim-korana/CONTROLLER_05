

#include "cJSON.h"
#include "string.h"
#include "jsontool.h"

bool JSON_item_control(cJSON *rt,const char *item)
{
    if (cJSON_GetObjectItem(rt, item)) return true;
    else return false;
};

bool JSON_getstring(cJSON *rt,const char *item, char *value, uint8_t len)
{
    if (JSON_item_control(rt, item)) {
              char *tmp = (char *)cJSON_GetObjectItem(rt,item)->valuestring;
              if (strlen(tmp)>len) return false;
              strcpy(value , tmp);
              return true; 
	                                 } 
    else return false;
};

bool JSON_getint(cJSON *rt,const char *item, uint8_t *value)
{
    if (JSON_item_control(rt, item)) {
        *value = cJSON_GetObjectItem(rt,item)->valueint;
        return true;
    }
    return false;
};

bool JSON_getbool(cJSON *rt,const char *item, bool *value)
{
    if (JSON_item_control(rt, item)) {
        *value = (bool *)cJSON_GetObjectItem(rt,item)->valueint;
        return true;
    }
    return false;
}

bool JSON_getfloat(cJSON *rt,const char *item, float *value)
{
    if (JSON_item_control(rt, item)) {
        *value = cJSON_GetObjectItem(rt,item)->valuedouble;
        return true;
    }
    return false;
}

bool JSON_getlong(cJSON *rt,const char *item, uint64_t *value)
{
    if (JSON_item_control(rt, item)) {
        *value = cJSON_GetObjectItem(rt,item)->valuedouble;
        return true;
    }
    return false;
}

bool JSON_getlong(cJSON *rt,const char *item, uint32_t *value)
{
    if (JSON_item_control(rt, item)) {
        *value = cJSON_GetObjectItem(rt,item)->valuedouble;
        return true;
    }
    return false;
}

