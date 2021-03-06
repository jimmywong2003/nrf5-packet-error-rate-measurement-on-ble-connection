/**
 * Copyright (c) 2015 - 2018, Nordic Semiconductor ASA
 *
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without modification,
 * are permitted provided that the following conditions are met:
 *
 * 1. Redistributions of source code must retain the above copyright notice, this
 *    list of conditions and the following disclaimer.
 *
 * 2. Redistributions in binary form, except as embedded into a Nordic
 *    Semiconductor ASA integrated circuit in a product or a software update for
 *    such product, must reproduce the above copyright notice, this list of
 *    conditions and the following disclaimer in the documentation and/or other
 *    materials provided with the distribution.
 *
 * 3. Neither the name of Nordic Semiconductor ASA nor the names of its
 *    contributors may be used to endorse or promote products derived from this
 *    software without specific prior written permission.
 *
 * 4. This software, with or without modification, must only be used with a
 *    Nordic Semiconductor ASA integrated circuit.
 *
 * 5. Any software provided in binary form under this license must not be reverse
 *    engineered, decompiled, modified and/or disassembled.
 *
 * THIS SOFTWARE IS PROVIDED BY NORDIC SEMICONDUCTOR ASA "AS IS" AND ANY EXPRESS
 * OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY, NONINFRINGEMENT, AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL NORDIC SEMICONDUCTOR ASA OR CONTRIBUTORS BE
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE
 * GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT
 * OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 */
/**
 * @brief Blinky Sample Application main file.
 *
 * This file contains the source code for a sample server application using the LED Button service.
 */

#include <stdint.h>
#include <string.h>
#include "nordic_common.h"
#include "nrf.h"
#include "app_error.h"
#include "ble.h"
#include "ble_err.h"
#include "ble_hci.h"
#include "app_uart.h"
#include "ble_srv_common.h"
#include "ble_advdata.h"
#include "ble_conn_params.h"
#include "nrf_sdh.h"
#include "nrf_sdh_ble.h"
#include "boards.h"
#include "app_timer.h"
#include "app_button.h"

#include "nrf_ble_gatt.h"
#include "nrf_ble_qwr.h"
#include "nrf_pwr_mgmt.h"

#include "ble_lbs.h"
#include "ble_nus.h"
#include "ble_image_transfer_service.h"

#include "app_scheduler.h"
#include "nrf_delay.h"

#include "nrf_log.h"
#include "nrf_log_ctrl.h"
#include "nrf_log_default_backends.h"

#if defined(UART_PRESENT)
#include "nrf_uart.h"
#endif
#if defined(UARTE_PRESENT)
#include "nrf_uarte.h"
#endif

#include "app_display.h"

#define ADVERTISING_LED BSP_BOARD_LED_0 /**< Is on when device is advertising. */
#define CONNECTED_LED BSP_BOARD_LED_1   /**< Is on when device has connected. */
#define LEDBUTTON_LED BSP_BOARD_LED_2   /**< LED to be toggled with the help of the LED Button Service. */

#define APP_FEATURE_NOT_SUPPORTED BLE_GATT_STATUS_ATTERR_APP_BEGIN + 2 /**< Reply when unsupported features are requested. */

#define PHY_BUTTON BSP_BUTTON_0
#define TX_POWER_BUTTON BSP_BUTTON_1
#define APP_STATE_BUTTON BSP_BUTTON_2
#define LEDBUTTON_BUTTON BSP_BUTTON_3 /**< Button that will trigger the notification event with the LED Button Service */

#define DEVICE_NAME "Peripheral_PER" /**< Name of device. Will be included in the advertising data. */

#define NUS_SERVICE_UUID_TYPE BLE_UUID_TYPE_VENDOR_BEGIN /**< UUID type for the Nordic UART Service (vendor specific). */

#define APP_BLE_OBSERVER_PRIO 3 /**< Application's BLE observer priority. You shouldn't need to modify this value. */
#define APP_BLE_CONN_CFG_TAG 1  /**< A tag identifying the SoftDevice BLE configuration. */

#define APP_ADV_INTERVAL 300                                   /**< The advertising interval (in units of 0.625 ms; this value corresponds to 40 ms). */
#define APP_ADV_DURATION BLE_GAP_ADV_TIMEOUT_GENERAL_UNLIMITED /**< The advertising time-out (in units of seconds). When set to 0, we will never time out. */
#define RESTART_ADVERTISING_TIMEOUT_MS 2000

#define MIN_CONN_INTERVAL MSEC_TO_UNITS(7.5, UNIT_1_25_MS) /**< Minimum acceptable connection interval (0.5 seconds). */
#define MAX_CONN_INTERVAL MSEC_TO_UNITS(7.5, UNIT_1_25_MS) /**< Maximum acceptable connection interval (1 second). */
#define SLAVE_LATENCY 0                                    /**< Slave latency. */
#define CONN_SUP_TIMEOUT MSEC_TO_UNITS(10000, UNIT_10_MS)   /**< Connection supervisory time-out (4 seconds). */

#define FIRST_CONN_PARAMS_UPDATE_DELAY APP_TIMER_TICKS(5000) /**< Time from initiating event (connect or start of notification) to first time sd_ble_gap_conn_param_update is called (15 seconds). */
#define NEXT_CONN_PARAMS_UPDATE_DELAY APP_TIMER_TICKS(20000) /**< Time between each call to sd_ble_gap_conn_param_update after the first call (5 seconds). */
#define MAX_CONN_PARAMS_UPDATE_COUNT 3                       /**< Number of attempts before giving up the connection parameter negotiation. */

#define BUTTON_DETECTION_DELAY APP_TIMER_TICKS(50) /**< Delay from a GPIOTE event until a button is reported as pushed (in number of timer ticks). */

#define SCHED_MAX_EVENT_DATA_SIZE APP_TIMER_SCHED_EVENT_DATA_SIZE /**< Maximum size of scheduler events. */
#ifdef SVCALL_AS_NORMAL_FUNCTION
#define SCHED_QUEUE_SIZE 20 /**< Maximum number of events in the scheduler queue. More is needed in case of Serialization. */
#else
#define SCHED_QUEUE_SIZE 10 /**< Maximum number of events in the scheduler queue. */
#endif

#ifdef NRF52840_XXAA
#define TX_POWER_LEVEL (8) /**< TX Power Level value. This will be set both in the TX Power service, in the advertising data, and also used to set the radio transmit power. */
#else
#define TX_POWER_LEVEL (4) /**< TX Power Level value. This will be set both in the TX Power service, in the advertising data, and also used to set the radio transmit power. */
#endif

#define UART_TX_BUF_SIZE 256 /**< UART TX buffer size. */
#define UART_RX_BUF_SIZE 256 /**< UART RX buffer size. */

#define DEAD_BEEF 0xDEADBEEF /**< Value used as error code on stack dump, can be used to identify stack location on stack unwind. */

#define CONNECTION_ONE_SECOND_INTERVAL APP_TIMER_TICKS(1000) /**< Battery level measurement interval (ticks). */

APP_TIMER_DEF(m_connection_one_sec_timer_id);

static bool m_connection_one_sec_timer_is_running = false;

BLE_NUS_DEF(m_nus, NRF_SDH_BLE_TOTAL_LINK_COUNT); /**< BLE NUS service instance. */
BLE_ITS_DEF(m_its, NRF_SDH_BLE_TOTAL_LINK_COUNT); /**< BLE IMAGE TRANSFER service instance. */
BLE_LBS_DEF(m_lbs);                               /**< LED Button Service instance. */

NRF_BLE_GATT_DEF(m_gatt); /**< GATT module instance. */
NRF_BLE_QWR_DEF(m_qwr);   /**< Context for the Queued Write module.*/

APP_TIMER_DEF(m_restart_advertising_timer_id);

static uint16_t m_conn_handle = BLE_CONN_HANDLE_INVALID; /**< Handle of the current connection. */

static uint8_t m_adv_handle = BLE_GAP_ADV_SET_HANDLE_NOT_SET;           /**< Advertising handle used to identify an advertising set. */
static uint8_t m_enc_advdata[BLE_GAP_ADV_SET_DATA_SIZE_MAX];            /**< Buffer for storing an encoded advertising set. */
static uint8_t m_enc_scan_response_data[BLE_GAP_ADV_SET_DATA_SIZE_MAX]; /**< Buffer for storing an encoded scan data. */

static uint16_t m_ble_nus_max_data_len = BLE_GATT_ATT_MTU_DEFAULT - 3; /**< Maximum length of data (in bytes) that can be transmitted to the peer by the Nordic UART service module. */

static ble_its_ble_params_info_t m_ble_params_info = {20, 50, 1, 1};
static uint16_t m_ble_its_max_data_len = BLE_GATT_ATT_MTU_DEFAULT - 3; /**< Maximum length of data (in bytes) that can be transmitted to the peer by the Nordic UART service module. */

app_display_content_t m_application_state = {0};

static uint8_t m_new_command_received = 0;
static uint8_t m_new_resolution, m_new_phy;
static bool m_stream_mode_active = false;

static void display_update(void);
static void advertising_start(void);

static uint8_t img_data_buffer[BLE_ITS_MAX_DATA_LEN];
static uint8_t m_buffer_transfer_index = 0;

static uint32_t frameCount = 0;
static uint32_t frameCount_previous = 0;
static uint32_t img_data_length = BLE_ITS_MAX_DATA_LEN;

#define TEST_FIRMWARE_SIZE (1024 * 1024)
//#define TEST_FIRMWARE_SIZE (100 * 1024)

static uint32_t start_time = 0;
static uint32_t finish_time = 0;
static uint32_t data_send = 0;

//static bool test_flag = false;
static bool start_thoughput_test = false;

#define DEBUG_GPIO_SEND_COUNT 

#ifdef DEBUG_GPIO_SEND_COUNT
#define GPIO_SEND_COUNT_PIN 31
#endif 

/**@brief Struct that contains pointers to the encoded advertising data. */
static ble_gap_adv_data_t m_adv_data =
{
        .adv_data =
        {
                .p_data = m_enc_advdata,
                .len = BLE_GAP_ADV_SET_DATA_SIZE_MAX
        },
        .scan_rsp_data =
        {
                .p_data = m_enc_scan_response_data,
                .len = BLE_GAP_ADV_SET_DATA_SIZE_MAX

        }
};

static void test_thoughput(void)
{
        uint32_t ret_code;
        do
        {
                memset(img_data_buffer, m_buffer_transfer_index, m_ble_its_max_data_len);
                ret_code = ble_its_send_file_fragment(&m_its, img_data_buffer, m_ble_its_max_data_len);
                if (ret_code == NRF_SUCCESS)
                {
                        img_data_length = 0;
                        frameCount++;
#ifdef DEBUG_GPIO_SEND_COUNT
                        nrf_gpio_pin_toggle(GPIO_SEND_COUNT_PIN);
#endif
                        data_send += m_ble_its_max_data_len;
                }
                m_buffer_transfer_index++;
                m_buffer_transfer_index = m_buffer_transfer_index % 10;

                if (data_send > TEST_FIRMWARE_SIZE)
                {
                        finish_time = app_timer_cnt_get();
                        start_thoughput_test = false;
                        //NRF_LOG_INFO("Total time = %d.", (finish_time - start_time)/32768);

                        NRF_LOG_INFO("Time of finish transfer = %d", finish_time);
                        double time_ms_float = (finish_time - start_time) / 32.768;
                        uint32_t time_ms = (uint32_t) time_ms_float;
                        uint32_t bit_count = (data_send * 8);
                        float throughput_kbps = ((bit_count / (time_ms / 1000.f)) / 1000.f);


                        NRF_LOG_INFO("Done.");
                        NRF_LOG_INFO("=============================");
                        NRF_LOG_INFO("Time: %u.%.2u seconds elapsed.", (time_ms / 1000), (time_ms % 1000));
                        NRF_LOG_INFO("Throughput: " NRF_LOG_FLOAT_MARKER " Kbps.",
                                     NRF_LOG_FLOAT(throughput_kbps));
                        NRF_LOG_INFO("=============================");
                        NRF_LOG_INFO("Sent %u KBytes of ATT payload.", data_send/1024);
                        NRF_LOG_INFO("Retrieving amount of bytes received from peer...");

                        if (m_connection_one_sec_timer_is_running)
                        {
                                ret_code = app_timer_stop(m_connection_one_sec_timer_id);
                                APP_ERROR_CHECK(ret_code);
                                m_connection_one_sec_timer_is_running = false;
                        }
                        break;
                }
        } while (ret_code == NRF_SUCCESS);
}

static void connection_one_second_timeout_handler(void *p_context)
{
        UNUSED_PARAMETER(p_context);

        uint32_t throughput_kbps = ((frameCount - frameCount_previous) * m_ble_its_max_data_len * 8);

        NRF_LOG_INFO("%d, %d, %06d bits, %02d %%", frameCount, frameCount - frameCount_previous, throughput_kbps, data_send * 100 / TEST_FIRMWARE_SIZE);
        frameCount_previous = frameCount;
        m_application_state.throughput_kbps = throughput_kbps / 1000;

        display_update();
}

/**@brief Function for assert macro callback.
 *
 * @details This function will be called in case of an assert in the SoftDevice.
 *
 * @warning This handler is an example only and does not fit a final product. You need to analyze
 *          how your product is supposed to react in case of Assert.
 * @warning On assert from the SoftDevice, the system can only recover on reset.
 *
 * @param[in] line_num    Line number of the failing ASSERT call.
 * @param[in] p_file_name File name of the failing ASSERT call.
 */
void assert_nrf_callback(uint16_t line_num, const uint8_t *p_file_name)
{
        app_error_handler(DEAD_BEEF, line_num, p_file_name);
}

/**@brief Function for the LEDs initialization.
 *
 * @details Initializes all LEDs used by the application.
 */
static void leds_init(void)
{
        bsp_board_init(BSP_INIT_LEDS);
}

static void restart_advertising_callback(void *p)
{
        advertising_start();
}

/**@brief Function for the Timer initialization.
 *
 * @details Initializes the timer module.
 */
static void timers_init(void)
{
        // Initialize timer module, making it use the scheduler
        ret_code_t err_code = app_timer_init();
        APP_ERROR_CHECK(err_code);

        err_code = app_timer_create(&m_restart_advertising_timer_id, APP_TIMER_MODE_SINGLE_SHOT, restart_advertising_callback);
        APP_ERROR_CHECK(err_code);

        err_code = app_timer_create(&m_connection_one_sec_timer_id, APP_TIMER_MODE_REPEATED, connection_one_second_timeout_handler);
        APP_ERROR_CHECK(err_code);
}

/**@brief Function for changing the tx power.
 */
static void tx_power_set(void)
{

        // ret_code_t err_code = sd_ble_gap_tx_power_set(BLE_GAP_TX_POWER_ROLE_CONN, m_conn_handle, TX_POWER_LEVEL);
        ret_code_t err_code = sd_ble_gap_tx_power_set(BLE_GAP_TX_POWER_ROLE_CONN, m_conn_handle, m_application_state.tx_power * 4);
        APP_ERROR_CHECK(err_code);
}

/**@brief Function for the GAP initialization.
 *
 * @details This function sets up all the necessary GAP (Generic Access Profile) parameters of the
 *          device including the device name, appearance, and the preferred connection parameters.
 */
static void gap_params_init(void)
{
        ret_code_t err_code;
        ble_gap_conn_params_t gap_conn_params;
        ble_gap_conn_sec_mode_t sec_mode;

        BLE_GAP_CONN_SEC_MODE_SET_OPEN(&sec_mode);

        err_code = sd_ble_gap_device_name_set(&sec_mode,
                                              (const uint8_t *)DEVICE_NAME,
                                              strlen(DEVICE_NAME));
        APP_ERROR_CHECK(err_code);

        memset(&gap_conn_params, 0, sizeof(gap_conn_params));

        gap_conn_params.min_conn_interval = MIN_CONN_INTERVAL;
        gap_conn_params.max_conn_interval = MAX_CONN_INTERVAL;
        gap_conn_params.slave_latency = SLAVE_LATENCY;
        gap_conn_params.conn_sup_timeout = CONN_SUP_TIMEOUT;

        err_code = sd_ble_gap_ppcp_set(&gap_conn_params);
        APP_ERROR_CHECK(err_code);

        ble_gap_addr_t ble_address = {.addr_type = BLE_GAP_ADDR_TYPE_RANDOM_STATIC,
                                      .addr_id_peer = 0,
                                      .addr = {0xC3, 0x11, 0x99, 0x33, 0x44, 0xFF}};
        err_code = sd_ble_gap_addr_set(&ble_address);
        APP_ERROR_CHECK(err_code);
}

static void its_data_handler(ble_its_t *p_its, uint8_t const *p_data, uint16_t length)
{

}

/**@brief Function for handling the data from the Nordic UART Service.
 *
 * @details This function will process the data received from the Nordic UART BLE Service and send
 *          it to the UART module.
 *
 * @param[in] p_evt       Nordic UART Service event.
 */
/**@snippet [Handling the data received over BLE] */
static void nus_data_handler(ble_nus_evt_t *p_evt)
{
        if (p_evt->type == BLE_NUS_EVT_RX_DATA)
        {
                uint32_t err_code;

//                if (p_evt->params.rx_data.length == )

                if (p_evt->params.rx_data.p_data[0] == '#')
                {
                        static uint8_t string_per[BLE_NUS_MAX_DATA_LEN];
                        memcpy(string_per, p_evt->params.rx_data.p_data, p_evt->params.rx_data.length);
                        NRF_LOG_INFO("=============================");
                        NRF_LOG_INFO("Accumulated Packet Error Rate Result");
                        NRF_LOG_INFO("%s", string_per);
                        NRF_LOG_INFO("=============================");
                }
                else
                {
                        uint16_t packet_error_rate = p_evt->params.rx_data.p_data[0];
                        NRF_LOG_INFO("Length = %d, PER = %d %%", p_evt->params.rx_data.length, packet_error_rate);
                        //m_application_state.rssi[p_ble_evt->evt.gap_evt.conn_handle] = p_ble_evt->evt.gap_evt.params.rssi_changed.rssi;
                        m_application_state.per = packet_error_rate;
                }



                for (uint32_t i = 0; i < p_evt->params.rx_data.length; i++)
                {
                        do
                        {
                                err_code = app_uart_put(p_evt->params.rx_data.p_data[i]);
                                if ((err_code != NRF_SUCCESS) && (err_code != NRF_ERROR_BUSY))
                                {
                                        NRF_LOG_ERROR("Failed receiving NUS message. Error 0x%x. ", err_code);
                                        APP_ERROR_CHECK(err_code);
                                }
                        } while (err_code == NRF_ERROR_BUSY);
                }
                if (p_evt->params.rx_data.p_data[p_evt->params.rx_data.length - 1] == '\r')
                {
                        while (app_uart_put('\n') == NRF_ERROR_BUSY);
                }
        }
        else if (p_evt->type == BLE_NUS_EVT_COMM_STARTED)
        {
                NRF_LOG_INFO("NUS : BLE_NUS_EVT_COMM_STARTED");
        }
        else if (p_evt->type == BLE_NUS_EVT_COMM_STOPPED)
        {
                NRF_LOG_INFO("NUS : BLE_NUS_EVT_COMM_STOPPED");
        }
}
/**@snippet [Handling the data received over BLE] */

/**@brief Function for handling events from the GATT library. */
void gatt_evt_handler(nrf_ble_gatt_t *p_gatt, nrf_ble_gatt_evt_t const *p_evt)
{
        uint32_t data_length;
        if ((m_conn_handle == p_evt->conn_handle) && (p_evt->evt_id == NRF_BLE_GATT_EVT_ATT_MTU_UPDATED))
        {
                data_length = p_evt->params.att_mtu_effective - OPCODE_LENGTH - HANDLE_LENGTH;
                //m_ble_params_info.mtu = m_ble_its_max_data_len;
                m_ble_its_max_data_len = data_length;
                m_ble_nus_max_data_len = data_length;
                NRF_LOG_INFO("gatt_event: ATT MTU is set to 0x%X (%d)", data_length, data_length);
        }
        else if ((m_conn_handle == p_evt->conn_handle) && (p_evt->evt_id == NRF_BLE_GATT_EVT_DATA_LENGTH_UPDATED))
        {
                data_length = p_evt->params.att_mtu_effective - OPCODE_LENGTH - HANDLE_LENGTH - 4;
                NRF_LOG_INFO("gatt_event: Data len is set to 0x%X (%d)", data_length, data_length);
                m_ble_its_max_data_len = data_length;
                m_ble_nus_max_data_len = data_length;
                m_ble_params_info.mtu = m_ble_its_max_data_len;
                ble_its_ble_params_info_send(&m_its, &m_ble_params_info);
        }
        NRF_LOG_DEBUG("ATT MTU exchange completed. central 0x%x peripheral 0x%x",
                      p_gatt->att_mtu_desired_central,
                      p_gatt->att_mtu_desired_periph);
}

/**@brief Function for initializing the GATT module.
 */
static void gatt_init(void)
{
        ret_code_t err_code;

        err_code = nrf_ble_gatt_init(&m_gatt, gatt_evt_handler);
        APP_ERROR_CHECK(err_code);

        err_code = nrf_ble_gatt_att_mtu_periph_set(&m_gatt, NRF_SDH_BLE_GATT_MAX_MTU_SIZE);
        APP_ERROR_CHECK(err_code);

        err_code = nrf_ble_gatt_data_length_set(&m_gatt, BLE_CONN_HANDLE_INVALID, NRF_SDH_BLE_GAP_DATA_LENGTH);
        APP_ERROR_CHECK(err_code);
}

/**@brief Function for initializing the Advertising functionality.
 *
 * @details Encodes the required advertising data and passes it to the stack.
 *          Also builds a structure to be passed to the stack when starting advertising.
 */
static void advertising_init(void)
{
        ret_code_t err_code;
        ble_advdata_t advdata;
        ble_advdata_t srdata;

        ble_uuid_t adv_uuids[] = {{LBS_UUID_SERVICE, m_lbs.uuid_type}};

        // Build and set advertising data.
        memset(&advdata, 0, sizeof(advdata));

        advdata.name_type = BLE_ADVDATA_FULL_NAME;
        advdata.include_appearance = false;
        advdata.flags = BLE_GAP_ADV_FLAGS_LE_ONLY_GENERAL_DISC_MODE;

        memset(&srdata, 0, sizeof(srdata));

        // srdata.uuids_complete.uuid_cnt = sizeof(adv_uuids) / sizeof(adv_uuids[0]);
        // srdata.uuids_complete.p_uuids  = adv_uuids;

        err_code = ble_advdata_encode(&advdata, m_adv_data.adv_data.p_data, &m_adv_data.adv_data.len);
        APP_ERROR_CHECK(err_code);

        err_code = ble_advdata_encode(&srdata, m_adv_data.scan_rsp_data.p_data, &m_adv_data.scan_rsp_data.len);
        APP_ERROR_CHECK(err_code);

        ble_gap_adv_params_t adv_params;

        // Set advertising parameters.
        memset(&adv_params, 0, sizeof(adv_params));

        if (m_application_state.phy == APP_PHY_CODED || m_application_state.phy == APP_PHY_MULTI)
        {
                adv_params.primary_phy = BLE_GAP_PHY_CODED;
                adv_params.secondary_phy = BLE_GAP_PHY_CODED;
                adv_params.properties.type = BLE_GAP_ADV_TYPE_EXTENDED_CONNECTABLE_NONSCANNABLE_UNDIRECTED;
        }
        else
        {
                adv_params.primary_phy = BLE_GAP_PHY_1MBPS;
                adv_params.secondary_phy = BLE_GAP_PHY_1MBPS;
                adv_params.properties.type = BLE_GAP_ADV_TYPE_CONNECTABLE_SCANNABLE_UNDIRECTED;
        }

        adv_params.duration = APP_ADV_DURATION;
        adv_params.p_peer_addr = NULL;
        adv_params.filter_policy = BLE_GAP_ADV_FP_ANY;
        adv_params.interval = APP_ADV_INTERVAL;

        err_code = sd_ble_gap_adv_set_configure(&m_adv_handle, &m_adv_data, &adv_params);
        APP_ERROR_CHECK(err_code);

        err_code = sd_ble_gap_tx_power_set(BLE_GAP_TX_POWER_ROLE_ADV, m_adv_handle, m_application_state.tx_power * 4);
        APP_ERROR_CHECK(err_code);
}

/**@brief Function for handling Queued Write Module errors.
 *
 * @details A pointer to this function will be passed to each service which may need to inform the
 *          application about an error.
 *
 * @param[in]   nrf_error   Error code containing information about what went wrong.
 */
static void nrf_qwr_error_handler(uint32_t nrf_error)
{
        APP_ERROR_HANDLER(nrf_error);
}

/**@brief Function for handling write events to the LED characteristic.
 *
 * @param[in] p_lbs     Instance of LED Button Service to which the write applies.
 * @param[in] led_state Written/desired state of the LED.
 */
static void led_write_handler(uint16_t conn_handle, ble_lbs_t *p_lbs, uint8_t led_state)
{
        uint32_t err_code;
        if (led_state)
        {
                bsp_board_led_on(LEDBUTTON_LED);
                NRF_LOG_INFO("Received LED ON!");

                // Get the request fro central in order to start the throughput test
                if (start_thoughput_test)
                {
                        start_thoughput_test = false;
                        data_send = 0;
                }
                else
                {
                        start_thoughput_test = true;
                        start_time = app_timer_cnt_get();
                        finish_time = 0;
                        data_send = 0;
                        NRF_LOG_INFO("Time of starting transfer = %d", start_time);
                        test_thoughput();
                        if (m_connection_one_sec_timer_is_running == false)
                        {
                                err_code = app_timer_start(m_connection_one_sec_timer_id, CONNECTION_ONE_SECOND_INTERVAL, NULL);
                                APP_ERROR_CHECK(err_code);
                                m_connection_one_sec_timer_is_running = true;
                        }
                }
        }
        else
        {
                bsp_board_led_off(LEDBUTTON_LED);
                NRF_LOG_INFO("Received LED OFF!");
        }
}
/**@brief   Function for handling app_uart events.
 *
 * @details This function will receive a single character from the app_uart module and append it to
 *          a string. The string will be be sent over BLE when the last character received was a
 *          'new line' '\n' (hex 0x0A) or if the string has reached the maximum data length.
 */
/**@snippet [Handling the data received over UART] */
void uart_event_handle(app_uart_evt_t *p_event)
{
        static uint8_t data_array[BLE_NUS_MAX_DATA_LEN];
        static uint8_t index = 0;
        uint32_t err_code;

        switch (p_event->evt_type)
        {
        case APP_UART_DATA_READY:
                UNUSED_VARIABLE(app_uart_get(&data_array[index]));
                index++;

                if ((data_array[index - 1] == '\n') ||
                    (data_array[index - 1] == '\r') ||
                    (index >= m_ble_nus_max_data_len))
                {
                        if (index > 1)
                        {
                                NRF_LOG_DEBUG("Ready to send data over BLE NUS");
                                NRF_LOG_HEXDUMP_DEBUG(data_array, index);
                                do
                                {
                                        uint16_t length = (uint16_t)index;
                                        err_code = ble_nus_data_send(&m_nus, data_array, &length, m_conn_handle);
                                        if ((err_code != NRF_ERROR_INVALID_STATE) &&
                                            (err_code != NRF_ERROR_RESOURCES) &&
                                            (err_code != NRF_ERROR_NOT_FOUND))
                                        {
                                                APP_ERROR_CHECK(err_code);
                                        }
                                } while (err_code == NRF_ERROR_RESOURCES);
                        }
                }
                break;

        case APP_UART_COMMUNICATION_ERROR:
                APP_ERROR_HANDLER(p_event->data.error_communication);
                break;

        case APP_UART_FIFO_ERROR:
                APP_ERROR_HANDLER(p_event->data.error_code);
                break;

        default:
                break;
        }
}

/**@brief  Function for initializing the UART module.
 */
/**@snippet [UART Initialization] */
static void uart_init(void)
{
        uint32_t err_code;
        app_uart_comm_params_t const comm_params =
        {
                .rx_pin_no = RX_PIN_NUMBER,
                .tx_pin_no = TX_PIN_NUMBER,
                .rts_pin_no = RTS_PIN_NUMBER,
                .cts_pin_no = CTS_PIN_NUMBER,
                .flow_control = APP_UART_FLOW_CONTROL_DISABLED,
                .use_parity = false,
#if defined(UART_PRESENT)
                .baud_rate = NRF_UART_BAUDRATE_115200
#else
                .baud_rate = NRF_UARTE_BAUDRATE_115200
#endif
        };

        APP_UART_FIFO_INIT(&comm_params,
                           UART_RX_BUF_SIZE,
                           UART_TX_BUF_SIZE,
                           uart_event_handle,
                           APP_IRQ_PRIORITY_LOWEST,
                           err_code);
        APP_ERROR_CHECK(err_code);
}
/**@snippet [UART Initialization] */

/**@brief Function for initializing services that will be used by the application.
 */
static void services_init(void)
{
        ret_code_t err_code;
        ble_lbs_init_t init = {0};
        nrf_ble_qwr_init_t qwr_init = {0};
        ble_nus_init_t nus_init;
        ble_its_init_t its_init;

        // Initialize Queued Write Module.
        qwr_init.error_handler = nrf_qwr_error_handler;

        err_code = nrf_ble_qwr_init(&m_qwr, &qwr_init);
        APP_ERROR_CHECK(err_code);

        // Initialize LBS.
        init.led_write_handler = led_write_handler;

        err_code = ble_lbs_init(&m_lbs, &init);
        APP_ERROR_CHECK(err_code);

        // Initialize NUS.
        memset(&nus_init, 0, sizeof(nus_init));

        nus_init.data_handler = nus_data_handler;

        err_code = ble_nus_init(&m_nus, &nus_init);

        APP_ERROR_CHECK(err_code);

        // Initialize ITS.
        memset(&its_init, 0, sizeof(its_init));

        its_init.data_handler = its_data_handler;

        err_code = ble_its_init(&m_its, &its_init);
        APP_ERROR_CHECK(err_code);
}

/**@brief Function for handling the Connection Parameters Module.
 *
 * @details This function will be called for all events in the Connection Parameters Module that
 *          are passed to the application.
 *
 * @note All this function does is to disconnect. This could have been done by simply
 *       setting the disconnect_on_fail config parameter, but instead we use the event
 *       handler mechanism to demonstrate its use.
 *
 * @param[in] p_evt  Event received from the Connection Parameters Module.
 */
static void on_conn_params_evt(ble_conn_params_evt_t *p_evt)
{
        ret_code_t err_code;

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
        ret_code_t err_code;
        ble_conn_params_init_t cp_init;

        memset(&cp_init, 0, sizeof(cp_init));

        cp_init.p_conn_params = NULL;
        cp_init.first_conn_params_update_delay = FIRST_CONN_PARAMS_UPDATE_DELAY;
        cp_init.next_conn_params_update_delay = NEXT_CONN_PARAMS_UPDATE_DELAY;
        cp_init.max_conn_params_update_count = MAX_CONN_PARAMS_UPDATE_COUNT;
        cp_init.start_on_notify_cccd_handle = BLE_GATT_HANDLE_INVALID;
        cp_init.disconnect_on_fail = false;
        cp_init.evt_handler = on_conn_params_evt;
        cp_init.error_handler = conn_params_error_handler;

        err_code = ble_conn_params_init(&cp_init);
        APP_ERROR_CHECK(err_code);
}

/**@brief Function for starting advertising.
 */
static void advertising_start(void)
{
        ret_code_t err_code;

        err_code = sd_ble_gap_adv_start(m_adv_handle, APP_BLE_CONN_CFG_TAG);
        if (err_code == NRF_SUCCESS)
        {
                bsp_board_led_on(ADVERTISING_LED);

                m_application_state.app_state = APP_STATE_ADVERTISING;
                display_update();
        }
        else
        {
                APP_ERROR_CHECK(err_code);
        }
}

static void ble_go_to_idle(void)
{
        ret_code_t err_code;
        switch (m_application_state.app_state)
        {
        case APP_STATE_ADVERTISING:
                err_code = sd_ble_gap_adv_stop(m_adv_handle);
                APP_ERROR_CHECK(err_code);
                break;

        case APP_STATE_CONNECTED:
        {
                if (m_conn_handle != BLE_CONN_HANDLE_INVALID)
                {
                        err_code = sd_ble_gap_disconnect(m_conn_handle, BLE_HCI_REMOTE_USER_TERMINATED_CONNECTION);
                        APP_ERROR_CHECK(err_code);
                }
        }
        break;

        case APP_STATE_DISCONNECTED:
                err_code = app_timer_stop(m_restart_advertising_timer_id);
                APP_ERROR_CHECK(err_code);
                break;
        }
        m_application_state.app_state = APP_STATE_IDLE;
        display_update();
}

static void request_phy(uint16_t c_handle, uint8_t phy)
{
        ble_gap_phys_t phy_req;
        phy_req.tx_phys = phy;
        phy_req.rx_phys = phy;
        sd_ble_gap_phy_update(c_handle, &phy_req);
        NRF_LOG_INFO("Request to do the PHY update");
}

/**@brief Function for handling BLE events.
 *
 * @param[in]   p_ble_evt   Bluetooth stack event.
 * @param[in]   p_context   Unused.
 */
static void ble_evt_handler(ble_evt_t const *p_ble_evt, void *p_context)
{
        ret_code_t err_code;

        switch (p_ble_evt->header.evt_id)
        {
        case BLE_GAP_EVT_CONNECTED:

                NRF_LOG_INFO("Connected");
                bsp_board_led_on(CONNECTED_LED);
                bsp_board_led_off(ADVERTISING_LED);

                m_conn_handle = p_ble_evt->evt.gap_evt.conn_handle;
                err_code = nrf_ble_qwr_conn_handle_assign(&m_qwr, m_conn_handle);
                APP_ERROR_CHECK(err_code);
                err_code = app_button_enable();
                APP_ERROR_CHECK(err_code);
                NRF_LOG_INFO("Actual connection parameter %d", p_ble_evt->evt.gap_evt.params.connected.conn_params.max_conn_interval);

                tx_power_set();

                err_code = sd_ble_gap_rssi_start(m_conn_handle, 1, 2);
                APP_ERROR_CHECK(err_code);

                if (m_application_state.phy == APP_PHY_2M)
                {
                        request_phy(0, BLE_GAP_PHY_2MBPS);
                }

                m_application_state.app_state = APP_STATE_CONNECTED;
                display_update();

                start_thoughput_test = false;

                break;

        case BLE_GAP_EVT_DISCONNECTED:
                NRF_LOG_INFO("Disconnected");
                bsp_board_led_off(CONNECTED_LED);
                m_conn_handle = BLE_CONN_HANDLE_INVALID;
                err_code = app_button_disable();
                APP_ERROR_CHECK(err_code);
                advertising_start();

                start_thoughput_test = false;
                break;

        case BLE_GAP_EVT_CONN_PARAM_UPDATE:
        {
                uint16_t max_con_int = p_ble_evt->evt.gap_evt.params.conn_param_update.conn_params.max_conn_interval;
                uint16_t min_con_int = p_ble_evt->evt.gap_evt.params.conn_param_update.conn_params.min_conn_interval;

                m_ble_params_info.con_interval = max_con_int;
                ble_its_ble_params_info_send(&m_its, &m_ble_params_info);
                NRF_LOG_INFO("Con params updated: CI %i, %i", (int)min_con_int, (int)max_con_int);
        }
        break;

        case BLE_GAP_EVT_SEC_PARAMS_REQUEST:
                // Pairing not supported
                err_code = sd_ble_gap_sec_params_reply(m_conn_handle,
                                                       BLE_GAP_SEC_STATUS_PAIRING_NOT_SUPP,
                                                       NULL,
                                                       NULL);
                APP_ERROR_CHECK(err_code);
                break;

        case BLE_GAP_EVT_PHY_UPDATE_REQUEST:
        {
                NRF_LOG_DEBUG("PHY update request.");
                ble_gap_phys_t const phys =
                {
                        .rx_phys = BLE_GAP_PHY_AUTO,
                        .tx_phys = BLE_GAP_PHY_AUTO,
                };
                err_code = sd_ble_gap_phy_update(p_ble_evt->evt.gap_evt.conn_handle, &phys);
                APP_ERROR_CHECK(err_code);
        }
        break;

        case BLE_GATTS_EVT_SYS_ATTR_MISSING:
                // No system attributes have been stored.
                err_code = sd_ble_gatts_sys_attr_set(m_conn_handle, NULL, 0, 0);
                APP_ERROR_CHECK(err_code);
                break;

        case BLE_GATTC_EVT_TIMEOUT:
                // Disconnect on GATT Client timeout event.
                NRF_LOG_DEBUG("GATT Client Timeout.");
                err_code = sd_ble_gap_disconnect(p_ble_evt->evt.gattc_evt.conn_handle,
                                                 BLE_HCI_REMOTE_USER_TERMINATED_CONNECTION);
                APP_ERROR_CHECK(err_code);
                break;

        case BLE_GATTS_EVT_TIMEOUT:
                // Disconnect on GATT Server timeout event.
                NRF_LOG_DEBUG("GATT Server Timeout.");
                err_code = sd_ble_gap_disconnect(p_ble_evt->evt.gatts_evt.conn_handle,
                                                 BLE_HCI_REMOTE_USER_TERMINATED_CONNECTION);
                APP_ERROR_CHECK(err_code);
                break;

        case BLE_EVT_USER_MEM_REQUEST:
                err_code = sd_ble_user_mem_reply(p_ble_evt->evt.gattc_evt.conn_handle, NULL);
                APP_ERROR_CHECK(err_code);
                break;

        case BLE_GATTS_EVT_RW_AUTHORIZE_REQUEST:
        {
                ble_gatts_evt_rw_authorize_request_t req;
                ble_gatts_rw_authorize_reply_params_t auth_reply;

                req = p_ble_evt->evt.gatts_evt.params.authorize_request;

                if (req.type != BLE_GATTS_AUTHORIZE_TYPE_INVALID)
                {
                        if ((req.request.write.op == BLE_GATTS_OP_PREP_WRITE_REQ) ||
                            (req.request.write.op == BLE_GATTS_OP_EXEC_WRITE_REQ_NOW) ||
                            (req.request.write.op == BLE_GATTS_OP_EXEC_WRITE_REQ_CANCEL))
                        {
                                if (req.type == BLE_GATTS_AUTHORIZE_TYPE_WRITE)
                                {
                                        auth_reply.type = BLE_GATTS_AUTHORIZE_TYPE_WRITE;
                                }
                                else
                                {
                                        auth_reply.type = BLE_GATTS_AUTHORIZE_TYPE_READ;
                                }
                                auth_reply.params.write.gatt_status = APP_FEATURE_NOT_SUPPORTED;
                                err_code = sd_ble_gatts_rw_authorize_reply(p_ble_evt->evt.gatts_evt.conn_handle,
                                                                           &auth_reply);
                                APP_ERROR_CHECK(err_code);
                        }
                }
        }
        break; // BLE_GATTS_EVT_RW_AUTHORIZE_REQUEST

        case BLE_GAP_EVT_RSSI_CHANGED:
                m_application_state.rssi[p_ble_evt->evt.gap_evt.conn_handle] = p_ble_evt->evt.gap_evt.params.rssi_changed.rssi;
                //                display_update();
                break;

        case BLE_GATTS_EVT_HVN_TX_COMPLETE:
                if (start_thoughput_test)
                {
                        //                      NRF_LOG_INFO("%x", p_ble_evt->evt.gatts_evt.conn_handle);
                        test_thoughput();
                }

                break;

        default:
                // No implementation needed.
                break;
        }
}

static void display_update()
{
        static bool first_update = true;
        if (first_update)
        {
                m_application_state.main_title = "Throughput (PER)";
                app_display_create_main_screen(&m_application_state);
                first_update = false;
        }
        else
        {
                app_display_update_main_screen(&m_application_state);
        }
        app_display_update();
}

/**@brief Function for the Event Scheduler initialization.
 */
static void scheduler_init(void)
{
        APP_SCHED_INIT(SCHED_MAX_EVENT_DATA_SIZE, SCHED_QUEUE_SIZE);
}

/**@brief Function for initializing the BLE stack.
 *
 * @details Initializes the SoftDevice and the BLE event interrupt.
 */
static void ble_stack_init(void)
{
        ret_code_t err_code;

        err_code = nrf_sdh_enable_request();
        APP_ERROR_CHECK(err_code);

        // Configure the BLE stack using the default settings.
        // Fetch the start address of the application RAM.
        uint32_t ram_start = 0;
        err_code = nrf_sdh_ble_default_cfg_set(APP_BLE_CONN_CFG_TAG, &ram_start);
        APP_ERROR_CHECK(err_code);

        // Enable BLE stack.
        err_code = nrf_sdh_ble_enable(&ram_start);
        APP_ERROR_CHECK(err_code);

        err_code = sd_power_mode_set(NRF_POWER_MODE_LOWPWR);
        APP_ERROR_CHECK(err_code);

        err_code = sd_power_dcdc_mode_set(NRF_POWER_DCDC_ENABLE);
        APP_ERROR_CHECK(err_code);

        // Register a handler for BLE events.
        NRF_SDH_BLE_OBSERVER(m_ble_observer, APP_BLE_OBSERVER_PRIO, ble_evt_handler, NULL);
}

/**@brief Function for handling events from the button handler module.
 *
 * @param[in] pin_no        The pin that the event applies to.
 * @param[in] button_action The button action (press/release).
 */
static void button_event_handler(uint8_t pin_no, uint8_t button_action)
{
        ret_code_t err_code;

        switch (pin_no)
        {
        case PHY_BUTTON:
                if (button_action == APP_BUTTON_PUSH && m_application_state.app_state == APP_STATE_IDLE)
                {
                        m_application_state.phy = (m_application_state.phy + 1) % APP_PHY_LIST_END;
                        advertising_init();
                        display_update();
                }
                break;

        case TX_POWER_BUTTON:
                if (button_action == APP_BUTTON_PUSH)
                {
                        m_application_state.tx_power = (m_application_state.tx_power + 1) % 3;
                        int8_t tx_power = (int8_t)(m_application_state.tx_power * 4);
                        if (m_application_state.app_state == APP_STATE_CONNECTED)
                        {
                                err_code = sd_ble_gap_tx_power_set(BLE_GAP_TX_POWER_ROLE_CONN, m_conn_handle, tx_power);
                                APP_ERROR_CHECK(err_code);
                        }
                        else
                        {
                                err_code = sd_ble_gap_tx_power_set(BLE_GAP_TX_POWER_ROLE_ADV, m_adv_handle, tx_power);
                                APP_ERROR_CHECK(err_code);
                        }
                        display_update();
                }
                break;

        case APP_STATE_BUTTON:
                if (button_action == APP_BUTTON_PUSH)
                {
                        if (m_application_state.app_state == APP_STATE_IDLE)
                        {
                                advertising_start();
                        }
                        else
                        {
                                ble_go_to_idle();
                        }
                }
                break;
        case LEDBUTTON_BUTTON:
                if (button_action == APP_BUTTON_PUSH)
                {
                        uint16_t length = 5;
                        uint8_t data_array[20];
                        sprintf(data_array,"start");
                        err_code = ble_nus_data_send(&m_nus, data_array, &length, m_conn_handle);
                        if ((err_code != NRF_ERROR_INVALID_STATE) &&
                            (err_code != NRF_ERROR_RESOURCES) &&
                            (err_code != NRF_ERROR_NOT_FOUND))
                        {
                                APP_ERROR_CHECK(err_code);
                        }
                }
                break;

        default:
                APP_ERROR_HANDLER(pin_no);
                break;
        }
}

/**@brief Function for initializing the button handler module.
 */
static void buttons_init(void)
{
        ret_code_t err_code;

        //The array must be static because a pointer to it will be saved in the button handler module.
        static app_button_cfg_t buttons[] =
        {
                {PHY_BUTTON, false, BUTTON_PULL, button_event_handler},
                {TX_POWER_BUTTON, false, BUTTON_PULL, button_event_handler},
                {APP_STATE_BUTTON, false, BUTTON_PULL, button_event_handler},
                {LEDBUTTON_BUTTON, false, BUTTON_PULL, button_event_handler}
        };

        err_code = app_button_init(buttons, ARRAY_SIZE(buttons),
                                   BUTTON_DETECTION_DELAY);
        APP_ERROR_CHECK(err_code);
}

static void log_init(void)
{
        ret_code_t err_code = NRF_LOG_INIT(NULL);
        APP_ERROR_CHECK(err_code);

        NRF_LOG_DEFAULT_BACKENDS_INIT();
}

/**@brief Function for initializing power management.
 */
static void power_management_init(void)
{
        ret_code_t err_code;
        err_code = nrf_pwr_mgmt_init();
        APP_ERROR_CHECK(err_code);
}

/**@brief Function for handling the idle state (main loop).
 *
 * @details If there is no pending log operation, then sleep until next the next event occurs.
 */
static void idle_state_handle(void)
{
        app_sched_execute();
        while (NRF_LOG_PROCESS())
                ;
        nrf_pwr_mgmt_run();
}

/**@brief Function for application main entry.
 */
int main(void)
{

        bool erase_bonds;
        ret_code_t err_code;

        /* enable instruction cache */
        NRF_NVMC->ICACHECNF = (NVMC_ICACHECNF_CACHEEN_Enabled << NVMC_ICACHECNF_CACHEEN_Pos) +
                              (NVMC_ICACHECNF_CACHEPROFEN_Disabled << NVMC_ICACHECNF_CACHEPROFEN_Pos);

        // Initialize.
        log_init();
        uart_init();
        leds_init();
        timers_init();
        buttons_init();
        power_management_init();
        ble_stack_init();
        scheduler_init();
        gap_params_init();
        gatt_init();
        services_init();
        advertising_init();
        conn_params_init();

        // Start execution.
        NRF_LOG_INFO("Packet Error Rate : Peripheral LBS + NUS.");

        app_display_init(&m_application_state);
        display_update();

        err_code = app_button_enable();
        APP_ERROR_CHECK(err_code);

#ifdef DEBUG_GPIO_SEND_COUNT
        nrf_gpio_cfg_output(GPIO_SEND_COUNT_PIN);
        nrf_gpio_pin_clear(GPIO_SEND_COUNT_PIN);
#endif 
        // Enter main loop.
        for (;;)
        {
                idle_state_handle();
        }
}

/**
 * @}
 */
