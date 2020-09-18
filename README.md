# A Handheld Readout Device(HRD) for Battery Management Systems
Monitoring Battery Quality and reduce waste.

A standalone HRD to enable long-term diagnostic observations and 
documentation of battery quality.

Supported:

- TEXAS INSTRUMENTS bq40z50
- ATMEL ATmega32HVB

![My image](https://github.com/peterloes/HRD/blob/master/Getting_Started_Tutorial/2_Electronic_board.jpg)

Rechargeable batteries are more and more dominating our modern life.
We find them in our smartphones and notebooks, in e-bikes and e-cars but also in many toys and
basically everywhere where wired power supply is absent.
The charge state of these batteries is usually reported by highlighting a certain portion of a bar, representing full charge.
But is this indication always correct and how can I check whether the full capacity of my battery is still available?

In many rechargeable batteries, technical parameters are controlled by a Battery Management System(BMS).
This device monitors physical parameters like Voltage, Current, Temperature and State of Charge but also
calculates useful values like "Total Number of Charge Cycles", "Total Operation Time since First Use "or"
Energy Delivered since Last Charge".

In product manufacturing, this information is important for checking the battery quality before final assembly
on the product. For battery-driven medical devices, reliable knowledge about the battery status can be crucial.
For the end-user of a battery driven device, access to these data would be beneficial to allow for an informed decision
whether the device could be repaired or needs to be replaced.
 
The efficient use of batteries is especially important, as batteries are difficult to recycle and thus constitute a
burden for the environment. Unfortunately, common BMSs do not display the stored data by default.

![My image](https://github.com/peterloes/HRD/blob/master/Getting_Started_Tutorial/2_Mechanik_HRD.JPG)

Battery controller probing requests. 0x0A=Atmel, 0x16=TI

Actual Current is signed.

Power management for the LC-Display.

Display will be scrolled (11 digits).

The standalone HRD features EFM32 ...the world´s most energy friendly microcontrollers

ARM Cortex-M3 EFM32G230F128

https://github.com/peterloes/HRD/blob/master/Getting_Started_Tutorial/1_poster_overview.pdf

Supplier:

https://github.com/peterloes/HRD/blob/master/Getting_Started_Tutorial/3_Supplier.txt
