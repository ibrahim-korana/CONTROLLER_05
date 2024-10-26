#include <string.h>
#include <fcntl.h>
#include "esp_http_server.h"
#include "esp_chip_info.h"
#include "esp_random.h"
#include "esp_log.h"
#include "esp_vfs.h"
#include "cJSON.h"

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/timers.h"
#include <sys/param.h>

#include "../core.h"

static const char *REST_TAG = "REST";

#define FILE_PATH_MAX (ESP_VFS_PATH_MAX + 128)
#define SCRATCH_BUFSIZE (12288)
#define PARAM_TEMP (60)

typedef struct rest_server_context {
    char base_path[ESP_VFS_PATH_MAX + 1];
    char scratch[SCRATCH_BUFSIZE];
} rest_server_context_t;

#define CHECK_FILE_EXTENSION(filename, ext) (strcasecmp(&filename[strlen(filename) - strlen(ext)], ext) == 0)

home_network_config_t netconfig;
config_t globalconfig;
web_callback_t callback;

int replace(char *buf, char* tmp)
{
	int pp=-2;
	//printf("PAR %s %d\n",buf, strlen(buf));
    if (strcmp(buf,"TUS1")==0) sprintf(tmp,"%d",globalconfig.tus1);
    if (strcmp(buf,"TUS2")==0) sprintf(tmp,"%d",globalconfig.tus2);
    if (strcmp(buf,"TUS3")==0) sprintf(tmp,"%d",globalconfig.tus3);
    if (strcmp(buf,"POW1")==0) sprintf(tmp,"%d",globalconfig.pow1);
    if (strcmp(buf,"POW2")==0) sprintf(tmp,"%d",globalconfig.pow2);
    if (strcmp(buf,"POW3")==0) sprintf(tmp,"%d",globalconfig.pow3);

	if (strcmp(buf,"MQTT")==0) sprintf(tmp,"%s",(char *)globalconfig.mqtt_server);
	if (strcmp(buf,"KEEP")==0) sprintf(tmp,"%d",globalconfig.mqtt_keepalive);
	if (strcmp(buf,"DEVID")==0) sprintf(tmp,"%d",globalconfig.device_id);
    if (strcmp(buf,"PROJE")==0) sprintf(tmp,"%d",globalconfig.project_number);
	if (strcmp(buf,"WEB")==0) {
		if (globalconfig.http_start==1) strcpy(tmp,"checked"); else strcpy(tmp," ");
	                          }
	if (strcmp(buf,"TCP")==0) {
			if (globalconfig.tcpserver_start==1) strcpy(tmp,"checked"); else strcpy(tmp," ");
                              }
    if (strcmp(buf,"UDP")==0) {
			if (globalconfig.daliserver_start==1) strcpy(tmp,"checked"); else strcpy(tmp," ");        
		                      }
    if (strcmp(buf,"TSYNC")==0) {
			if (globalconfig.time_sync==1) strcpy(tmp,"checked"); else strcpy(tmp," ");
		                          }                              

		if (strcmp(buf,"OUT1")==0) {
			if (netconfig.wan_type==1)
				strcpy(tmp,"checked");
			else
				strcpy(tmp," ");
		                            }
		if (strcmp(buf,"OUT2")==0) {
			if (netconfig.wan_type==2)
				strcpy(tmp,"checked");
			else
				strcpy(tmp," ");
		                            }

		if (strcmp(buf,"GETIP1")==0) {
			if (netconfig.ipstat==1)
				strcpy(tmp,"checked");
			else
				strcpy(tmp," ");
		                            }
		if (strcmp(buf,"GETIP2")==0) {
			if (netconfig.ipstat==2)
				strcpy(tmp,"checked");
			else
				strcpy(tmp," ");
		                            }
		if (strcmp(buf,"IP")==0) sprintf(tmp,"%s",(char *)netconfig.ip);
		if (strcmp(buf,"NETMASK")==0) sprintf(tmp,"%s",(char *)netconfig.netmask);
		if (strcmp(buf,"GATEWAY")==0) sprintf(tmp,"%s",(char *)netconfig.gateway);
		if (strcmp(buf,"SSID")==0) sprintf(tmp,"%s",(char *)netconfig.wifi_ssid);
		if (strcmp(buf,"PASS")==0) sprintf(tmp,"%s",(char *)netconfig.wifi_pass);
		if (strcmp(buf,"CHAN")==0) sprintf(tmp,"%d",netconfig.channel);
        if (strcmp(buf,"DNS1")==0) sprintf(tmp,"%s",(char *)netconfig.dns);
        if (strcmp(buf,"DNS2")==0) sprintf(tmp,"%s",(char *)netconfig.backup_dns);
        if (strcmp(buf,"WRET")==0) sprintf(tmp,"%d",netconfig.WIFI_MAXIMUM_RETRY);


	if(strlen(tmp)>0) {pp-=strlen(buf);pp+=strlen(tmp);}
	    else
	    {
	    	strcpy(tmp,buf);
	    }
	return pp;
}

int find_replace(char *buf, char* tmp)
{
    char *sourge_pointer = buf;
    char *point_pointer = buf;
    int counter = 0, Basla=1, Bitir=0, Ret=0;

    int i = 0, j= 0;
    while (*sourge_pointer!='\0')
    {

    	if (*sourge_pointer=='&')
    	{
    		counter = sourge_pointer-point_pointer;
    		if (i==0) {
    			        Basla=Basla+counter;i++;

    		    } else {
    			   Bitir=Basla+counter;
    			   char *ww = (char*)malloc(PARAM_TEMP);
    			   char *ss = (char*)malloc(PARAM_TEMP);
    			   memset(ww,0,PARAM_TEMP);memset(ss,0,PARAM_TEMP);
    			   memcpy(ww,buf+Basla,Bitir-Basla-1);
    			   Ret=Ret+replace(ww,ss);
    			   //Parametreyi tmp ye ekle
    			   for (int p=0;p<strlen(ss);p++) tmp[j++]=ss[p];
    			   free(ww);
    			   free(ss);
    			   Basla=Bitir+1;
    			   i=0;sourge_pointer++;
                }
    		point_pointer = sourge_pointer;
    	};
    	if (i==0) {tmp[j++]=*sourge_pointer;}
    	sourge_pointer++;
    }
    tmp[j]='\0';
   return Ret;
}


void getRegister(char *buf, int tip) {
    char * abuf = (char *)calloc(1,500);

  strcpy(buf, "<html><head><meta charset='UTF-8'><meta name='viewport' content='width=1024'><meta name='viewport' content='width=device-width, initial-scale=0.5'> \
  <TITLE>SmartQ Setup</TITLE><link rel='stylesheet' type='text/css' href='main2.css'></head><body><div class='container' id='container'> \
  <div id='header' class='header'><table class='Headt1' border='0'><tr><td align='left' width='17%'> </td><td align='left' colspan=5> <a><h2>Register</h2></a></br>");
  
  strcpy(abuf,"</tr></table><hr style='background-color: #11516e; height: 7px;'/>\
  <table style='width:300px' align='center' border='0' padding='5px'> ");
  strcat(buf,abuf);
  strcat(buf,"<tr><th>Device ID</th><th>Register ID</th><th>Name</th><th>Started</th></tr>");

    strcpy(abuf,"<form action='/api/register' method='POST' align='center'><tr>><td colspan='4' align='center'> \
     <input style='height:50px; width:300px' type='submit' value='REGISTER'</input> \
     </td></tr></form>");
   strcat(buf,abuf);  
   strcpy(abuf,"<tr><td colspan='4' align='center'><a href='/'>MAIN PAGE</a> </td></tr>");
   strcat(buf,abuf); 
   strcpy(abuf,"</table></body></html>");
  strcat(buf,abuf); 
  
  free(abuf);
}  


void getBack(char *buf) {
    char * abuf = (char *)calloc(1,500);

  strcpy(buf, "<html><head><meta charset='UTF-8'><meta name='viewport' content='width=1024'><meta name='viewport' content='width=device-width, initial-scale=0.5'> \
  <TITLE>SmartQ Setup</TITLE><link rel='stylesheet' type='text/css' href='/main2.css'></head><body><div class='container' id='container'> \
  <div id='header' class='header'><table class='Headt1' border='0'><tr><td align='left' width='17%'> </td><td align='left' colspan=5> <a><h2>Setup</h2></a></br>");
  
  strcpy(abuf,"</tr></table><hr style='background-color: #11516e; height: 7px;'/> \
  <table style='width:100%' border='0' padding='5px'><tr><td></td><td align='center'> \
  ");
  strcat(buf,abuf);

  sprintf(abuf,"<br><a>The configuration is saved and the device is reset. Please reconnect to the device</a>");
  strcat(buf,abuf);
  
  strcpy(abuf,"</td></tr><tr><td></td><td> \
     </td></tr><tr></br></br><td colspan='2' align='center'><a href='/'>MAIN PAGE</a> </td> \
     </tr></table></body></html>");
  strcat(buf,abuf);   
  free(abuf); 
}


static esp_err_t set_content_type_from_file(httpd_req_t *req, const char *filepath)
{
    const char *type = "text/plain";
    if (CHECK_FILE_EXTENSION(filepath, ".html")) {
        type = "text/html";
    } else if (CHECK_FILE_EXTENSION(filepath, ".js")) {
        type = "application/javascript";
    } else if (CHECK_FILE_EXTENSION(filepath, ".css")) {
        type = "text/css";
    } else if (CHECK_FILE_EXTENSION(filepath, ".png")) {
        type = "image/png";
    } else if (CHECK_FILE_EXTENSION(filepath, ".ico")) {
        type = "image/x-icon";
    } else if (CHECK_FILE_EXTENSION(filepath, ".svg")) {
        type = "text/xml";
    }
    return httpd_resp_set_type(req, type);
}

static esp_err_t rest_common_get_handler(httpd_req_t *req)
{
    char filepath[FILE_PATH_MAX];

    rest_server_context_t *rest_context = (rest_server_context_t *)req->user_ctx;
    strlcpy(filepath, rest_context->base_path, sizeof(filepath));
    if (req->uri[strlen(req->uri) - 1] == '/') {
        strlcat(filepath, "/index.html", sizeof(filepath));
    } else {
        strlcat(filepath, req->uri, sizeof(filepath));
    }
 
    int fd = open(filepath, O_RDONLY, 0);
    if (fd == -1) {
        ESP_LOGE(REST_TAG, "Failed to open file : %s", filepath);
        /* Respond with 500 Internal Server Error */
        httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Failed to read existing file");
        return ESP_FAIL;
    }

    set_content_type_from_file(req, filepath);

    char *chunk = rest_context->scratch;
    ssize_t read_bytes;
    do {

        read_bytes = read(fd, chunk, SCRATCH_BUFSIZE);
        if (read_bytes == -1) {
            ESP_LOGE(REST_TAG, "Failed to read file : %s", filepath);
        } else if (read_bytes > 0) {
        	    char *tmp = (char*)malloc(SCRATCH_BUFSIZE);
        	    memset(tmp,0,SCRATCH_BUFSIZE);
        	    read_bytes= read_bytes + find_replace(chunk,tmp);
        	    strcpy(chunk,tmp);
        	    free(tmp);
            if (httpd_resp_send_chunk(req, chunk, read_bytes) != ESP_OK) {
                close(fd);
                ESP_LOGE(REST_TAG, "File sending failed!");
                httpd_resp_sendstr_chunk(req, NULL);
                httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Failed to send file");
                return ESP_FAIL;
            }
        }
    } while (read_bytes > 0);
    close(fd);
    httpd_resp_send_chunk(req, NULL, 0);
    return ESP_OK;
}


static esp_err_t register_handler(httpd_req_t *req)
{
    httpd_resp_set_type(req, "text/html");
    char *buf = (char *)calloc(1,3000);
    getRegister(buf,1);    
    httpd_resp_sendstr(req, buf);
    free(buf);
    return ESP_OK;
}



int strpos(char *hay, const char *needle, int offset)
{
   char haystack[strlen(hay)];
   strncpy(haystack, hay+offset, strlen(hay)-offset);
   char *p = strstr(haystack, needle);
   if (p) return p - haystack+offset;
   return -1;
}

int findparam(char *buf, const char* key, char *argv)
{
    char *tm = (char*) calloc(1,400);
    strcpy(tm,buf);
    char *token = strtok(tm, "&");
    char *bf = (char*) calloc(1,50);
    memset(argv,0,49);

    while (token != NULL)
    {     
        strcpy(bf,token);        
        int i = strpos(bf,"=",0);
        char *bir = (char*) calloc(1,50);
        memcpy(bir,bf,i);
        //free(bf);
        if (strcmp(bir,key)==0)
          {
             char *iki = (char*) calloc(1,50);
             memcpy(iki,&bf[i+1],strlen(bf)-(i+1));
             strcpy(argv,iki);
           
             free(tm);
             free(bir);
             free(iki);
             return 1;
          }
           
        free(bir);
        token = strtok(NULL, "&");
    }
    return 0;
} 


void timerCallBack( TimerHandle_t xTimer )
{
    esp_restart();
}


int replacechar(char *str, char orig, char rep) {
    char *ix = str;
    int n = 0;
    while((ix = strchr(ix, orig)) != NULL) {
        *ix++ = rep;
        n++;
    }
    return n;
}

static esp_err_t save_handler(httpd_req_t *req)
{
    int total_len = req->content_len;
    int cur_len = 0;
    char *buf = ((rest_server_context_t *)(req->user_ctx))->scratch;
    int received = 0;
    if (total_len >= SCRATCH_BUFSIZE) {
        httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "content too long");
        return ESP_FAIL;
    }
    while (cur_len < total_len) {
        received = httpd_req_recv(req, buf + cur_len, total_len);
        if (received <= 0) {
            httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Failed to post control value");
            return ESP_FAIL;
        }
        cur_len += received;
    }
    buf[total_len] = '\0';
    printf("BUF %s\n",buf);

    char *aaa = (char *) calloc(1,50);
    int p = 0;

    findparam(buf,"MQTT",aaa);
    strcpy((char *)globalconfig.mqtt_server,aaa);

    findparam(buf,"DEVID",aaa);
    p=atoi(aaa);
    globalconfig.device_id = p;

    findparam(buf,"KEEP",aaa);
    p=atoi(aaa);
    globalconfig.mqtt_keepalive = p;

    findparam(buf,"COMM",aaa);
    p=atoi(aaa);
    globalconfig.comminication = p;

    findparam(buf,"POW1",aaa);
    p=atoi(aaa);
    globalconfig.pow1 = p;
    if (globalconfig.pow1<30 && globalconfig.pow1>254) globalconfig.pow1=200;

    findparam(buf,"POW2",aaa);
    p=atoi(aaa);
    globalconfig.pow2 = p;
    if (globalconfig.pow2<30 && globalconfig.pow2>254) globalconfig.pow2=200;

    findparam(buf,"POW3",aaa);
    p=atoi(aaa);
    globalconfig.pow3 = p;
    if (globalconfig.pow3<30 && globalconfig.pow3>254) globalconfig.pow3=200;


    findparam(buf,"PROJE",aaa);
    p=atoi(aaa);
    globalconfig.project_number = p;

    findparam(buf,"OUT",aaa);
    p=atoi(aaa);
    netconfig.wan_type = (home_wan_type_t)p;

    findparam(buf,"CHAN",aaa);
    p=atoi(aaa);
    netconfig.channel = p;

    findparam(buf,"WRET",aaa);
    p=atoi(aaa);
    netconfig.WIFI_MAXIMUM_RETRY = p;

    findparam(buf,"TUS1",aaa);
    p=atoi(aaa);
    globalconfig.tus1 = p;

    findparam(buf,"TUS2",aaa);
    p=atoi(aaa);
    globalconfig.tus2 = p;

    findparam(buf,"TUS3",aaa);
    p=atoi(aaa);
    globalconfig.tus3 = p;

    findparam(buf,"GETIP",aaa);
    p=atoi(aaa);
    netconfig.ipstat = (home_ipstat_type_t)p;

    findparam(buf,"IP",aaa);
    strcpy((char *)netconfig.ip,aaa);

    findparam(buf,"NETMASK",aaa);
    strcpy((char *)netconfig.netmask,aaa);

    findparam(buf,"GATEWAY",aaa);
    strcpy((char *)netconfig.gateway,aaa);

    findparam(buf,"SSID",aaa);
    replacechar(aaa,'+',' ');
    strcpy((char *)netconfig.wifi_ssid,aaa);

    findparam(buf,"PASS",aaa);
    replacechar(aaa,'+',' ');
    strcpy((char *)netconfig.wifi_pass,aaa);

    findparam(buf,"DNS1",aaa);
    strcpy((char *)netconfig.dns,aaa);

    findparam(buf,"DNS2",aaa);
    strcpy((char *)netconfig.backup_dns,aaa);

    findparam(buf,"WEB",aaa);
    if (strcmp(aaa,"on")==0) globalconfig.http_start = 1; else globalconfig.http_start = 0;

    findparam(buf,"TCP",aaa);
    if (strcmp(aaa,"on")==0) globalconfig.tcpserver_start = 1; else globalconfig.tcpserver_start = 0;

    findparam(buf,"UDP",aaa);
    if (strcmp(aaa,"on")==0) globalconfig.daliserver_start = 1; else globalconfig.daliserver_start = 0;

    findparam(buf,"TSYNC",aaa);
    if (strcmp(aaa,"on")==0) globalconfig.time_sync = 1; else globalconfig.time_sync = 0;




    callback(netconfig, globalconfig );

    char *bb = (char *)calloc(1,2000);
    getBack(bb);
    httpd_resp_sendstr(req, bb);
    free(bb);
    printf("Restarting now.\n");
    fflush(stdout);

    int id = 1;
    TimerHandle_t tmr = xTimerCreate("MyTimer", pdMS_TO_TICKS(500), pdTRUE, ( void * )id, &timerCallBack);
    if( xTimerStart(tmr, 10 ) != pdPASS ) {
     printf("Timer start error");
    }

    return ESP_OK;
}

static esp_err_t default_handler(httpd_req_t *req)
{
    ESP_ERROR_CHECK(esp_event_post(SYSTEM_EVENTS, SYSTEM_DEFAULT_RESET, NULL, 0, portMAX_DELAY));
    
    char *bb = (char *)calloc(1,2000);
    getBack(bb);
    httpd_resp_sendstr(req, bb);
    free(bb);
    printf("Restarting now.\n");
    fflush(stdout);

    int id = 1;
    TimerHandle_t tmr = xTimerCreate("MyTimer", pdMS_TO_TICKS(500), pdTRUE, ( void * )id, &timerCallBack);
    if( xTimerStart(tmr, 10 ) != pdPASS ) {
     printf("Timer start error");
    }
    
    return ESP_OK;
}

//--------------------------------------------------
/* Max length a file path can have on storage */
//#define FILE_PATH_MAX (ESP_VFS_PATH_MAX + CONFIG_SPIFFS_OBJ_NAME_LEN)

/* Max size of an individual file. Make sure this
 * value is same as that set in upload_script.html */
#define MAX_FILE_SIZE   (200*1024) // 200 KB
#define MAX_FILE_SIZE_STR "200KB"

/* Scratch buffer size */
//#define SCRATCH_BUFSIZE  8192
struct file_server_data {
    /* Base path of file storage */
    char base_path[ESP_VFS_PATH_MAX + 1];

    /* Scratch buffer for temporary storage during file transfer */
    char scratch[SCRATCH_BUFSIZE];
};



static const char* get_path_from_uri(char *dest, const char *base_path, const char *uri, size_t destsize)
{
    const size_t base_pathlen = strlen(base_path);
    size_t pathlen = strlen(uri);

    const char *quest = strchr(uri, '?');
    if (quest) {
        pathlen = MIN(pathlen, quest - uri);
    }
    const char *hash = strchr(uri, '#');
    if (hash) {
        pathlen = MIN(pathlen, hash - uri);
    }

    if (base_pathlen + pathlen + 1 > destsize) {
        /* Full path string won't fit into destination buffer */
        return NULL;
    }

    /* Construct full path (base + path) */
    strcpy(dest, base_path);
    strlcpy(dest + base_pathlen, uri, pathlen + 1);

    /* Return pointer to path, skipping the base */
    return dest + base_pathlen;
}

static esp_err_t upload_post_handler(httpd_req_t *req)
{
    char filepath[FILE_PATH_MAX];
    FILE *fd = NULL;
    //struct stat file_stat;

    /* Skip leading "/upload" from URI to get filename */
    /* Note sizeof() counts NULL termination hence the -1 */
    const char *filename = get_path_from_uri(filepath, ((struct file_server_data *)req->user_ctx)->base_path,
                                             req->uri + sizeof("/upload") - 1, sizeof(filepath));
    if (!filename) {
        /* Respond with 500 Internal Server Error */
        httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Filename too long");
        return ESP_FAIL;
    }

    /* Filename cannot have a trailing '/' */
    if (filename[strlen(filename) - 1] == '/') {
        ESP_LOGE(REST_TAG, "Invalid filename : %s", filename);
        httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Invalid filename");
        return ESP_FAIL;
    }
    
    /*
    if (stat(filepath, &file_stat) == 0) {
        ESP_LOGE(REST_TAG, "File already exists : %s", filepath);
         Respond with 400 Bad Request 
        httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "File already exists");
        return ESP_FAIL;
    }
    */

    /* File cannot be larger than a limit */
    if (req->content_len > MAX_FILE_SIZE) {
        ESP_LOGE(REST_TAG, "File too large : %d bytes", req->content_len);
        /* Respond with 400 Bad Request */
        httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST,
                            "File size must be less than "
                            MAX_FILE_SIZE_STR "!");
        /* Return failure to close underlying connection else the
         * incoming file content will keep the socket busy */
        return ESP_FAIL;
    }

    fd = fopen(filepath, "w");
    if (!fd) {
        ESP_LOGE(REST_TAG, "Failed to create file : %s", filepath);
        /* Respond with 500 Internal Server Error */
        httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Failed to create file");
        return ESP_FAIL;
    }

    ESP_LOGI(REST_TAG, "Receiving file : %s...", filename);

    /* Retrieve the pointer to scratch buffer for temporary storage */
    char *buf = ((struct file_server_data *)req->user_ctx)->scratch;
    int received;

    /* Content length of the request gives
     * the size of the file being uploaded */
    int remaining = req->content_len;

    while (remaining > 0) {

        ESP_LOGI(REST_TAG, "Remaining size : %d", remaining);
        /* Receive the file part by part into a buffer */
        if ((received = httpd_req_recv(req, buf, MIN(remaining, SCRATCH_BUFSIZE))) <= 0) {
            if (received == HTTPD_SOCK_ERR_TIMEOUT) {
                /* Retry if timeout occurred */
                continue;
            }

            /* In case of unrecoverable error,
             * close and delete the unfinished file*/
            fclose(fd);
            unlink(filepath);

            ESP_LOGE(REST_TAG, "File reception failed!");
            /* Respond with 500 Internal Server Error */
            httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Failed to receive file");
            return ESP_FAIL;
        }

        /* Write buffer content to file on storage */
        if (received && (received != fwrite(buf, 1, received, fd))) {
            /* Couldn't write everything to file!
             * Storage may be full? */
            fclose(fd);
            unlink(filepath);

            ESP_LOGE(REST_TAG, "File write failed!");
            /* Respond with 500 Internal Server Error */
            httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Failed to write file to storage");
            return ESP_FAIL;
        }

        /* Keep track of remaining size of
         * the file left to be uploaded */
        remaining -= received;
    }

    /* Close file upon upload completion */
    fclose(fd);
    ESP_LOGI(REST_TAG, "File reception complete");

    /* Redirect onto root to see the updated file list */
    httpd_resp_set_status(req, "303 See Other");
    httpd_resp_set_hdr(req, "Location", "/");
    /*
#ifdef CONFIG_EXAMPLE_HTTPD_CONN_CLOSE_HEADER
    httpd_resp_set_hdr(req, "Connection", "close");
#endif
*/
    httpd_resp_sendstr(req, "File uploaded successfully");
    return ESP_OK;
}


esp_err_t start_rest_server(const char *base_path, 
                            home_network_config_t wifi, 
                            config_t glb,
                            web_callback_t cb)
{
    netconfig = wifi;
    globalconfig = glb;
    callback = cb;

    rest_server_context_t *rest_context = (rest_server_context_t *)calloc(1, sizeof(rest_server_context_t));
    if(rest_context==NULL) {printf("No memory for rest context"); return ESP_FAIL;}
    strlcpy(rest_context->base_path, base_path, sizeof(rest_context->base_path));

    httpd_handle_t server = NULL;
    httpd_config_t config = HTTPD_DEFAULT_CONFIG();
    config.uri_match_fn = httpd_uri_match_wildcard;

    ESP_LOGI(REST_TAG, "Starting HTTP Server");
    if (httpd_start(&server, &config)!=ESP_OK) {printf("Start server failed"); free(rest_context); return ESP_FAIL;}

    httpd_uri_t default_uri = {
        .uri = "/api/default",
        .method = HTTP_POST,
        .handler = default_handler,
        .user_ctx = rest_context
    };
    httpd_register_uri_handler(server, &default_uri);

    httpd_uri_t setup_uri = {
        .uri = "/setup.html",
        .method = HTTP_POST,
        //.handler = setup_handler,
		.handler = rest_common_get_handler,
        .user_ctx = rest_context
    };
    httpd_register_uri_handler(server, &setup_uri);

    httpd_uri_t register_uri = {
        .uri = "/register.html",
        .method = HTTP_POST,
        .handler = register_handler,
        .user_ctx = rest_context
    };
    httpd_register_uri_handler(server, &register_uri);

    httpd_uri_t save_uri = {
        .uri = "/api/save",
        .method = HTTP_POST,
        .handler = save_handler,
        .user_ctx = rest_context
    };
    httpd_register_uri_handler(server, &save_uri);
    /*
    httpd_uri_t registerCall_uri = {
        .uri = "/api/register",
        .method = HTTP_POST,
        .handler = registerCall_handler,
        .user_ctx = rest_context
    };
    httpd_register_uri_handler(server, &registerCall_uri);
    */

    httpd_uri_t common_get_uri = {
        .uri = "/*",
        .method = HTTP_GET,
        .handler = rest_common_get_handler,
        .user_ctx = rest_context
    };
    httpd_register_uri_handler(server, &common_get_uri);

    httpd_uri_t common_post_uri = {
        .uri = "/upload.html",
        .method = HTTP_POST,
        .handler = rest_common_get_handler,
        .user_ctx = rest_context
    };
    httpd_register_uri_handler(server, &common_post_uri);


    httpd_uri_t file_upload = {
        .uri       = "/upload/*",   // Match all URIs of type /upload/path/to/file
        .method    = HTTP_POST,
        .handler   = upload_post_handler,
        .user_ctx  = rest_context    // Pass server data as context
    };
    httpd_register_uri_handler(server, &file_upload);


    return ESP_OK;
    
}
