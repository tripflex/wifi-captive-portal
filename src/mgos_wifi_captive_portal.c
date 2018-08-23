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

#include "common/json_utils.h"
#include "mgos_rpc.h"
#include "mgos_wifi.h"
#include "mgos_timers.h"
#include "mgos_wifi_captive_portal.h"

static const char *s_ap_ip = "192.168.4.1";
static const char *s_portal_hostname = "setup.device.local";
static const char *s_listening_addr = "udp://:53";
static int s_serve_gzip;

static struct mg_serve_http_opts s_http_server_opts;

char *get_redirect_url(void)
{
    static char redirect_url[256];
    // Set URI as HTTPS if ssl cert configured, otherwise use http
    c_snprintf(redirect_url, sizeof redirect_url, "%s://%s", (mgos_sys_config_get_http_ssl_cert() ? "https" : "http"), s_portal_hostname);
    return redirect_url;
}

// Captive Portal 302 Redirect Handler
static void redirect_ev_handler(struct mg_connection *nc, int ev, void *ev_data, void *user_data)
{
    struct http_message *hm = (struct http_message *)ev_data;

    if (ev != MG_EV_HTTP_REQUEST)
        return;

    char *redirect_url = get_redirect_url();

    LOG(LL_INFO, ("Redirecting to %s for Captive Portal", redirect_url ) );
    mg_http_send_redirect(nc, 302, mg_mk_str(redirect_url), mg_mk_str(NULL));
    (void)user_data;
}

// Captive Portal DNS Handler
static void dns_ev_handler(struct mg_connection *c, int ev, void *ev_data,
                                    void *user_data)
{
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
        fprintf(stdout, "Q type %d name %s\n", rr->rtype, rname);
        if (rr->rtype == MG_DNS_A_RECORD)
        {
            LOG( LL_INFO, ( "DNS Send A Record Response with IP %s", s_ap_ip ) );
            uint32_t ip = inet_addr(s_ap_ip);
            mg_dns_reply_record(&reply, rr, NULL, rr->rtype, 10, &ip, 4);
        }
    }
    mg_dns_send_reply(c, &reply);
    mbuf_free(&reply_buf);
    (void)user_data;
}

static void ip_aquired_cb(int ev, void *ev_data, void *userdata)
{

    mgos_event_trigger(MGOS_WIFI_CAPTIVE_PORTAL_TEST_SUCCESS, userdata);

    if (userdata != NULL && mgos_sys_config_get_portal_wifi_copy())
    {

        struct mgos_config_wifi_sta *sta;
        sta = userdata;

        LOG(LL_DEBUG, ("Copying SSID and Password to STA 1 config (wifi.sta)"));
        mgos_sys_config_set_wifi_sta_ssid(sta->ssid);
        mgos_sys_config_set_wifi_sta_pass(sta->pass);

        char *err = NULL;
        if (!save_cfg(&mgos_sys_config, &err))
        {
            LOG(LL_ERROR, ("Copy STA Values, Save Config Error: %s", err));
            free(err);
        } else {
            int reboot_ms = (mgos_sys_config_get_portal_wifi_reboot() * 1000);
            if (reboot_ms > 0)
            {
                mgos_system_restart_after(reboot_ms);
            }
        }

        // Remove handler after first connection
        mgos_event_remove_handler(MGOS_WIFI_EV_STA_IP_ACQUIRED, ip_aquired_cb, userdata);
    }
}

static void http_msg_print(const struct http_message *msg)
{
    // LOG(LL_INFO, ("     message: \"%.*s\"\n", msg->message.len, msg->message.p));
    LOG(LL_INFO, ("      method: \"%.*s\"\n", msg->method.len, msg->method.p));
    LOG(LL_INFO, ("         uri: \"%.*s\"\n", msg->uri.len, msg->uri.p));
}

static void root_handler(struct mg_connection *nc, int ev, void *p, void *user_data)
{
    (void)user_data;
    if (ev != MG_EV_HTTP_REQUEST)
        return;


    LOG(LL_INFO, ("Root Handler -- Checking for CaptivePortal UserAgent"));
    struct http_message *msg = (struct http_message *)(p);
    http_msg_print(msg);

    // Init our http server options (set in mgos_wifi_captive_portal_start)
    struct mg_serve_http_opts opts;
    memcpy(&opts, &s_http_server_opts, sizeof(opts));

    // Check Host header for our hostname (to serve captive portal)
    struct mg_str *hhdr = mg_get_http_header(msg, "Host");
    // LOG( LL_INFO, ( "Root Handler -- Captive Portal HOST HEADER: %s", hhdr->p ) );

    // Host matches our portal hostname, so now we either serve assets (JS/CSS) or HTML file
    // if (hhdr != NULL && mg_casecmp(s_portal_hostname, hhdr->p) == 0)
    if (hhdr != NULL && strstr(hhdr->p, s_portal_hostname) != NULL)
    {
        // TODO: check Accept-Encoding header for gzip before serving gzip
        LOG(LL_INFO, ("Root Handler -- Host matches Captive Portal Host \n"));
        // Check if gzip file was requested
        struct mg_str uri = mg_mk_str_n(msg->uri.p, msg->uri.len);
        bool gzip = strncmp(uri.p + uri.len - 3, ".gz", 3) == 0;
        // Check if URI is root directory
        bool uriroot = strcmp( uri.p, "/" ) == 0;
        
        opts.index_files = "wifi_portal.html";

        // If gzip file requested (js/css) set Content-Encoding
        if (gzip){
            LOG(LL_INFO, ("Root Handler -- gzip Asset Requested -- Adding Content-Encoding Header \n"));
            opts.extra_headers = "Content-Encoding: gzip";
        }

        if (uriroot)
        {
            LOG(LL_INFO, ("Root Handler -- Captive Portal Root Requested\n"));
            // Set index file to our portal HTML file
            // opts.index_files = "wifi_portal.html";
            if( s_serve_gzip ){
                LOG(LL_INFO, ("Root Handler -- Captive Portal Serving GZIP HTML \n"));
                mg_http_serve_file(nc, msg, "wifi_portal.html.gz", mg_mk_str("text/html"), mg_mk_str("Content-Encoding: gzip"));
                return;
            } else {
                LOG(LL_INFO, ("Root Handler -- Captive Portal Serving HTML \n"));
                mg_http_serve_file(nc, msg, "wifi_portal.html", mg_mk_str("text/html"), mg_mk_str("Access-Control-Allow-Origin: *"));
                return;
            }

        }

    } else {

        // Check User-Agent string for "CaptiveNetworkSupport" to issue redirect (AFTER checking for Captive Portal Host)
        struct mg_str *uahdr = mg_get_http_header(msg, "User-Agent");
        if (uahdr != NULL)
        {
            // LOG(LL_INFO, ("Root Handler -- Found USER AGENT: %s \n", uahdr->p));

            if (strstr(uahdr->p, "CaptiveNetworkSupport") != NULL)
            {
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
                                                      struct mg_str args)
{

    static struct mgos_config_wifi_sta test_sta_vals;

    test_sta_vals.enable = 1; // Same as (*test_sta_vals).enable
    char *ssid = NULL;
    char *pass = NULL;

    LOG(LL_INFO, ("WiFi.PortalSave RPC Handler Parsing JSON") );

    json_scanf(args.p, args.len, ri->args_fmt, &ssid, &pass);

    if (ssid == NULL)
    {
        mg_rpc_send_errorf(ri, 400, "SSID is required!" );
        return;
    }

    LOG(LL_INFO, ("WiFi.PortalSave RPC Handler ssid: %s", ssid));

    test_sta_vals.ssid = ssid;
    test_sta_vals.pass = pass;
    
    bool result = mgos_wifi_setup_sta(&test_sta_vals);
    mg_rpc_send_responsef(ri, "{ testing: %Q, result: %B }", test_sta_vals.ssid, result);

    mgos_event_trigger(MGOS_WIFI_CAPTIVE_PORTAL_TEST_START, &test_sta_vals);

    mgos_event_add_handler(MGOS_WIFI_EV_STA_IP_ACQUIRED, ip_aquired_cb, &test_sta_vals);

    free(ssid);
    free(pass);

    (void)cb_arg;
    (void)fi;
}

bool mgos_wifi_captive_portal_start(void)
{
    LOG(LL_INFO, ("Starting WiFi Captive Portal..."));
    // Add RPC
    struct mg_rpc *c = mgos_rpc_get_global();
    mg_rpc_add_handler(c, "WiFi.PortalSave", "{ssid: %Q, pass: %Q}", mgos_wifi_captive_portal_save_rpc_handler, NULL);

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

    if (dns_c == NULL)
    {
        LOG(LL_ERROR, ("Failed to initialize DNS listener"));
        return false;
    }
    else
    {
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

    // Rewrites are basically useless in MOS right now since SPIFFS only supports flat filesystem
    // static char rewrites[256];
    // c_snprintf(rewrites, sizeof rewrites, "@%s=/wifi_portal.html", mgos_sys_config_get_portal_wifi_hostname() );
    // s_http_server_opts.url_rewrites = rewrites;

    // static char portal_endpoint[256];
    // // Endpoint includes preprended slash already, add trailing slash and * wildcard (for gzip)
    // c_snprintf(portal_endpoint, sizeof portal_endpoint, "%s/*", mgos_sys_config_get_portal_wifi_endpoint() );
    // LOG( LL_INFO, ( "Registering Captive Portal Endpoint %s", portal_endpoint ) );
    // mgos_register_http_endpoint(portal_endpoint, portal_handler, NULL);

    /**
     * Root handler to check for User-Agent captive portal support, check for our redirect hostname to serve portal HTML file,
     * and to serve CSS and JS assets to client (after matching hostname in Host header)
     */
    mgos_register_http_endpoint("/", root_handler, NULL);

    // Known HTTP GET requests to check for Captive Portal
    mgos_register_http_endpoint("/mobile/status.php", redirect_ev_handler, NULL);         // Android 8.0 (Samsung s9+)
    mgos_register_http_endpoint("/generate_204", redirect_ev_handler, NULL);              // Android
    mgos_register_http_endpoint("/gen_204", redirect_ev_handler, NULL);                   // Android 9.0
    mgos_register_http_endpoint("/ncsi.txt", redirect_ev_handler, NULL);                  // Windows
    mgos_register_http_endpoint("/hotspot-detect.html", redirect_ev_handler, NULL);       // iOS 8/9
    mgos_register_http_endpoint("/library/test/success.html", redirect_ev_handler, NULL); // iOS 8/9

    return true;
}

bool mgos_wifi_captive_portal_init(void)
{
    mgos_event_register_base(MGOS_WIFI_CAPTIVE_PORTAL_EV_BASE, "Wifi Captive Portal");

    // Check if config is set to enable captive portal on boot
    if (mgos_sys_config_get_portal_wifi_enable())
    {
        mgos_wifi_captive_portal_start();
    }

    return true;
}