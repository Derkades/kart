# Kart

## Dashboard

Information and settings screen on steering wheel. PlatformIO project for STM32.

## Controller

Receives analog signal from acceleration/deceleration pedals (or rather, forwards and backwards pedals). The target speed/torque is sent to the two hoverboard controllers via serial, to ensure they both run at exactly the same speed.

## Hoverboard

Motor controllers (2x). For firmware see https://codeberg.org/raphson/hoverboard
