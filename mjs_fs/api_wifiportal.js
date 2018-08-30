let WifiCaptivePortal = {
    _start: ffi('int mgos_wifi_captive_portal_start()'),
    start: function(){
        return this._start();
    }
};

/**
 * MGOS_WIFI_CAPTIVE_PORTAL_TEST_START
 * 
 * Called when test is started via RPC
 */
WifiCaptivePortal.START = Event.baseNumber("WCP");

/**
 * MGOS_WIFI_CAPTIVE_PORTAL_TEST_SUCCESS
 * 
 * Succesful test called via RPC method(ev_data: struct mgos_config_wifi_sta * sta)
 */
WifiCaptivePortal.SUCCESS = WifiCaptivePortal.START + 1;

/**
 * MGOS_WIFI_CAPTIVE_PORTAL_TEST_FAILED
 * 
 * Succesful test called via RPC method(ev_data: struct mgos_config_wifi_sta * sta)
 */
WifiCaptivePortal.FAILED = WifiCaptivePortal.START + 2;