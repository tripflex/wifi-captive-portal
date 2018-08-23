var WiFiPortal = {
    _msg_proto: function (elID) {
        return {
            $el: document.getElementById(elID),
            show: function (msg) {
                this.$el.innerHTML = msg;
                this.$el.style.display = 'block';
            },
            hide: function () {
                this.$el.style.display = 'none';
            }
        };
    },
    init: function(){
        WiFiPortal.Info = new WiFiPortal._msg_proto( "info" );
        WiFiPortal.Error = new WiFiPortal._msg_proto( "error" );
        document.getElementById("rescan").addEventListener('click', WiFiPortal.rescan);
        document.getElementById("save").addEventListener('click', WiFiPortal.save);
        WiFiPortal.rescan(); // Refresh initially to load wifi networks
    },
    Info: {},
    Error: {},
    Test: {
        _timeout: 60,
        _checks: 0,
        _interval: 5, // Interval (in seconds) to check wifi status
        success: false,
        timedout: false,
        ssid: false,
        init: function(){
            this._checks = 0; // Reset number of checks to 0
            this.success = false;
            setTimeout(WiFiPortal.Test.timeout, (this._timeout * 1000) );
        },
        timeout: function(){
            if( ! this.success ){
                this.timedout = true;
                WiFiPortal.Info.hide();
                WiFiPortal.Error.show('Test has timed out after ' + this._timeout + ' seconds, please check the credentials and try again.');
            }
        },
        check: function(){

            WiFiPortal.rpcCall( 'GET', 'Sys.GetInfo', 'Checking device WiFi status...', false, function(resp){
                var errorMsg = 'Error'; // placeholder

                if( resp ){
                    
                    if( resp.wifi && resp.wifi.status && resp.wifi.ssid ){

                        // "got ip" means successful connection to WiFi, also check that SSId matches one we're testing against
                        if( resp.wifi.status === 'got ip' && resp.wifi.ssid === WiFiPortal.Test.ssid ){
                            WiFiPortal.Test.success = true;
                            WiFiPortal.Error.hide();
                            WiFiPortal.Info.show('WiFi connection successful!');
                        } else {
                            errorMsg = 'WiFi current status is ' + resp.wifi.status;
                        }

                    } else {
                        errorMsg = 'Received response, error getting WiFi status';
                    }

                } else {
                    WiFiPortal.Info.hide();
                    WiFiPortal.Error.show( 'Error getting WiFi status, trying again in 5 seconds...' );
                }

                WiFiPortal.Test._checks++;

                if( ! WiFiPortal.Test.success && ! WiFiPortal.Test.timedout ){
                    WiFiPortal.Info.hide();
                    WiFiPortal.Error.show(errorMsg + ', check ' + WiFiPortal.Test._checks + ', trying again in ' + WiFiPortal.Test._interval + ' seconds...');
                    setTimeout(WiFiPortal.Test.check, (WiFiPortal.Test._interval * 1000) );
                }
            });

        }
    },
    save: function(){
        var ssid = document.getElementById('networks').value;
        var password = document.getElementById('password').value;
        
        WiFiPortal.Test.ssid = ssid; // Set SSID value in test so we can verify connection is to that exact SSID

        WiFiPortal.rpcCall('POST', 'WiFi.PortalSave', 'Sending credentials to device to test...', { ssid: ssid, pass: password } , function( resp ){
            // True means we received a response, but no data
            if( resp && resp !== true ){
                WiFiPortal.Error.hide(); // Hide error when saving (to remove stale errors)
                WiFiPortal.Info.show('Device is testing WiFi connection, please wait...');
                WiFiPortal.Test.init();
            } else {
                WiFiPortal.Error.show('Error sending credentials to device, please try again');
            }

        });
    },
    rescan: function () {

        WiFiPortal.rpcCall('POST', 'Wifi.Scan', 'Scanning for WiFi networks in range of device...', false, function ( resp ) {
            
            if (resp && resp.length > 0) {

                var netSelect = document.getElementById("networks");
                netSelect.removeAttribute("disabled"); // Remove disabled (on page load)
                netSelect.innerHTML = '<option value="-1" disabled="disabled" selected="selected">Select WiFi Networks</option>'; // clear any existing ones

                resp.forEach(function (net) {
                    // console.log( net );
                    var opt = document.createElement('option');
                    opt.innerHTML = net.ssid + " (" + net.bssid + ") - " + WiFiPortal.rssiToStrength(net.rssi) + "% / " + net.rssi;
                    opt.value = net.ssid;
                    netSelect.appendChild(opt);
                });

                WiFiPortal.Info.show("Please select from one of the " + resp.length + " WiFi networks found.");

            } else {
                WiFiPortal.Info.hide();
                WiFiPortal.Error.show('No networks found, try again...');
            }

        });

    },
    rpcCall: function (type, rpc, optInfoMsg, data, callback) {

        httpRequest = new XMLHttpRequest();

        if (!httpRequest) {
            WiFiPortal.Error.show('Unable to create an XMLHttpRequest, try to manually set');
            return false;
        }

        if( optInfoMsg !== undefined && optInfoMsg ){
            WiFiPortal.Info.show(optInfoMsg);
        }

        httpRequest.onreadystatechange = function () {

            if (httpRequest.readyState !== XMLHttpRequest.DONE) {
                console.log('rpcCall httpRequest readyState is NOT done!', httpRequest.readyState );
                return false;
            }

            if (httpRequest.status !== 200) {
                console.log( 'rpcCall httpRequest status is NOT 200!', httpRequest );

                if( httpRequest.responseText && httpRequest.responseText.length > 0 ){
                    WiFiPortal.Error.show( "Error from device ( " + httpRequest.responseText + " ) -- Please try again");
                    callback(true);
                } else {
                    callback(false);
                }
                return;
            } 

            console.log('responseText', httpRequest.responseText);
            var httpResponse = JSON.parse(httpRequest.responseText);
            console.log('httpResponse', httpResponse);

            callback(httpResponse);
        };

        // httpRequest.setRequestHeader('Content-Type', 'application/x-www-form-urlencoded');
        httpRequest.open(type, '/rpc/' + rpc );
        httpRequest.setRequestHeader("Content-Type", "application/json"); // must be after open
        httpRequest.send( data );
    },
    rssiToStrength: function (rssi) {
        if (rssi == 0 || rssi <= -100) {
            quality = 0;
        } else if (rssi >= -50) {
            quality = 100;
        } else {
            quality = 2 * (rssi + 100);
        }

        return quality;
    },
};

// Init once the entire DOM is loaded
document.addEventListener('DOMContentLoaded', WiFiPortal.init );