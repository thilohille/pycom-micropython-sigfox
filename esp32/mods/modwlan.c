/*
 * Copyright (c) 2016, Pycom Limited.
 *
 * This software is licensed under the GNU GPL version 3 or any
 * later version, with permitted additional terms. For more information
 * see the Pycom Licence v1.0 document supplied with this file, or
 * available at https://www.pycom.io/opensource/licensing
 */

#include <stdint.h>
#include <stdbool.h>
#include <stdio.h>
#include <string.h>

#include "py/mpconfig.h"
#include "py/obj.h"
#include "py/objstr.h"
#include "py/runtime.h"
#include "py/stream.h"
#include "py/mphal.h"
#include "py/mperrno.h"
#include "py/misc.h"

#include "esp_heap_caps.h"
#include "sdkconfig.h"
#include "esp_system.h"
#include "esp_spi_flash.h"
#include "nvs_flash.h"
#include "esp_event.h"
#include "esp_wifi.h"
#include "esp_wifi_types.h"
#include "esp_event_loop.h"
#include "ff.h"
#include "esp_wpa2.h"

//#include "timeutils.h"
#include "netutils.h"
#include "modnetwork.h"
#include "modusocket.h"
#include "modwlan.h"
#include "pybioctl.h"
//#include "pybrtc.h"
#include "serverstask.h"
#include "mpexception.h"
#include "antenna.h"
#include "modussl.h"

#include "lwip/sockets.h"
#include "lwip/dns.h"
#include "lwip/netdb.h"
#include "lwipsocket.h"

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/event_groups.h"

/******************************************************************************
 DEFINE TYPES
 ******************************************************************************/

/******************************************************************************
 DEFINE CONSTANTS
 ******************************************************************************/
//#define CLR_STATUS_BIT_ALL(status)      (status = 0)
//#define SET_STATUS_BIT(status, bit)     (status |= ( 1 << (bit)))
//#define CLR_STATUS_BIT(status, bit)     (status &= ~(1 << (bit)))
//#define GET_STATUS_BIT(status, bit)     (0 != (status & (1 << (bit))))
//
//#define IS_NW_PROCSR_ON(status)         GET_STATUS_BIT(status, STATUS_BIT_NWP_INIT)
//#define IS_CONNECTED(status)            GET_STATUS_BIT(status, STATUS_BIT_CONNECTION)
//#define IS_IP_LEASED(status)            GET_STATUS_BIT(status, STATUS_BIT_IP_LEASED)
//#define IS_IP_ACQUIRED(status)          GET_STATUS_BIT(status, STATUS_BIT_IP_ACQUIRED)
//#define IS_SMART_CFG_START(status)      GET_STATUS_BIT(status, STATUS_BIT_SMARTCONFIG_START)
//#define IS_P2P_DEV_FOUND(status)        GET_STATUS_BIT(status, STATUS_BIT_P2P_DEV_FOUND)
//#define IS_P2P_REQ_RCVD(status)         GET_STATUS_BIT(status, STATUS_BIT_P2P_REQ_RECEIVED)
//#define IS_CONNECT_FAILED(status)       GET_STATUS_BIT(status, STATUS_BIT_CONNECTION_FAILED)
//#define IS_PING_DONE(status)            GET_STATUS_BIT(status, STATUS_BIT_PING_DONE)
//
//#define MODWLAN_SL_SCAN_ENABLE          1
//#define MODWLAN_SL_SCAN_DISABLE         0
//#define MODWLAN_SL_MAX_NETWORKS         20
//
//#define MODWLAN_MAX_NETWORKS            20
//#define MODWLAN_SCAN_PERIOD_S           3600     // 1 hour
//#define MODWLAN_WAIT_FOR_SCAN_MS        1050
//#define MODWLAN_CONNECTION_WAIT_MS      2
//
//#define ASSERT_ON_ERROR(x)              ASSERT((x) >= 0)
//
//#define IPV4_ADDR_STR_LEN_MAX           (16)

#define FILE_READ_SIZE                      256

/******************************************************************************
 DECLARE PRIVATE DATA
 ******************************************************************************/
wlan_obj_t wlan_obj = {
//        .mode = -1,
//        .status = 0,
//        .ip = 0,
//        .auth = MICROPY_PORT_WLAN_AP_SECURITY,
//        .channel = MICROPY_PORT_WLAN_AP_CHANNEL,
//        .ssid = MICROPY_PORT_WLAN_AP_SSID,
//        .key = MICROPY_PORT_WLAN_AP_KEY,
//        .mac = {0},
//        //.ssid_o = {0},
//        //.bssid = {0},
//    #if (MICROPY_PORT_HAS_TELNET || MICROPY_PORT_HAS_FTP)
//        .servers_enabled = false,
//    #endif
    .started = false
};
//
//STATIC const mp_irq_methods_t wlan_irq_methods;

typedef struct _wlan_if_obj_t {
    mp_obj_base_t base;
    int if_id;
} wlan_if_obj_t;

static EventGroupHandle_t wifi_event_group;


// Event bits
const int CONNECTED_BIT = BIT0;

/******************************************************************************
 DECLARE PUBLIC DATA
 ******************************************************************************/
//OsiLockObj_t wlan_LockObj; TODO

/******************************************************************************
 DECLARE PRIVATE FUNCTIONS
 ******************************************************************************/
//STATIC void wlan_reenable (SlWlanMode_t mode);
STATIC void wlan_servers_start (void);
STATIC void wlan_servers_stop (void);
//STATIC void wlan_reset (void);
STATIC void wlan_validate_mode (uint mode);
STATIC void wlan_set_mode (uint mode);
STATIC void wlan_setup_ap (const char *ssid, uint32_t auth, const char *key, uint32_t channel, bool add_mac, bool hidden);
STATIC void wlan_validate_ssid_len (uint32_t len);
STATIC uint32_t wlan_set_ssid_internal (const char *ssid, uint8_t len, bool add_mac);
STATIC void wlan_validate_security (uint8_t auth, const char *key);
STATIC void wlan_set_security_internal (uint8_t auth, const char *key);
STATIC void wlan_validate_channel (uint8_t channel);
STATIC void wlan_set_antenna (uint8_t antenna);
static esp_err_t wlan_event_handler(void *ctx, system_event_t *event);
STATIC void wlan_do_connect (const char* ssid, const char* bssid, const wifi_auth_mode_t auth, const char* key, int32_t timeout, const wlan_wpa2_ent_obj_t * const wpa2_ent);
//STATIC void wlan_get_sl_mac (void);
//STATIC void wlan_wep_key_unhexlify (const char *key, char *key_out);
//STATIC void wlan_lpds_irq_enable (mp_obj_t self_in);
//STATIC void wlan_lpds_irq_disable (mp_obj_t self_in);
//STATIC bool wlan_scan_result_is_unique (const mp_obj_list_t *nets, uint8_t *bssid);

//STATIC void wlan_event_handler_cb (System_Event_t *event);

static char *wlan_read_file (const char *file_path, vstr_t *vstr);
//*****************************************************************************
//
//! \brief The Function Handles WLAN Events
//!
//! \param[in]  pWlanEvent - Pointer to WLAN Event Info
//!
//! \return None
//!
//*****************************************************************************
void wlan_pre_init (void) {
    tcpip_adapter_init();
    wifi_event_group = xEventGroupCreate();
    ESP_ERROR_CHECK(esp_event_loop_init(wlan_event_handler, NULL));
    wlan_obj.base.type = (mp_obj_t)&mod_network_nic_type_wlan;
}

void wlan_setup (int32_t mode, const char *ssid, uint32_t auth, const char *key, uint32_t channel, uint32_t antenna, bool add_mac, bool hidden) {

    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));

    ESP_ERROR_CHECK(esp_wifi_set_storage(WIFI_STORAGE_RAM));

    // stop the servers
    wlan_servers_stop();

    MP_THREAD_GIL_EXIT();

    esp_wifi_get_mac(WIFI_IF_STA, wlan_obj.mac);

    wlan_set_antenna(antenna);
    wlan_set_mode(mode);

    if (mode != WIFI_MODE_STA) {
        const char* dns_server = "192.168.4.1" ;
        tcpip_adapter_dns_info_t dns_info;
        int v = ipaddr_aton(dns_server, &dns_info.ip) ;
        ESP_ERROR_CHECK(tcpip_adapter_set_dns_info(
            TCPIP_ADAPTER_IF_AP,
            TCPIP_ADAPTER_DNS_MAIN,
        &dns_info));        
        wlan_setup_ap (ssid, auth, key, channel, add_mac, hidden);
    }

    esp_wifi_start();

    MP_THREAD_GIL_ENTER();

    wlan_obj.started = true;

    // start the servers before returning
    wlan_servers_start();
}

void wlan_get_mac (uint8_t *macAddress) {
    if (macAddress) {
        memcpy (macAddress, wlan_obj.mac, sizeof(wlan_obj.mac));
    }
}

void wlan_get_ip (uint32_t *ip) {
//    if (ip) {
//        *ip = IS_IP_ACQUIRED(wlan_obj.status) ? wlan_obj.ip : 0;
//    }
}

//bool wlan_is_connected (void) {
////    return (GET_STATUS_BIT(wlan_obj.status, STATUS_BIT_CONNECTION) &&
////            (GET_STATUS_BIT(wlan_obj.status, STATUS_BIT_IP_ACQUIRED) || wlan_obj.mode != ROLE_STA));
//}

void wlan_set_current_time (uint32_t seconds_since_2000) {
//    timeutils_struct_time_t tm;
//    timeutils_seconds_since_2000_to_struct_time(seconds_since_2000, &tm);
//
//    SlDateTime_t sl_datetime = {0};
//    sl_datetime.sl_tm_day  = tm.tm_mday;
//    sl_datetime.sl_tm_mon  = tm.tm_mon;
//    sl_datetime.sl_tm_year = tm.tm_year;
//    sl_datetime.sl_tm_hour = tm.tm_hour;
//    sl_datetime.sl_tm_min  = tm.tm_min;
//    sl_datetime.sl_tm_sec  = tm.tm_sec;
//    sl_DevSet(SL_DEVICE_GENERAL_CONFIGURATION, SL_DEVICE_GENERAL_CONFIGURATION_DATE_TIME, sizeof(SlDateTime_t), (uint8_t *)(&sl_datetime));
}

void wlan_off_on (void) {
    // no need to lock the WLAN object on every API call since the servers and the MicroPtyhon
    // task have the same priority
//    wlan_reenable(wlan_obj.mode);
}

//*****************************************************************************
// DEFINE STATIC FUNCTIONS
//*****************************************************************************

STATIC esp_err_t wlan_event_handler(void *ctx, system_event_t *event) {
    switch(event->event_id) {
    case SYSTEM_EVENT_STA_START:
        break;
    case SYSTEM_EVENT_STA_CONNECTED:
    {
        system_event_sta_connected_t *_event = (system_event_sta_connected_t *)&event->event_info;
        memcpy(wlan_obj.bssid, _event->bssid, 6);
        wlan_obj.channel = _event->channel;
        wlan_obj.auth = _event->authmode;
    }
        break;
    case SYSTEM_EVENT_STA_GOT_IP:
        xEventGroupSetBits(wifi_event_group, CONNECTED_BIT);
        break;
    case SYSTEM_EVENT_STA_DISCONNECTED:
        xEventGroupClearBits(wifi_event_group, CONNECTED_BIT);
        system_event_sta_disconnected_t *disconn = &event->event_info.disconnected;
        switch (disconn->reason) {
            case WIFI_REASON_AUTH_FAIL:
                wlan_obj.disconnected = true;
                break;
            default:
                // let other errors through and try to reconnect.
                break;
        }
        if (!wlan_obj.disconnected) {
            wifi_mode_t mode;
            if (esp_wifi_get_mode(&mode) == ESP_OK) {
                if (mode & WIFI_MODE_STA) {
                    esp_wifi_connect();
                }
            }
        }
        break;
    default:
        break;
    }
    return ESP_OK;
}

//STATIC void wlan_reenable (SlWlanMode_t mode) {
////    // stop and start again
////    sl_LockObjLock (&wlan_LockObj, SL_OS_WAIT_FOREVER);
////    sl_Stop(SL_STOP_TIMEOUT);
////    wlan_clear_data();
////    wlan_obj.mode = sl_Start(0, 0, 0);
////    sl_LockObjUnlock (&wlan_LockObj);
////    ASSERT (wlan_obj.mode == mode);
//}

STATIC void wlan_servers_start (void) {
    // start the servers if they were enabled before
    if (wlan_obj.enable_servers) {
        servers_start();
    }
}

STATIC void wlan_servers_stop (void) {
    if (servers_are_enabled()) {
        wlan_obj.enable_servers = true;
    }

    // stop all other processes using the wlan engine
    if (wlan_obj.enable_servers) {
        servers_stop();
    }
}

STATIC void wlan_setup_ap (const char *ssid, uint32_t auth, const char *key, uint32_t channel, bool add_mac, bool hidden) {
    uint32_t ssid_len = wlan_set_ssid_internal (ssid, strlen(ssid), add_mac);
    wlan_set_security_internal(auth, key);

    // get the current config and then change it
    wifi_config_t config;
    esp_wifi_get_config(WIFI_IF_AP, &config);
    strcpy((char *)config.ap.ssid, (char *)wlan_obj.ssid);
    config.ap.ssid_len = ssid_len;
    config.ap.authmode = wlan_obj.auth;
    strcpy((char *)config.ap.password, (char *)wlan_obj.key);
    config.ap.channel = channel;
    wlan_obj.channel = channel;
    config.ap.max_connection = 4;
    config.ap.ssid_hidden = (uint8_t)hidden;
    esp_wifi_set_config(WIFI_IF_AP, &config);
}

//STATIC void wlan_reset (void) {
//    wlan_servers_stop();
////    wlan_reenable (wlan_obj.mode); FIXME
//    wlan_servers_start();
//}

STATIC void wlan_validate_mode (uint mode) {
    if (mode < WIFI_MODE_STA || mode > WIFI_MODE_APSTA) {
        nlr_raise(mp_obj_new_exception_msg(&mp_type_ValueError, mpexception_value_invalid_arguments));
    }
}

STATIC void wlan_set_mode (uint mode) {
    wlan_obj.mode = mode;
    esp_wifi_set_mode(mode);
    wifi_ps_type_t wifi_ps_type;
    if (mode != WIFI_MODE_STA || wlan_obj.pwrsave == false) {
        wifi_ps_type = WIFI_PS_NONE;
    } else {
        wifi_ps_type = WIFI_PS_MODEM;
    }
    // set the power saving mode
    esp_wifi_set_ps(wifi_ps_type);
}

STATIC void wlan_validate_ssid_len (uint32_t len) {
    if (len > MODWLAN_SSID_LEN_MAX) {
        nlr_raise(mp_obj_new_exception_msg(&mp_type_ValueError, mpexception_value_invalid_arguments));
    }
}

STATIC uint32_t wlan_set_ssid_internal (const char *ssid, uint8_t len, bool add_mac) {
    // save the ssid
    memcpy(&wlan_obj.ssid, ssid, len);
    // append the last 2 bytes of the MAC address, since the use of this functionality is under our control
    // we can assume that the lenght of the ssid is less than (32 - 5)
    if (add_mac) {
        snprintf((char *)&wlan_obj.ssid[len], sizeof(wlan_obj.ssid) - len, "-%02x%02x", wlan_obj.mac[4], wlan_obj.mac[5]);
        len += 5;
    }
    wlan_obj.ssid[len] = '\0';
    return len;
}

STATIC void wlan_validate_security (uint8_t auth, const char *key) {
    if (auth < WIFI_AUTH_WEP && auth > WIFI_AUTH_WPA2_ENTERPRISE) {
        goto invalid_args;
    }
//    if (auth == AUTH_WEP) {
//        for (mp_uint_t i = strlen(key); i > 0; i--) {
//            if (!unichar_isxdigit(*key++)) {
//                goto invalid_args;
//            }
//        }
//    }
    return;

invalid_args:
    nlr_raise(mp_obj_new_exception_msg(&mp_type_ValueError, mpexception_value_invalid_arguments));
}

STATIC void wlan_set_security_internal (uint8_t auth, const char *key) {
    wlan_obj.auth = auth;
//    uint8_t wep_key[32];
    if (key != NULL) {
        strcpy((char *)wlan_obj.key, key);
//        if (auth == SL_SEC_TYPE_WEP) {
//            wlan_wep_key_unhexlify(key, (char *)&wep_key);
//            key = (const char *)&wep_key;
//            len /= 2;
//        }
    } else {
        wlan_obj.key[0] = '\0';
    }
}

STATIC void wlan_validate_channel (uint8_t channel) {
    if (channel < 1 || channel > 11) {
        nlr_raise(mp_obj_new_exception_msg(&mp_type_ValueError, mpexception_value_invalid_arguments));
    }
}

STATIC void wlan_set_antenna (uint8_t antenna) {
    wlan_obj.antenna = antenna;
    antenna_select(antenna);
}

STATIC void wlan_validate_certificates (wlan_wpa2_ent_obj_t *wpa2_ent) {
    if ((wpa2_ent->client_key_path == NULL || wpa2_ent->client_cert_path == NULL) && wpa2_ent->username == NULL) {
        goto cred_error;
    } else if (wpa2_ent->client_key_path != NULL && wpa2_ent->client_cert_path != NULL && wpa2_ent->username != NULL) {
        goto cred_error;
    }

    if (wpa2_ent->identity == NULL) {
        nlr_raise(mp_obj_new_exception_msg(&mp_type_ValueError, "indentiy required for WPA2_ENT authentication"));
    } else if (strlen(wpa2_ent->identity) > 127) {
        nlr_raise(mp_obj_new_exception_msg_varg(&mp_type_ValueError, "invalid identity length %d", strlen(wpa2_ent->identity)));
    }

    return;

cred_error:
    nlr_raise(mp_obj_new_exception_msg(&mp_type_ValueError, "invalid WAP2_ENT credentials"));
}

STATIC void wlan_do_connect (const char* ssid, const char* bssid, const wifi_auth_mode_t auth, const char* key,
                             int32_t timeout, const wlan_wpa2_ent_obj_t * const wpa2_ent) {

    esp_wpa2_config_t wpa2_config = WPA2_CONFIG_INIT_DEFAULT();
    wifi_config_t wifi_config;
    memset(&wifi_config, 0, sizeof(wifi_config));

    // first close any active connections
    wlan_obj.disconnected = true;
    esp_wifi_disconnect();

    strcpy((char *)wlan_obj.ssid, ssid);
    strcpy((char *)wifi_config.sta.ssid, ssid);

    if (key) {
        strcpy((char *)wifi_config.sta.password, key);
        strcpy((char *)wlan_obj.key, key);
    }
    if (bssid) {
        memcpy(wifi_config.sta.bssid, bssid, sizeof(wifi_config.sta.bssid));
        wifi_config.sta.bssid_set = true;
    }

    wifi_config.sta.scan_method = WIFI_ALL_CHANNEL_SCAN;
    wifi_config.sta.sort_method = WIFI_CONNECT_AP_BY_SIGNAL;

    if (ESP_OK != esp_wifi_set_config(WIFI_IF_STA, &wifi_config)) {
        goto os_error;
    }

    if (auth == WIFI_AUTH_WPA2_ENTERPRISE) {
        // CA Certificate is not mandatory
        if (wpa2_ent->ca_certs_path != NULL) {
            if (wlan_read_file(wpa2_ent->ca_certs_path, &wlan_obj.vstr_ca)) {
                if (ESP_OK != esp_wifi_sta_wpa2_ent_set_ca_cert((unsigned char*)wlan_obj.vstr_ca.buf, (int)wlan_obj.vstr_ca.len)) {
                    goto os_error;
                }
            } else {
                goto invalid_file;
            }
        }

        // client certificate is necessary only in EAP-TLS method, this is ensured by wlan_validate_certificates() function
        if (wpa2_ent->client_key_path != NULL && wpa2_ent->client_cert_path != NULL) {
            if (wlan_read_file(wpa2_ent->client_key_path, &wlan_obj.vstr_key) && wlan_read_file(wpa2_ent->client_cert_path, &wlan_obj.vstr_cert)) {
                if (ESP_OK != esp_wifi_sta_wpa2_ent_set_cert_key((unsigned char*)wlan_obj.vstr_cert.buf, (int)wlan_obj.vstr_cert.len,
                                                                 (unsigned char*)wlan_obj.vstr_key.buf, (int)wlan_obj.vstr_key.len, NULL, 0)) {
                    goto os_error;
                }
            } else {
                goto invalid_file;
            }
        }

        if (ESP_OK != esp_wifi_sta_wpa2_ent_set_identity((unsigned char *)wpa2_ent->identity, strlen(wpa2_ent->identity))) {
            goto os_error;
        }

        if (wpa2_ent->username != NULL || key != NULL) {
            if (ESP_OK != esp_wifi_sta_wpa2_ent_set_username((unsigned char *)wpa2_ent->username, strlen(wpa2_ent->username))) {
                goto os_error;
            }

            if (ESP_OK != esp_wifi_sta_wpa2_ent_set_password((unsigned char *)key, strlen(key))) {
                goto os_error;
            }
        }

        if (ESP_OK != esp_wifi_sta_wpa2_ent_enable(&wpa2_config)) {
            goto os_error;
        }
    }

    if (ESP_OK != esp_wifi_connect()) {
        goto os_error;
    }

    wlan_obj.disconnected = false;
    wlan_obj.auth = auth;
    memcpy(&wlan_obj.wpa2_ent, wpa2_ent, sizeof(wlan_wpa2_ent_obj_t));

    return;

invalid_file:
    nlr_raise(mp_obj_new_exception_msg(&mp_type_ValueError, "invalid file path"));

os_error:
    nlr_raise(mp_obj_new_exception_msg(&mp_type_OSError, mpexception_os_operation_failed));

    // TODO Add timeout handling!!
}

STATIC char *wlan_read_file (const char *file_path, vstr_t *vstr) {
    vstr_init(vstr, FILE_READ_SIZE);
    char *filebuf = vstr->buf;
    mp_uint_t actualsize;
    mp_uint_t totalsize = 0;

    FIL fp;
    FRESULT res = f_open(&fp, file_path, FA_READ);
    if (res != FR_OK) {
        return NULL;
    }

    while (true) {
        FRESULT res = f_read(&fp, filebuf, FILE_READ_SIZE, (UINT *)&actualsize);
        if (res != FR_OK) {
            f_close(&fp);
            return NULL;
        }
        totalsize += actualsize;
        if (actualsize < FILE_READ_SIZE) {
            break;
        } else {
            filebuf = vstr_extend(vstr, FILE_READ_SIZE);
        }
    }
    f_close(&fp);

    vstr->len = totalsize;
    vstr_null_terminated_str(vstr);
    return vstr->buf;
}


//STATIC void wlan_get_sl_mac (void) {
//    // Get the MAC address
////    uint8_t macAddrLen = SL_MAC_ADDR_LEN;
////    sl_NetCfgGet(SL_MAC_ADDRESS_GET, NULL, &macAddrLen, wlan_obj.mac);
//}

//STATIC void wlan_wep_key_unhexlify (const char *key, char *key_out) {
//    byte hex_byte = 0;
//    for (mp_uint_t i = strlen(key); i > 0 ; i--) {
//        hex_byte += unichar_xdigit_value(*key++);
//        if (i & 1) {
//            hex_byte <<= 4;
//        } else {
//            *key_out++ = hex_byte;
//            hex_byte = 0;
//        }
//    }
//}

//STATIC void wlan_lpds_irq_enable (mp_obj_t self_in) {
//    wlan_obj_t *self = self_in;
//    self->irq_enabled = true;
//}
//
//STATIC void wlan_lpds_irq_disable (mp_obj_t self_in) {
//    wlan_obj_t *self = self_in;
//    self->irq_enabled = false;
//}
//
//STATIC int wlan_irq_flags (mp_obj_t self_in) {
//    wlan_obj_t *self = self_in;
//    return self->irq_flags;
//}
//
//STATIC bool wlan_scan_result_is_unique (const mp_obj_list_t *nets, uint8_t *bssid) {
//    for (int i = 0; i < nets->len; i++) {
//        // index 1 in the list is the bssid
//        mp_obj_str_t *_bssid = (mp_obj_str_t *)((mp_obj_tuple_t *)nets->items[i])->items[1];
//        if (!memcmp (_bssid->data, bssid, SL_BSSID_LENGTH)) {
//            return false;
//        }
//    }
//    return true;
//}

/******************************************************************************/
// Micro Python bindings; WLAN class

/// \class WLAN - WiFi driver

STATIC mp_obj_t wlan_init_helper(wlan_obj_t *self, const mp_arg_val_t *args) {
    // get the mode
    int8_t mode = args[0].u_int;
    wlan_validate_mode(mode);

    // get the ssid
    const char *ssid = NULL;
    if (args[1].u_obj != NULL) {
        ssid = mp_obj_str_get_str(args[1].u_obj);
        wlan_validate_ssid_len(strlen(ssid));
    }

    // get the auth config
    uint8_t auth = WIFI_AUTH_OPEN;
    const char *key = NULL;
    if (args[2].u_obj != mp_const_none) {
        mp_obj_t *sec;
        mp_obj_get_array_fixed_n(args[2].u_obj, 2, &sec);
        auth = mp_obj_get_int(sec[0]);
        key = mp_obj_str_get_str(sec[1]);
        if (strlen(key) < 8 || strlen(key) > 32) {
            nlr_raise(mp_obj_new_exception_msg(&mp_type_ValueError, "invalid key length"));
        }
        wlan_validate_security(auth, key);
    }

    // get the channel
    uint8_t channel = args[3].u_int;
    wlan_validate_channel(channel);

    // get the antenna type
    uint8_t antenna;
    if (args[4].u_obj == MP_OBJ_NULL) {
        // first gen module, so select the internal antenna
        if (micropy_hw_antenna_diversity_pin_num == MICROPY_FIRST_GEN_ANT_SELECT_PIN_NUM) {
            antenna = ANTENNA_TYPE_INTERNAL;
        } else {
            antenna = ANTENNA_TYPE_MANUAL;
        }
    } else if (args[4].u_obj == mp_const_none) {
        antenna = ANTENNA_TYPE_MANUAL;
    } else {
        antenna = mp_obj_get_int(args[4].u_obj);
    }
    antenna_validate_antenna(antenna);

    wlan_obj.pwrsave = args[5].u_bool;
    bool hidden = args[6].u_bool;

    if (mode != WIFI_MODE_STA) {
        if (ssid == NULL) {
            nlr_raise(mp_obj_new_exception_msg(&mp_type_ValueError, "AP SSID not given"));
        }
        if (auth == WIFI_AUTH_WPA2_ENTERPRISE) {
            nlr_raise(mp_obj_new_exception_msg(&mp_type_ValueError, "WPA2_ENT not supported in AP mode"));
        }
    }

    // initialize the wlan subsystem
    wlan_setup(mode, (const char *)ssid, auth, (const char *)key, channel, antenna, false, hidden);
    mod_network_register_nic(&wlan_obj);

    return mp_const_none;
}

STATIC const mp_arg_t wlan_init_args[] = {
    { MP_QSTR_id,                             MP_ARG_INT,  {.u_int = 0} },
    { MP_QSTR_mode,         MP_ARG_KW_ONLY  | MP_ARG_INT,  {.u_int = WIFI_MODE_STA} },
    { MP_QSTR_ssid,         MP_ARG_KW_ONLY  | MP_ARG_OBJ,  {.u_obj = MP_OBJ_NULL} },
    { MP_QSTR_auth,         MP_ARG_KW_ONLY  | MP_ARG_OBJ,  {.u_obj = mp_const_none} },
    { MP_QSTR_channel,      MP_ARG_KW_ONLY  | MP_ARG_INT,  {.u_int = 1} },
    { MP_QSTR_antenna,      MP_ARG_KW_ONLY  | MP_ARG_OBJ,  {.u_obj = MP_OBJ_NULL} },
    { MP_QSTR_power_save,   MP_ARG_KW_ONLY  | MP_ARG_BOOL, {.u_bool = false} },
    { MP_QSTR_hidden,       MP_ARG_KW_ONLY  | MP_ARG_BOOL, {.u_bool = false} },
};
STATIC mp_obj_t wlan_make_new(const mp_obj_type_t *type, mp_uint_t n_args, mp_uint_t n_kw, const mp_obj_t *all_args) {
    // parse args
    mp_map_t kw_args;
    mp_map_init_fixed_table(&kw_args, n_kw, all_args + n_args);
    // parse args
    mp_arg_val_t args[MP_ARRAY_SIZE(wlan_init_args)];
    mp_arg_parse_all(n_args, all_args, &kw_args, MP_ARRAY_SIZE(args), wlan_init_args, args);

    // setup the object
    wlan_obj_t *self = &wlan_obj;
    self->base.type = (mp_obj_t)&mod_network_nic_type_wlan;

    // give it to the sleep module
    //pyb_sleep_set_wlan_obj(self); // FIXME

    if (n_kw > 0 || !wlan_obj.started) {
        // check the peripheral id
        if (args[0].u_int != 0) {
            nlr_raise(mp_obj_new_exception_msg(&mp_type_OSError, mpexception_os_resource_not_avaliable));
        }
        // start the peripheral
        wlan_init_helper(self, &args[1]);
    }
    return (mp_obj_t)self;
}

STATIC mp_obj_t wlan_init(mp_uint_t n_args, const mp_obj_t *pos_args, mp_map_t *kw_args) {
    // parse args
    mp_arg_val_t args[MP_ARRAY_SIZE(wlan_init_args) - 1];
    mp_arg_parse_all(n_args - 1, pos_args + 1, kw_args, MP_ARRAY_SIZE(args), &wlan_init_args[1], args);
    return wlan_init_helper(pos_args[0], args);
}
STATIC MP_DEFINE_CONST_FUN_OBJ_KW(wlan_init_obj, 1, wlan_init);

mp_obj_t wlan_deinit(mp_obj_t self_in) {

    if (servers_are_enabled()) {
       wlan_servers_stop();
    }

    if (wlan_obj.started) {
        esp_wifi_stop();
        wlan_obj.started = false;
    }
    return mp_const_none;
}
STATIC MP_DEFINE_CONST_FUN_OBJ_1(wlan_deinit_obj, wlan_deinit);

STATIC mp_obj_t wlan_scan(mp_obj_t self_in) {
    STATIC const qstr wlan_scan_info_fields[] = {
        MP_QSTR_ssid, MP_QSTR_bssid, MP_QSTR_sec, MP_QSTR_channel, MP_QSTR_rssi
    };

    // check for the correct wlan mode
    if (wlan_obj.mode == WIFI_MODE_AP) {
        nlr_raise(mp_obj_new_exception_msg(&mp_type_OSError, mpexception_os_request_not_possible));
    }

    MP_THREAD_GIL_EXIT();
    esp_wifi_scan_start(NULL, true);
    MP_THREAD_GIL_ENTER();

    uint16_t ap_num;
    wifi_ap_record_t *ap_record_buffer;
    wifi_ap_record_t *ap_record;
    mp_obj_t nets = mp_obj_new_list(0, NULL);

    esp_wifi_scan_get_ap_num(&ap_num); // get the number of scanned APs

    if (ap_num > 0) {
        ap_record_buffer = pvPortMalloc(ap_num * sizeof(wifi_ap_record_t));
        if (ap_record_buffer == NULL) {
            mp_raise_OSError(MP_ENOMEM);
        }

        // get the scanned AP list
        if (ESP_OK == esp_wifi_scan_get_ap_records(&ap_num, (wifi_ap_record_t *)ap_record_buffer)) {
            for (int i = 0; i < ap_num; i++) {
                ap_record = &ap_record_buffer[i];
                mp_obj_t tuple[5];
                tuple[0] = mp_obj_new_str((const char *)ap_record->ssid, strlen((char *)ap_record->ssid), false);
                tuple[1] = mp_obj_new_bytes((const byte *)ap_record->bssid, sizeof(ap_record->bssid));
                tuple[2] = mp_obj_new_int(ap_record->authmode);
                tuple[3] = mp_obj_new_int(ap_record->primary);
                tuple[4] = mp_obj_new_int(ap_record->rssi);

                // add the network to the list
                mp_obj_list_append(nets, mp_obj_new_attrtuple(wlan_scan_info_fields, 5, tuple));
            }
        }
        vPortFree(ap_record_buffer);
    }

    return nets;
}
STATIC MP_DEFINE_CONST_FUN_OBJ_1(wlan_scan_obj, wlan_scan);

STATIC mp_obj_t wlan_connect(mp_uint_t n_args, const mp_obj_t *pos_args, mp_map_t *kw_args) {
    STATIC const mp_arg_t allowed_args[] = {
        { MP_QSTR_ssid,                 MP_ARG_REQUIRED | MP_ARG_OBJ, },
        { MP_QSTR_auth,                                   MP_ARG_OBJ, {.u_obj = mp_const_none} },
        { MP_QSTR_bssid,                MP_ARG_KW_ONLY  | MP_ARG_OBJ, {.u_obj = mp_const_none} },
        { MP_QSTR_timeout,              MP_ARG_KW_ONLY  | MP_ARG_OBJ, {.u_obj = mp_const_none} },
        { MP_QSTR_ca_certs,             MP_ARG_KW_ONLY  | MP_ARG_OBJ, {.u_obj = mp_const_none} },
        { MP_QSTR_keyfile,              MP_ARG_KW_ONLY  | MP_ARG_OBJ, {.u_obj = mp_const_none} },
        { MP_QSTR_certfile,             MP_ARG_KW_ONLY  | MP_ARG_OBJ, {.u_obj = mp_const_none} },
        { MP_QSTR_identity,             MP_ARG_KW_ONLY  | MP_ARG_OBJ, {.u_obj = mp_const_none} },
    };

    // check for the correct wlan mode
    if (wlan_obj.mode == WIFI_MODE_AP) {
        nlr_raise(mp_obj_new_exception_msg(&mp_type_OSError, mpexception_os_request_not_possible));
    }

    // parse args
    mp_arg_val_t args[MP_ARRAY_SIZE(allowed_args)];
    mp_arg_parse_all(n_args - 1, pos_args + 1, kw_args, MP_ARRAY_SIZE(allowed_args), allowed_args, args);

    // get the ssid
    const char *ssid = mp_obj_str_get_str(args[0].u_obj);
    wlan_validate_ssid_len(strlen(ssid));

    wlan_wpa2_ent_obj_t wpa2_ent = {NULL, NULL, NULL, NULL, NULL};

    // get the auth
    const char *key = NULL;
    const char *user = NULL;
    uint8_t auth = WIFI_AUTH_MAX;
    if (args[1].u_obj != mp_const_none) {
        mp_obj_t *sec;
        uint32_t a_len;
        mp_obj_get_array(args[1].u_obj, &a_len, &sec);
        if (a_len == 1) {
            auth = mp_obj_get_int(sec[0]);
            if (auth != WIFI_AUTH_WPA2_ENTERPRISE) {
                goto auth_error;
            }
        } else if (a_len == 2) {
            if (sec[0] != mp_const_none) {
                auth = mp_obj_get_int(sec[0]);
                if (auth == WIFI_AUTH_WPA2_ENTERPRISE) {
                    goto auth_error;
                }
            }
            key = mp_obj_str_get_str(sec[1]);
        } else if (a_len == 3) {
            auth = mp_obj_get_int(sec[0]);
            if (auth != WIFI_AUTH_WPA2_ENTERPRISE) {
                goto auth_error;
            }
            if (sec[1] != mp_const_none) {
                user = mp_obj_str_get_str(sec[1]);
            }
            if (sec[2] != mp_const_none) {
                key = mp_obj_str_get_str(sec[2]);
            }
        } else {
            goto auth_error;
        }
    }

    // get the bssid
    const char *bssid = NULL;
    if (args[2].u_obj != mp_const_none) {
        bssid = mp_obj_str_get_str(args[2].u_obj);
    }

    // get the timeout
    int32_t timeout = -1;
    if (args[3].u_obj != mp_const_none) {
        timeout = mp_obj_get_int(args[3].u_obj);
    }

    // get the ca_certificate
    if (args[4].u_obj != mp_const_none) {
        wpa2_ent.ca_certs_path = mp_obj_str_get_str(args[4].u_obj);
    }

    // get the private key
    if (args[5].u_obj != mp_const_none) {
        wpa2_ent.client_key_path = mp_obj_str_get_str(args[5].u_obj);
    }

    // get the client certificate
    if (args[6].u_obj != mp_const_none) {
        wpa2_ent.client_cert_path = mp_obj_str_get_str(args[6].u_obj);
    }

    // get the identity
    if (args[7].u_obj != mp_const_none) {
        wpa2_ent.identity = mp_obj_str_get_str(args[7].u_obj);
    }

    if (auth == WIFI_AUTH_WPA2_ENTERPRISE) {
        wpa2_ent.username = user;
        wlan_validate_certificates(&wpa2_ent);
    }

    // connect to the requested access point
    wlan_do_connect (ssid, bssid, auth, key, timeout, &wpa2_ent);

    return mp_const_none;

auth_error:
    nlr_raise(mp_obj_new_exception_msg(&mp_type_ValueError, "invalid authentication tuple"));
}
STATIC MP_DEFINE_CONST_FUN_OBJ_KW(wlan_connect_obj, 1, wlan_connect);

STATIC mp_obj_t wlan_disconnect(mp_obj_t self_in) {
    // check for the correct wlan mode
    if (wlan_obj.mode == WIFI_MODE_AP) {
        nlr_raise(mp_obj_new_exception_msg(&mp_type_OSError, mpexception_os_request_not_possible));
    }
    esp_wifi_disconnect();
    wlan_obj.disconnected = true;
    return mp_const_none;
}
STATIC MP_DEFINE_CONST_FUN_OBJ_1(wlan_disconnect_obj, wlan_disconnect);

STATIC mp_obj_t wlan_isconnected(mp_obj_t self_in) {
    wlan_if_obj_t *self = MP_OBJ_TO_PTR(self_in);
    if (self->if_id == WIFI_IF_STA) {
        tcpip_adapter_ip_info_t info;
        tcpip_adapter_get_ip_info(WIFI_IF_STA, &info);
        return mp_obj_new_bool(info.ip.addr != 0);
    } else {
        wifi_sta_list_t sta;
        esp_wifi_ap_get_sta_list(&sta);
        return mp_obj_new_bool(sta.num != 0);
}}
STATIC MP_DEFINE_CONST_FUN_OBJ_1(wlan_isconnected_obj, wlan_isconnected);

STATIC mp_obj_t wlan_status(size_t n_args, const mp_obj_t *args) {
    if (n_args == 1) {
        // no arguments: return None until link status is implemented
        return mp_const_none;
    }

    // one argument: return status based on query parameter
    switch ((uintptr_t)args[1]) {
        case (uintptr_t)MP_OBJ_NEW_QSTR(MP_QSTR_stations): {
            // return list of connected stations, only if in soft-AP mode
            mp_obj_t list = mp_obj_new_list(0, NULL);
            if (args[0] != WIFI_IF_STA){
                wifi_sta_list_t station_list;
                ESP_ERROR_CHECK(esp_wifi_ap_get_sta_list(&station_list));
                wifi_sta_info_t *stations = (wifi_sta_info_t*)station_list.sta;
                for (int i = 0; i < station_list.num; ++i) {
                    mp_obj_tuple_t *t = mp_obj_new_tuple(1, NULL);
                    t->items[0] = mp_obj_new_bytes(stations[i].mac, sizeof(stations[i].mac));
                    mp_obj_list_append(list, t);
                }
            }
            return list;
        }

        default:
            mp_raise_ValueError("unknown status param");
    }

    return mp_const_none;
}
STATIC MP_DEFINE_CONST_FUN_OBJ_VAR_BETWEEN(wlan_status_obj, 1, 2, wlan_status);

STATIC mp_obj_t wlan_ifconfig (mp_uint_t n_args, const mp_obj_t *pos_args, mp_map_t *kw_args) {
    STATIC const mp_arg_t wlan_ifconfig_args[] = {
        { MP_QSTR_id,               MP_ARG_INT,     {.u_int = 0} },
        { MP_QSTR_config,           MP_ARG_OBJ,     {.u_obj = MP_OBJ_NULL} },
    };

    // parse args
    mp_arg_val_t args[MP_ARRAY_SIZE(wlan_ifconfig_args)];
    mp_arg_parse_all(n_args - 1, pos_args + 1, kw_args, MP_ARRAY_SIZE(args), wlan_ifconfig_args, args);

    // check the interface id
    tcpip_adapter_if_t adapter_if;
    if (args[0].u_int > 1) {
        nlr_raise(mp_obj_new_exception_msg(&mp_type_OSError, mpexception_os_resource_not_avaliable));
    } else if (args[0].u_int == 0) {
        adapter_if = TCPIP_ADAPTER_IF_STA;
    } else {
        adapter_if = TCPIP_ADAPTER_IF_AP;
    }

    tcpip_adapter_dns_info_t dns_info;
    // get the configuration
    if (args[1].u_obj == MP_OBJ_NULL) {
        // get
        tcpip_adapter_ip_info_t ip_info;
        tcpip_adapter_get_dns_info(adapter_if, TCPIP_ADAPTER_DNS_MAIN, &dns_info);
        if (ESP_OK == tcpip_adapter_get_ip_info(adapter_if, &ip_info)) {
            mp_obj_t ifconfig[4] = {
                netutils_format_ipv4_addr((uint8_t *)&ip_info.ip.addr, NETUTILS_BIG),
                netutils_format_ipv4_addr((uint8_t *)&ip_info.netmask.addr, NETUTILS_BIG),
                netutils_format_ipv4_addr((uint8_t *)&ip_info.gw.addr, NETUTILS_BIG),
                netutils_format_ipv4_addr((uint8_t *)&dns_info.ip, NETUTILS_BIG)
            };
            return mp_obj_new_tuple(4, ifconfig);
        } else {
            nlr_raise(mp_obj_new_exception_msg(&mp_type_OSError, mpexception_os_operation_failed));
        }
    } else { // set the configuration
        if (MP_OBJ_IS_TYPE(args[1].u_obj, &mp_type_tuple)) {
            // set a static ip
            mp_obj_t *items;
            mp_obj_get_array_fixed_n(args[1].u_obj, 4, &items);

            tcpip_adapter_ip_info_t ip_info;
            netutils_parse_ipv4_addr(items[0], (uint8_t *)&ip_info.ip.addr, NETUTILS_BIG);
            netutils_parse_ipv4_addr(items[1], (uint8_t *)&ip_info.netmask.addr, NETUTILS_BIG);
            netutils_parse_ipv4_addr(items[2], (uint8_t *)&ip_info.gw.addr, NETUTILS_BIG);
            netutils_parse_ipv4_addr(items[3], (uint8_t *)&dns_info.ip, NETUTILS_BIG);

            if (adapter_if == TCPIP_ADAPTER_IF_STA) {
                tcpip_adapter_dhcpc_stop(TCPIP_ADAPTER_IF_STA);
                tcpip_adapter_set_ip_info(adapter_if, &ip_info);
                tcpip_adapter_set_dns_info(adapter_if, TCPIP_ADAPTER_DNS_MAIN, &dns_info);
            } else {
                tcpip_adapter_dhcps_stop(TCPIP_ADAPTER_IF_AP);
                tcpip_adapter_set_ip_info(adapter_if, &ip_info);
                tcpip_adapter_set_dns_info(adapter_if, TCPIP_ADAPTER_DNS_MAIN, &dns_info);
                tcpip_adapter_dhcps_start(TCPIP_ADAPTER_IF_AP);
            }
        } else {
            // check for the correct string
            const char *mode = mp_obj_str_get_str(args[1].u_obj);
            if (strcmp("dhcp", mode) && strcmp("auto", mode)) {
                nlr_raise(mp_obj_new_exception_msg(&mp_type_ValueError, mpexception_value_invalid_arguments));
            }

            if (ESP_OK != tcpip_adapter_dhcpc_start(adapter_if)) {
                nlr_raise(mp_obj_new_exception_msg(&mp_type_OSError, mpexception_os_operation_failed));
            }
        }
        return mp_const_none;
    }
}
STATIC MP_DEFINE_CONST_FUN_OBJ_KW(wlan_ifconfig_obj, 1, wlan_ifconfig);

STATIC mp_obj_t wlan_mode (mp_uint_t n_args, const mp_obj_t *args) {
    wlan_obj_t *self = args[0];
    if (n_args == 1) {
        return mp_obj_new_int(self->mode);
    } else {
        uint mode = mp_obj_get_int(args[1]);
        wlan_validate_mode(mode);
        wlan_set_mode(mode);
        return mp_const_none;
    }
}
STATIC MP_DEFINE_CONST_FUN_OBJ_VAR_BETWEEN(wlan_mode_obj, 1, 2, wlan_mode);

STATIC mp_obj_t wlan_ssid (mp_uint_t n_args, const mp_obj_t *args) {
    wlan_obj_t *self = args[0];
    if (n_args == 1) {
        return mp_obj_new_str((const char *)self->ssid, strlen((const char *)self->ssid), false);
    } else {
        const char *ssid = mp_obj_str_get_str(args[1]);
        wlan_validate_ssid_len(strlen(ssid));
        mp_uint_t ssid_len = wlan_set_ssid_internal (ssid, strlen(ssid), false);
        // get the current config and then change it
        wifi_config_t config;
        esp_wifi_get_config(WIFI_IF_AP, &config);
        memcpy((char *)config.ap.ssid, (char *)wlan_obj.ssid, ssid_len);
        config.ap.ssid_len = ssid_len;
        esp_wifi_set_config(WIFI_IF_AP, &config);
        return mp_const_none;
    }
}
STATIC MP_DEFINE_CONST_FUN_OBJ_VAR_BETWEEN(wlan_ssid_obj, 1, 2, wlan_ssid);

STATIC mp_obj_t wlan_bssid (mp_obj_t self_in) {
    wlan_obj_t *self = self_in;
    return mp_obj_new_bytes((const byte *)self->bssid, 6);
}
STATIC MP_DEFINE_CONST_FUN_OBJ_1(wlan_bssid_obj, wlan_bssid);

STATIC mp_obj_t wlan_auth (mp_uint_t n_args, const mp_obj_t *args) {
    wlan_obj_t *self = args[0];
    if (n_args == 1) {
        if (self->auth == WIFI_AUTH_OPEN) {
            return mp_const_none;
        } else {
            mp_obj_t security[2];
            security[0] = mp_obj_new_int(self->auth);
            security[1] = mp_obj_new_str((const char *)self->key, strlen((const char *)self->key), false);
            return mp_obj_new_tuple(2, security);
        }
    } else {
        // get the auth config
        uint8_t auth = WIFI_AUTH_OPEN;
        const char *key = NULL;
        if (args[1] != mp_const_none) {
            mp_obj_t *sec;
            mp_obj_get_array_fixed_n(args[1], 2, &sec);
            auth = mp_obj_get_int(sec[0]);
            key = mp_obj_str_get_str(sec[1]);
            wlan_validate_security(auth, key);
        }
        wlan_set_security_internal(auth, key);
        wifi_config_t config;
        esp_wifi_get_config(WIFI_IF_AP, &config);
        config.ap.authmode = wlan_obj.auth;
        strcpy((char *)config.ap.password, (char *)wlan_obj.key);
        esp_wifi_set_config(WIFI_IF_AP, &config);
        return mp_const_none;
    }
}
STATIC MP_DEFINE_CONST_FUN_OBJ_VAR_BETWEEN(wlan_auth_obj, 1, 2, wlan_auth);

STATIC mp_obj_t wlan_channel (mp_uint_t n_args, const mp_obj_t *args) {
    wlan_obj_t *self = args[0];
    if (n_args == 1) {
        return mp_obj_new_int(self->channel);
    } else {
        uint8_t channel  = mp_obj_get_int(args[1]);
        wlan_validate_channel(channel);
        wifi_config_t config;
        esp_wifi_get_config(WIFI_IF_AP, &config);
        config.ap.channel = channel;
        wlan_obj.channel = channel;
        esp_wifi_set_config(WIFI_IF_AP, &config);
        return mp_const_none;
    }
}
STATIC MP_DEFINE_CONST_FUN_OBJ_VAR_BETWEEN(wlan_channel_obj, 1, 2, wlan_channel);

STATIC mp_obj_t wlan_antenna (mp_uint_t n_args, const mp_obj_t *args) {
    wlan_obj_t *self = args[0];
    if (n_args == 1) {
        return mp_obj_new_int(self->antenna);
    } else {
        uint8_t antenna  = mp_obj_get_int(args[1]);
        antenna_validate_antenna(antenna);
        wlan_set_antenna(antenna);
        return mp_const_none;
    }
}
STATIC MP_DEFINE_CONST_FUN_OBJ_VAR_BETWEEN(wlan_antenna_obj, 1, 2, wlan_antenna);

STATIC mp_obj_t wlan_mac (mp_uint_t n_args, const mp_obj_t *args) {
    wlan_obj_t *self = args[0];
    if (n_args == 1) {
        return mp_obj_new_bytes((const byte *)self->mac, sizeof(self->mac));
    } else {
//        mp_buffer_info_t bufinfo;
//        mp_get_buffer_raise(args[1], &bufinfo, MP_BUFFER_READ);
//        if (bufinfo.len != 6) {
//            nlr_raise(mp_obj_new_exception_msg(&mp_type_ValueError, mpexception_value_invalid_arguments));
//        }
//        memcpy(self->mac, bufinfo.buf, SL_MAC_ADDR_LEN);
//        sl_NetCfgSet(SL_MAC_ADDRESS_SET, 1, SL_MAC_ADDR_LEN, (uint8_t *)self->mac);
//        wlan_reset(); FIXME
        return mp_const_none;
    }
}
STATIC MP_DEFINE_CONST_FUN_OBJ_VAR_BETWEEN(wlan_mac_obj, 1, 2, wlan_mac);

//STATIC mp_obj_t wlan_irq (mp_uint_t n_args, const mp_obj_t *pos_args, mp_map_t *kw_args) {
//    mp_arg_val_t args[mp_irq_INIT_NUM_ARGS];
//    mp_arg_parse_all(n_args - 1, pos_args + 1, kw_args, mp_irq_INIT_NUM_ARGS, mp_irq_init_args, args);
//
//    wlan_obj_t *self = pos_args[0];
//
//    // check the trigger, only one type is supported
//    if (mp_obj_get_int(args[0].u_obj) != MODWLAN_WIFI_EVENT_ANY) {
//        goto invalid_args;
//    }
//
//    // check the power mode
//    if (mp_obj_get_int(args[3].u_obj) != PYB_PWR_MODE_LPDS) {
//        goto invalid_args;
//    }
//
//    // create the callback
//    mp_obj_t _irq = mp_irq_new (self, args[2].u_obj, &wlan_irq_methods);
//    self->irq_obj = _irq;
//
//    // enable the irq just before leaving
//    wlan_lpds_irq_enable(self);
//
//    return _irq;
//
//invalid_args:
//    nlr_raise(mp_obj_new_exception_msg(&mp_type_ValueError, mpexception_value_invalid_arguments));
//}
//STATIC MP_DEFINE_CONST_FUN_OBJ_KW(wlan_irq_obj, 1, wlan_irq);

STATIC const mp_map_elem_t wlan_locals_dict_table[] = {
    { MP_OBJ_NEW_QSTR(MP_QSTR_init),                (mp_obj_t)&wlan_init_obj },
    { MP_OBJ_NEW_QSTR(MP_QSTR_deinit),              (mp_obj_t)&wlan_deinit_obj },
    { MP_OBJ_NEW_QSTR(MP_QSTR_scan),                (mp_obj_t)&wlan_scan_obj },
    { MP_OBJ_NEW_QSTR(MP_QSTR_connect),             (mp_obj_t)&wlan_connect_obj },
    { MP_OBJ_NEW_QSTR(MP_QSTR_disconnect),          (mp_obj_t)&wlan_disconnect_obj },
    { MP_OBJ_NEW_QSTR(MP_QSTR_isconnected),         (mp_obj_t)&wlan_isconnected_obj },
    { MP_OBJ_NEW_QSTR(MP_QSTR_status),              (mp_obj_t)&wlan_status_obj },
    { MP_OBJ_NEW_QSTR(MP_QSTR_ifconfig),            (mp_obj_t)&wlan_ifconfig_obj },
    { MP_OBJ_NEW_QSTR(MP_QSTR_mode),                (mp_obj_t)&wlan_mode_obj },
    { MP_OBJ_NEW_QSTR(MP_QSTR_ssid),                (mp_obj_t)&wlan_ssid_obj },
    { MP_OBJ_NEW_QSTR(MP_QSTR_bssid),               (mp_obj_t)&wlan_bssid_obj },
    { MP_OBJ_NEW_QSTR(MP_QSTR_auth),                (mp_obj_t)&wlan_auth_obj },
    { MP_OBJ_NEW_QSTR(MP_QSTR_channel),             (mp_obj_t)&wlan_channel_obj },
    { MP_OBJ_NEW_QSTR(MP_QSTR_antenna),             (mp_obj_t)&wlan_antenna_obj },
    { MP_OBJ_NEW_QSTR(MP_QSTR_mac),                 (mp_obj_t)&wlan_mac_obj },
//    { MP_OBJ_NEW_QSTR(MP_QSTR_irq),                 (mp_obj_t)&wlan_irq_obj },
    // { MP_OBJ_NEW_QSTR(MP_QSTR_connections),         (mp_obj_t)&wlan_connections_obj },
    // { MP_OBJ_NEW_QSTR(MP_QSTR_urn),                 (mp_obj_t)&wlan_urn_obj },

    // class constants
    { MP_OBJ_NEW_QSTR(MP_QSTR_STA),                 MP_OBJ_NEW_SMALL_INT(WIFI_MODE_STA) },
    { MP_OBJ_NEW_QSTR(MP_QSTR_AP),                  MP_OBJ_NEW_SMALL_INT(WIFI_MODE_AP) },
    { MP_OBJ_NEW_QSTR(MP_QSTR_STA_AP),              MP_OBJ_NEW_SMALL_INT(WIFI_MODE_APSTA) },
    { MP_OBJ_NEW_QSTR(MP_QSTR_WEP),                 MP_OBJ_NEW_SMALL_INT(WIFI_AUTH_WEP) },
    { MP_OBJ_NEW_QSTR(MP_QSTR_WPA),                 MP_OBJ_NEW_SMALL_INT(WIFI_AUTH_WPA_PSK) },
    { MP_OBJ_NEW_QSTR(MP_QSTR_WPA2),                MP_OBJ_NEW_SMALL_INT(WIFI_AUTH_WPA2_PSK) },
    { MP_OBJ_NEW_QSTR(MP_QSTR_WPA2_ENT),            MP_OBJ_NEW_SMALL_INT(WIFI_AUTH_WPA2_ENTERPRISE) },
    { MP_OBJ_NEW_QSTR(MP_QSTR_INT_ANT),             MP_OBJ_NEW_SMALL_INT(ANTENNA_TYPE_INTERNAL) },
    { MP_OBJ_NEW_QSTR(MP_QSTR_EXT_ANT),             MP_OBJ_NEW_SMALL_INT(ANTENNA_TYPE_EXTERNAL) },
//    { MP_OBJ_NEW_QSTR(MP_QSTR_ANY_EVENT),           MP_OBJ_NEW_SMALL_INT(MODWLAN_WIFI_EVENT_ANY) },
};
STATIC MP_DEFINE_CONST_DICT(wlan_locals_dict, wlan_locals_dict_table);

const mod_network_nic_type_t mod_network_nic_type_wlan = {
    .base = {
        { &mp_type_type },
        .name = MP_QSTR_WLAN,
        .make_new = wlan_make_new,
        .locals_dict = (mp_obj_t)&wlan_locals_dict,
    },

    .n_gethostbyname = lwipsocket_gethostbyname,
    .n_socket = lwipsocket_socket_socket,
    .n_close = lwipsocket_socket_close,
    .n_bind = lwipsocket_socket_bind,
    .n_listen = lwipsocket_socket_listen,
    .n_accept = lwipsocket_socket_accept,
    .n_connect = lwipsocket_socket_connect,
    .n_send = lwipsocket_socket_send,
    .n_recv = lwipsocket_socket_recv,
    .n_sendto = lwipsocket_socket_sendto,
    .n_recvfrom = lwipsocket_socket_recvfrom,
    .n_setsockopt = lwipsocket_socket_setsockopt,
    .n_settimeout = lwipsocket_socket_settimeout,
    .n_ioctl = lwipsocket_socket_ioctl,
};

//STATIC const mp_irq_methods_t wlan_irq_methods = {
//    .init = wlan_irq,
//    .enable = wlan_lpds_irq_enable,
//    .disable = wlan_lpds_irq_disable,
//    .flags = wlan_irq_flags,
//};
