

#include <stdbool.h>
#include <stdint.h>
#include "ble_advdata.h"
#include "nordic_common.h"
#include "softdevice_handler.h"
#include "bsp.h"
#include "app_timer.h"
#include "device_manager.h"
#include "ble.h"
#include "ble_hci.h"
#include "ble_srv_common.h"
#include "ble_advdata.h"
#include "ble_advertising.h"
#include "ble_conn_params.h"
#include "pstorage.h"
#include "nrf_log.h"
#include "garage_service.h"
#include "com_def.h"
#include "nrf_gpio.h"


#define CENTRAL_LINK_COUNT              0                                 /**<number of central links used by the application. When changing this number remember to adjust the RAM settings*/
#define PERIPHERAL_LINK_COUNT           1                                 /**<number of peripheral links used by the application. When changing this number remember to adjust the RAM settings*/

#define DEVICE_NAME                      "Karlsen_Garage"                 /**< Name of device. Will be included in the advertising data. */

#define APP_ADV_INTERVAL                1600                              /**< The advertising interval (in units of 0.625 ms. This value corresponds to (1 s/1600) 0.5 s). */
#define APP_ADV_TIMEOUT_IN_SECONDS      0	                              /**< The advertising timeout in units of seconds. */

#define IS_SRVC_CHANGED_CHARACT_PRESENT 0                                 /**< Include or not the service_changed characteristic. if not enabled, the server's database cannot be changed for the lifetime of the device*/

#define APP_CFG_NON_CONN_ADV_TIMEOUT    0                                 /**< Time for which the device must be advertising in non-connectable mode (in seconds). 0 disables timeout. */
#define NON_CONNECTABLE_ADV_INTERVAL    MSEC_TO_UNITS(100, UNIT_0_625_MS) /**< The advertising interval for non-connectable advertisement (100 ms). This value can vary between 100ms to 10.24s). */

#define APP_BEACON_INFO_LENGTH          0x17                              /**< Total length of information advertised by the Beacon. */
#define APP_ADV_DATA_LENGTH             0x15                              /**< Length of manufacturer specific data in the advertisement. */
#define APP_DEVICE_TYPE                 0x02                              /**< 0x02 refers to Beacon. */
#define APP_MEASURED_RSSI               0xC3                              /**< The Beacon's measured RSSI at 1 meter distance in dBm. */
#define APP_COMPANY_IDENTIFIER          0x0059                            /**< Company identifier for Nordic Semiconductor ASA. as per www.bluetooth.org. */
#define APP_MAJOR_VALUE                 0x01, 0x02                        /**< Major value used to identify Beacons. */ 
#define APP_MINOR_VALUE                 0x03, 0x04                        /**< Minor value used to identify Beacons. */ 
#define APP_BEACON_UUID                 0x01, 0x12, 0x23, 0x34, \
                                        0x45, 0x56, 0x67, 0x78, \
                                        0x89, 0x9a, 0xab, 0xbc, \
                                        0xcd, 0xde, 0xef, 0xf0            /**< Proprietary UUID for Beacon. */

#define DEAD_BEEF                       0xDEADBEEF                        /**< Value used as error code on stack dump, can be used to identify stack location on stack unwind. */




//These are the values in connected mode, how often the central is adviced to connect and how often the device can ignore
//Does not matter much in this context since it will be used for short connect and then release
#define MIN_CONN_INTERVAL                MSEC_TO_UNITS(100, UNIT_1_25_MS)           /**< Minimum acceptable connection interval (0.1 seconds). */
#define MAX_CONN_INTERVAL                MSEC_TO_UNITS(200, UNIT_1_25_MS)           /**< Maximum acceptable connection interval (0.2 second). */
#define SLAVE_LATENCY                    0                                          /**< Slave latency. */
#define CONN_SUP_TIMEOUT                 MSEC_TO_UNITS(4000, UNIT_10_MS)            /**< Connection supervisory timeout (4 seconds). */

#define FIRST_CONN_PARAMS_UPDATE_DELAY   APP_TIMER_TICKS(5000, APP_TIMER_PRESCALER) /**< Time from initiating event (connect or start of notification) to first time sd_ble_gap_conn_param_update is called (5 seconds). */
#define NEXT_CONN_PARAMS_UPDATE_DELAY    APP_TIMER_TICKS(30000, APP_TIMER_PRESCALER)/**< Time between each call to sd_ble_gap_conn_param_update after the first call (30 seconds). */
#define MAX_CONN_PARAMS_UPDATE_COUNT     3                                          /**< Number of attempts before giving up the connection parameter negotiation. */

#define SEC_PARAM_BOND                   1                                          /**< Perform bonding. */
#define SEC_PARAM_MITM                   1                                          /**< Man In The Middle protection required. */
#define SEC_PARAM_IO_CAPABILITIES        BLE_GAP_IO_CAPS_DISPLAY_ONLY               /**< No I/O capabilities. */
#define SEC_PARAM_OOB                    0                                          /**< Out Of Band data not available. */
#define SEC_PARAM_MIN_KEY_SIZE           7                                          /**< Minimum encryption key size. */
#define SEC_PARAM_MAX_KEY_SIZE           16                                         /**< Maximum encryption key size. */

#define BONDING_TIMEOUT					 7200*1000									/*Do not allow bonding for two hours when NO_BOND_FAILED*/ 						 

#if defined(USE_UICR_FOR_MAJ_MIN_VALUES)
#define MAJ_VAL_OFFSET_IN_BEACON_INFO   18                                /**< Position of the MSB of the Major Value in m_beacon_info array. */
#define UICR_ADDRESS                    0x10001080                        /**< Address of the UICR register used by this example. The major and minor versions to be encoded into the advertising data will be picked up from this location. */
#endif


uint8_t									failed_auth = 0;					/** Failed authentications, global scope for use in device manager*/


static dm_application_instance_t        m_app_handle;               	  /**< Application identifier allocated by device manager */
static uint16_t                         m_conn_handle = BLE_CONN_HANDLE_INVALID;   /**< Handle of the current connection. */

static ble_uuid_t m_adv_uuids[] = {{BLE_UUID_GARAGE_SERVICE, BLE_UUID_TYPE_VENDOR_BEGIN},
								  {BLE_UUID_DEVICE_INFORMATION_SERVICE, BLE_UUID_TYPE_BLE}}; /**< Universally unique service identifiers. */

static	ble_garage_t					m_garage_service;

APP_TIMER_DEF(m_block_bond_timer_id);												/* Defined timer for blocking further bonding when failing */



/**@brief Callback function for asserts in the SoftDevice.
 *
 * @details This function will be called in case of an assert in the SoftDevice.
 *
 * @warning This handler is an example only and does not fit a final product. You need to analyze
 *          how your product is supposed to react in case of Assert.
 * @warning On assert from the SoftDevice, the system can only recover on reset.
 *
 * @param[in]   line_num   Line number of the failing ASSERT call.
 * @param[in]   file_name  File name of the failing ASSERT call.
 */
void assert_nrf_callback(uint16_t line_num, const uint8_t * p_file_name)
{
	NRF_LOG_DEBUG("Hello World\r\n");
    app_error_handler(DEAD_BEEF, line_num, p_file_name);
	
}

/**@brief Function for putting the chip into sleep mode.
 *
 * @note This function will not return.
 */
static void sleep_mode_enter(void)
{
	uint32_t err_code;
	
    // Go to system-off mode (this function will not return; wakeup will cause a reset).
    err_code = sd_power_system_off();
    APP_ERROR_CHECK(err_code);
}

/**@brief Function for handling advertising events.
 *
 * @details This function will be called for advertising events which are passed to the application.
 *
 * @param[in] ble_adv_evt  Advertising event.
 */
static void on_adv_evt(ble_adv_evt_t ble_adv_evt)
{
    //uint32_t err_code;

    switch (ble_adv_evt)
    {
        case BLE_ADV_EVT_FAST:
            //err_code = bsp_indication_set(BSP_INDICATE_ADVERTISING);
            //APP_ERROR_CHECK(err_code);
            break;
        case BLE_ADV_EVT_IDLE:
            sleep_mode_enter();
            break;
        default:
            break;
    }
}

/**@brief Function for handling the Connection Parameters Module.
 *
 * @details This function will be called for all events in the Connection Parameters Module which
 *          are passed to the application.
 *          @note All this function does is to disconnect. This could have been done by simply
 *                setting the disconnect_on_fail config parameter, but instead we use the event
 *                handler mechanism to demonstrate its use.
 *
 * @param[in] p_evt  Event received from the Connection Parameters Module.
 */
static void on_conn_params_evt(ble_conn_params_evt_t * p_evt)
{
    uint32_t err_code;

    if (p_evt->evt_type == BLE_CONN_PARAMS_EVT_FAILED)
    {
        err_code = sd_ble_gap_disconnect(m_conn_handle, BLE_HCI_CONN_INTERVAL_UNACCEPTABLE);
        APP_ERROR_CHECK(err_code);
    }
}

/**@brief Function for handling a Connection Parameters error.
 *
 * @param[in] nrf_error  Error code containing information about what went wrong.
 */
static void conn_params_error_handler(uint32_t nrf_error)
{
    APP_ERROR_HANDLER(nrf_error);
}

/**@brief Function for initializing the Connection Parameters module.
 */
static void conn_params_init(void)
{
    uint32_t               err_code;
    ble_conn_params_init_t cp_init;

    memset(&cp_init, 0, sizeof(cp_init));

    cp_init.p_conn_params                  = NULL;
    cp_init.first_conn_params_update_delay = FIRST_CONN_PARAMS_UPDATE_DELAY;
    cp_init.next_conn_params_update_delay  = NEXT_CONN_PARAMS_UPDATE_DELAY;
    cp_init.max_conn_params_update_count   = MAX_CONN_PARAMS_UPDATE_COUNT;
    cp_init.start_on_notify_cccd_handle    = BLE_GATT_HANDLE_INVALID;
    cp_init.disconnect_on_fail             = false;
    cp_init.evt_handler                    = on_conn_params_evt;
    cp_init.error_handler                  = conn_params_error_handler;

    err_code = ble_conn_params_init(&cp_init);
    APP_ERROR_CHECK(err_code);
}

/**@brief Function for initializing services that will be used by the application.
 */
static void services_init(void)
{
    // OUR_JOB: Add code to initialize the services used by the application.
	ble_garage_service_init(&m_garage_service);
}

/**@brief Function for initializing the Advertising functionality.
 *
 * @details Encodes the required advertising data and passes it to the stack.
 *          Also builds a structure to be passed to the stack when starting advertising.
 */
static void advertising_init(void)
{
    uint32_t      err_code;
    ble_advdata_t advdata;
	ble_advdata_t srdata;

    // Build and set advertising data.
    memset(&advdata, 0, sizeof(advdata));
    advdata.name_type             = BLE_ADVDATA_FULL_NAME;
	advdata.flags                 = BLE_GAP_ADV_FLAGS_LE_ONLY_GENERAL_DISC_MODE;
	advdata.include_appearance    = true;
    
	ble_adv_modes_config_t options = {0};
    options.ble_adv_fast_enabled  = BLE_ADV_FAST_ENABLED;
    options.ble_adv_fast_interval = APP_ADV_INTERVAL;
    options.ble_adv_fast_timeout  = APP_ADV_TIMEOUT_IN_SECONDS;
	
	//Build and set scan response data, needed due to bigger size of custom uuid
	memset(&srdata, 0, sizeof(srdata));
	srdata.uuids_complete.uuid_cnt = sizeof(m_adv_uuids) / sizeof(m_adv_uuids[0]);
    srdata.uuids_complete.p_uuids  = m_adv_uuids;

    err_code = ble_advertising_init(&advdata, &srdata, &options, on_adv_evt, NULL);
    APP_ERROR_CHECK(err_code);
}

/**@brief Handle events for bond timer expiry.
 *
 * @param[in]   p_context   parameter registered in timer start function.
 */
static void bond_timer_expiry_handler(void * p_context)
{
    failed_auth = 0;																				//
}


/**@brief Function for handling the Device Manager events.
 *
 * @param[in] p_event  Data associated to the device manager event.
 */
static uint32_t device_manager_evt_handler(dm_handle_t const * p_handle,
                                           dm_event_t const  * p_event,
                                           ret_code_t        event_result)
{
	uint32_t      err_code;
	
	switch (p_event->event_id)
    {
		case DM_EVT_SECURITY_SETUP_COMPLETE:						
			if (event_result == BLE_GAP_SEC_STATUS_CONFIRM_VALUE)							//Failed authentication, accept three tries then lock for new 
			{																			   //bonding for 2 hours
				failed_auth++;
				NRF_LOG_DEBUG("!!!Failed Bonding!!!!!!!");
				if (failed_auth >= NO_BOND_FAILED)
				{
					err_code = app_timer_create(&m_block_bond_timer_id, APP_TIMER_MODE_SINGLE_SHOT, bond_timer_expiry_handler);	
					APP_ERROR_CHECK(err_code);
					err_code = app_timer_start(m_block_bond_timer_id,APP_TIMER_TICKS(7200*1000, APP_TIMER_PRESCALER), NULL);
					APP_ERROR_CHECK(err_code);
					NRF_LOG_DEBUG("!!!Failed Bonding, Timer SET!!!!!!!");
				}
			}
			break;
		case DM_EVT_LINK_SECURED:
			failed_auth = 0;																//Whenever someone connects securely, reset fail counter		
	}
	
	//uint32_t tmp = p_event->event_id;
	NRF_LOG_DEBUG("Event!!!device manager event!!!!!!!!");
	NRF_LOG_HEX(p_event->event_id);
	NRF_LOG_DEBUG("!!!!!");
	NRF_LOG_HEX(event_result);
	NRF_LOG_DEBUG("Event!!!device manager event!!!!!!!!\r\n");

    return NRF_SUCCESS;
}

/**@brief Function for the Device Manager initialization.
 *
 *
 *  
 */
static void device_manager_init()
{
    uint32_t               err_code;
    dm_init_param_t        init_param = {.clear_persistent_data = false};			//Locked to clear right now, don't know why
    dm_application_param_t register_param;

    // Initialize persistent storage module.
    err_code = pstorage_init();
    APP_ERROR_CHECK(err_code);

    err_code = dm_init(&init_param);
    APP_ERROR_CHECK(err_code);

    memset(&register_param.sec_param, 0, sizeof (ble_gap_sec_params_t));

    register_param.sec_param.bond         = SEC_PARAM_BOND;
    register_param.sec_param.mitm         = SEC_PARAM_MITM;
    register_param.sec_param.io_caps      = SEC_PARAM_IO_CAPABILITIES;
    register_param.sec_param.oob          = SEC_PARAM_OOB;
    register_param.sec_param.min_key_size = SEC_PARAM_MIN_KEY_SIZE;
    register_param.sec_param.max_key_size = SEC_PARAM_MAX_KEY_SIZE;
    register_param.evt_handler            = device_manager_evt_handler;
    register_param.service_type           = DM_PROTOCOL_CNTXT_GATT_SRVR_ID;

    err_code = dm_register(&m_app_handle, &register_param);
    APP_ERROR_CHECK(err_code);
}

/**@brief Function for the GAP initialization.
 *
 * @details This function sets up all the necessary GAP (Generic Access Profile) parameters of the
 *          device including the device name, appearance, and the preferred connection parameters.
 */
static void gap_params_init(void)
{
    uint32_t                err_code;
    ble_gap_conn_params_t   gap_conn_params;
    ble_gap_conn_sec_mode_t sec_mode;

	BLE_GAP_CONN_SEC_MODE_SET_ENC_WITH_MITM(&sec_mode);							//Require shared secret and encryption	

    err_code = sd_ble_gap_device_name_set(&sec_mode,
                                          (const uint8_t *)DEVICE_NAME,
                                          strlen(DEVICE_NAME));
    APP_ERROR_CHECK(err_code);

   
    err_code = sd_ble_gap_appearance_set(BLE_APPEARANCE_GENERIC_REMOTE_CONTROL);
    APP_ERROR_CHECK(err_code);

    memset(&gap_conn_params, 0, sizeof(gap_conn_params));

    gap_conn_params.min_conn_interval = MIN_CONN_INTERVAL;
    gap_conn_params.max_conn_interval = MAX_CONN_INTERVAL;
    gap_conn_params.slave_latency     = SLAVE_LATENCY;
    gap_conn_params.conn_sup_timeout  = CONN_SUP_TIMEOUT;

    err_code = sd_ble_gap_ppcp_set(&gap_conn_params);							//This really sets the suggestion as seen by the central
    APP_ERROR_CHECK(err_code);
	
	uint8_t passkey[] = "613118";												//Use a fixed PIN code for bonding and security
	ble_opt_t ble_opt;
	ble_opt.gap_opt.passkey.p_passkey = &passkey[0];
	err_code =  sd_ble_opt_set(BLE_GAP_OPT_PASSKEY, &ble_opt);
	APP_ERROR_CHECK(err_code);
}


/**@brief Function for handling the Application's BLE Stack events.
 *
 * @param[in]   p_ble_evt   Bluetooth stack event.
 */
static void on_ble_evt(ble_evt_t * p_ble_evt)
{
	//uint16_t garbish;
	//Handle some sort of BLE events
	//NRF_LOG_DEBUG("Event!!!!!!!!!!!");
	NRF_LOG_HEX(p_ble_evt->header.evt_id);
	NRF_LOG_DEBUG("AA\n");
	/*NRF_LOG_HEX(p_ble_evt->header.evt_len);
	NRF_LOG_DEBUG("Event!!!!!!!!!!!\r\n");*/
//	NRF_LOG_HEX(p_ble_evt->evt.gap_evt.conn_handle);
	
	switch (p_ble_evt->header.evt_id)
    {
        case BLE_GAP_EVT_CONNECTED:
            m_conn_handle = p_ble_evt->evt.gap_evt.conn_handle;
			//NRF_LOG_DEBUG("It gets connected here!\r\n");
            break;
			
		case BLE_GATTS_EVT_RW_AUTHORIZE_REQUEST:
			break;

		case BLE_GAP_EVT_CONN_SEC_UPDATE:
			NRF_LOG_HEX(p_ble_evt->evt.gap_evt.params.conn_sec_update.conn_sec.sec_mode.sm);
			NRF_LOG_DEBUG("BB");
			NRF_LOG_HEX(p_ble_evt->evt.gap_evt.params.conn_sec_update.conn_sec.sec_mode.lv);
			NRF_LOG_DEBUG("BB");
			NRF_LOG_HEX(p_ble_evt->evt.gap_evt.params.conn_sec_update.conn_sec.encr_key_size);
			NRF_LOG_DEBUG("BB\n");
			break;
        default:
            // No implementation needed.
            break;
    }
}

/**@brief Function for dispatching a BLE stack event to all modules with a BLE stack event handler.
 *
 * @details This function is called from the BLE Stack event interrupt handler after a BLE stack
 *          event has been received.
 *
 * @param[in]   p_ble_evt   Bluetooth stack event.
 */
static void ble_evt_dispatch(ble_evt_t * p_ble_evt)
{
	dm_ble_evt_handler(p_ble_evt);									//The device manager will handle all bonding etc.
	ble_conn_params_on_ble_evt(p_ble_evt);							//Seems to be required, not obvius why
	
	on_ble_evt(p_ble_evt);											//Actual handler
	ble_advertising_on_ble_evt(p_ble_evt);							//Need to be here for advertising according to documentation
	ble_garage_on_ble_evt(&m_garage_service,p_ble_evt);

}


/**@brief Function for dispatching a system event to interested modules.
 *
 * @details This function is called from the System event interrupt handler after a system
 *          event has been received.
 *
 * @param[in]   sys_evt   System stack event.
 */
static void sys_evt_dispatch(uint32_t sys_evt)
{
	pstorage_sys_event_handler(sys_evt);							//Mandatory for persistent storage
	ble_advertising_on_sys_evt(sys_evt);							//Need to be there for advertising events

}

char toHex(char no)
{
	
	if (no > 9)
		no = no - 10 + 65;
	else
		no = no + 48;
	
	return no;
}

void convert(uint32_t err_code, char *str)
{

	char tmp;
	
	str[0] = '0';
	str[1] = 'x';
	tmp = (err_code >> 28) & 0x0f;
	str[2] = toHex(tmp);
	tmp = (err_code >> 24) & 0x0f;
	str[3] = toHex(tmp);
	tmp = (err_code >> 20) & 0x0f;
	str[4] = toHex(tmp);
	tmp = (err_code >> 16) & 0x0f;
	str[5] = toHex(tmp);
	tmp = (err_code >> 12) & 0x0f;
	str[6] = toHex(tmp);
	tmp = (err_code >> 8) & 0x0f;
	str[7] = toHex(tmp);
	tmp = (err_code >> 4) & 0x0f;
	str[8] = toHex(tmp);
	tmp = (err_code >> 0) & 0x0f;
	str[9] = toHex(tmp);
	str[10] = '\r';
	str[11] = '\n';
	str[12] = '\0';
}

void app_error_fault_handler(uint32_t id, uint32_t pc, uint32_t info)
{
	char str[13];
	//((error_info_t *)info)->err_code
	convert(((error_info_t *)info)->err_code, str);
	NRF_LOG_DEBUG(str);
    // On assert, the system can only recover with a reset.
#ifndef DEBUG
  //  NVIC_SystemReset();
#else

#ifdef BSP_DEFINES_ONLY
    LEDS_ON(LEDS_MASK);
#else
  //  UNUSED_VARIABLE(bsp_indication_set(BSP_INDICATE_FATAL_ERROR));
    // This call can be used for debug purposes during application development.
    // @note CAUTION: Activating this code will write the stack to flash on an error.
    //                This function should NOT be used in a final product.
    //                It is intended STRICTLY for development/debugging purposes.
    //                The flash write will happen EVEN if the radio is active, thus interrupting
    //                any communication.
    //                Use with care. Uncomment the line below to use.
    //ble_debug_assert_handler(error_code, line_num, p_file_name);
#endif // BSP_DEFINES_ONLY

  //  app_error_save_and_stop(id, pc, info);

#endif // DEBUG
}


/**@brief Function for initializing the BLE stack.
 *
 * @details Initializes the SoftDevice and the BLE event interrupt.
 */
static void ble_stack_init(void)
{
    uint32_t err_code;

    // Initialize the SoftDevice handler module.
    SOFTDEVICE_HANDLER_INIT(NRF_CLOCK_LFCLKSRC_XTAL_20_PPM, NULL);

    //Retrieve default parameters
	ble_enable_params_t ble_enable_params;
	err_code = softdevice_enable_get_default_config(CENTRAL_LINK_COUNT,
                                                    PERIPHERAL_LINK_COUNT,
                                                    &ble_enable_params);
	APP_ERROR_CHECK(err_code);

    //Check the ram settings against the used number of links
    CHECK_RAM_START_ADDR(CENTRAL_LINK_COUNT,PERIPHERAL_LINK_COUNT);

    // Enable BLE stack.
    err_code = softdevice_enable(&ble_enable_params);
    APP_ERROR_CHECK(err_code);
	
	// Register with the SoftDevice handler module for BLE events.		
    err_code = softdevice_ble_evt_handler_set(ble_evt_dispatch);
    APP_ERROR_CHECK(err_code);
	
    // Register with the SoftDevice handler module for System events.
    err_code = softdevice_sys_evt_handler_set(sys_evt_dispatch);
    APP_ERROR_CHECK(err_code);
}


/**@brief Function for doing power management.
 */
static void power_manage(void)
{
    uint32_t err_code = sd_app_evt_wait();
    APP_ERROR_CHECK(err_code);
}


/**
 * @brief Function for application main entry.
 */
int main(void)
{
    uint32_t err_code;
	
    // Initialize.
	//nrf_gpio_cfg_output(0);
	nrf_gpio_cfg(0, NRF_GPIO_PIN_DIR_OUTPUT, NRF_GPIO_PIN_INPUT_DISCONNECT, NRF_GPIO_PIN_NOPULL, NRF_GPIO_PIN_S0H1, NRF_GPIO_PIN_NOSENSE);
	NRF_LOG_INIT();
	NRF_LOG_DEBUG("direkt efter init\r\n");
	
    APP_TIMER_INIT(APP_TIMER_PRESCALER, APP_TIMER_OP_QUEUE_SIZE, false);
	//APP_ERROR_CHECK(err_code);
    //err_code = bsp_init(BSP_INIT_LED, APP_TIMER_TICKS(100, APP_TIMER_PRESCALER), NULL);		//Only leds right now
    //APP_ERROR_CHECK(err_code);
    
	ble_stack_init();
	device_manager_init();
	gap_params_init();
	services_init();
    advertising_init();
	conn_params_init();

    // Start execution.
    err_code = ble_advertising_start(BLE_ADV_MODE_FAST);
    APP_ERROR_CHECK(err_code);
	NRF_LOG_DEBUG("kollar\r\n");
	NRF_LOG_DEBUG("Hello World!\r\n");

    // Enter main loop.
    for (;; )
    {
       // power_manage();
    }
}


/**
 * @}
 */
