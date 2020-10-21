# EJA Intelligent Buoy B - Firmware

EJA Intelligent Buoy B - Firmware - 2020 Hackaday Dream Team Challenge for Conservation X Labs

This repository contains the firmware (Arduino) project used with the [electronic design of the Intelligent Buoy B](https://github.com/leonardoward/eja-intelligent-buoy-b-electronics).

## Requisites ##

1. [ESP32 Add-on in Arduino IDE](https://randomnerdtutorials.com/installing-the-esp32-board-in-arduino-ide-windows-instructions/)
2. [ESPAsyncWebServer](https://github.com/me-no-dev/ESPAsyncWebServer)
3. [AsyncTCP](https://github.com/me-no-dev/AsyncTCP)
4. [SPIFFS File System](https://randomnerdtutorials.com/esp32-web-server-spiffs-spi-flash-file-system/)
5. [ESP32 Async Over The Air](https://github.com/ayushsharma82/AsyncElegantOTA)
6. [TinyGPSPlus](https://github.com/mikalhart/TinyGPSPlus)
7. [LoRa](https://randomnerdtutorials.com/esp32-lora-rfm95-transceiver-arduino-ide/)

## User Interface ##

The ESP32 creates an Asynchronous Web Server, with the following credentials:

```
const char* ssid     = "EJA_Buoy_B";
const char* password = "123456789";
```

With those credentials it is possible to access the WiFi network.

![alt text](./img/ssid.jpg "Wifi network")

The ESP32 creates a Domain Name System (DNS) to assign a domain name to the web server. That means that it is not necessary to know the server IP, it is possible to access the web server using the defined domain name for the host, in this case, the name is:

```
const char* host = "www.buoy_b.eja";
```

Once the device (phone/computer) is connected to the server (EJA_Onboard_Gateway), open a browser and visit the following url:

```
http://www.buoy_b.eja/
```

The previous link displays the web page used for HOME:

<img src="./img/home.jpg" width="50%">

There is a header with the name "EJA Buoy B" that opens a sidebar with links to the different test functionalities. It is also possible to access those pages using the buttons in HOME.

<img src="./img/menu.jpg" width="50%">

The page http://www.buoy_b.eja/gps shows the data from the GPS module.

<img src="./img/gps.jpg" width="50%">

The page http://www.buoy_b.eja/lora shows internal messages related to LoRa. In the ESP32 that information is stored in the variable:

```
String lora_all_msg = "";
```

<img src="./img/lora.jpg" width="50%">

The page "http://www.buoy_b.eja/terminal" shows internal messages. In the ESP32 that information is stored in the variable:

```
String terminal_messages = "";
```

<img src="./img/terminal.jpg" width="50%">
