document.addEventListener('DOMContentLoaded', function() {
    var netsHttpRequest;
    var testHttpRequest;
    var checkHttpRequest;
    var connectionChecks = 0;

    var errEl = document.getElementById("error");
    var infoEl = document.getElementById("info");

    function showError( message ){
        errEl.innerHTML = message;
        errEl.style.display = 'block';
    }
    function hideError(){
        errEl.style.display = 'none';
    }
    function showInfo( message ){
        infoEl.style.display = 'block';
        infoEl.innerHTML = message;
    }
    function hideInfo( message ){
        infoEl.style.display = 'none';
    }

    function refreshNetworks() {
        netsHttpRequest = new XMLHttpRequest();

        if (!netsHttpRequest) {
            showError( 'Unable to create an XMLHttpRequest, try to manually set' );
            return false;
        }

        showInfo( 'Scanning for WiFi networks in range of device...' );

        netsHttpRequest.onreadystatechange = function(){

            if (netsHttpRequest.readyState !== XMLHttpRequest.DONE || netsHttpRequest.status !== 200) {
                //hideInfo();
                //showError('There was a problem with the request.');
                return;
            }

            var parsedNets = JSON.parse( netsHttpRequest.responseText );
            // console.log( parsedNets );
            
            if( parsedNets.length > 0 ){

                var netSelect = document.getElementById("networks");
                netSelect.removeAttribute("disabled"); // Remove disabled (on page load)
                netSelect.innerHTML = '<option value="-1" disabled="disabled" selected="selected">Select WiFi Networks</option>'; // clear any existing ones

                parsedNets.forEach( function( net ){
                    // console.log( net );
                    var opt = document.createElement('option');
                    opt.innerHTML = net.ssid + " (" + net.bssid + ") - " + getStrengthFromRSSI( net.rssi ) + "% / " + net.rssi;
                    opt.value = net.ssid;
                    netSelect.appendChild(opt);
                });

                showInfo( "Please select from one of the " + parsedNets.length + " WiFi networks found.  This only configures WiFi settings, use the mobile app to configure other settings." );

            } else {
                hideInfo();
                showError( 'No networks found, try again...');
            }

        };
        // netsHttpRequest.setRequestHeader('Content-Type', 'application/x-www-form-urlencoded');
        netsHttpRequest.open('POST', '/rpc/Wifi.Scan');
        netsHttpRequest.setRequestHeader("Content-Type", "application/json"); // must be after open
        netsHttpRequest.send();
    }

    function testCredentials(){
        testHttpRequest = new XMLHttpRequest();
        
        showInfo( 'Testing WiFi connection and credentials...' );

        if (!testHttpRequest) {
            showError( "Unable to create an XMLHttpRequest and test" );
            return false;
        }

        var ssid = document.getElementById('networks').value;
        var password = document.getElementById('password').value;

        testHttpRequest.onreadystatechange = function(){
        
            if (netsHttpRequest.readyState !== XMLHttpRequest.DONE || netsHttpRequest.status !== 200) {
                return;
            }
            
            hideError();
            showInfo( 'Testing credentials, please wait...' );
            

            setTimeout( function(){
                hideInfo();
                showError( 'Test has timed out after 60 seconds, please check the credentials and try again.' );
            }, 60000 );
        };
        // testHttpRequest.setRequestHeader('Content-Type', 'application/x-www-form-urlencoded');
        testHttpRequest.open('POST', '/rpc/Provision.WiFi');
        testHttpRequest.setRequestHeader("Content-Type", "application/json"); // must be after open
        testHttpRequest.send( JSON.stringify( { ssid: ssid, pass: password } ) );
    }

    function checkWifiSuccess(){
        showInfo('Checking wifi connection status ...');

        checkHttpRequest = new XMLHttpRequest();

        if (!checkHttpRequest) {
            showError( "Unable to create an XMLHttpRequest and check" );
            return false;
        }

        checkHttpRequest.onreadystatechange = function(){
        
            if (netsHttpRequest.readyState !== XMLHttpRequest.DONE || netsHttpRequest.status !== 200) {
                return;
            }
            
            connectionChecks++;

            var checkResults = JSON.parse( netsHttpRequest.responseText );
            console.log( checkResults );

            var ssid = document.getElementById('networks').value;
            
            // "got ip" will be results if wifi "got ip"
            if( checkResults && checkResults.success && checkResults.ssid == ssid ){

                showInfo('WiFi connection success, disabling config mode, and rebooting...');

            } else {

            if( connectionChecks > 15 ){
                log('WiFi conection has failed after 15 checks, please verify SSID and password are correct, and try again.');
            } else {
                log('WiFi connection failed or still attempting connection...retying...');
                window.setTimeout( function(){
                checkWifiCreds();
                }, 2000 );
            }


            }

            showInfo( 'Test started, waiting for results...' );
            setTimeout( checkWifiSuccess, 2000 );

        };

        checkHttpRequest.open('GET', '/rpc/Sys.GetInfo');
        checkHttpRequest.setRequestHeader("Content-Type", "application/json"); // must be after open
        checkHttpRequest.send();
    }

    function getStrengthFromRSSI(rssi){

        if (rssi == 0 || rssi <= -100) {
            quality = 0;
        } else if (rssi >= -50) {
            quality = 100;
        } else {
            quality = 2 * (rssi + 100);
        }

        return quality;
    }

    document.getElementById("rescan").addEventListener('click', refreshNetworks);
    document.getElementById("save").addEventListener('click', testCredentials);
    refreshNetworks(); // Refresh initially to load wifi networks
});