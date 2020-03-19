# ESP8266RelaySwitch
This contains the code to operate the ESP8266 relay installed behind a wall plug/switch

This solves most of the commonly faced issues

1. mDNS and host name not working
2. hostname not being recognized on the router
3. ota not working

Also this sketch also saves file to the EEPROM and retrieves from it. Make sure the following screenshot is adhered to.

![OnlySketch](https://user-images.githubusercontent.com/25843597/75938762-91878e80-5e56-11ea-8dbd-adefeb12d703.png)


REM Launch CMD as admin - works only in windows

netsh interface set interface "AsusMB" disable
netsh interface set interface "AsusMB" enable

netsh interface set interface "EdimaxUSB" disable
netsh interface set interface "EdimaxUSB" enable
netsh wlan disconnect
netsh wlan disconnect interface="EdimaxUSB"
netsh wlan connect name=YOUR_WIFI_SID

