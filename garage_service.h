
#ifndef GARAGE_SERVICE_H__
#define GARAGE_SERVICE_H__

#include <stdint.h>
#include "ble.h"
#include "ble_srv_common.h"

#define BLE_UUID_GARAGE_BASE_UUID			{0x22, 0x0D, 0x8B, 0xCF, 0xE7, 0x0F, 0x4D, 0xAA, 0xA4, 0x8F, 0x5E, 0xD1, 0xA4, 0x64, 0xF0, 0xC2} // 128-bit base UUID
#define BLE_UUID_GARAGE_SERVICE				0xA000 			// Just a random, but recognizable value
#define BLE_UUID_PORT_CHAR					0xA001			//For opening
#define BLE_UUID_PORT_CONFIRM_CHAR			0xA002			//For reading confirmation

typedef struct
{
    uint16_t    				service_handle;     			/**< Handle of Our Service (as provided by the BLE stack). */
	ble_gatts_char_handles_t    port_char_handles;
    uint8_t                     uuid_type;
    uint16_t                    conn_handle;
} ble_garage_t;

/**@brief Function for initializing our new service.
 *
 * @param[in]   p_garage_service       Pointer to our Service structure.
 */
void ble_garage_service_init(ble_garage_t * p_garage_service);

void ble_garage_on_ble_evt(ble_garage_t * p_garage_service, ble_evt_t * p_ble_evt);

#endif  /* _ GARAGE_SERVICE_H__ */
