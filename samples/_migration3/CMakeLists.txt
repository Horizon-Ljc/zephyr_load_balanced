# SPDX-License-Identifier: Apache-2.0

cmake_minimum_required(VERSION 3.20.0)

find_package(Zephyr REQUIRED HINTS $ENV{ZEPHYR_BASE})
project(_migraition1)

# set(CMAKE_BUILD_TYPE Debug)

target_include_directories(app PUBLIC /home/rlk/zephyrproject/zephyr/kernel/include)
target_include_directories(app PUBLIC /home/rlk/zephyrproject/zephyr/arch/x86/include)
# target_include_directories(app PUBLIC /home/rlk/zephyrproject/zephyr/arch/arm/include)

target_sources(app PRIVATE src/main.c)
target_sources(app PRIVATE src/utility.c)