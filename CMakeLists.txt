cmake_minimum_required(VERSION 3.20.0)
find_package(Zephyr REQUIRED HINTS $ENV{ZEPHYR_BASE})

project(VCP_Renderer)

target_sources(app PRIVATE src/main.c)
target_sources(app PRIVATE src/volumeControlService.c)
target_sources(app PRIVATE src/bluetoothManager.c)
target_sources(app PRIVATE src/peripherals.c)