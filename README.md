# Mongoose OS Wifi Captive Portal

This library adds a captive portal to the wifi AP, when a client connects (desktop/mobile, etc), it will prompt the user to "Sign in to Network", and will display a webpage for the user to setup/configure wifi.

## Features
- Custom setting and helper functions to enable/disable Captive Portal
- Completely vanilla JavaScript, no jQuery, Zepto, or other libraries required (because we all know space is limited)
- Displays a dropdown of available networks to connect to
- Validates user provided SSID and Password
- Uses gzipped data for small filesize and fast loading (see dev below for using/customizing files)
- Save/Copy SSID and Password to STA 1 (`wifi.sta`) configuration after succesful test
- Reboot after succesful SSID/Password test (after saving files)

## Settings
Check the `mos.yml` file for latest settings, all settings listed below are defaults

```yaml
- [ "portal.wifi.enable", "b", false, {title: "Enable WiFi captive portal on device boot"}]
- [ "portal.wifi.gzip", "b", true, {title: "Whether or not to serve gzip HTML file (set to false to serve standard HTML for dev)"}]
- [ "portal.wifi.hostname", "s", "setup.device.local", {title: "Hostname to use for captive portal redirect"}]
- [ "portal.wifi.copy", "b", true, {title: "Copy SSID and Password to wifi.sta after succesful test"}]
- [ "portal.wifi.reboot", "i", 0, {title: "0 to disable, or value (in seconds) to wait and then reboot device, after successful test (and copy/save values)"}]
```

## Required Libraries
*These libraries are already defined as dependencies of this library, and is just here for reference (you're probably already using these anyways)*
- Wifi
- RPC Service Wifi (used to obtain available networks)
- RPC Common (used to verify connection to network)

## Directories and Files

- `include` directory contains any C header files
- `src` directory contains any C files
- `portal_src` directory contains source files for the captive portal (unminified and not gzipped) *these are not copied to the device on build*
- `fs` directory contains the captive portal gzipped files (css/js/index)

## C Functions
```C
bool mgos_wifi_captive_portal_start(void)
```

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