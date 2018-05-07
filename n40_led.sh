#!/bin/sh

BLUE_VALUE="$1"
ORANGE_VALUE="$2"

BLUE_PIN=188
ORANGE_PIN=187

PWM_PERIOD=10000 #microseconds

if ! BASE=$(cat "/sys/class/gpio/gpiochip256/base"); then
  echo "Couldn't determine gpio pin base"
  exit
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

