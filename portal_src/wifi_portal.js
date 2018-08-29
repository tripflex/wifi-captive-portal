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
        WiFiPortal.Buttons.init();
        WiFiPortal.Info = new WiFiPortal._msg_proto( "info" );
        WiFiPortal.Error = new WiFiPortal._msg_proto( "error" );
       
        WiFiPortal.check( function(resp){

            console.log( 'Info response', resp );
            // Call rescan after initial call to get device info
            WiFiPortal.rescan();

        }, false );
    },
    check: function( callback, infoMessage ){

        WiFiPortal.Buttons.disableAll( 'Please wait, checking...' );

        WiFiPortal.rpcCall('GET', 'Sys.GetInfo', infoMessage ? infoMessage : false, false, function (resp) {
            var responseVal = 'Error'; // placeholder

            if (resp && resp !== true ) {
                var jsonResponse = resp.wifi ? resp.wifi : resp;
                var stringifyJson = JSON.stringify(jsonResponse, undefined, 2);
                responseVal = WiFiPortal.highlight(stringifyJson);
            } else {
                responseVal = "Unable to get info from device";
            }

            document.getElementById("response").innerHTML = responseVal ? responseVal : '';

            // infoMessage is only sent when called by Test obj
            if( ! infoMessage ){
                WiFiPortal.Buttons.enableAll();
            }

            // Need to check if function since this is called by event handler
            if ( typeof callback === "function" ) {
                callback( resp );
            }
        });
    },
    Info: {},
    Error: {},
    Timers: {
        clear: function(){
            WiFiPortal.Timers.Test.remove();
            WiFiPortal.Timers.Timeout.remove();
        },
        Timeout: {
            _id: false,
            init: function(){
                WiFiPortal.Timers.Timeout.remove();
                // Timeout callback ( add 2 second to timeout to pad for initial check)
                WiFiPortal.Timers.Timeout._id = setTimeout(WiFiPortal.Test.timeout, (WiFiPortal.Test._timeout * 1000) + 2000);
            },
            remove: function(){
                if( WiFiPortal.Timers.Timeout._id ){
                    clearTimeout( WiFiPortal.Timers.Timeout._id );
                    WiFiPortal.Timers.Timeout._id = false;
                }
            }
        },
        Test: {
            _id: false,
            init: function(){
                WiFiPortal.Timers.Test.remove();
                WiFiPortal.Timers.Test._id = setTimeout(WiFiPortal.Test.check, (WiFiPortal.Test._interval * 1000));
            },
            remove: function(){
                if( WiFiPortal.Timers.Test._id ){
                    clearTimeout( WiFiPortal.Timers.Test._id );
                    WiFiPortal.Timers.Test._id = false;
                }
            }
        }
    },
    Buttons: {
        _proto: function (elID, clickCB) {
            var el = document.getElementById(elID);
            if (el) {
                el.addEventListener('click', clickCB);
            }
            return {
                $el: el,
                preVal: false,
                update: function (msg) {
                    this.$el.innerHTML = msg;
                },
                disable: function (msg) {
                    // this.preVal is set to false after it's enabled, so only set if button is enabled
                    if (!this.preVal) {
                        this.preVal = this.$el.innerHTML;
                    }
                    this.update(msg);
                    this.$el.disabled = true;
                },
                enable: function () {
                    // Only set val back to preVal if there is one set
                    if (this.preVal) {
                        this.$el.innerHTML = this.preVal;
                        this.preVal = false;
                    }
                    this.$el.disabled = false;
                }
            };
        },
        _all: {},
        _ids: ["rescan", "save", "check"],
        init: function () {
            for (var i = 0; i < WiFiPortal.Buttons._ids.length; i++) {
                var elID = WiFiPortal.Buttons._ids[i];
                WiFiPortal.Buttons._all[elID] = new WiFiPortal.Buttons._proto(elID, WiFiPortal[elID]);
            }
        },
        enableAll: function () {
            for (var btn in WiFiPortal.Buttons._all) {
                WiFiPortal.Buttons._all[btn].enable();
            }
        },
        disableAll: function (msg) {
            for (var btn in WiFiPortal.Buttons._all) {
                WiFiPortal.Buttons._all[btn].disable(msg);
            }
        }
    },
    Test: {
        _timeout: 30,
        _checks: 0,
        _interval: 5, // Interval (in seconds) to check wifi status
        success: false,
        timedout: false,
        ssid: false,
        init: function(){
            WiFiPortal.Test._checks = 0; // Reset number of checks to 0
            WiFiPortal.Test.success = false; // Reset success
            WiFiPortal.Test.timedout = false; // Reset timed out
            // Initial Check after sending creds to device (if connect is succesful it's normally very quick)
            setTimeout( WiFiPortal.Test.check, 900 );
            // Init timeout timer
            WiFiPortal.Timers.Timeout.init();
        },
        timeout: function(){

            if( ! WiFiPortal.Test.success ){
                WiFiPortal.Test.timedout = true;
                WiFiPortal.Error.show('Test has timed out after ' + WiFiPortal.Test._timeout + ' seconds. Please check the SSID and Password and try again.');
                WiFiPortal.Info.hide();
                // Empty status box
                var responseDiv = document.getElementById("response");
                if( responseDiv ){
                    responseDiv.innerHTML = '';
                }
            }

            // Clear all timers on timeout
            WiFiPortal.Timers.clear();
            // Re-enable all buttons after timeout
            WiFiPortal.Buttons.enableAll();
        },
        check: function(){

            WiFiPortal.check( function(resp){
               var errorMsg = 'Error'; // placeholder

               if (resp && resp !== true) {

                   if (resp.wifi) {

                       // Output response wifi JSON formatted (2 spaces), and with syntax highlighting
                       var stringifyJson = JSON.stringify(resp.wifi, undefined, 2);
                       document.getElementById("response").innerHTML = WiFiPortal.highlight(stringifyJson);

                       if (resp.wifi.status && resp.wifi.ssid) {
                           // "got ip" means successful connection to WiFi, also check that SSId matches one we're testing against
                           if (resp.wifi.status === 'got ip' && resp.wifi.ssid === WiFiPortal.Test.ssid) {
                               WiFiPortal.Test.success = true;
                               WiFiPortal.Error.hide();
                               WiFiPortal.Info.show('WiFi connection successful! Connected to ' + resp.wifi.ssid);
                               WiFiPortal.Buttons.enableAll();
                               WiFiPortal.Timers.clear(); // Clear any timers after succesful connection
                           } else {
                               errorMsg = 'WiFi current status is ' + resp.wifi.status;
                           }
                       }

                   } else {
                       errorMsg = 'Received response, error getting WiFi status';
                   }

               } else {
                    WiFiPortal.Info.hide();
                    WiFiPortal.Error.show('Error getting WiFi status, trying again in 5 seconds...');
               }

                WiFiPortal.Test._checks++;
                // Only reschedule next check timer if test was not a success and test has not timed out yet
                if ( ! WiFiPortal.Test.success && ! WiFiPortal.Test.timedout ) {
                    WiFiPortal.Error.hide();
                    WiFiPortal.Info.show(errorMsg + ', check ' + WiFiPortal.Test._checks + ', trying again in ' + WiFiPortal.Test._interval + ' seconds...');
                    WiFiPortal.Timers.Test.init();
                }

            }, 'Checking device WiFi status...' );
        }
    },
    save: function(){
        var ssid = document.getElementById('networks').value;
        var password = document.getElementById('password').value;
        
        if( ! ssid || ssid.length < 1 ){
            WiFiPortal.Info.hide();
            WiFiPortal.Error.show( 'You must select an SSID from the dropdown!' );
            return;
        }

        WiFiPortal.Buttons.disableAll('Please wait, sending...');

        WiFiPortal.Test.ssid = ssid; // Set SSID value in test so we can verify connection is to that exact SSID

        WiFiPortal.rpcCall('POST', 'WiFi.PortalSave', 'Sending credentials to device to test...', { ssid: ssid, pass: password } , function( resp ){
            // True means we received a response, but no data
            if( resp && resp !== true && resp.result !== undefined ){

                if( resp.result === false ){
                    WiFiPortal.Error.show('Error from device setting up STA! Check SSID and Password and try again!');
                    WiFiPortal.Buttons.enableAll();
                } else {
                    WiFiPortal.Error.hide(); // Hide error when saving (to remove stale errors)
                    WiFiPortal.Info.show('Device is testing WiFi connection, please wait...');
                    WiFiPortal.Test.init();
                }

            } else {
                WiFiPortal.Error.show('Error sending credentials to device, please try again');
                WiFiPortal.Buttons.enableAll();
            }

        });
    },
    rescan: function () {

        WiFiPortal.Buttons.disableAll('Scanning...');

        WiFiPortal.rpcCall('POST', 'Wifi.Scan', 'Scanning for WiFi networks in range of device...', false, function ( resp ) {
            
            if (resp && resp.length > 0) {

                var netSelect = document.getElementById("networks");
                netSelect.removeAttribute("disabled"); // Remove disabled (on page load)
                netSelect.innerHTML = '<option value="-1" disabled="disabled" selected="selected">Please select an SSID...</option>'; // clear any existing ones
                var SSIDs = [];

                resp.forEach(function (net) {
                    if( SSIDs.indexOf( net.ssid ) > -1 ){
                        return; // prevent adding dupe SSIDs as all we send is the SSID to connect to
                    }
                    var opt = document.createElement('option');
                    opt.innerHTML = net.ssid + " (" + net.bssid + ") - " + WiFiPortal.rssiToStrength(net.rssi) + "% / " + net.rssi;
                    opt.value = net.ssid;
                    netSelect.appendChild(opt);
                    SSIDs.push(net.ssid);
                });

                WiFiPortal.Info.show("Please select from one of the " + resp.length + " WiFi networks found.");

            } else {
                WiFiPortal.Info.hide();
                WiFiPortal.Error.show('No networks found, try again...');
            }

            WiFiPortal.Buttons.enableAll();

        });

    },
    rpcCall: function (type, rpc, optInfoMsg, data, callback) {

        httpRequest = new XMLHttpRequest();

        if (!httpRequest) {
            WiFiPortal.Error.show('Unable to create an XMLHttpRequest, try to manually set');
            return callback( false );
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
        httpRequest.send(JSON.stringify(data));
    },
    rssiToStrength: function (rssi) {
        if (rssi === 0 || rssi <= -100) {
            quality = 0;
        } else if (rssi >= -50) {
            quality = 100;
        } else {
            quality = 2 * (rssi + 100);
        }

        return quality;
    },
    highlight:function(json){
        json = json.replace(/&/g, '&amp;').replace(/</g, '&lt;').replace(/>/g, '&gt;');
        return json.replace(/("(\\u[a-zA-Z0-9]{4}|\\[^u]|[^\\"])*"(\s*:)?|\b(true|false|null)\b|-?\d+(?:\.\d*)?(?:[eE][+\-]?\d+)?)/g, function (match) {
            var cls = 'number';
            if (/^"/.test(match)) {
                if (/:$/.test(match)) {
                    cls = 'key';
                } else {
                    cls = 'string';
                }
            } else if (/true|false/.test(match)) {
                cls = 'boolean';
            } else if (/null/.test(match)) {
                cls = 'null';
            }
            return '<span class="' + cls + '">' + match + '</span>';
        });
    }
};

// Init once the entire DOM is loaded
document.addEventListener('DOMContentLoaded', WiFiPortal.init );