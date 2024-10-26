
#ifndef _JSON_TOOL_H_
#define _JSON_TOOL_H_

  #include "cJSON.h"
  #include "lwip/err.h"
  #include "lwip/sys.h"
  #include "lwip/etharp.h"

  bool JSON_item_control(cJSON *rt,const char *item);
  bool JSON_getstring(cJSON *rt,const char *item, char *value, uint8_t len);
  bool JSON_getint(cJSON *rt,const char *item, uint8_t *value);
  bool JSON_getbool(cJSON *rt,const char *item, bool *value);
  bool JSON_getfloat(cJSON *rt,const char *item, float *value);
  bool JSON_getlong(cJSON *rt,const char *item, uint64_t *value);
  bool JSON_getlong(cJSON *rt,const char *item, uint32_t *value);

#endif