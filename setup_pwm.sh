#!/bin/sh

/usr/local/sbin/pwmconf

echo 0 > /sys/class/pwm/pwmchip0/export
echo 1000000 > /sys/class/pwm/pwmchip0/pwm0/period
echo 1000000 > /sys/class/pwm/pwmchip0/pwm0/duty_cycle
echo inversed > /sys/class/pwm/pwmchip0/pwm0/polarity
echo 1 > /sys/class/pwm/pwmchip0/pwm0/enable

chmod 664 /sys/class/pwm/pwmchip0/pwm0/duty_cycle /sys/class/pwm/pwmchip0/pwm0/enable /sys/class/pwm/pwmchip0/pwm0/polarity /sys/class/pwm/pwmchip0/pwm0/period
chgrp gpio /sys/class/pwm/pwmchip0/pwm0/duty_cycle /sys/class/pwm/pwmchip0/pwm0/enable /sys/class/pwm/pwmchip0/pwm0/polarity /sys/class/pwm/pwmchip0/pwm0/period
