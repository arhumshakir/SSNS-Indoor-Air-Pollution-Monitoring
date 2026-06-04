#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>

LOG_MODULE_REGISTER(ssns_edge, LOG_LEVEL_INF);

int main(void) {
    LOG_INF("Smart Sensor Network Systems - Edge Node Initialized.");
    while (1) {
        k_sleep(K_MSEC(1000));
    }
    return 0;
}