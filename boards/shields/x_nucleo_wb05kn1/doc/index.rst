.. _x-nucleo-wb05kn1:

X-NUCLEO-WB05KN1: BLE expansion board
#####################################

Overview
********
The X-NUCLEO-WB05KN1 is a Bluetooth Low Energy evaluation board based on the
BlueNRG-LPS RF module to allow expansion of the STM32 Nucleo boards.

The X-NUCLEO-WB05KN1 is compatible out of the box with the Arduino UNO R3 connector,
and it also supports ST Morpho connector layout, which should be mounted if required. The
X-NUCLEO-WB05KN1 interfaces with the host microcontroller via UART (default) or SPI peripheral.

Configurations
**************

X-NUCLEO-WB05KN1 can be utilized as a Bluetooth Low-Energy controller shield
with a UART or SPI host controller interface (HCI-UART/HCI-SPI).

The UART default settings are:

* Baudrate: 921600 Kbps
* 8 bits, no parity, 1 stop bit

+----------+-----------------------+
| UART Pin | Arduino Connector Pin |
+==========+=======================+
| RX       | D0                    |
+----------+-----------------------+
| TX       | D1                    |
+----------+-----------------------+

.. note::
   Please, bear in mind in order to use SPI interface you need to change the shield firmware
   to ``SPI_WITH_UPDATER_CONTROLLER`` according to the SDK provided by ST.

IRQ and reset pins are also necessary in addition to SPI pins.

+----------------+-----------------------+
| SPI Config Pin | Arduino Connector Pin |
+================+=======================+
| SCK            | D13                   |
+----------------+-----------------------+
| MISO           | D12                   |
+----------------+-----------------------+
| MOSI           | D11                   |
+----------------+-----------------------+
| CS             | D10                   |
+----------------+-----------------------+
| IRQ            | A0                    |
+----------------+-----------------------+
| RESET          | D7                    |
+----------------+-----------------------+

Programming
***********

Activate the presence of the shield for the project build by adding the
``-DSHIELD=x_nucleo_wb05kn1_uart`` or ``-DSHIELD=x_nucleo_wb05kn1_spi`` arg to the build command
based on UART or SPI interface:

 .. zephyr-app-commands::
    :zephyr-app: your_app
    :board: your_board_name
    :shield: x_nucleo_wb05kn1_uart
    :goals: build
or
 .. zephyr-app-commands::
    :zephyr-app: your_app
    :board: your_board_name
    :shield: x_nucleo_wb05kn1_spi
    :goals: build
