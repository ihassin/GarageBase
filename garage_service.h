/* Copyright (c) Nordic Semiconductor ASA
 * All rights reserved.
 * 
 * Redistribution and use in source and binary forms, with or without modification,
 * are permitted provided that the following conditions are met:
 * 
 *   1. Redistributions of source code must retain the above copyright notice, this
 *   list of conditions and the following disclaimer.
 * 
 *   2. Redistributions in binary form must reproduce the above copyright notice, this
 *   list of conditions and the following disclaimer in the documentation and/or
 *   other materials provided with the distribution.
 * 
 *   3. Neither the name of Nordic Semiconductor ASA nor the names of other
 *   contributors to this software may be used to endorse or promote products
 *   derived from this software without specific prior written permission.
 * 
 *   4. This software must only be used in a processor manufactured by Nordic
 *   Semiconductor ASA, or in a processor manufactured by a third party that
 *   is used in combination with a processor manufactured by Nordic Semiconductor.
 * 
 * 
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE FOR
 * ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON
 * ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
 * SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 * 
 */

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
