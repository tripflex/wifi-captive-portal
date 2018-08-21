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
#include "mgos_wifi_captive_portal.h"

static char *s_our_ip = "192.168.4.1";
static const char *s_portal_redirect_url = "http://192.168.4.1/wifi";
static const char *s_listening_addr = "udp://:53";
static struct mg_serve_http_opts s_http_server_opts;

char *get_redirect_url(void){
    static char redirect_url[256];

    // Set URI as HTTPS if ssl cert configured, otherwise use http
    c_snprintf(redirect_url, sizeof redirect_url, "%s://%s", ( mgos_sys_config_get_http_ssl_cert() ? "https" : "http" ), mgos_sys_config_get_portal_wifi_hostname());

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
        // fprintf(stdout, "Q type %d name %s\n", rr->rtype, rname);
        if (rr->rtype == MG_DNS_A_RECORD)
        {
            uint32_t ip = inet_addr(s_our_ip);
            mg_dns_reply_record(&reply, rr, NULL, rr->rtype, 10, &ip, 4);
        }
    }
    mg_dns_send_reply(c, &reply);
    mbuf_free(&reply_buf);
    (void)user_data;
}

static void http_msg_print(const struct http_message *msg)
{
    LOG(LL_INFO, ("     message: \"%.*s\"\n", msg->message.len, msg->message.p));
    LOG(LL_INFO, ("      method: \"%.*s\"\n", msg->method.len, msg->method.p));
    LOG(LL_INFO, ("         uri: \"%.*s\"\n", msg->uri.len, msg->uri.p));
}

static void handle_gzip(struct mg_connection *nc, struct http_message *msg,
                        struct mg_serve_http_opts *opts)
{
    struct mg_str uri = mg_mk_str_n(msg->uri.p, msg->uri.len);
    bool gzip = strncmp(uri.p + uri.len - 3, ".gz", 3) == 0;
    if (gzip)
    {
        opts->extra_headers = "Content-Encoding: gzip";
    }
}

static void root_handler(struct mg_connection *nc, int ev, void *p, void *user_data)
{
    (void)user_data;
    if (ev != MG_EV_HTTP_REQUEST)
        return;


    LOG(LL_INFO, ("Root Handler -- Checking for CaptivePortal UserAgent"));
    struct http_message *msg = (struct http_message *)(p);
    http_msg_print(msg);

    struct mg_str *uahdr = mg_get_http_header(msg, "User-Agent");
    
    if( uahdr != NULL ){
        LOG(LL_INFO, ("Root Handler -- Found USER AGENT: %s \n", uahdr->p));

        if (strstr(uahdr->p, "CaptiveNetworkSupport") != NULL ){
            LOG(LL_INFO, ( "Root Handler -- Found USER AGENT CaptiveNetworkSupport -- Sending Redirect!\n" ) );
            redirect_ev_handler( nc, ev, p, user_data );
            return;
        }
    }

    // Serve from root using our http server options (set in mgos_wifi_captive_portal_start)
    struct mg_serve_http_opts opts;
    memcpy(&opts, &s_http_server_opts, sizeof(opts));
    // Potentially handle any gzip requests
    handle_gzip(nc, msg, &opts);
    mg_serve_http(nc, msg, opts);
}

static void portal_handler(struct mg_connection *nc, int ev, void *p, void *user_data)
{
    (void)user_data;
    if (ev != MG_EV_HTTP_REQUEST)
    {
        return;
    }
    LOG(LL_INFO, ("Portal Handler Called"));
    struct http_message *msg = (struct http_message *)(p);

    http_msg_print(msg);

    struct mg_serve_http_opts opts;
    memcpy(&opts, &s_http_server_opts, sizeof(opts));
    
    // handle_gzip(nc, msg, &opts);

    struct mg_str uri = mg_mk_str_n(msg->uri.p, msg->uri.len);
    bool gzip = strncmp(uri.p + uri.len - 3, ".gz", 3) == 0;
    // bool portal = strncmp(uri.p, , 3) == 0;

    if (gzip)
    {
        mg_http_serve_file(nc, msg, uri.p, mg_mk_str("text/html"), mg_mk_str("Content-Encoding: gzip"));
        opts.extra_headers = "Content-Encoding: gzip";
        // mg_serve_http(nc, msg, opts);

    } else {

        mg_http_serve_file(nc, msg, "portal.html", mg_mk_str("text/html"), mg_mk_str("Access-Control-Allow-Origin: *"));
    }

    // mg_serve_http(nc, msg, opts);
}

bool mgos_wifi_captive_portal_start(void)
{
    LOG(LL_INFO, ("Starting WiFi Captive Portal..."));

    // if (mgos_sys_config_get_wifi_ap_enable() && mgos_sys_config_get_wifi_ap_hostname() != NULL) {

    // }
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

    // Add GZIP mime types for css.gz and js.gz
    s_http_server_opts.custom_mime_types = ".js.gz=application/javascript; charset=utf-8,.css.gz=text/css; charset=utf-8";

    // CORS
    s_http_server_opts.extra_headers = "Access-Control-Allow-Origin: *";

    // Build rewrite for hostname, pointing at portal HTML file
    static char rewrites[256];
    c_snprintf(rewrites, sizeof rewrites, "@%s=/wifi_portal.html", mgos_sys_config_get_portal_wifi_hostname() );
    s_http_server_opts.url_rewrites = rewrites;

    // static char portal_endpoint[256];
    // // Endpoint includes preprended slash already, add trailing slash and * wildcard (for gzip)
    // c_snprintf(portal_endpoint, sizeof portal_endpoint, "%s/*", mgos_sys_config_get_portal_wifi_endpoint() );
    // LOG( LL_DEBUG, ( "Registering Captive Portal Endpoint %s", portal_endpoint ) );

    /**
     * Root handler to check for User-Agent captive portal support, and to serve portal assets
     * Because we redirect for captive portal to a hostname, "technically" the assets will be served from root HTTP directory
     */
    mgos_register_http_endpoint("/", root_handler, NULL);

    // Portal web UI endpoint
    // mgos_register_http_endpoint(portal_endpoint, portal_handler, NULL);

    // Known HTTP GET requests to check for Captive Portal
    mgos_register_http_endpoint("/mobile/status.php", redirect_ev_handler, NULL);         // Android 8.0 (Samsung s9+)
    mgos_register_http_endpoint("/generate_204", redirect_ev_handler, NULL);              // Android
    mgos_register_http_endpoint("/ncsi.txt", redirect_ev_handler, NULL);                  // Windows
    mgos_register_http_endpoint("/hotspot-detect.html", redirect_ev_handler, NULL);       // iOS 8/9
    mgos_register_http_endpoint("/library/test/success.html", redirect_ev_handler, NULL); // iOS 8/9

    return true;
}

bool mgos_wifi_captive_portal_init(void)
{
    // Check if config is set to enable captive portal on boot
    if (mgos_sys_config_get_portal_wifi_enable())
    {
        mgos_wifi_captive_portal_start();
    }

    return true;
}