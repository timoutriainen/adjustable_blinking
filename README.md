# Adjustable LED blinking
Linux kernel driver for blinking a LED on a Raspberry Pi 3

## Installation

### First backup your Raspberry Pi 3 image just in case something goes wrong

https://www.raspberrypi.org/forums/viewtopic.php?t=26463

## Install kernel headers for Raspbian

https://github.com/notro/rpi-source/wiki -> follow instructions from Install onwards

### Clone this repository to Raspberry Pi 3

git clone https://github.com/timoutriainen/adjustable_blinking adjustable_blinking

### Build driver:

execute build.sh

./build.sh

### Install driver:

execute install_module.sh

./install_mdule.sh

### Remove driver:

execute remove_module.sh

./remove_module.sh

### Wiring:

Connect a LED to the GPIO4 and GND of the Raspberry Pi 3

## Usage:

### Start blinking:

./echo_attrib.sh on

### Stop blinking:

./echo_attrib.sh off

### Change blinking interval to a different value, for example to 150 ms (interval value must be between 50 and 1000):

./echo_attrib.sh 150

### Ask blinking interval:

./cat_attrib.sh
