# garagedoor
Garage door closer

This is a custom firmware image for an otherwise crummy garage door closer from Amazon: https://www.amazon.com/gp/product/B07NNVX1K7/

(Work in progress)

Features:
- Autonomous operation. This makes the unit autonomous, and can work even if the wifi is off.
- Lockdown mode. This mode makes the unit ignore OPEN commands from the cloud.

## JTAG notes:
Here's how to connect the JTAG.
- STLINK v2: (from Left to Right, where the ST logo, LED, and USB cables are facing you)
  - (Use the STM8 port)
  - Blue, Black, Green, Red

- The board: (from Left to Right, where the board is facing up, and the relay is facing you, and the wifi module is facing away from you)
  - (Use J1)
  - Blue, Black, Green, Red

- The board: (other connectors)
  - (Same alignment as with JTAG) The board is face up (components point to the sky), and the relay faces you, and the wifi faces away from you).
  - The mains connector is on the right side.
  - The mains cable: White is close to you (Near the capacitor), Black is far away.
  - J3 is the switch to the garage motor. Square pad is the black wire.
  - J4 is the door sensor. Square pad is the black wire.
  
