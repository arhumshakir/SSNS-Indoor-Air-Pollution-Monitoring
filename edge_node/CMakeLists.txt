#
# CMakeLists.txt -- IAQ edge node (nRF52840 DK)
#
cmake_minimum_required(VERSION 3.20.0)

find_package(Zephyr REQUIRED HINTS $ENV{ZEPHYR_BASE})
project(iaq_edge_node)

target_sources(app PRIVATE
    src/main.c
    src/sensirion_common.c
    src/scd41.c
    src/sps30.c
    src/ccs811.c
    src/mcp9808.c
    src/ldr.c
)
