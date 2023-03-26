# ESP32 SRAM PUF for IoT

This repo contains a firmware for IoT devices based on ESP32 platform. The project implements a software-based SRAM PUF (Physically Unclonable Function) that can be used to generate a unique value. This value can then be utilized to implement security primitives. The project aims to provide a secure and reliable way of storing and transmitting data in IoT devices.

## Table of Contents

1. [About the Project](#about-the-project)
1. [Getting Started](#getting-started)
    1. [Set up the tools](#set-up-the-tools)
    1. [Set up the environment variables](#set-up-the-environment-variables)
    1. [Build the project](#build-the-project)
    1. [Flash the firmware](#flash-the-firmware)
    1. [Monitoring](#monitoring)
1. [License](#license)
1. [Authors](#authors)

## About the Project

The project implements a software-based SRAM PUF (Physically Unclonable Function) that can be used to generate a unique value. This value can then be utilized to implement security primitives. The project aims to provide a secure and reliable way of storing and transmitting data in IoT devices.

**[Back to top](#table-of-contents)**

## Getting Started

This section should provide instructions for other developers to

These instructions will get you a copy of the project up and running on your local machine for development and testing purposes. See deployment for notes on how to deploy the project on a live system.

### Set up the tools

The first time you clone this repo, or when the ESP IDF version is changed, you need to install dependencies with following command:

```
esp-idf/install.sh esp32
```

### Set up the environment variables

Each time you want to open a terminal you have to use this command to export the required environment variabled:

```
. esp-idf/export.sh
```

### Build the project

Build the project by running:

```
idf.py build
```

### Flash the firmware

Connect your ESP32 board to the computer and check under what serial port the board is visible.
Serial ports have the following patterns in their names:
* Linux: starting with `/dev/tty`
* macOS: starting with `/dev/cu.`

Flash the binaries onto your ESP32 board by running:

```
idf.py -p PORT flash
```

Replace `PORT` with your ESP32 board’s serial port name.

### Monitoring

To run the serial monitor (IDF monitor) you can do as follow:

```
idf.py -p PORT monitor
``` 

To exit IDF monitor use the shortcut `Ctrl+]`.

**[Back to top](#table-of-contents)**

## License

Copyright (c) 2023 Cristiano Di Bari

This project is licensed under the MIT License - see [LICENSE](LICENSE) file for details.

**[Back to top](#table-of-contents)**

## Authors

* **[Cristiano Di Bari](https://github.com/cridiba)**

**[Back to top](#table-of-contents)**
