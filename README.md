# LORA-GPS-Tracker

**This page is a work in progress!**

<img src="docs/todo.png" width="500px"></a>

This LoRa-GPS-Tracker was designed to just test three modules, which I haven't worked with yet: the RFM95 LoRa module, the ESP32-S2-Mini module and the ATGM336h GPS module. All combined this can be used as a GPS-Tracker for LoRa based systems. Additionally to the three modules, the board includes a 0.96" OLED display, two user buttons, a user RGB LED, a LiPo battery charger with a 16340 cell holder, and a dual LDO setup, which is very handy when it comes to deep sleeping.

Here are some specs of the board:
- ESP32-S2-Mini
- USB Type-C for charging the battery and flashing the ESP32
- Dual LDO setup, which will reduce the sleep current (the main power sinks can be shut down before entering a sleep mode)
- Power friendly OLED display
- Dimensions are XXxXX


## Connections

GPIO ESP32 | Function | Mode
-------- | -------- | --------
GPIO1 | LORA_RST | Output
GPIO2 | LORA_MISO | Input
GPIO3 | LORA_MOSI | Output
GPIO4 | LORA_SCK | Output
GPIO5 | LORA_CS | Output
GPIO6 | LORA_INT | Input
GPIO7 | LORA_DIO1 | Data
GPIO8 | LORA_DIO2 | Data
GPIO12 | Button2 | Input
GPIO14 | Enable LDO2 | Output
GPIO15 | BAT_ADC | Input
GPIO16 | Button1 | Input
GPIO19 | USB_DN | Data
GPIO20 | USB_DP | Data
GPIO34 | OLED_RST | Output
GPIO35 | I2C_SCL | Output
GPIO36 | I2C_SDA | Data
GPIO37 | LED_CLK | Output
GPIO38 | LED_SDI | Data
GPIO40 | GPS_PPS | Input
GPIO41 | GPS_RXD | Input
GPIO42 | GPS_TXD | Output
