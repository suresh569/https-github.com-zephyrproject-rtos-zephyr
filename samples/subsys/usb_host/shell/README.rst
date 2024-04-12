.. zephyr:code-sample:: usb-host-shell
   :name: USB Host shell
   :relevant-api: usb_host_core_api

   Use shell commands to interact with USB host stack.

Overview
********

The sample enables USB host support and the shell function.
It is primarily intended to aid in the development and testing of USB controller
drivers and new USB support.

Building and flashing
*********************

Assuming the board has a supported USB host controller, the example can be
built like:

.. zephyr-app-commands::
   :zephyr-app: samples/subsys/usb_host/shell
   :board: nucleo_h723zg
   :goals: flash
   :compact:

Sample shell interaction
========================

.. code-block:: console

    uart:~$ usbh init
    host: USB host initialized
    uart:~$ usbh enable
    host: USB host enabled
    uart:~$ usbh device address 1
    host: New device address is 0x01
    uart:~$ usbh device descriptor device
    bLength			18
    bDescriptorType		1
    bcdUSB			200
    bDeviceClass		0
    bDeviceSubClass		0
    bDeviceProtocol		0
    bMaxPacketSize0		64
    idVendor			483
    idProduct			3748
    bcdDevice			100
    iManufacturer		1
    iProduct			2
    iSerial			3
    bNumConfigurations		1
    uart:~$ usbh device descriptor configuration 0
    bLength			9
    bDescriptorType		2
    wTotalLength		27
    bNumInterfaces		1
    bConfigurationValue		1
    iConfiguration		0
    bmAttributes		80
    bMaxPower			100 mA
