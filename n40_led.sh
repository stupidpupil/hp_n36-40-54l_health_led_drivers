#!/bin/sh

BLUE_VALUE="$1"
ORANGE_VALUE="$2"

BLUE_PIN=188
ORANGE_PIN=187

PWM_PERIOD=10000 #microseconds

BIOS_YEAR=$(cut -d "/" -f 3 /sys/class/dmi/id/bios_date)
BIOS_MONTH=$(cut -d "/" -f 1 /sys/class/dmi/id/bios_date)
BIOS_DAY=$(cut -d "/" -f 2 /sys/class/dmi/id/bios_date)

MIN_BIOS_DATE=20110729

if ! (echo "$BIOS_YEAR-$BIOS_MONTH-$BIOS_DAY" | grep -Eq '^[0-9]{4}-[0-9]{2}-[0-9]{2}$') ; then
  echo "Couldn't understand BIOS date"
  echo "BIOS date should be 07/29/2011 or later"
  exit 1
fi

BIOS_DATE=$((BIOS_YEAR*10000+BIOS_MONTH*100+BIOS_DAY))

if [ $((BIOS_DATE < MIN_BIOS_DATE)) -eq 1 ]; then
  echo "BIOS date should be 07/29/2011 or later"
  exit 1
fi

if ! modprobe i2c-piix4; then
  echo "Couldn't insert i2c-piix4 driver"
  exit 1
fi

if ! modprobe gpio-sb8xx; then
  echo "Couldn't insert gpio-sb8xx driver"
  exit 1
fi

if ! modprobe softpwm; then
  echo "Couldn't insert softpwm driver"
  exit 1
fi

if ! BASE=$(cat "/sys/class/gpio/gpiochip256/base"); then
  echo "Couldn't determine gpio pin base"
  exit 1
fi

BLUE_PIN=$((BASE+BLUE_PIN))
ORANGE_PIN=$((BASE+ORANGE_PIN))

PWM_BASE_DIR="/sys/class/soft_pwm"

for pin in $ORANGE_PIN $BLUE_PIN ; do
  pwmPath="$PWM_BASE_DIR/pwm$pin"

  if [ ! -d "$pwmPath" ]; then
    if ! echo "$pin" > "$PWM_BASE_DIR/export"; then
      echo "Couldn't export pwm for $pin"
      exit
    fi
  fi

  echo "$PWM_PERIOD" > "$pwmPath/period"

done

BLUE_PULSE=$(((100-BLUE_VALUE)*PWM_PERIOD/100))
ORANGE_PULSE=$(((100-ORANGE_VALUE)*PWM_PERIOD/100))

echo "$BLUE_PULSE" > "$PWM_BASE_DIR/pwm$BLUE_PIN/pulse"
echo "$ORANGE_PULSE" > "$PWM_BASE_DIR/pwm$ORANGE_PIN/pulse"

