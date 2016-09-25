

#include <stdint.h>
#include <string.h>
#include "garage_service.h"
#include "ble_srv_common.h"
#include "app_error.h"
#include "app_timer.h"
#include "nrf_log.h"
#include "softdevice_handler.h"
#include <sha256.h>
#include "nrf_gpio.h"
#include "com_def.h"

#define	NONCE_LENGTH	20
#define PASS_LENGTH		6
#define PASSWORD		"A21B15"
#define PASSWORD_DIGEST_LENGTH	32
#define QUEUED_WRITE_LENGTH	PASSWORD_DIGEST_LENGTH+14 	//12 for headers and two extra, don't know why		

APP_TIMER_DEF(m_toggle_time_id);													/* Defined timer for handling the toggle time to drive the port remote */

static bool 	nonce_valid = false;
static uint8_t	password_match = 0;		
static uint8_t	p_nonce_buff[NONCE_LENGTH+PASS_LENGTH];
static uint8_t	queued_write[QUEUED_WRITE_LENGTH];
static uint8_t 	received_hashed_password[PASSWORD_DIGEST_LENGTH];

static void on_connect(ble_garage_t * p_garage_service, ble_evt_t * p_ble_evt) {
	
	//SEGGER_RTT_WriteString(0, "does it ever get stored!!!!!!!!.\r\n");
	//NRF_LOG_HEX(p_ble_evt->evt.gap_evt.conn_handle);
	//SEGGER_RTT_WriteString(0, "does it ever get stored!!!!!!!!.\r\n");
    p_garage_service->conn_handle = p_ble_evt->evt.gap_evt.conn_handle;
}

static uint32_t port_char_add(ble_garage_t * p_garage_service) {
    ble_gatts_char_md_t char_md;
    ble_gatts_attr_t    attr_char_value;
    ble_uuid_t          ble_uuid;
    ble_gatts_attr_md_t attr_md;

    memset(&char_md, 0, sizeof(char_md));
    
    char_md.char_props.read   = 1;
    char_md.char_props.write  = 1;
    char_md.p_char_user_desc  = NULL;
    char_md.p_char_pf         = NULL;
    char_md.p_user_desc_md    = NULL;
    char_md.p_cccd_md         = NULL;
    char_md.p_sccd_md         = NULL;
    
    ble_uuid.type = p_garage_service->uuid_type;
    ble_uuid.uuid = BLE_UUID_PORT_CHAR;
    
    memset(&attr_md, 0, sizeof(attr_md));

    BLE_GAP_CONN_SEC_MODE_SET_ENC_WITH_MITM(&attr_md.read_perm);
    BLE_GAP_CONN_SEC_MODE_SET_ENC_WITH_MITM(&attr_md.write_perm);
    attr_md.vloc       = BLE_GATTS_VLOC_STACK;
    attr_md.rd_auth    = 1;
    attr_md.wr_auth    = 1;
    attr_md.vlen       = 1;
    memset(&attr_char_value, 0, sizeof(attr_char_value));

    attr_char_value.p_uuid       = &ble_uuid;
    attr_char_value.p_attr_md    = &attr_md;
    attr_char_value.init_len     = 32*sizeof(uint8_t);
    attr_char_value.init_offs    = 0;
    attr_char_value.max_len      = 32*sizeof(uint8_t);
    
    return sd_ble_gatts_characteristic_add(p_garage_service->service_handle, &char_md,
                                           &attr_char_value,
                                           &p_garage_service->port_char_handles);
    NRF_LOG_DEBUG("addd character\r\n");
}

static uint32_t port_confirm_char_add(ble_garage_t * p_garage_service) {
	
    ble_gatts_char_md_t char_md;
    ble_gatts_attr_t    attr_char_value;
    ble_uuid_t          ble_uuid;
    ble_gatts_attr_md_t attr_md;

    memset(&char_md, 0, sizeof(char_md));
    
    char_md.char_props.read   = 1;
    char_md.char_props.write  = 0;
    char_md.p_char_user_desc  = NULL;
    char_md.p_char_pf         = NULL;
    char_md.p_user_desc_md    = NULL;
    char_md.p_cccd_md         = NULL;
    char_md.p_sccd_md         = NULL;
    
    ble_uuid.type = p_garage_service->uuid_type;
    ble_uuid.uuid = BLE_UUID_PORT_CONFIRM_CHAR;
    
    memset(&attr_md, 0, sizeof(attr_md));

    BLE_GAP_CONN_SEC_MODE_SET_ENC_WITH_MITM(&attr_md.read_perm);
    //BLE_GAP_CONN_SEC_MODE_SET_ENC_WITH_MITM(&attr_md.write_perm);
    attr_md.vloc       = BLE_GATTS_VLOC_STACK;
    attr_md.rd_auth    = 1;
    attr_md.wr_auth    = 0;
    attr_md.vlen       = 0;
    memset(&attr_char_value, 0, sizeof(attr_char_value));

    attr_char_value.p_uuid       = &ble_uuid;
    attr_char_value.p_attr_md    = &attr_md;
    attr_char_value.init_len     = sizeof(uint8_t);						//32*sizeof(uint8_t);
    attr_char_value.init_offs    = 0;
    attr_char_value.max_len      = sizeof(uint8_t);						//32*sizeof(uint8_t);
    attr_char_value.p_value      = NULL;
    
    return sd_ble_gatts_characteristic_add(p_garage_service->service_handle, &char_md,
                                           &attr_char_value,
                                           &p_garage_service->port_char_handles);
}

static void toggle_expiry_handler(void * p_context)
{
    	nrf_gpio_pin_clear(0);																			//
}


static void port_open_handler(ble_garage_t * p_garage_service, ble_evt_t * p_ble_evt) {
	
	uint32_t   								err_code;
	ble_gatts_rw_authorize_reply_params_t	auth_reply;
	sha256_context_t						ctx;
	uint8_t 								hashed_password[PASSWORD_DIGEST_LENGTH];
	
	memset(&auth_reply, 0 ,sizeof(ble_gatts_rw_authorize_reply_params_t));
	
	auth_reply.type = BLE_GATTS_AUTHORIZE_TYPE_WRITE;
	auth_reply.params.write.gatt_status = BLE_GATT_STATUS_SUCCESS;
	
	NRF_LOG_HEX(p_ble_evt->evt.gatts_evt.params.authorize_request.request.write.op);
	NRF_LOG_HEX(p_ble_evt->evt.gatts_evt.params.authorize_request.request.write.len);
	NRF_LOG_HEX(p_ble_evt->evt.gatts_evt.params.authorize_request.request.write.offset);
	
	//The hashed password is not received in one go
	if (p_ble_evt->evt.gatts_evt.params.authorize_request.request.write.op == BLE_GATTS_OP_PREP_WRITE_REQ) {
		for (uint8_t i = 0; i < p_ble_evt->evt.gatts_evt.params.authorize_request.request.write.len; i++) {
			received_hashed_password[i+p_ble_evt->evt.gatts_evt.params.authorize_request.request.write.offset] =
				p_ble_evt->evt.gatts_evt.params.authorize_request.request.write.data[i];	
		}
	} else if (p_ble_evt->evt.gatts_evt.params.authorize_request.request.write.op == BLE_GATTS_OP_EXEC_WRITE_REQ_NOW) {
		//Compare received hashed password with calculated, also check that it is non used challange
		memset(&ctx, 0 ,sizeof(sha256_context_t));
		err_code = sha256_init(&ctx);
		APP_ERROR_CHECK(err_code);
		err_code = sha256_update(&ctx,p_nonce_buff,NONCE_LENGTH+PASS_LENGTH);
		APP_ERROR_CHECK(err_code);
		err_code = sha256_final(&ctx, hashed_password);
		APP_ERROR_CHECK(err_code);
		bool match_tmp = true;
		for (uint8_t i = 0; i < PASSWORD_DIGEST_LENGTH; i++) {
			if (hashed_password[i] != received_hashed_password[i]) {
				match_tmp = false;	
			}
		}
		if (match_tmp) {
			NRF_LOG_DEBUG("Password match, success!\r\n");
			if (nonce_valid) {
				//Execute opening
				nrf_gpio_pin_set(0);
				err_code = app_timer_create(&m_toggle_time_id, APP_TIMER_MODE_SINGLE_SHOT, toggle_expiry_handler);	
				APP_ERROR_CHECK(err_code);
				err_code = app_timer_start(m_toggle_time_id,APP_TIMER_TICKS(1000, APP_TIMER_PRESCALER), NULL);
				APP_ERROR_CHECK(err_code);
				password_match = 1;
				nonce_valid = false;
				
			} else {
				NRF_LOG_DEBUG("Non valid nonce used!\r\n");	
			}
		} else {
			NRF_LOG_DEBUG("Password match, fail!\r\n");
			nonce_valid = false;
		}
	}
	
	err_code = sd_ble_gatts_rw_authorize_reply(p_garage_service->conn_handle,&auth_reply);
	APP_ERROR_CHECK(err_code);	
}

static void request_challenge(ble_garage_t * p_garage_service, ble_evt_t * p_ble_evt) {

	uint32_t   								err_code;
	uint8_t									bytes_available;
	ble_gatts_rw_authorize_reply_params_t	 auth_reply;
	
	memset(&auth_reply, 0 ,sizeof(ble_gatts_rw_authorize_reply_params_t));
	
	auth_reply.type = BLE_GATTS_AUTHORIZE_TYPE_READ;

	err_code = sd_rand_application_bytes_available_get(&bytes_available);
	APP_ERROR_CHECK(err_code);	

	if (bytes_available < NONCE_LENGTH) {
		auth_reply.params.read.gatt_status = BLE_GATT_STATUS_ATTERR_UNLIKELY_ERROR;
		NRF_LOG_DEBUG("Random numbers, fail\r\n");
	} else {
		err_code = sd_rand_application_vector_get(p_nonce_buff, NONCE_LENGTH);
		APP_ERROR_CHECK(err_code);
		auth_reply.params.read.gatt_status = BLE_GATT_STATUS_SUCCESS;
		NRF_LOG_DEBUG("Random numbers, success\r\n");
		auth_reply.params.read.update = 1;
		auth_reply.params.read.len = NONCE_LENGTH;
		auth_reply.params.read.p_data = p_nonce_buff;
	}

	err_code = sd_ble_gatts_rw_authorize_reply(p_garage_service->conn_handle,&auth_reply);
	APP_ERROR_CHECK(err_code);
	
	nonce_valid = true;
	//nrf_gpio_pin_toggle(0);
}

static void memory_request(ble_garage_t *p_garage_service, ble_evt_t *p_ble_evt) {	
	
	uint32_t   				err_code;
	ble_user_mem_block_t	mem_reply;
	
	if (p_ble_evt->evt.common_evt.params.user_mem_request.type == BLE_USER_MEM_TYPE_GATTS_QUEUED_WRITES) {
		memset(&mem_reply, 0 ,sizeof(ble_user_mem_block_t));
		memset(queued_write, 0, QUEUED_WRITE_LENGTH);
		mem_reply.len = QUEUED_WRITE_LENGTH;
		mem_reply.p_mem = queued_write;
		NRF_LOG_DEBUG("Responding to memory request, for queued write\r\n");
		err_code = sd_ble_user_mem_reply(p_garage_service->conn_handle, &mem_reply);
		APP_ERROR_CHECK(err_code);
	} else {
		NRF_LOG_DEBUG("Invalid memory reuest\r\n");
	}
}

static void memory_release(ble_garage_t *p_garage_service, ble_evt_t *p_ble_evt) {

	NRF_LOG_DEBUG("Memory released, for queued write, static so nothing to do\r\n");
}



static void check_password(ble_garage_t * p_garage_service, ble_evt_t * p_ble_evt) {
	
	uint32_t									err_code;
	ble_gatts_rw_authorize_reply_params_t		 auth_reply;
	
	memset(&auth_reply, 0 ,sizeof(ble_gatts_rw_authorize_reply_params_t));
	
	auth_reply.type = BLE_GATTS_AUTHORIZE_TYPE_READ;

	auth_reply.params.read.gatt_status = BLE_GATT_STATUS_SUCCESS;
	auth_reply.params.read.update = 1;
	auth_reply.params.read.len = 1; 
	auth_reply.params.read.p_data = &password_match;

	err_code = sd_ble_gatts_rw_authorize_reply(p_garage_service->conn_handle,&auth_reply);
	APP_ERROR_CHECK(err_code);
	NRF_LOG_DEBUG("password confirm sent\r\n");
	password_match = 0; 
}



void ble_garage_on_ble_evt(ble_garage_t * p_garage_service, ble_evt_t * p_ble_evt) {

    switch (p_ble_evt->header.evt_id) {
        case BLE_GAP_EVT_CONNECTED:
            on_connect(p_garage_service, p_ble_evt);
            break;
            
        case BLE_GAP_EVT_DISCONNECTED:
			break;
            
        case BLE_GATTS_EVT_WRITE:
			NRF_LOG_DEBUG("Write should not really happen!\r\n"); 
            break;
			
		case BLE_EVT_USER_MEM_REQUEST:
			memory_request(p_garage_service, p_ble_evt);
			break;
	
		case BLE_EVT_USER_MEM_RELEASE:
			memory_release(p_garage_service, p_ble_evt);
			break;
			
		case BLE_GATTS_EVT_RW_AUTHORIZE_REQUEST:
		
			if (p_ble_evt->evt.gatts_evt.params.authorize_request.type == BLE_GATTS_AUTHORIZE_TYPE_WRITE) {
				//The only write case is sending back password
				NRF_LOG_DEBUG("Password write recieved: ");
				NRF_LOG_HEX(p_ble_evt->evt.gatts_evt.params.authorize_request.request.write.context.char_uuid.uuid);
				NRF_LOG_DEBUG("\r\n");
            	port_open_handler(p_garage_service, p_ble_evt);
			} else if (p_ble_evt->evt.gatts_evt.params.authorize_request.type == BLE_GATTS_AUTHORIZE_TYPE_READ) {
				NRF_LOG_HEX(p_ble_evt->evt.gatts_evt.params.authorize_request.request.read.context.char_uuid.uuid);
				if (p_ble_evt->evt.gatts_evt.params.authorize_request.request.read.context.char_uuid.uuid == BLE_UUID_PORT_CHAR) {
					NRF_LOG_DEBUG("Password challange received: ");
					NRF_LOG_HEX(p_ble_evt->evt.gatts_evt.params.authorize_request.request.read.context.char_uuid.uuid);
					NRF_LOG_DEBUG("\r\n");
					request_challenge(p_garage_service, p_ble_evt);
				} else {													//Port confirm char BLE_UUID_PORT_CONFIRM_CHAR
					NRF_LOG_DEBUG("Password confirmation request received: ");
					NRF_LOG_HEX(p_ble_evt->evt.gatts_evt.params.authorize_request.request.read.context.char_uuid.uuid);
					NRF_LOG_DEBUG("\r\n");
					check_password(p_garage_service, p_ble_evt);
				}
			}
            break;
            
        default:
            // No implementation needed.
            break;
    }
}

void ble_garage_service_init(ble_garage_t * p_garage_service) {

	uint32_t	err_code;
	char* 		pass = PASSWORD;
	
	NRF_LOG_INIT();
	
	for (uint8_t i = 0; i < PASS_LENGTH; i++) {
		p_nonce_buff[NONCE_LENGTH+i] = pass[i];
	}
	
	nrf_gpio_pin_clear(0);
	
    // OUR_JOB: Declare 16 bit service and 128 bit base UUIDs and add them to BLE stack table     
    ble_uuid128_t		base_uuid = {BLE_UUID_GARAGE_BASE_UUID};
	ble_uuid_t        	service_uuid;
    
    service_uuid.uuid = BLE_UUID_GARAGE_SERVICE;
	
    err_code = sd_ble_uuid_vs_add(&base_uuid, &p_garage_service->uuid_type);
    APP_ERROR_CHECK(err_code);    

	service_uuid.type = p_garage_service->uuid_type;
    // OUR_JOB: Add our service
    err_code = sd_ble_gatts_service_add(BLE_GATTS_SRVC_TYPE_PRIMARY,
                                        &service_uuid,
                                        &p_garage_service->service_handle);
    APP_ERROR_CHECK(err_code);

	err_code = port_char_add(p_garage_service);
	APP_ERROR_CHECK(err_code);
	err_code = port_confirm_char_add(p_garage_service);
	APP_ERROR_CHECK(err_code);
}
