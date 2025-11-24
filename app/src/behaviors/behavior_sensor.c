#include <zephyr/kernel.h>
#include <zephyr/device.h>
#include <zephyr/drivers/sensor.h>
#include <zephyr/logging/log.h>

#include <zmk/behavior.h>
#include <zmk/event_manager.h>
#include <zmk/events/keycode_state_changed.h>

LOG_MODULE_DECLARE(zmk, CONFIG_ZMK_LOG_LEVEL);

struct behavior_sensor_config {
    const struct device *sensor_dev;
    int32_t threshold;
};

static int behavior_sensor_init(const struct device *dev) {
    const struct behavior_sensor_config *cfg = dev->config;

    if (!device_is_ready(cfg->sensor_dev)) {
        LOG_ERR("Sensor device not ready");
        return -EINVAL;
    }

    LOG_INF("behavior-sensor initialized");
    return 0;
}

static int behavior_sensor_keypress(struct zmk_behavior_binding *binding,
                                    struct zmk_behavior_binding_event event) {
    const struct device *dev = zmk_behavior_get_binding_device(binding);
    const struct behavior_sensor_config *cfg = dev->config;

    struct sensor_value val;

    int err = sensor_sample_fetch(cfg->sensor_dev);
    if (err) {
        LOG_ERR("Failed sensor_sample_fetch: %d", err);
        return err;
    }

    err = sensor_channel_get(cfg->sensor_dev, SENSOR_CHAN_PROX, &val);
    if (err) {
        LOG_ERR("Failed sensor_channel_get: %d", err);
        return err;
    }

    int32_t value = val.val1;

    LOG_DBG("Sensor value = %d, threshold = %d", value, cfg->threshold);

    if (value >= cfg->threshold) {
        LOG_INF("Threshold reached → sending key");
        ZMK_EVENT_RAISE(new_zmk_keycode_state_changed((uint32_t)binding->param1, true));
    }

    return 0;
}

static const struct behavior_driver_api behavior_sensor_driver_api = {
    .binding_pressed = behavior_sensor_keypress,
};

#define DT_DRV_COMPAT zmk_behavior_sensor

#define SENSOR_INST(n) \
    static struct behavior_sensor_config behavior_sensor_config_##n = { \
        .sensor_dev = DEVICE_DT_GET(DT_INST_PHANDLE(n, sensor)), \
        .threshold = DT_INST_PROP(n, threshold), \
    }; \
    DEVICE_DT_INST_DEFINE(n, behavior_sensor_init, NULL, NULL, \
                          &behavior_sensor_config_##n, APPLICATION, \
                          CONFIG_KERNEL_INIT_PRIORITY_DEFAULT, \
                          &behavior_sensor_driver_api);

DT_INST_FOREACH_STATUS_OKAY(SENSOR_INST)
