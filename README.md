"# fli3d_rx433" 

This software will receive CCSDS telemetry packages transmitted over 433 MHz radio by the Fli3d payload, and forward these packages to Yamcs over WiFi/UDP.

To be used with a NodeMCU (ESP8266), with WL101-341 433 MHz receiver on pin D1.

````NodeMCU                         WL101-341
  GND------------------------------GND
  D1-------------------------------Data
  5V-------------------------------VCC````
