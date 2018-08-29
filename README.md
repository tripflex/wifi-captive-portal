# Mongoose OS Wifi Captive Portal

[![Gitter](https://badges.gitter.im/cesanta/mongoose-os.svg)](https://gitter.im/cesanta/mongoose-os?utm_source=badge&utm_medium=badge&utm_campaign=pr-badge)

- [Mongoose OS Wifi Captive Portal](#mongoose-os-wifi-captive-portal)
    - [Author](#author)
    - [Features](#features)
    - [Settings](#settings)
        - [Setting Details](#setting-details)
            - [Reboot Setting `portal.wifi.reboot`](#reboot-setting-portalwifireboot)
    - [Installation/Usage](#installationusage)
    - [Required Libraries](#required-libraries)
    - [How it works](#how-it-works)
            - [Known Endpoints](#known-endpoints)
    - [Ideal Flow](#ideal-flow)
    - [Directories and Files](#directories-and-files)
    - [Available Functions/Methods](#available-functionsmethods)
        - [C Functions](#c-functions)
        - [MJS](#mjs)
    - [Dev for HTML/CSS/JS](#dev-for-htmlcssjs)
    - [RPC Endpoints](#rpc-endpoints)
        - [Response](#response)
    - [Events](#events)
    - [Other Remarks](#other-remarks)
    - [Changelog](#changelog)
    - [License](#license)

This library adds a captive portal to the wifi AP, when a client connects (desktop/mobile, etc), it will prompt the user to "Sign in to Network", and will display a webpage for the user to setup/configure wifi.

![OSX Captive Portal](https://raw.githubusercontent.com/tripflex/wifi-captive-portal/dev/osx-portal.gif)

## Author
Myles McNamara ( https://smyl.es )

## Features
- Provides web UI for testing and configuring WiFi
- Mobile and desktop devices prompt the "Login to network" window/notification
- Custom setting and helper functions to enable/disable Captive Portal
- RPC endpoint for setting and testing wifi credentials (can be used without captive portal)
- **Completely vanilla JavaScript**, no jQuery, Zepto, or other libraries required (because we all know space is limited)
- Unminified and non-gzipped files are only `14.2kb` total in size ( `wifi_portal.css - 3kb`, `wifi_portal.html - 1.45kb`, `wifi_portal.js - 9.67kb` )
- Minified and gzipped files are only `3.26kb` total in size ( `wifi_portal.min.css.gz - 735b`, `wifi_portal.html.gz - 561b`, `wifi_portal.min.js.gz - 2kb` )
- Displays a dropdown of available networks to connect to
- Validates user provided SSID and Password
- Uses gzipped data for small filesize and fast loading (see dev below for using/customizing files)
- Save/Copy SSID and Password to STA 1 (`wifi.sta`) configuration after succesful test
- Reboot after sucesful SSID/Password test (after saving files)

## Settings
Check the `mos.yml` file for latest settings, all settings listed below are defaults

```yaml
  - [ "portal.wifi.enable", "b", true, {title: "Enable WiFi captive portal on device boot"}]
  - [ "portal.wifi.rpc", "b", true, {title: "Enable Captive Portal RPC Endpoint regardless of whether captive portal is enabled/started"}]
  - [ "portal.wifi.gzip", "b", true, {title: "Whether or not to serve gzip HTML file (set to false to serve standard HTML for dev)"}]
  - [ "portal.wifi.hostname", "s", "setup.device.portal", {title: "Hostname to use for captive portal redirect"}]
  - [ "portal.wifi.copy", "b", true, {title: "Copy SSID and Password to wifi.sta after succesful test"}]
  - [ "portal.wifi.disable", "i", 2, {title: "0 - do nothing, 1 - Disable AP (wifi.ap.enable), 2 - Disable AP and Captive Portal (portal.wifi.enable) -- after successful test and copy/save values"}]
  - [ "portal.wifi.reboot", "i", 15, {title: "0 to disable, or value (in seconds) to wait and then reboot device, after successful test (and copy/save values)"}]

```

### Setting Details
```yaml
portal.wifi.disable
```
Set this value to `0` to not disable anything, set to `1` to disable AP (set `wifi.ap.enable` to `false`), set to `2` to disable AP (set `wifi.ap.enable` to `false` and set `portal.wifi.enable` to `false`) ... after a sucesful test.

#### Reboot Setting `portal.wifi.reboot`
- The reboot setting is defined as an integer, with 0 (zero) being disabled, and anything greater than 0 enabling the setting.  
- This value is based in **Seconds**
- Device will reboot X seconds after succesful Wifi credential test, and saving values (if enabled)

## Installation/Usage
Add this lib your `mos.yml` file under `libs:`

```yaml
  - origin: https://github.com/tripflex/wifi-captive-portal
```

## Required Libraries
*These libraries are already defined as dependencies of this library, and is just here for reference (you're probably already using these anyways)*
- Wifi
- RPC Service Wifi (used to obtain available networks)
- RPC Common (used to verify connection to network)

## How it works
When device boots up, if `portal.wifi.enable` is set to `true` (default is `false`) captive portal is initialized. If `portal.wifi.enable` is not set to `true` you must either call `mgos_wifi_captive_portal_start` in C, or `WifiCaptivePortal.start()` in mjs to initialize captive portal.

#### Known Endpoints
Initialization enables a DNS responder for any `A` DNS record, that responds with the AP's IP address.  Captive Portal also adds numerous HTTP endpoints for known Captive Portal device endpoints:
- `/mobile/status.php` Android 8.0 (Samsung s9+)
- `/generate_204` Android
- `/gen_204` Android
- `/ncsi.txt` Windows
- `/hotspot-detect.html` iOS
- `/library/test/success.html` iOS

A root endpoint is also added, `/` to detect `CaptiveNetworkSupport` in the User-Agent of device, to redirect to captive portal.

When one of these endpoints is detected from a device (mobile/desktop), it will automatically redirect (with a `302` redirect), to the config value from `portal.wifi.hostname` (default is `setup.device.local`).

If on a mobile device, the user should be prompted to "Login to Wifi Network", or on desktop with captive portal support, it should open a window.

The root endpoint is also used, to detect the value in the `Host` header, and if it matches the `portal.wifi.hostname` value, we assume the access is meant for the captive portal.  This allows you to serve HTML files via your device, without captive portal taking over the `index.html` file.

If the `portal.wifi.gzip` value is `true` (default is `true`), the device will serve the `wifi_portal.html.gz` file (which also references the `wifi_portal.min.css.gz` and `wifi_portal.min.js.gz` files), to serve everything as gzipped files, to consume the least amount of space required.

If `portal.wifi.gzip` is `false` the device will attempt to serve `wifi_portal.html` file instead (make sure read below under "Dev for HTML/CSS/JS") -- but you must manually copy this file yourself to your `fs` as it is NOT copied to device by default.

On the initial load of captive portal page, a scan will be initiated immediately to scan for available networks from the device, and the dropdown will be updated with the available SSID's the device can connect to.

Once the user enters the password (if there is one), the page will then call the custom RPC endpoint from this library, `WiFi.PortalSave`, which initiates a connection test to the STA using provided credentials.

The captive portal will then wait `2` seconds for first initial check, and then every `5` seconds it will make an RPC call to `Sys.GetInfo` to see if the connection was succesful or not.  After `30` seconds, if the connection is not succesful, a timeout is assumed and notice will be shown on the screen (these values configurable in javascript file).  `30` seconds was chosen as default wifi lib connect timeout is `30` seconds.

If device succesfully connects to the SSID, and `portal.wifi.copy` is set to `true` (default is `true`) the values will be saved to `wifi.sta`

If `portal.wifi.disable_ap` is set to `true` (default is `true`), `wifi.ap.enable` will also be saved with value of `false` (does NOT immediately disable AP, device must reboot for changes to apply)

If `portal.wifi.reboot` is a value greater than `0` (default is `10` -- `0` means disabled), the device will then reboot after the value in that setting (based in seconds)

## Ideal Flow
The ideal flow process for the captive portal setup, is as follows:
- AP is enabled on boot (or by code base)
- User configures wifi settings
- On succesful connection, device saves values, disables AP, and reboots .. automatically connecting to WiFi after reboot

## Directories and Files

- `include` directory contains any C header files
- `src` directory contains any C files
- `portal_src` directory contains source files for the captive portal (unminified and not gzipped) *these are not copied to the device on build*
- `fs` directory contains the captive portal gzipped files (css/js/index)

## Available Functions/Methods

### C Functions
```C
bool mgos_wifi_captive_portal_start(void)
```

### MJS
```javascript
load( 'api_wifiportal.js' );
WifiCaptivePortal.start();
```

## Dev for HTML/CSS/JS
If you wish to customize the html, JS, or CSS files for the portal, you can copy them from the `portal_src` directory, to your root `fs` directory, modify them, and update your `mos.yml` with this value in `config_schema`:

```yaml
- [ "portal.wifi.gzip", false ]
```

This disables serving the gzipped version of main portal HTML file `wifi_portal.html.gz` and instead will serve a `wifi_portal.html` file from your root filesystem on the device.

You can also just use the `dev` branch in your project which has the unminified/ungzipped files in the `fs` directory already, see the dev branch for more details:

```yaml
  - origin: https://github.com/tripflex/wifi-captive-portal
   version: dev
```

## RPC Endpoints

`WiFi.PortalSave` - `{ssid: YOURSSID, pass: PASSWORD }`
The RPC endpoint will be available even if you do not initalize/start the captive portal, it can be disabled by setting `portal.wifi.rpc` in config to `false`

### Response
```json
{ 
    testing: THESSID,
    result: RESULT 
}
```
`RESULT` will be either `true` for succesful setup of STA, or `false` if it failed

## Events
`MGOS_WIFI_CAPTIVE_PORTAL_TEST_START` - Test started from RPC call ( `ev_data: struct mgos_config_wifi_sta *sta` )
`MGOS_WIFI_CAPTIVE_PORTAL_TEST_SUCCESS` - Succesful test called via RPC method ( `ev_data: struct mgos_config_wifi_sta *sta` )
`MGOS_WIFI_CAPTIVE_PORTAL_TEST_FAILED` - Failed connection test (15 reconnect attempts) after RPC call ( `ev_data: struct mgos_config_wifi_sta *sta` )

## Other Remarks
- You need to manually set the AP to be enabled on boot, otherwise the captive portal will not run.  This is as simple as defining in your `mos.yml` setting it enabled:

```yaml
- [ "wifi.ap.enable", true ]
```

## Changelog

**1.0.0** (Aug 25, 2018) - Initial release

## License
Apache 2.0
