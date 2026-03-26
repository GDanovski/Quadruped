/*
 * Copyright (C) 2026 Georgi Danovski
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <https://www.gnu.org/licenses/>.
 */

#include <zephyr/devicetree.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include <zephyr/sys/atomic.h>

#include "led/led.h"

LOG_MODULE_REGISTER(led, CONFIG_LED_MODULE_LOG_LEVEL);

#define APP_LED_NODE DT_NODELABEL(led_module)

#if !DT_NODE_HAS_STATUS(APP_LED_NODE, okay)
#error "Devicetree node led_module is not defined"
#endif

static const struct gpio_dt_spec app_led = GPIO_DT_SPEC_GET(APP_LED_NODE, led_gpios);
static atomic_t app_led_initialized = ATOMIC_INIT(0);

static int led_init_internal(void)
{
    if (atomic_get(&app_led_initialized) != 0)
    {
        return 0;
    }

    if (!gpio_is_ready_dt(&app_led))
    {
        return -ENODEV;
    }

    int ret = gpio_pin_configure_dt(&app_led, GPIO_OUTPUT_INACTIVE);

    if (ret != 0)
    {
        return ret;
    }

    atomic_set(&app_led_initialized, 1);
    return 0;
}

static int led_sys_init(void)
{
    int ret = led_init_internal();

    if (ret != 0)
    {
        LOG_ERR("led_init failed during SYS_INIT: %d", ret);
    }

    return ret;
}

SYS_INIT(led_sys_init, APPLICATION, CONFIG_APPLICATION_INIT_PRIORITY);

int led_set(bool on)
{
    if (atomic_get(&app_led_initialized) == 0)
    {
        return -EACCES;
    }

    return gpio_pin_set_dt(&app_led, on ? 1 : 0);
}

int led_toggle(void)
{
    if (atomic_get(&app_led_initialized) == 0)
    {
        return -EACCES;
    }

    return gpio_pin_toggle_dt(&app_led);
}
