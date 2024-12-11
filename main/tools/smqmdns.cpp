


#include "mdns.h"
#include "esp_mac.h"

const char *MDNS_HOSTNAME = "SMQ_01";

char *generate_hostname(void)
{
    uint8_t mac[6];
    char   *hostname;
    esp_read_mac(mac, ESP_MAC_WIFI_STA);
    if (-1 == asprintf(&hostname, "%s_%02X%02X", MDNS_HOSTNAME, mac[4], mac[5])) {
        abort();
    }
    return hostname;
}

void initialise_mdns(void)
{
    char *hostname = (char*)GlobalConfig.mdnshost;

    //initialize mDNS
    ESP_ERROR_CHECK( mdns_init() );
    ESP_ERROR_CHECK( mdns_hostname_set(hostname) );
    ESP_LOGI(TAG, "mdns hostname set to: [%s]", hostname);
    //set default mDNS instance name
    ESP_ERROR_CHECK( mdns_instance_name_set(hostname) );
    
    mdns_txt_item_t attr[1] = {          
        {"board", "SMQ.01"}
    };
    //initialize service
    ESP_ERROR_CHECK( mdns_service_add(hostname, "_http", "_tcp", 80, attr, 1) );
    ESP_ERROR_CHECK( mdns_service_subtype_add_for_host(hostname, "_http", "_tcp", NULL, "_server") );

    //free(hostname);
}


static const char *ip_protocol_str[] = {"V4", "V6", "MAX"};

static void mdns_print_results(mdns_result_t *results)
{
    mdns_result_t *r = results;
    mdns_ip_addr_t *a = NULL;
    int i = 1, t;

    cJSON *root = cJSON_CreateObject();
    cJSON_AddStringToObject(root, "com", "find_mdns");
    cJSON *gear = cJSON_CreateArray();
    cJSON_AddItemToObject(root, "gear", gear);

    while (r) {
        cJSON *Lm;
        cJSON_AddItemToArray(gear, Lm = cJSON_CreateObject());

        if (r->esp_netif) {
            printf("%d: Interface: %s, Type: %s, TTL: %" PRIu32 "\n", i++, esp_netif_get_ifkey(r->esp_netif),
                   ip_protocol_str[r->ip_protocol], r->ttl);
        }
        if (r->instance_name) {
            printf("  PTR : %s.%s.%s\n", r->instance_name, r->service_type, r->proto);
        }
        if (r->hostname) {
            printf("  SRV : %s.local:%u\n", r->hostname, r->port);
            cJSON_AddItemToObject(Lm, "nm", cJSON_CreateString(r->hostname));
        }
        if (r->txt_count) {
            printf("  TXT : [%zu] ", r->txt_count);
            
            cJSON *txt = cJSON_CreateArray();;
            cJSON_AddItemToObject(Lm, "txt", txt);

            for (t = 0; t < r->txt_count; t++) {
                printf("%s=%s(%d); ", r->txt[t].key, r->txt[t].value ? r->txt[t].value : "NULL", r->txt_value_len[t]);
                char *ddd;
                asprintf(&ddd,"%s >> %s",r->txt[t].key,r->txt[t].value);
                cJSON_AddItemToObject(txt, "key", cJSON_CreateString(ddd));
                free(ddd);
            }
            printf("\n");
        }
        a = r->addr;
        while (a) {
            if (a->addr.type == ESP_IPADDR_TYPE_V6) {
                printf("  AAAA: " IPV6STR "\n", IPV62STR(a->addr.u_addr.ip6));
            } else {
                printf("  A   : " IPSTR "\n", IP2STR(&(a->addr.u_addr.ip4)));
                char *aa;
                asprintf(&aa,IPSTR,IP2STR(&(a->addr.u_addr.ip4)));
                cJSON_AddItemToObject(Lm, "ip", cJSON_CreateString(aa));
                free(aa);
            }
            a = a->next;
        }
        r = r->next;
    }
    char *dat = cJSON_PrintUnformatted(root);
        tcpserver.Send(dat);
     printf("%s\n",dat);
        cJSON_free(dat);
        cJSON_Delete(root);
}

static void query_mdns_service(const char *service_name, const char *proto)
{
    ESP_LOGI(TAG, "Query PTR: %s.%s.local", service_name, proto);

    mdns_result_t *results = NULL;
    esp_err_t err = mdns_query_ptr(service_name, proto, 3000, 20,  &results);
    if (err) {
        ESP_LOGE(TAG, "Query Failed: %s", esp_err_to_name(err));
        return;
    }
    if (!results) {
        ESP_LOGW(TAG, "No results found!");
        return;
    }

    mdns_print_results(results);
    mdns_query_results_free(results);
}
