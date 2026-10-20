#include "ble_link.h"
#include "caipo_ble_proto.h"
#include "r03.h"

#include <stdio.h>
#include <string.h>

#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "sdkconfig.h"

#include "host/ble_gap.h"
#include "host/ble_gatt.h"
#include "host/ble_hs.h"
#include "host/util/util.h"
#include "nimble/nimble_port.h"
#include "nimble/nimble_port_freertos.h"
#include "services/gap/ble_svc_gap.h"
#include "services/gatt/ble_svc_gatt.h"

/* Provided by the NimBLE store (bonds in NVS); no public header declares it. */
void ble_store_config_init(void);

static const char *TAG = "ble_link";

/* A100000X-5fa6-43f5-9827-201f62bd377d, NimBLE wants the 16 bytes reversed. */
#define CAIPO_UUID128(last) \
    BLE_UUID128_INIT(0x7d, 0x37, 0xbd, 0x62, 0x1f, 0x20, 0x27, 0x98, \
                     0xf5, 0x43, 0xa6, 0x5f, (last), 0x00, 0x00, 0xa1)

static const ble_uuid128_t uuid_svc = CAIPO_UUID128(0x00);
static const ble_uuid128_t uuid_data_out = CAIPO_UUID128(0x01); /* notify: device -> app */
static const ble_uuid128_t uuid_codec = CAIPO_UUID128(0x02);    /* read: codec ID */
static const ble_uuid128_t uuid_data_in = CAIPO_UUID128(0x03);  /* write: app -> device */
static const ble_uuid128_t uuid_ctrl = CAIPO_UUID128(0x04);     /* write + notify: control */
static const ble_uuid16_t uuid_bas = BLE_UUID16_INIT(0x180F);
static const ble_uuid16_t uuid_bat_level = BLE_UUID16_INIT(0x2A19);

static struct {
    ble_link_cb_t cb;
    void *ctx;
    uint16_t conn;
    bool connected;
    bool encrypted;
    bool sub_data;
    bool sub_ctrl;
    bool sub_bat;
    uint16_t mtu;
    uint8_t battery;
    uint8_t own_addr_type;
    uint8_t hb_counter;
    bool ready; /* init completed; NimBLE host running */
    char name[32];
    uint16_t h_data_out, h_codec, h_data_in, h_ctrl, h_bat;
} s;

static void emit(ble_link_event_t ev, const uint8_t *data, size_t len)
{
    if (s.cb) {
        s.cb(ev, data, len, s.ctx);
    }
}

/* ----------------------------------------------------------------- GATT */

static int gatt_access(uint16_t conn_handle, uint16_t attr_handle,
                       struct ble_gatt_access_ctxt *ctxt, void *arg)
{
    (void)conn_handle;
    (void)arg;

    switch (ctxt->op) {
    case BLE_GATT_ACCESS_OP_READ_CHR:
        if (attr_handle == s.h_codec) {
            uint8_t v = CAIPO_CODEC_OPUS_16K_20MS;
            return os_mbuf_append(ctxt->om, &v, 1) == 0 ? 0 : BLE_ATT_ERR_INSUFFICIENT_RES;
        }
        if (attr_handle == s.h_bat) {
            return os_mbuf_append(ctxt->om, &s.battery, 1) == 0 ? 0 : BLE_ATT_ERR_INSUFFICIENT_RES;
        }
        return BLE_ATT_ERR_UNLIKELY;

    case BLE_GATT_ACCESS_OP_WRITE_CHR: {
        uint8_t buf[R03_MAX_MSG_LEN];
        uint16_t len = 0;
        if (OS_MBUF_PKTLEN(ctxt->om) > sizeof(buf)) {
            return BLE_ATT_ERR_INVALID_ATTR_VALUE_LEN;
        }
        if (ble_hs_mbuf_to_flat(ctxt->om, buf, sizeof(buf), &len) != 0) {
            return BLE_ATT_ERR_UNLIKELY;
        }
        if (attr_handle == s.h_data_in) {
            emit(BLE_LINK_EVT_DATA_WRITE, buf, len);
        } else if (attr_handle == s.h_ctrl) {
            emit(BLE_LINK_EVT_CTRL_WRITE, buf, len);
        } else {
            return BLE_ATT_ERR_UNLIKELY;
        }
        return 0;
    }

    default:
        return BLE_ATT_ERR_UNLIKELY;
    }
}

static const struct ble_gatt_svc_def gatt_svcs[] = {
    {
        .type = BLE_GATT_SVC_TYPE_PRIMARY,
        .uuid = &uuid_svc.u,
        .characteristics = (struct ble_gatt_chr_def[]) {
            {
                .uuid = &uuid_data_out.u,
                .access_cb = gatt_access,
                .flags = BLE_GATT_CHR_F_NOTIFY,
                .val_handle = &s.h_data_out,
            },
            {
                .uuid = &uuid_codec.u,
                .access_cb = gatt_access,
                .flags = BLE_GATT_CHR_F_READ | BLE_GATT_CHR_F_READ_ENC,
                .val_handle = &s.h_codec,
            },
            {
                .uuid = &uuid_data_in.u,
                .access_cb = gatt_access,
                .flags = BLE_GATT_CHR_F_WRITE | BLE_GATT_CHR_F_WRITE_NO_RSP | BLE_GATT_CHR_F_WRITE_ENC,
                .val_handle = &s.h_data_in,
            },
            {
                .uuid = &uuid_ctrl.u,
                .access_cb = gatt_access,
                .flags = BLE_GATT_CHR_F_WRITE | BLE_GATT_CHR_F_WRITE_NO_RSP | BLE_GATT_CHR_F_WRITE_ENC |
                         BLE_GATT_CHR_F_NOTIFY,
                .val_handle = &s.h_ctrl,
            },
            { 0 },
        },
    },
    {
        .type = BLE_GATT_SVC_TYPE_PRIMARY,
        .uuid = &uuid_bas.u,
        .characteristics = (struct ble_gatt_chr_def[]) {
            {
                .uuid = &uuid_bat_level.u,
                .access_cb = gatt_access,
                .flags = BLE_GATT_CHR_F_READ | BLE_GATT_CHR_F_NOTIFY,
                .val_handle = &s.h_bat,
            },
            { 0 },
        },
    },
    { 0 },
};

/* ------------------------------------------------------------------ GAP */

static int gap_event(struct ble_gap_event *event, void *arg);

static void advertise(void)
{
    struct ble_hs_adv_fields fields;
    memset(&fields, 0, sizeof(fields));
    fields.flags = BLE_HS_ADV_F_DISC_GEN | BLE_HS_ADV_F_BREDR_UNSUP;
    fields.uuids128 = (ble_uuid128_t *)&uuid_svc;
    fields.num_uuids128 = 1;
    fields.uuids128_is_complete = 1;
    int rc = ble_gap_adv_set_fields(&fields);
    if (rc != 0) {
        ESP_LOGE(TAG, "adv_set_fields rc=%d", rc);
        return;
    }

    /* The 128-bit UUID fills the advertising PDU; the name goes in the scan response. */
    struct ble_hs_adv_fields rsp;
    memset(&rsp, 0, sizeof(rsp));
    rsp.name = (uint8_t *)s.name;
    rsp.name_len = (uint8_t)strlen(s.name);
    rsp.name_is_complete = 1;
    rc = ble_gap_adv_rsp_set_fields(&rsp);
    if (rc != 0) {
        ESP_LOGE(TAG, "adv_rsp_set_fields rc=%d", rc);
        return;
    }

    struct ble_gap_adv_params params;
    memset(&params, 0, sizeof(params));
    params.conn_mode = BLE_GAP_CONN_MODE_UND;
    params.disc_mode = BLE_GAP_DISC_MODE_GEN;
    params.itvl_min = BLE_GAP_ADV_FAST_INTERVAL2_MIN; /* 100 ms */
    params.itvl_max = BLE_GAP_ADV_FAST_INTERVAL2_MAX; /* 150 ms */
    rc = ble_gap_adv_start(s.own_addr_type, NULL, BLE_HS_FOREVER, &params, gap_event, NULL);
    if (rc != 0 && rc != BLE_HS_EALREADY) {
        ESP_LOGE(TAG, "adv_start rc=%d", rc);
        return;
    }
    ESP_LOGI(TAG, "advertising as \"%s\"", s.name);
}

static void request_conn_params(void)
{
    /* Units: interval 1.25 ms, supervision timeout 10 ms. */
    struct ble_gap_upd_params p = {
        .itvl_min = (CONFIG_CAIPO_BLE_CONN_ITVL_MIN_MS * 4) / 5,
        .itvl_max = (CONFIG_CAIPO_BLE_CONN_ITVL_MAX_MS * 4) / 5,
        .latency = 0,
        .supervision_timeout = CONFIG_CAIPO_BLE_SUPERVISION_TIMEOUT_MS / 10,
        .min_ce_len = 0,
        .max_ce_len = 0,
    };
    int rc = ble_gap_update_params(s.conn, &p);
    if (rc != 0) {
        ESP_LOGW(TAG, "conn param update request rc=%d", rc);
    }
}

static void reset_link_state(void)
{
    s.conn = BLE_HS_CONN_HANDLE_NONE;
    s.connected = false;
    s.encrypted = false;
    s.sub_data = s.sub_ctrl = s.sub_bat = false;
    s.mtu = BLE_ATT_MTU_DFLT;
}

static int gap_event(struct ble_gap_event *event, void *arg)
{
    (void)arg;
    struct ble_gap_conn_desc desc;

    switch (event->type) {
    case BLE_GAP_EVENT_CONNECT:
        if (event->connect.status != 0) {
            ESP_LOGW(TAG, "connect failed status=%d", event->connect.status);
            advertise();
            return 0;
        }
        reset_link_state();
        s.conn = event->connect.conn_handle;
        s.connected = true;
        ESP_LOGI(TAG, "connected handle=%u", s.conn);
        request_conn_params();
        /* Ask for encryption now: a bonded phone re-encrypts silently, a new
         * one gets the system pairing dialog. */
        {
            int rc = ble_gap_security_initiate(s.conn);
            if (rc != 0) {
                ESP_LOGW(TAG, "security_initiate rc=%d", rc);
            }
        }
        emit(BLE_LINK_EVT_CONNECTED, NULL, 0);
        return 0;

    case BLE_GAP_EVENT_DISCONNECT: {
        uint8_t reason = (uint8_t)event->disconnect.reason;
        ESP_LOGI(TAG, "disconnected reason=0x%02x", reason);
        reset_link_state();
        emit(BLE_LINK_EVT_DISCONNECTED, &reason, 1);
        advertise();
        return 0;
    }

    case BLE_GAP_EVENT_ENC_CHANGE:
        if (event->enc_change.status != 0) {
            ESP_LOGW(TAG, "encryption failed status=%d", event->enc_change.status);
            return 0;
        }
        if (ble_gap_conn_find(event->enc_change.conn_handle, &desc) == 0) {
            s.encrypted = desc.sec_state.encrypted;
            ESP_LOGI(TAG, "link encrypted=%d authenticated=%d bonded=%d", desc.sec_state.encrypted,
                     desc.sec_state.authenticated, desc.sec_state.bonded);
            if (s.encrypted) {
                emit(BLE_LINK_EVT_ENCRYPTED, NULL, 0);
            }
        }
        return 0;

    case BLE_GAP_EVENT_CONN_UPDATE:
        if (ble_gap_conn_find(event->conn_update.conn_handle, &desc) == 0) {
            ESP_LOGI(TAG, "conn params: interval %u.%02u ms, latency %u, supervision timeout %u ms",
                     (desc.conn_itvl * 5) / 4, ((desc.conn_itvl * 5) % 4) * 25, desc.conn_latency,
                     desc.supervision_timeout * 10);
        }
        return 0;

    case BLE_GAP_EVENT_MTU: {
        s.mtu = event->mtu.value;
        uint8_t le[2] = { (uint8_t)s.mtu, (uint8_t)(s.mtu >> 8) };
        ESP_LOGI(TAG, "MTU %u, chunk payload %u", s.mtu, ble_link_chunk_payload());
        emit(BLE_LINK_EVT_MTU, le, 2);
        return 0;
    }

    case BLE_GAP_EVENT_SUBSCRIBE:
        if (event->subscribe.attr_handle == s.h_data_out) {
            s.sub_data = event->subscribe.cur_notify;
        } else if (event->subscribe.attr_handle == s.h_ctrl) {
            s.sub_ctrl = event->subscribe.cur_notify;
        } else if (event->subscribe.attr_handle == s.h_bat) {
            s.sub_bat = event->subscribe.cur_notify;
        }
        ESP_LOGI(TAG, "subscriptions: data=%d ctrl=%d battery=%d", s.sub_data, s.sub_ctrl, s.sub_bat);
        return 0;

    case BLE_GAP_EVENT_REPEAT_PAIRING:
        /* The phone forgot us but we still hold its bond: drop ours and let it pair again. */
        if (ble_gap_conn_find(event->repeat_pairing.conn_handle, &desc) == 0) {
            ble_store_util_delete_peer(&desc.peer_id_addr);
        }
        return BLE_GAP_REPEAT_PAIRING_RETRY;

    case BLE_GAP_EVENT_PASSKEY_ACTION:
        /* No I/O capability: Just Works, nothing to do. */
        return 0;

    case BLE_GAP_EVENT_ADV_COMPLETE:
        advertise();
        return 0;

    default:
        return 0;
    }
}

/* ----------------------------------------------------------------- host */

static void on_reset(int reason)
{
    ESP_LOGE(TAG, "host reset, reason=%d", reason);
}

static void on_sync(void)
{
    int rc = ble_hs_util_ensure_addr(0);
    if (rc != 0) {
        ESP_LOGE(TAG, "ensure_addr rc=%d", rc);
        return;
    }
    rc = ble_hs_id_infer_auto(0, &s.own_addr_type);
    if (rc != 0) {
        ESP_LOGE(TAG, "infer_auto rc=%d", rc);
        return;
    }
    uint8_t addr[6] = { 0 };
    ble_hs_id_copy_addr(s.own_addr_type, addr, NULL);
    snprintf(s.name, sizeof(s.name), "%s%02X%02X", CONFIG_CAIPO_BLE_NAME_PREFIX, addr[1], addr[0]);
    ble_svc_gap_device_name_set(s.name);
    ESP_LOGI(TAG, "address %02x:%02x:%02x:%02x:%02x:%02x", addr[5], addr[4], addr[3], addr[2], addr[1], addr[0]);
    advertise();
}

static void host_task(void *param)
{
    (void)param;
    nimble_port_run(); /* returns only after nimble_port_stop() */
    nimble_port_freertos_deinit();
}

static esp_err_t notify(uint16_t handle, bool subscribed, const uint8_t *data, size_t len)
{
    if (!s.connected || !s.encrypted || !subscribed) {
        return ESP_ERR_INVALID_STATE;
    }
    if (len > (size_t)(s.mtu - 3)) {
        return ESP_ERR_INVALID_SIZE;
    }
    struct os_mbuf *om = ble_hs_mbuf_from_flat(data, (uint16_t)len);
    if (om == NULL) {
        return ESP_ERR_NO_MEM;
    }
    int rc = ble_gatts_notify_custom(s.conn, handle, om); /* consumes om */
    if (rc == BLE_HS_ENOMEM) {
        return ESP_ERR_NO_MEM;
    }
    return rc == 0 ? ESP_OK : ESP_FAIL;
}

static void heartbeat_task(void *param)
{
    (void)param;
    for (;;) {
        vTaskDelay(pdMS_TO_TICKS(CONFIG_CAIPO_BLE_HEARTBEAT_MS));
        if (s.connected && s.encrypted && s.sub_ctrl) {
            uint8_t msg[2] = { CAIPO_CTRL_HEARTBEAT, s.hb_counter++ };
            esp_err_t err = notify(s.h_ctrl, s.sub_ctrl, msg, sizeof(msg));
            if (err != ESP_OK) {
                ESP_LOGW(TAG, "heartbeat notify failed: %s", esp_err_to_name(err));
            }
        }
    }
}

/* ----------------------------------------------------------- public API */

esp_err_t ble_link_init(ble_link_cb_t cb, void *ctx)
{
#ifndef CONFIG_CAIPO_BLE_ENABLED
    (void)cb; (void)ctx;
    return ESP_ERR_NOT_SUPPORTED; /* CONFIG_CAIPO_BLE_ENABLED=n (e.g. Wokwi) */
#endif
    memset(&s, 0, sizeof(s));
    s.cb = cb;
    s.ctx = ctx;
    s.battery = 100;
    reset_link_state();
    snprintf(s.name, sizeof(s.name), "%s", CONFIG_CAIPO_BLE_NAME_PREFIX);

    esp_err_t err = nimble_port_init();
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "nimble_port_init: %s", esp_err_to_name(err));
        return err;
    }

    ble_hs_cfg.reset_cb = on_reset;
    ble_hs_cfg.sync_cb = on_sync;
    ble_hs_cfg.store_status_cb = ble_store_util_status_rr;

    /* LE Secure Connections, Just Works, bonding. The device has no display
     * or keypad, so this is the strongest pairing it can offer. */
    ble_hs_cfg.sm_io_cap = BLE_SM_IO_CAP_NO_IO;
    ble_hs_cfg.sm_bonding = 1;
    ble_hs_cfg.sm_mitm = 0;
    ble_hs_cfg.sm_sc = 1;
    ble_hs_cfg.sm_our_key_dist = BLE_SM_PAIR_KEY_DIST_ENC | BLE_SM_PAIR_KEY_DIST_ID;
    ble_hs_cfg.sm_their_key_dist = BLE_SM_PAIR_KEY_DIST_ENC | BLE_SM_PAIR_KEY_DIST_ID;

    ble_svc_gap_init();
    ble_svc_gatt_init();

    int rc = ble_gatts_count_cfg(gatt_svcs);
    if (rc != 0) {
        ESP_LOGE(TAG, "gatts_count_cfg rc=%d", rc);
        return ESP_FAIL;
    }
    rc = ble_gatts_add_svcs(gatt_svcs);
    if (rc != 0) {
        ESP_LOGE(TAG, "gatts_add_svcs rc=%d", rc);
        return ESP_FAIL;
    }

    rc = ble_att_set_preferred_mtu(512);
    if (rc != 0) {
        ESP_LOGW(TAG, "set_preferred_mtu rc=%d", rc);
    }

    ble_store_config_init();
    nimble_port_freertos_init(host_task);

    if (xTaskCreate(heartbeat_task, "ble_hb", 3072, NULL, 5, NULL) != pdPASS) {
        return ESP_ERR_NO_MEM;
    }
    s.ready = true;
    return ESP_OK;
}

esp_err_t ble_link_notify_data(const uint8_t *data, size_t len)
{
    return notify(s.h_data_out, s.sub_data, data, len);
}

esp_err_t ble_link_notify_ctrl(const uint8_t *data, size_t len)
{
    return notify(s.h_ctrl, s.sub_ctrl, data, len);
}

void ble_link_set_battery(uint8_t percent)
{
    s.battery = percent > 100 ? 100 : percent;
    if (s.connected && s.sub_bat) {
        (void)notify(s.h_bat, s.sub_bat, &s.battery, 1);
    }
}

bool ble_link_is_connected(void) { return s.connected; }
bool ble_link_is_encrypted(void) { return s.connected && s.encrypted; }
uint16_t ble_link_mtu(void) { return s.mtu; }
uint16_t ble_link_chunk_payload(void) { return r03_chunk_payload_for_mtu(s.mtu); }
const char *ble_link_name(void) { return s.name; }

esp_err_t ble_link_forget_bonds(void)
{
    if (!s.ready) {
        return ESP_ERR_INVALID_STATE;
    }
    int rc = ble_store_clear();
    ESP_LOGI(TAG, "bonds cleared rc=%d", rc);
    if (s.connected) {
        ble_gap_terminate(s.conn, BLE_ERR_REM_USER_CONN_TERM);
    }
    return rc == 0 ? ESP_OK : ESP_FAIL;
}
