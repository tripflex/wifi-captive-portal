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

#ifndef SMYLES_MOS_LIBS_WIFI_CAPTIVE_PORTAL_H_
#define SMYLES_MOS_LIBS_WIFI_CAPTIVE_PORTAL_H_

#include <stdbool.h>
#include <mgos.h>
#include "mgos_http_server.h"
#include "mgos_event.h"

#define MGOS_WIFI_CAPTIVE_PORTAL_EV_BASE MGOS_EVENT_BASE('W', 'C', 'P')

enum mgos_wifi_captive_portal_event
{
    /**
     * Fired when RPC call is made to WiFi.PortalSave,
     * and test is started
     * 
     * ev_data: struct mgos_config_wifi_sta *sta
     */
    MGOS_WIFI_CAPTIVE_PORTAL_TEST_START = MGOS_WIFI_CAPTIVE_PORTAL_EV_BASE,
    MGOS_WIFI_CAPTIVE_PORTAL_TEST_END, //TODO
    /**
     * Fired when succesful connection to Wifi
     * 
     * ev_data: struct mgos_config_wifi_sta *sta
     */
    MGOS_WIFI_CAPTIVE_PORTAL_TEST_SUCCESS,
    MGOS_WIFI_CAPTIVE_PORTAL_TEST_FAILED //TODO
};

#ifdef __cplusplus
extern "C"
{
#endif /* __cplusplus */

/**
 * Start captive portal (init RPC, DNS, and HTTP Endpoints)
 */
bool mgos_wifi_captive_portal_start(void);

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /* SMYLES_MOS_LIBS_WIFI_CAPTIVE_PORTAL_H_ */