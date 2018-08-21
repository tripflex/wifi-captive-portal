# Mongoose OS Wifi Captive Portal

This library adds a captive portal to the wifi AP, when a client connects (desktop/mobile, etc), it will prompt the user to "Sign in to Network", and will display a webpage for the user to setup/configure wifi.

## Features
- Custom setting and helper functions to enable/disable Captive Portal
- Completely vanilla JavaScript, no jQuery, Zepto, or other libraries required (because we all know space is limited)
- Displays a dropdown of available networks to connect to
- Validates user provided SSID and Password
- Uses gzipped data for small filesize and fast loading

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

## License
Apache 2.0