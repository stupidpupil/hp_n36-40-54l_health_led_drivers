# Health LED for HP Microservers N36L/N40L/N54L

This repository contains 3 drivers intended for use in HP Microservers N36L/N40L/N54L to allow control of their 'Health LED'.

They are packaged as [DKMS](https://en.wikipedia.org/wiki/Dynamic_Kernel_Module_Support) modules, a script is provided to cobble them into Debian packages, and another script is provided as an example.

## Installation
```sh
# Install requirements
sudo apt-get install -qq fakeroot dkms
sudo apt install linux-headers-$(uname -r)

# Install the drivers
git clone https://github.com/stupidpupil/hp_n36-40-54l_health_led_drivers.git
cd hp_n36-40-54l_health_led_drivers
/bin/sh build_all_debs.sh
sudo dpkg -i dist/*.deb

# Remove the existing drivers and blacklist sp5100_tco
sudo rmmod i2c_piix4
sudo rmmod sp5100_tco
echo "blacklist sp5100_tco" | sudo tee /etc/modprobe.d/blacklist-sp5100_tco.conf

# Try the example script
sudo /bin/sh n40_led.sh 0 100 # Switch the Health LED to orange
sudo /bin/sh n40_led.sh 100 100 # Switch the Health LED to pink (blue+orange)
sudo /bin/sh n40_led.sh 5 5 # Reduce the brightness of the pink

```

## About the Health LED
The 'Health LED' contains three different coloured LEDs - blue, orange and red - as far as I know.
I don't know how to control the red LED, but the blue and orange LEDs are 
controlled using the general-purpose-input-output (GPIO) pins from the southbridge (AMD SB820M) of the motherboard itself.

Specifically:
* the blue LED is controlled using pin 188 and 
* the orange LED is controlled using pin 187. 

(These pins are also one of the SMBus/i2c pairs - 'port 4' on the main SMBus controller. 
In other versions of the southbridge they're also used for PS/2 ports. This is documented in the [AMD SB820M Southbridge Databook](https://support.amd.com/TechDocs/47283.pdf).)

### Older versions of the BIOS
Older versions of the BIOS for N36L/N40L/N54L's don't seem to support accessing these GPIO pins. These drivers are known to work on at least the *07/29/2011* version of the BIOS. 

You can check what version your machine is running with the command `sudo dmidecode --string "bios-release-date"`, or by booting into the *ROM-BASED SETUP UTILITY* (by pressing F10 at startup) and checking the *BIOS Version* shown.

## The drivers

### i2c-piix4
A backported version of [04b6fca](https://github.com/torvalds/linux/commit/04b6fcaba346e1ce76321ba9b0fd549da4c37ac2), likely to appear in Linux 4.17. Importantly, this allows other drivers to access shared parts of the AMD SB820M southbridge.

Only very minor changes were required to get it to compile under Linux 4.9. I have also (crudely) disabled 'port 4' on the main SMBus controller in the driver, as in the N36L/N40L/N54L the relevant pins are used as GPIO to control the Health LED.

You might want to investigate [fetzerch's repo for using sensors on the i2c bus](https://github.com/fetzerch/hp-n54l-drivers). Note that you don't need to use fetzerch's i2c-piix4 driver.

Copyright (c) 1998 - 2002 Frodo Looijaard and Philip Edelbrock, and others shown on the GitHub link above.

### gpio-sb8xx
A driver to allow controlling the GPIO pins on the AMD SB820M southbridge. It was [submitted by Tobias Diedrich in 2015](https://patchwork.kernel.org/patch/6651771/), but was not included in the mainline kernel.

Minor updates were required to get it to compile under Linux 4.9. It has also been patched to ignore requests to change the direction of pins to 'out' where those pins already had a direction of 'out'; this avoids a situation where a direction change is unnecessarily requested and fails as the pin is not marked as safe for use as an 'out' pin.

Copyright (c) 2015 Tobias Diedrich.

### softpwm
Generic software-only driver for generating PWM signals via high resolution timers and GPIO lib interface.

Copyright (C) 2010 Antonio Galea, modified by Sergio Tanzilli.

## Issues
You'll need to [blacklist](https://wiki.debian.org/KernelModuleBlacklisting) the *sp5100_tco* watchdog driver. I don't believe this watchdog works on the Microservers.

It's possible to provoke a kernel panic by removing the *i2c-piix4* driver while using the GPIO pins via the *gpio-sb8xx* driver. Don't do that.
