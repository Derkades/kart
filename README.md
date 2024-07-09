# Kart

## Dashboard

Information and settings screen on steering wheel. PlatformIO project for STM32.

## Controller

Receives analog signal from acceleration/deceleration pedals (or rather, forwards and backwards pedals). The target speed/torque is sent to the two hoverboard controllers via serial, to ensure they both run at exactly the same speed.

## Hoverboard

Motor controllers (2x). Can be compiled and flashed using the Makefile or PlatformIO.

Initialize submodule:
```
git submodule update --init
```

To apply changes to the submodule:
```
./hoverboard-patch-apply.sh
```

To save changes to the submodule, to a patch file in this repository:
```
./hoverboard-patch-create.sh
git add hoverboard.patch
git commit
```
