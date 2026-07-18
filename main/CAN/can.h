#ifndef SFRCAN
#include "main.h"

#include "esp_twai.h"
#include "esp_twai_onchip.h"

#include "pin.h"
#include "string.h"
#include "espnow.h"
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "string.h"
#include "esp_twai.h"
#include "esp_twai_onchip.h"

#define CAN_CMD_ID 0x010
typedef enum {
    eCMD_RESET          = 0b00000001,
    eCMD_CLEAR_MINMAX   = 0b00000010,
    eCMD_CLEAR_ERRORS   = 0b00000100,
    eCMD_REFLASH_MODE   = 0b00001000,
    eCMD_NORMAL_MODE    = 0b00010000,
} eCAN_CMD_t;

esp_err_t CAN_init(boolean bEnableRx);
esp_err_t CAN_transmit(twai_node_handle_t stCANBus, const CAN_frame_t *stFrame);
bool CAN_receive_callback(twai_node_handle_t stCANBus, const twai_rx_done_event_data_t *edata, void *stRxCallback);
esp_err_t CAN_receive_debug();
void CAN_bus_diagnosics();
const char* CAN_error_state_to_string(twai_error_state_t stState);
esp_err_t CAN_empty_ESPNOW_buffer(twai_node_handle_t stCANBus);
bool CAN_receive_callback_no_queue(twai_node_handle_t stCANBus, const twai_rx_done_event_data_t *edata, void *stRxCallback);
void CAN_CMD_response(twai_frame_t stRxFrame);
void CAN_clear_rx_buffer(void);

#define KILL_MSG_ID 0x001

extern QueueHandle_t xCANRingBuffer;

#ifdef GPIO_CAN0_TX
extern twai_node_handle_t stCANBus0;
#endif
#ifdef GPIO_CAN1_TX
extern twai_node_handle_t stCANBus1;
#endif

/*  Kill/NKill Msg Format 
    ID: 0x001
    DLC: 2
    Data: [State, Source]
        State: Kill level, KillLevel_t
        Source: Kill source, KillSource_t
        Checksum: (State + Source) % 256
*/

typedef enum {
    KILL_NONE = 0,
    KILL_INVERTER,
    KILL_HV,
    KILL_ALL
} KillLevel_t;

typedef enum {
    KILL_SOURCE_NONE = 0,
    KILL_SOURCE_HVIL,
    KILL_SOURCE_BMS,
    KILL_SOURCE_IMD,
    KILL_SOURCE_CELL_OVERTEMP,
    KILL_SOURCE_CELL_OVERVOLT,
    KILL_SOURCE_CELL_UNDERVOLT,
    KILL_SOURCE_LV_UNDERVOLT,
    KILL_SOURCE_APPS_DRIFT,
    KILL_SOURCE_APPS_1,
    KILL_SOURCE_APPS_2,
} KillSource_t;

static const char *KILL_DISCRIPTION[] = {
    "None",
    "HVIL is broken!",
    "BMS Unhappy!",
    "IMD Triggered!",
    "Cell Over Temperature!",
    "Cell Over Voltage!",
    "Cell Under Voltage!",
    "LV Under Voltage!",
    "APPS drift over limit!",
    "APPS 1 failure!",
    "APPS 2 failure!",
};

#endif //SFRCAN