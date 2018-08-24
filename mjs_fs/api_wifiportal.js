let WifiCaptivePortal = {
    _start: ffi('int mgos_wifi_captive_portal_start()'),
    start: function(){
        return this._start();
    }
};