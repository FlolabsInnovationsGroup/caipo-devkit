#pragma once
/*
 * Application-level opcodes carried on the control characteristic
 * (A1000004, both directions). R03 transfer messages live in r03.h and
 * travel on A1000001 (device -> app) and A1000003 (app -> device).
 *
 * Heartbeat is part of the agreed contract. 0x20..0x2F are the PROPOSED
 * device commands from the app team's task page (pending confirmation);
 * the firmware answers CMD_RESULT UNSUPPORTED for anything not implemented.
 */

/* device -> app, every CONFIG_CAIPO_BLE_HEARTBEAT_MS: [0x10][counter u8] */
#define CAIPO_CTRL_HEARTBEAT      0x10

/* PROPOSED commands, app -> device */
#define CAIPO_CMD_GET_STATUS      0x20 /* no payload */
#define CAIPO_CMD_CAPTURE_IMAGE   0x22 /* no payload */
#define CAIPO_CMD_RECORD_VIDEO    0x23 /* [duration s u8] */
#define CAIPO_CMD_RECORD_AUDIO    0x24 /* [duration s u8] */
#define CAIPO_CMD_SYNC_NOW        0x25 /* no payload */
#define CAIPO_CMD_SET_TIME        0x26 /* [unix time u32 LE] */
#define CAIPO_CMD_START_STREAM    0x27 /* no payload */
#define CAIPO_CMD_STOP_STREAM     0x29 /* no payload */

/* PROPOSED, device -> app */
#define CAIPO_EVT_STATUS          0x21 /* [state u8][battery % u8][flags u8][pending uploads u16] */
#define CAIPO_EVT_STREAM_READY    0x28 /* [IPv4 4][port u16][token 16] */
#define CAIPO_EVT_CMD_RESULT      0x2F /* [command opcode][result u8] */

/* STATUS.state (mirrors the supervisor state machine) */
#define CAIPO_STATE_STANDBY          0
#define CAIPO_STATE_WAKE             1
#define CAIPO_STATE_POWERING_UP      2
#define CAIPO_STATE_PI_BOOTING       3
#define CAIPO_STATE_ACTIVE           4
#define CAIPO_STATE_SHUTDOWN         5
#define CAIPO_STATE_PI_SHUTTING_DOWN 6
#define CAIPO_STATE_POWER_GATING     7

/* STATUS.flags */
#define CAIPO_FLAG_REC_AUDIO      (1u << 0)
#define CAIPO_FLAG_REC_VIDEO      (1u << 1)
#define CAIPO_FLAG_CHARGING       (1u << 2)
#define CAIPO_FLAG_PI_ON          (1u << 3)
#define CAIPO_FLAG_WIFI_CONNECTED (1u << 4)
#define CAIPO_FLAG_QUEUED_FOR_APP (1u << 5) /* our addition: recordings waiting for the phone */
#define CAIPO_FLAG_STORAGE_FULL   (1u << 6) /* our addition */

/* CMD_RESULT.result */
#define CAIPO_RESULT_OK          0 /* accepted (not necessarily finished) */
#define CAIPO_RESULT_BUSY        1
#define CAIPO_RESULT_UNSUPPORTED 2
#define CAIPO_RESULT_FAILED      3

/* Audio codec ID reported on A1000002 */
#define CAIPO_CODEC_OPUS_16K_20MS 0x15
