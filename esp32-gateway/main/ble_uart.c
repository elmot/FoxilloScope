#include <string.h>
#include <stdlib.h>
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "nimble/nimble_port.h"
#include "nimble/nimble_port_freertos.h"
#include "host/ble_hs.h"
#include "host/ble_gap.h"
#include "host/ble_uuid.h"
#include "host/ble_att.h"
#include "host/ble_gatt.h"
#include "services/gap/ble_svc_gap.h"
#include "services/gatt/ble_svc_gatt.h"
#include "gateway.h"

#define TAG "ble_uart"

void ble_store_config_init(void);

/* 6623a8e1-77d3-4e35-a01c-4d649ff5fb07 */
static const ble_uuid128_t osc_svc_uuid = BLE_UUID128_INIT(
    0x07, 0xFB, 0xF5, 0x9F, 0x64, 0x4D, 0x1C, 0xA0,
    0x35, 0x4E, 0xD3, 0x77, 0xE1, 0xA8, 0x23, 0x66);
/* 6623a8e2-77d3-4e35-a01c-4d649ff5fb07 */
static const ble_uuid128_t osc_tx_char_uuid = BLE_UUID128_INIT(
    0x07, 0xFB, 0xF5, 0x9F, 0x64, 0x4D, 0x1C, 0xA0,
    0x35, 0x4E, 0xD3, 0x77, 0xE2, 0xA8, 0x23, 0x66);
/* 6623a8e3-77d3-4e35-a01c-4d649ff5fb07 */
static const ble_uuid128_t osc_rx_char_uuid = BLE_UUID128_INIT(
    0x07, 0xFB, 0xF5, 0x9F, 0x64, 0x4D, 0x1C, 0xA0,
    0x35, 0x4E, 0xD3, 0x77, 0xE3, 0xA8, 0x23, 0x66);

static uint16_t osc_tx_val_handle;
static bool s_notify_enabled;
static uint16_t s_conn_handle;

static constexpr uint8_t s_slave_itvl_range[] = { 6, 0, 6, 0 };

static void ble_advertise(void);

static int osc_char_access([[maybe_unused]] uint16_t conn_handle, [[maybe_unused]] uint16_t attr_handle,
                           // ReSharper disable once CppParameterMayBeConstPtrOrRef
                           struct ble_gatt_access_ctxt* ctxt, [[maybe_unused]] void* arg)
{
    if (ctxt->op == BLE_GATT_ACCESS_OP_WRITE_CHR) {
        char buf[512];
        uint16_t len = 0;
        const int rc = ble_hs_mbuf_to_flat(ctxt->om, buf, sizeof(buf) - 1, &len);
        if (rc == 0 && len > 0) {
            buf[len] = '\0';
            uart_write_str(buf);
        }
        return 0;
    }
    if (ctxt->op == BLE_GATT_ACCESS_OP_READ_CHR)
        return BLE_ATT_ERR_READ_NOT_PERMITTED;
    return 0;
}

// ReSharper disable once CppDFAConstantFunctionResult, CppParameterMayBeConstPtrOrRef
static int osc_gap_event(struct ble_gap_event* event, [[maybe_unused]] void* arg)
{
    switch (event->type) {
    case BLE_GAP_EVENT_CONNECT:
        if (event->connect.status == 0) {
            ble_gap_adv_stop();
            s_notify_enabled = false;
            s_conn_handle = event->connect.conn_handle;
            led_refresh();

            struct ble_gap_conn_desc desc;
            if (ble_gap_conn_find(s_conn_handle, &desc) == 0)
                ESP_LOGI(TAG, "connected itvl=%dms lat=%d supv=%dms",
                         desc.conn_itvl * 5 / 4, desc.conn_latency,
                         desc.supervision_timeout * 10);
            kick_out_ws_client();
        } else {
            ble_advertise();
        }
        return 0;

    case BLE_GAP_EVENT_DISCONNECT:
        s_notify_enabled = false;
        s_conn_handle = 0;
        led_refresh();
        ble_advertise();
        return 0;

    case BLE_GAP_EVENT_SUBSCRIBE:
        ESP_LOGI(TAG, "subscribe notify=%d", event->subscribe.cur_notify);
        if (event->subscribe.cur_notify) {
            s_notify_enabled = true;
            const uint16_t ch = event->subscribe.conn_handle;
            ble_gap_set_prefered_le_phy(ch, BLE_GAP_LE_PHY_2M_MASK, BLE_GAP_LE_PHY_2M_MASK, 0);
            ble_gap_set_data_len(ch, 251, 2120);
            static const struct ble_gap_upd_params cp = {
                .itvl_min = 6, .itvl_max = 6, .latency = 0,
                .supervision_timeout = 600,
            };
            led_refresh();
            ble_gap_update_params(ch, &cp);
        }
        return 0;

    default:
        return 0;
    }
}

static void ble_advertise()
{
    struct ble_hs_adv_fields fields = {0};
    fields.flags = BLE_HS_ADV_F_DISC_GEN | BLE_HS_ADV_F_BREDR_UNSUP;
    fields.uuids128_is_complete = 1;
    fields.num_uuids128 = 1;
    fields.uuids128 = &osc_svc_uuid;
    fields.slave_itvl_range = s_slave_itvl_range;
    ble_gap_adv_set_fields(&fields);

    struct ble_hs_adv_fields rsp_fields = {0};
    rsp_fields.name = (uint8_t *)CONFIG_OSC_BLE_DEVICE_NAME;
    rsp_fields.name_len = strlen(CONFIG_OSC_BLE_DEVICE_NAME);
    rsp_fields.name_is_complete = 1;
    ble_gap_adv_rsp_set_fields(&rsp_fields);

    struct ble_gap_adv_params adv_params = {0};
    adv_params.conn_mode = BLE_GAP_CONN_MODE_UND;
    adv_params.disc_mode = BLE_GAP_DISC_MODE_GEN;
    ble_gap_adv_start(BLE_OWN_ADDR_PUBLIC, nullptr, BLE_HS_FOREVER,
                      &adv_params, osc_gap_event, nullptr);
}

static void ble_host_sync()
{
    ble_att_set_preferred_mtu(256);
    ble_advertise();
}

static void ble_host_reset(const int reason)
{
    ESP_LOGI(TAG, "reset reason=%d", reason);
}

static const struct ble_gatt_svc_def gatt_svr_svcs[] = {
    {
        .type = BLE_GATT_SVC_TYPE_PRIMARY,
        .uuid = &osc_svc_uuid.u,
        .characteristics = (struct ble_gatt_chr_def[]) { {
            .uuid = &osc_tx_char_uuid.u,
            .access_cb = osc_char_access,
            .flags = BLE_GATT_CHR_F_NOTIFY,
            .val_handle = &osc_tx_val_handle,
        }, {
            .uuid = &osc_rx_char_uuid.u,
            .access_cb = osc_char_access,
            .flags = BLE_GATT_CHR_F_WRITE,
        }, {
            0,
        } },
    }, {
        0,
    },
};

static void ble_host_task([[maybe_unused]]void *param)
{
    nimble_port_run();
    nimble_port_freertos_deinit();
}

void ble_transmit(const char *text,const int len)
{
    if (!s_notify_enabled || osc_tx_val_handle == 0 || len == 0) return;

    const int mtu = ble_att_mtu(s_conn_handle);
    int max_payload = mtu - 3;
    if (max_payload < 1) max_payload = 20;
    if (max_payload > 514) max_payload = 514;

    int offset = 0;
    int retries = 40;
    while (offset < len)
    {
        if (!s_notify_enabled) break;

        const int remaining = len - offset;
        const int chunk = remaining > max_payload ? max_payload : remaining;

        struct os_mbuf* om = ble_hs_mbuf_from_flat(text + offset, chunk);
        if (!om) goto retry;

        const int rc = ble_gatts_notify_custom(s_conn_handle, osc_tx_val_handle, om);
        if (rc == 0)
        {
            offset += chunk;
            retries = 40;
        }
        else if (rc == BLE_HS_EBUSY || rc == BLE_HS_ENOMEM)
        {
            goto retry;
        }
        else
        {
            ESP_LOGW(TAG, "notify failed rc=%d", rc);
            break;
        }

        taskYIELD();
        continue;

    retry:
        if (--retries == 0)
        {
            ESP_LOGW(TAG,"BLE transmit retry count exhausted");
            break;
        }
        vTaskDelay(1);
    }
}

void ble_uart_init()
{
    esp_log_level_set("NimBLE", ESP_LOG_WARN);
    if (nimble_port_init() != ESP_OK) return;

    ble_hs_cfg.sync_cb = ble_host_sync;
    ble_hs_cfg.reset_cb = ble_host_reset;
    ble_hs_cfg.store_status_cb = ble_store_util_status_rr;

    ble_svc_gap_device_name_set(CONFIG_OSC_BLE_DEVICE_NAME);
    ble_svc_gap_init();
    ble_svc_gatt_init();

    ble_gatts_count_cfg(gatt_svr_svcs);
    ble_gatts_add_svcs(gatt_svr_svcs);

    ble_store_config_init();

    nimble_port_freertos_init(ble_host_task);
}

void ble_disconnect_client()
{
    if (s_conn_handle != 0) {
        ble_gap_terminate(s_conn_handle, BLE_ERR_REM_USER_CONN_TERM);
    }
}

bool ble_any_connected()
{
    return s_notify_enabled;
}
