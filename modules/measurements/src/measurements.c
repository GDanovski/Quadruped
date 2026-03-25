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

#include <errno.h>
#include <stdbool.h>
#include <stdint.h>

#include <zephyr/devicetree.h>
#include <zephyr/device.h>
#include <zephyr/drivers/adc.h>
#include <zephyr/drivers/sensor.h>
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>

#include "measurements/measurements.h"

#define DT_DRV_COMPAT danovski_measurements

LOG_MODULE_REGISTER(measurements, CONFIG_MEASUREMENTS_MODULE_LOG_LEVEL);

BUILD_ASSERT(DT_HAS_COMPAT_STATUS_OKAY(DT_DRV_COMPAT),
             "No status=\"okay\" danovski,measurements nodes found in devicetree");

struct measurements_config
{
    struct adc_dt_spec adc;
    uint8_t resolution;
    uint8_t oversampling;
    bool calibrate;
    uint16_t voltage_divider_num;
    uint16_t voltage_divider_denom;
};

struct measurements_data
{
    int32_t millivolts;
};

static int measurements_sample_fetch_impl(const struct device *dev, enum sensor_channel chan)
{
    const struct measurements_config *cfg = dev->config;
    struct measurements_data *data = dev->data;
    int16_t raw;
    struct adc_sequence sequence = {
        .buffer = &raw,
        .buffer_size = sizeof(raw),
    };
    int ret;

    if ((chan != SENSOR_CHAN_ALL) && (chan != SENSOR_CHAN_VOLTAGE))
    {
        return -ENOTSUP;
    }

    ret = adc_sequence_init_dt(&cfg->adc, &sequence);
    if (ret != 0)
    {
        LOG_ERR("adc_sequence_init_dt failed: %d", ret);
        return ret;
    }

    sequence.resolution = cfg->resolution;
    sequence.oversampling = cfg->oversampling;
    sequence.calibrate = cfg->calibrate;

    ret = adc_read_dt(&cfg->adc, &sequence);
    if (ret != 0)
    {
        LOG_ERR("adc_read_dt failed: %d", ret);
        return ret;
    }

    data->millivolts = raw;
    ret = adc_raw_to_millivolts_dt(&cfg->adc, &data->millivolts);
    if (ret != 0)
    {
        LOG_ERR("adc_raw_to_millivolts_dt failed: %d", ret);
        return ret;
    }

    /* Apply voltage divider correction: V_actual = V_adc * (numerator / denominator) */
    if ((cfg->voltage_divider_denom > 0) && (cfg->voltage_divider_num != cfg->voltage_divider_denom))
    {
        data->millivolts = (data->millivolts * cfg->voltage_divider_num) / cfg->voltage_divider_denom;
    }

    return 0;
}

static int measurements_channel_get_impl(const struct device *dev,
                                         enum sensor_channel chan,
                                         struct sensor_value *val)
{
    const struct measurements_data *data = dev->data;

    if (chan != SENSOR_CHAN_VOLTAGE)
    {
        return -ENOTSUP;
    }

    val->val1 = data->millivolts / 1000;
    val->val2 = (data->millivolts % 1000) * 1000;

    return 0;
}

static const struct sensor_driver_api measurements_api = {
    .sample_fetch = measurements_sample_fetch_impl,
    .channel_get = measurements_channel_get_impl,
};

static int measurements_init(const struct device *dev)
{
    const struct measurements_config *cfg = dev->config;

    if (!adc_is_ready_dt(&cfg->adc))
    {
        LOG_ERR("ADC controller not ready");
        return -ENODEV;
    }

    return adc_channel_setup_dt(&cfg->adc);
}

#define MEASUREMENTS_DEFINE(inst)                                          \
    static struct measurements_data measurements_data_##inst;              \
    static const struct measurements_config measurements_config_##inst = { \
        .adc = ADC_DT_SPEC_INST_GET_BY_IDX(inst, 0),                       \
        .resolution = DT_INST_PROP_OR(inst, resolution, 12),               \
        .oversampling = DT_INST_PROP_OR(inst, oversampling, 4),            \
        .calibrate = DT_INST_PROP(inst, calibrate),                        \
        .voltage_divider_num = DT_INST_PROP_OR(inst, voltage_divider_numerator, 1),     \
        .voltage_divider_denom = DT_INST_PROP_OR(inst, voltage_divider_denominator, 1), \
    };                                                                     \
    SENSOR_DEVICE_DT_INST_DEFINE(inst,                                     \
                                 measurements_init,                        \
                                 NULL,                                     \
                                 &measurements_data_##inst,                \
                                 &measurements_config_##inst,              \
                                 POST_KERNEL,                              \
                                 CONFIG_MEASUREMENTS_MODULE_INIT_PRIORITY, \
                                 &measurements_api);

DT_INST_FOREACH_STATUS_OKAY(MEASUREMENTS_DEFINE)

const struct device *measurements_get_device(void)
{
    const struct device *dev = DEVICE_DT_GET(DT_NODELABEL(measurements));

    if (!device_is_ready(dev))
    {
        return NULL;
    }

    return dev;
}

int measurements_sample_fetch(void)
{
    const struct device *dev = measurements_get_device();

    if (dev == NULL)
    {
        return -ENODEV;
    }

    return sensor_sample_fetch_chan(dev, SENSOR_CHAN_VOLTAGE);
}

int measurements_channel_get(struct sensor_value *val)
{
    const struct device *dev = measurements_get_device();

    if (dev == NULL)
    {
        return -ENODEV;
    }

    return sensor_channel_get(dev, SENSOR_CHAN_VOLTAGE, val);
}
