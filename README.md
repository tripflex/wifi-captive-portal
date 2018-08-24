# Mongoose OS Wifi Captive Portal

- **This is the development branch (`dev` branch) which is to be used for development of the captive portal HTML/JS/CSS.**
- The `dev` branch has a few differences from `master` branch:
    - `portal.wifi.gzip` is set to `false` by default (`true` in master branch)
    - `portal.wifi.enable` is set to `true` by default (`false` in master branch)
    - There is no `portal_src` directory, as the regular `wifi_portal.html` `wifi_portal.css` and `wifi_portal.js` as located in the `fs` directory
    - This also means there are no gzip files in the `dev` branch either (so do not enable `gzip` while using `dev` branch)

- [Mongoose OS Wifi Captive Portal](#mongoose-os-wifi-captive-portal)
    - [Features](#features)
    - [Settings](#settings)
    - [Installation/Usage](#installationusage)
    - [Required Libraries](#required-libraries)
    - [How it works](#how-it-works)
            - [Known Endpoints](#known-endpoints)
    - [Ideal Flow](#ideal-flow)
    - [Directories and Files](#directories-and-files)
    - [C Functions](#c-functions)
        - [MJS](#mjs)
        - [Available Methods](#available-methods)
    - [Dev for HTML/CSS/JS](#dev-for-htmlcssjs)
    - [RPC Endpoints](#rpc-endpoints)
    - [Events](#events)
    - [Reboot Setting](#reboot-setting)
    - [License](#license)

This library adds a captive portal to the wifi AP, when a client connects (desktop/mobile, etc), it will prompt the user to "Sign in to Network", and will display a webpage for the user to setup/configure wifi.

![Screenshot](https://raw.githubusercontent.com/tripflex/wifi-captive-portal/dev/screenshot.png)

## Features
- Custom setting and helper functions to enable/disable Captive Portal
- Completely vanilla JavaScript, no jQuery, Zepto, or other libraries required (because we all know space is limited)
- Displays a dropdown of available networks to connect to
- Validates user provided SSID and Password
- Uses gzipped data for small filesize and fast loading (see dev below for using/customizing files)
- Save/Copy SSID and Password to STA 1 (`wifi.sta`) configuration after succesful test
- Reboot after succesful SSID/Password test (after saving files)

## Settings
Check the `mos.yml` file for latest settings, all settings listed below are defaults (**FOR THIS DEV BRANCH!**)

```yaml
  - [ "portal.wifi.enable", "b", true, {title: "Enable WiFi captive portal on device boot"}]
  - [ "portal.wifi.gzip", "b", false, {title: "Whether or not to serve gzip HTML file (set to false to serve standard HTML for dev)"}]
  - [ "portal.wifi.hostname", "s", "setup.device.local", {title: "Hostname to use for captive portal redirect"}]
  - [ "portal.wifi.copy", "b", true, {title: "Copy SSID and Password to wifi.sta after succesful test"}]
  - [ "portal.wifi.disable_ap", "b", true, {title: "Disable AP after succesful connection attempt (only if and after copying values, before reboot)"}]
  - [ "portal.wifi.reboot", "i", 0, {title: "0 to disable, or value (in seconds) to wait and then reboot device, after successful test (and copy/save values)"}]
```

## Installation/Usage
As this branch is specifically for development of the lib, to use the dev branch you must add to your `mos.yml` file like this:

```yaml
  - origin: https://github.com/tripflex/wifi-captive-portal
    version: dev
```

**Note** the `version: dev` which specifies to use the `dev` branch from GitHub

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

The captive portal will then wait `2` seconds for first initial check, and then every `5` seconds it will make an RPC call to `Sys.GetInfo` to see if the connection was succesful or not.  After `35` seconds, if the connection is not succesful, a timeout is assumed and notice will be shown on the screen (these values configurable in javascript file).  `35` seconds was chosen as default wifi lib connect timeout is `30` seconds.

If device succesfully connects to the SSID, and `portal.wifi.copy` is set to `true` (default is `true`) the values will be saved to `wifi.sta`

If `portal.wifi.disable_ap` is set to `true` (default is `true`), `wifi.ap.enable` will also be saved with value of `false` (does NOT immediately disable AP, device must reboot for changes to apply)

If `portal.wifi.reboot` is a value greater than `0` (default is `20` -- `0` means disabled), the device will then reboot after the value in that setting (based in seconds)

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

## C Functions
```C
bool mgos_wifi_captive_portal_start(void)
```

### MJS
```javascript
load( 'api_wifiportal.js' );
```

### Available Methods
```javascript
WifiCaptivePortal.start();
```
- Enable WiFi Captive portal

## Dev for HTML/CSS/JS
If you wish to customize the html, JS, or CSS files for the portal, you can copy them from the `portal_src` directory, to your root `fs` directory, modify them, and update your `mos.yml` with this value in `config_schema`:

```yaml
- [ "portal.wifi.gzip", false ]
```

This disables serving the gzipped version of main portal HTML file `wifi_portal.html.gz` and instead will serve a `wifi_portal.html` file from your root filesystem on the device.

## RPC Endpoints

`WiFi.PortalSave` - `{ssid: YOURSSID, pass: PASSWORD }`

## Events
`MGOS_WIFI_CAPTIVE_PORTAL_TEST_START` - Test started from RPC call
`MGOS_WIFI_CAPTIVE_PORTAL_TEST_SUCCESS` - Succesful test called via RPC method

## Reboot Setting
- The reboot setting is defined as an integer, with 0 (zero) being disabled, and anything greater than 0 enabling the setting.  
- This value is based in **Seconds**
- Device will reboot X seconds after succesful Wifi credential test, and saving values (if enabled)

## License
Apache 2.0