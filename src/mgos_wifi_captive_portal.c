/*
 * Copyright (c) 2018 Myles McNamara
 * All rights reserved
 *
 * Licensed under the Apache License, Version 2.0 (the ""License"");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an ""AS IS"" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */
#include <stdlib.h>
#include <string.h>

#include "common/json_utils.h"
#include "mgos_rpc.h"
#include "mgos_wifi.h"
#include "mgos_utils.h"
#include "mgos_timers.h"
#include "mgos_config.h"
#include "mgos_mongoose.h"
#include "mgos_wifi_captive_portal.h"

#include "mongoose.h"

static const char *s_ap_ip = "192.168.4.1";
static const char *s_portal_hostname = "setup.device.local";
static const char *s_listening_addr = "udp://:53";
static char *s_test_ssid = NULL;
static char *s_test_pass = NULL;

static int s_serve_gzip;
static int s_connection_retries = 0;
static int s_captive_portal_init = 0;
static int s_captive_portal_rpc_init = 0;

static struct mg_serve_http_opts s_http_server_opts;
static struct mgos_config_wifi_sta *sp_test_sta_vals = NULL;

static void remove_event_handlers(void);
static void add_event_handlers(void);

char *get_redirect_url(void){
    static char redirect_url[256];
    // Set URI as HTTPS if ssl cert configured, otherwise use http
    c_snprintf(redirect_url, sizeof redirect_url, "%s://%s", (mgos_sys_config_get_http_ssl_cert() ? "https" : "http"), s_portal_hostname);
    return redirect_url;
}

static void redirect_ev_handler(struct mg_connection *nc, int ev, void *ev_data, void *user_data){

    if (ev != MG_EV_HTTP_REQUEST)
        return;

    char *redirect_url = get_redirect_url();

    LOG(LL_INFO, ("Redirecting to %s for Captive Portal", redirect_url ) );
    mg_http_send_redirect(nc, 302, mg_mk_str(redirect_url), mg_mk_str(NULL));

    (void)ev_data;
    (void)user_data;
}

static void dns_ev_handler(struct mg_connection *c, int ev, void *ev_data,
                                    void *user_data){
    struct mg_dns_message *msg = (struct mg_dns_message *)ev_data;
    struct mbuf reply_buf;
    int i;

    if (ev != MG_DNS_MESSAGE)
        return;

    mbuf_init(&reply_buf, 512);
    struct mg_dns_reply reply = mg_dns_create_reply(&reply_buf, msg);
    for (i = 0; i < msg->num_questions; i++)
    {
        char rname[256];
        struct mg_dns_resource_record *rr = &msg->questions[i];
        mg_dns_uncompress_name(msg, &rr->name, rname, sizeof(rname) - 1);
        // LOG( LL_INFO, ( "Q type %d name %s\n", rr->rtype, rname ) );
        if (rr->rtype == MG_DNS_A_RECORD)
        {
            LOG(LL_DEBUG, ("DNS A Query for %s sending IP %s", rname, s_ap_ip));
            uint32_t ip = inet_addr(s_ap_ip);
            mg_dns_reply_record(&reply, rr, NULL, rr->rtype, 10, &ip, 4);
        }
    }
    mg_dns_send_reply(c, &reply);
    mbuf_free(&reply_buf);
    (void)user_data;
}

static void ip_aquired_cb(int ev, void *ev_data, void *userdata){
    char *connectedto = mgos_wifi_get_connected_ssid();
    // struct mgos_config_wifi_sta *sta = (struct mgos_config_wifi_sta *) sp_test_sta_vals;

    mgos_event_trigger(MGOS_WIFI_CAPTIVE_PORTAL_TEST_SUCCESS, sp_test_sta_vals);
    LOG(LL_INFO, ("Wifi Captive Portal IP Aquired from SSID %s", connectedto ) );
    free(connectedto);

    if ( sp_test_sta_vals != NULL && mgos_sys_config_get_portal_wifi_copy() ){
        LOG(LL_INFO, ("Copying SSID %s and Password %s to STA 1 config (wifi.sta)", sp_test_sta_vals->ssid, sp_test_sta_vals->pass ) );

        mgos_sys_config_set_wifi_sta_enable(true);
        mgos_sys_config_set_wifi_sta_ssid(sp_test_sta_vals->ssid);
        mgos_sys_config_set_wifi_sta_pass(sp_test_sta_vals->pass);

        int disable = mgos_sys_config_get_portal_wifi_disable();
        if ( disable == 1 || disable == 2 ){
            mgos_sys_config_set_wifi_ap_enable(false);
        }
        // Disable captive portal
        if( disable == 2 ){
            mgos_sys_config_set_portal_wifi_enable(false);
        }

        char *err = NULL;
        if (!save_cfg(&mgos_sys_config, &err)){
            LOG(LL_ERROR, ("Copy STA Values, Save Config Error: %s", err));
            free(err);
        } else {
            int reboot_ms = (mgos_sys_config_get_portal_wifi_reboot() * 1000);
            if (reboot_ms > 0){
                mgos_system_restart_after(reboot_ms);
            }
        }

    }

    remove_event_handlers();
    
    (void)ev;
    (void)ev_data;
    (void)userdata;
}

static void maybe_reconnect(int ev, void *ev_data, void *userdata){
    s_connection_retries++;
    LOG(LL_INFO, ("Wifi Captive Portal - Retrying Connection... Attempt %d", s_connection_retries ) );
    // Soooo ... if we call Sys.GetInfo before attempting to make test connection to STA
    // we will constantly get a DISCONNECTED and it will never connect ... not sure why or what to do about it
    mgos_wifi_connect();

    if (s_connection_retries > 15){
        remove_event_handlers();
        mgos_event_trigger(MGOS_WIFI_CAPTIVE_PORTAL_TEST_FAILED, sp_test_sta_vals);
    }
}

static void remove_event_handlers(void){
    mgos_event_remove_handler(MGOS_WIFI_EV_STA_DISCONNECTED, maybe_reconnect, NULL);
    mgos_event_remove_handler(MGOS_WIFI_EV_STA_IP_ACQUIRED, ip_aquired_cb, NULL);
}

static void add_event_handlers(void){
    // We use NULL for userdata to make sure they are removed correctly
    mgos_event_add_handler(MGOS_WIFI_EV_STA_IP_ACQUIRED, ip_aquired_cb, NULL);
    mgos_event_add_handler(MGOS_WIFI_EV_STA_DISCONNECTED, maybe_reconnect, NULL);
}

static void http_msg_print(const struct http_message *msg){
    // LOG(LL_INFO, ("     message: \"%.*s\"\n", msg->message.len, msg->message.p));
    LOG(LL_DEBUG, ("      method: \"%.*s\"", msg->method.len, msg->method.p));
    LOG(LL_DEBUG, ("         uri: \"%.*s\"", msg->uri.len, msg->uri.p));
}

static void root_handler(struct mg_connection *nc, int ev, void *p, void *user_data){
    (void)user_data;
    if (ev != MG_EV_HTTP_REQUEST)
        return;

    struct http_message *msg = (struct http_message *)(p);
    http_msg_print(msg);

    // Init our http server options (set in mgos_wifi_captive_portal_start)
    struct mg_serve_http_opts opts;
    memcpy(&opts, &s_http_server_opts, sizeof(opts));

    // Check Host header for our hostname (to serve captive portal)
    struct mg_str *hhdr = mg_get_http_header(msg, "Host");

    if (hhdr != NULL && strstr(hhdr->p, s_portal_hostname) != NULL){
        // TODO: check Accept-Encoding header for gzip before serving gzip
        LOG(LL_INFO, ("Root Handler -- Host matches Captive Portal Host \n"));
        // Check if gzip file was requested
        struct mg_str uri = mg_mk_str_n(msg->uri.p, msg->uri.len);
        bool gzip = strncmp(uri.p + uri.len - 3, ".gz", 3) == 0;
        // Check if URI is root directory --  /wifi_portal.min.js.gz HTTP/1.1
        bool uriroot = strncmp(uri.p, "/ HTTP", 6) == 0;

        // If gzip file requested (js/css) set Content-Encoding
        if (gzip){
            LOG(LL_INFO, ("Root Handler -- gzip Asset Requested -- Adding Content-Encoding Header \n"));
            opts.extra_headers = "Content-Encoding: gzip";
        }

        if (uriroot){
            LOG(LL_INFO, ("\nRoot Handler -- Captive Portal Root Requested\n"));

            if( s_serve_gzip ){
                opts.index_files = "wifi_portal.min.html.gz";
                LOG(LL_INFO, ("Root Handler -- Captive Portal Serving GZIP HTML \n"));
                mg_http_serve_file(nc, msg, "wifi_portal.min.html.gz", mg_mk_str("text/html"), mg_mk_str("Content-Encoding: gzip"));
                return;
            } else {
                opts.index_files = "wifi_portal.html";
                LOG(LL_INFO, ("Root Handler -- Captive Portal Serving HTML \n"));
                mg_http_serve_file(nc, msg, "wifi_portal.html", mg_mk_str("text/html"), mg_mk_str("Access-Control-Allow-Origin: *"));
                return;
            }

        } else {
            LOG(LL_DEBUG, ("\n Not URI Root, Actual: %s - %d\n", uri.p, uriroot));
        }

    } else {

        LOG(LL_INFO, ("Root Handler -- Checking for CaptivePortal UserAgent"));

        // Check User-Agent string for "CaptiveNetworkSupport" to issue redirect (AFTER checking for Captive Portal Host)
        struct mg_str *uahdr = mg_get_http_header(msg, "User-Agent");
        if (uahdr != NULL){
            // LOG(LL_INFO, ("Root Handler -- Found USER AGENT: %s \n", uahdr->p));

            if (strstr(uahdr->p, "CaptiveNetworkSupport") != NULL){
                LOG(LL_INFO, ("Root Handler -- Found USER AGENT CaptiveNetworkSupport -- Sending Redirect!\n"));
                redirect_ev_handler(nc, ev, p, user_data);
                return;
            }
        }
    }

    // Serve non-root requested file
    mg_serve_http(nc, msg, opts);
}

static void mgos_wifi_captive_portal_save_rpc_handler(struct mg_rpc_request_info *ri, void *cb_arg,
                                                      struct mg_rpc_frame_info *fi,
                                                      struct mg_str args){

    LOG(LL_INFO, ("WiFi.PortalSave RPC Handler Parsing JSON") );

    json_scanf(args.p, args.len, ri->args_fmt, &s_test_ssid, &s_test_pass );

    if (mgos_conf_str_empty(s_test_ssid)){
        mg_rpc_send_errorf(ri, 400, "SSID is required!" );
        return;
    }

    if (sp_test_sta_vals == NULL){
        // Allocate memory to store sta values in
        sp_test_sta_vals = (struct mgos_config_wifi_sta *)calloc(1, sizeof(*sp_test_sta_vals));
    }

    sp_test_sta_vals->enable = 1; // Same as (*test_sta_vals).enable
    sp_test_sta_vals->ssid = s_test_ssid;
    sp_test_sta_vals->pass = s_test_pass;

    // Make sure to remove any existing handlers (in case of previous RPC call)
    remove_event_handlers();

    LOG(LL_INFO, ("WiFi.PortalSave RPC Handler ssid: %s pass: %s", s_test_ssid, s_test_pass));

    mgos_wifi_disconnect();
    bool result = mgos_wifi_setup_sta(sp_test_sta_vals);
    mg_rpc_send_responsef(ri, "{ testing: %Q, result: %B }", s_test_ssid, result);

    if ( result ){
        mgos_event_trigger(MGOS_WIFI_CAPTIVE_PORTAL_TEST_START, sp_test_sta_vals);
        add_event_handlers();
    }

    (void)cb_arg;
    (void)fi;
}

bool mgos_wifi_captive_portal_start(void){

    if ( s_captive_portal_init ){
        LOG(LL_ERROR, ("Wifi captive portal already init! Ignoring call to start captive portal!"));
        return false;
    }

    // Not really sure how to handle this right now, since the AP can be brought up through code
    // so for now we don't mess with it

    // if( mgos_sys_config_get_wifi_ap_enable() ){
    //     LOG(LL_ERROR, ("Wifi captive portal not starting! You must enable AP - wifi.ap.enable"));
    //     return false;
    // }

    LOG(LL_INFO, ("Starting WiFi Captive Portal..."));

    /*
     *    TODO:
     *    Maybe need to figure out way to handle DNS for captive portal, if user has defined AP hostname,
     *    as WiFi lib automatically sets up it's own DNS responder for the hostname when one is set
     */
    // if (mgos_sys_config_get_wifi_ap_enable() && mgos_sys_config_get_wifi_ap_hostname() != NULL) {
    // }

    // Set IP address to respond to DNS queries with
    s_ap_ip = mgos_sys_config_get_wifi_ap_ip();
    // Set Hostname used for serving DNS captive portal
    s_portal_hostname = mgos_sys_config_get_portal_wifi_hostname();
    s_serve_gzip = mgos_sys_config_get_portal_wifi_gzip();

    // Bind DNS for Captive Portal
    struct mg_connection *dns_c = mg_bind(mgos_get_mgr(), s_listening_addr, dns_ev_handler, 0);
    mg_set_protocol_dns(dns_c);

    if (dns_c == NULL){
        LOG(LL_ERROR, ("Failed to initialize DNS listener"));
        return false;
    } else {
        LOG(LL_INFO, ("Captive Portal DNS Listening on %s", s_listening_addr));
    }

    // GZIP handling
    memset(&s_http_server_opts, 0, sizeof(s_http_server_opts));
    // s_http_server_opts.document_root = mgos_sys_config_get_http_document_root();
    // document_root should be root directory as portal files are copied directly to root directory (not in sub directory ... don't know how to do that with mos yet?)
    s_http_server_opts.document_root = "/";

    // Add GZIP mime types for HTML, JavaScript, and CSS files
    s_http_server_opts.custom_mime_types = ".html.gz=text/html; charset=utf-8,.js.gz=application/javascript; charset=utf-8,.css.gz=text/css; charset=utf-8";

    // CORS
    s_http_server_opts.extra_headers = "Access-Control-Allow-Origin: *";

    /**
     * Root handler to check for User-Agent captive portal support, check for our redirect hostname to serve portal HTML file,
     * and to serve CSS and JS assets to client (after matching hostname in Host header)
     */
    mgos_register_http_endpoint("/", root_handler, NULL);

    // captive.apple.com - DNS request for Mac OSX
    
    // Known HTTP GET requests to check for Captive Portal
    mgos_register_http_endpoint("/mobile/status.php", redirect_ev_handler, NULL);         // Android 8.0 (Samsung s9+)
    mgos_register_http_endpoint("/generate_204", redirect_ev_handler, NULL);              // Android
    mgos_register_http_endpoint("/gen_204", redirect_ev_handler, NULL);                   // Android 9.0
    mgos_register_http_endpoint("/ncsi.txt", redirect_ev_handler, NULL);                  // Windows
    mgos_register_http_endpoint("/hotspot-detect.html", redirect_ev_handler, NULL);       // iOS 8/9
    mgos_register_http_endpoint("/library/test/success.html", redirect_ev_handler, NULL); // iOS 8/9

    s_captive_portal_init = true;

    return true;
}

bool mgos_wifi_captive_portal_init_rpc(void){

    if( ! s_captive_portal_rpc_init ){
        // Add RPC
        struct mg_rpc *c = mgos_rpc_get_global();
        mg_rpc_add_handler(c, "WiFi.PortalSave", "{ssid: %Q, pass: %Q}", mgos_wifi_captive_portal_save_rpc_handler, NULL);
        s_captive_portal_rpc_init = true;
        return true;
    }

    return false;
}

bool mgos_wifi_captive_portal_init(void){
    mgos_event_register_base(MGOS_WIFI_CAPTIVE_PORTAL_EV_BASE, "Wifi Captive Portal");

    if( mgos_sys_config_get_portal_wifi_rpc() ){
        mgos_wifi_captive_portal_init_rpc();
    }

    // Check if config is set to enable captive portal on boot
    if (mgos_sys_config_get_portal_wifi_enable()){
        mgos_wifi_captive_portal_start();
    }

    return true;
}