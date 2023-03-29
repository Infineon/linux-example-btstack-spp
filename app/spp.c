/******************************************************************************
 * (c) 2020, Cypress Semiconductor Corporation. All rights reserved.
 *******************************************************************************
 * This software, including source code, documentation and related materials
 * ("Software"), is owned by Cypress Semiconductor Corporation or one of its
 * subsidiaries ("Cypress") and is protected by and subject to worldwide patent
 * protection (United States and foreign), United States copyright laws and
 * international treaty provisions. Therefore, you may use this Software only
 * as provided in the license agreement accompanying the software package from
 * which you obtained this Software ("EULA").
 *
 * If no EULA applies, Cypress hereby grants you a personal, non-exclusive,
 * non-transferable license to copy, modify, and compile the Software source
 * code solely for use in connection with Cypress's integrated circuit products.
 * Any reproduction, modification, translation, compilation, or representation
 * of this Software except as specified above is prohibited without the express
 * written permission of Cypress.
 *
 * Disclaimer: THIS SOFTWARE IS PROVIDED AS-IS, WITH NO WARRANTY OF ANY KIND,
 * EXPRESS OR IMPLIED, INCLUDING, BUT NOT LIMITED TO, NONINFRINGEMENT, IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE. Cypress
 * reserves the right to make changes to the Software without notice. Cypress
 * does not assume any liability arising out of the application or use of the
 * Software or any product or circuit described in the Software. Cypress does
 * not authorize its products for use in any products where a malfunction or
 * failure of the Cypress product may reasonably be expected to result in
 * significant property damage, injury or death ("High Risk Product"). By
 * including Cypress's product in a High Risk Product, the manufacturer of such
 * system or application assumes all risk of such use and in doing so agrees to
 * indemnify Cypress against all liability.
 *****************************************************************************/
/******************************************************************************
 * File Name: spp.c
 *
 * Description: This is the source file for SPP CE.
 *
 * Related Document: See README.md
 *
 *****************************************************************************/

/*******************************************************************************
 *      INCLUDES
 *******************************************************************************/
#include "wiced_bt_stack.h"
#include <string.h>
#include <stdlib.h>
#include "wiced_memory.h"
#include "stdio.h"
#include "wiced_bt_dev.h"
#include "wiced_memory.h"
#include "wiced_hal_nvram.h"
#include "spp.h"
#include "wiced_spp_int.h"
#include "wiced_bt_sdp.h"
#include "wiced_timer.h"

/*******************************************************************************
 *       MACROS
 ******************************************************************************/
#define BT_STACK_HEAP_SIZE (0xF000)
#define CASE_RETURN_STR(enum_val) \
    case enum_val:                \
        return #enum_val;
#define MAX_TX_BUFFER (1017)
#define SPP_NVRAM_ID WICED_NVRAM_VSID_START
#define WICED_EIR_BUF_MAX_SIZE (264)
#define SPP_TOTAL_DATA_TO_SEND (10000)
#define MAX_TX_RETRY (30)
#define TX_RETRY_TIMEOUT (100) /* msec */

/*******************************************************************************
 *       VARIABLE DEFINITIONS
 ******************************************************************************/
extern const wiced_bt_cfg_settings_t wiced_bt_cfg_settings;
extern const uint8_t sdp_database[];
wiced_bt_heap_t *p_default_heap = NULL;
uint32_t spp_rx_bytes = 0;
uint8_t pincode[4] = {0x30, 0x30, 0x30, 0x30};
int spp_send_offset = 0;
uint32_t time_start = 0;
wiced_timer_t spp_tx_timer;
uint32_t spp_tx_retry_count = 0;
uint8_t spp_send_buffer[SPP_MAX_PAYLOAD];
uint16_t spp_handle = 0;

/*******************************************************************************
 *       FUNCTION PROTOTYPES
 ******************************************************************************/
static const char *spp_get_bt_event_name(wiced_bt_management_evt_t event);
static void spp_print_bd_address(wiced_bt_device_address_t bdadr);
static wiced_result_t spp_management_callback(
    wiced_bt_management_evt_t event,
    wiced_bt_management_evt_data_t *p_event_data);
static void spp_write_eir(void);
static void spp_init(void);
static void spp_connection_up_callback(uint16_t handle, uint8_t *bda);
static void spp_connection_down_callback(uint16_t handle);
static wiced_bool_t spp_rx_data_callback(uint16_t handle, uint8_t *p_data, uint32_t data_len);
extern uint16_t wiced_app_cfg_sdp_record_get_size(void);
static int spp_write_nvram(int nvram_id, int data_len, void *p_data);
static int spp_read_nvram(int nvram_id, void *p_data, int data_len);
static void spp_tx_ack_timeout(WICED_TIMER_PARAM_TYPE arg);

/*******************************************************************************
 *       STRUCTURES AND ENUMERATIONS
 ******************************************************************************/
wiced_bt_spp_reg_t spp_reg =
    {
        SPP_RFCOMM_SCN,               /* RFCOMM service channel number for
                                         SPP connection */
        MAX_TX_BUFFER,                /* RFCOMM MTU for SPP connection */
        spp_connection_up_callback,   /* SPP connection established */
        NULL,                         /* SPP connection establishment failed,
                                         not used because this app never
                                         initiates connection */
        NULL,                         /* SPP service not found, not used
                                         because this app never initiates
                                         connection */
        spp_connection_down_callback, /* SPP connection disconnected */
        spp_rx_data_callback,         /* Data packet received */
};

/*******************************************************************************
 *       FUNCTION DEFINITION
 ******************************************************************************/

/*******************************************************************************
 * Function Name: spp_application_start
 *******************************************************************************
 * Summary:
 *  Set device configuration and start BT stack initialization. The actual
 *  application initialization will happen when stack reports that BT device
 *  is ready.
 *
 * Parameters: NONE
 *
 * Return: NONE
 *
 ******************************************************************************/
void spp_application_start()
{
    wiced_result_t wiced_result;

    WICED_BT_TRACE("************* SPP Application Start ************************\n");

    /* Register call back and configuration with stack */
    wiced_result = wiced_bt_stack_init(spp_management_callback, &wiced_bt_cfg_settings);

    /* Check if stack initialization was successful */
    if (WICED_BT_SUCCESS == wiced_result)
    {
        WICED_BT_TRACE("Bluetooth Stack Initialization Successful \n");
        /* Create default heap */
        p_default_heap = wiced_bt_create_heap("default_heap", NULL, BT_STACK_HEAP_SIZE, NULL, WICED_TRUE);
        if (p_default_heap == NULL)
        {
            WICED_BT_TRACE("create default heap error: size %d\n", BT_STACK_HEAP_SIZE);
            exit(EXIT_FAILURE);
        }
    }
    else
    {
        WICED_BT_TRACE("Bluetooth Stack Initialization failed!! \n");
        exit(EXIT_FAILURE);
    }
}

/*******************************************************************************
 * Function Name: spp_management_callback
 *******************************************************************************
 * Summary:
 *   This is a Bluetooth stack event handler function to receive management
 *   events from the BT stack and process as per the application.
 *
 * Parameters:
 *   wiced_bt_management_evt_t event : BT event code of one byte length
 *   wiced_bt_management_evt_data_t *p_event_data : Pointer to BT management
 *                                                  event structures
 *
 * Return:
 *  wiced_result_t: Error code from WICED_RESULT_LIST or BT_RESULT_LIST
 *
 ******************************************************************************/
static wiced_result_t spp_management_callback(wiced_bt_management_evt_t event,
                                              wiced_bt_management_evt_data_t *p_event_data)
{
    wiced_bt_device_address_t bda = {0};
    wiced_result_t result = WICED_BT_SUCCESS;
    wiced_bt_dev_encryption_status_t *p_encryption_status;
    wiced_bt_power_mgmt_notification_t *p_power_mgmt_notification;
    wiced_bt_dev_pairing_info_t *p_pairing_info;

    WICED_BT_TRACE("%s: Event: 0x%x %s\n", __FUNCTION__, event, spp_get_bt_event_name(event));

    switch (event)
    {
    case BTM_ENABLED_EVT:
        /* Bluetooth Controller and Host Stack Enabled */
        if (WICED_BT_SUCCESS == p_event_data->enabled.status)
        {
            wiced_bt_set_local_bdaddr(spp_bd_address,BLE_ADDR_PUBLIC);
            wiced_bt_dev_read_local_addr(bda);
            WICED_BT_TRACE("Local Bluetooth Address: ");
            spp_print_bd_address(bda);
            spp_init();
        }
        else
        {
            WICED_BT_TRACE("Bluetooth Enable Failed \n");
        }
        break;

    case BTM_DISABLED_EVT:
        WICED_BT_TRACE("Bluetooth Disabled \n");
        break;

    case BTM_PIN_REQUEST_EVT:
        WICED_BT_TRACE("remote address= %B\n", p_event_data->pin_request.bd_addr);
        wiced_bt_dev_pin_code_reply(*p_event_data->pin_request.bd_addr, result, 4, &pincode[0]);
        break;

    case BTM_USER_CONFIRMATION_REQUEST_EVT:
        /* This application always confirms peer's attempt to pair */
        wiced_bt_dev_confirm_req_reply(WICED_BT_SUCCESS, p_event_data->user_confirmation_request.bd_addr);
        break;

    case BTM_PAIRING_IO_CAPABILITIES_BR_EDR_REQUEST_EVT:
        /* This application supports only Just Works pairing */
        WICED_BT_TRACE("BTM_PAIRING_IO_CAPABILITIES_REQUEST_EVT bda %B\n",
                       p_event_data->pairing_io_capabilities_br_edr_request.bd_addr);
        p_event_data->pairing_io_capabilities_br_edr_request.local_io_cap = BTM_IO_CAPABILITIES_NONE;
        p_event_data->pairing_io_capabilities_br_edr_request.auth_req = BTM_AUTH_SINGLE_PROFILE_GENERAL_BONDING_NO;
        break;

    case BTM_PAIRING_COMPLETE_EVT:
        p_pairing_info = &p_event_data->pairing_complete.pairing_complete_info;
        WICED_BT_TRACE("Pairing Complete: %d\n", p_pairing_info->br_edr.status);
        result = WICED_BT_USE_DEFAULT_SECURITY;
        break;

    case BTM_ENCRYPTION_STATUS_EVT:
        p_encryption_status = &p_event_data->encryption_status;
        WICED_BT_TRACE("Encryption Status Event: bd (%B) res %d\n",
                       p_encryption_status->bd_addr, p_encryption_status->result);
        break;

    case BTM_PAIRED_DEVICE_LINK_KEYS_UPDATE_EVT:
        /* This application supports a single paired host, we can save keys
         * under the same NVRAM ID overwriting previous pairing if any
         */
        (void)spp_write_nvram(SPP_NVRAM_ID, sizeof(wiced_bt_device_link_keys_t),
                              &p_event_data->paired_device_link_keys_update);
        break;

    case BTM_PAIRED_DEVICE_LINK_KEYS_REQUEST_EVT:
        /* read existing key from the NVRAM  */
        if (spp_read_nvram(SPP_NVRAM_ID, &p_event_data->paired_device_link_keys_request,
                           sizeof(wiced_bt_device_link_keys_t)) != 0)
        {
            result = WICED_BT_SUCCESS;
        }
        else
        {
            result = WICED_BT_ERROR;
            WICED_BT_TRACE("Key retrieval failure\n");
        }
        break;

    case BTM_POWER_MANAGEMENT_STATUS_EVT:
        p_power_mgmt_notification = &p_event_data->power_mgmt_notification;
        WICED_BT_TRACE("Power mgmt status event: bd (%B) status:%d hci_status:%d\n", p_power_mgmt_notification->bd_addr,
                       p_power_mgmt_notification->status, p_power_mgmt_notification->hci_status);
        break;

    default:
        result = WICED_BT_USE_DEFAULT_SECURITY;
        break;
    }
    return result;
}

/*******************************************************************************
 * Function Name: spp_write_eir
 *******************************************************************************
 * Summary:
 *   Prepare extended inquiry response data.
 *
 * Parameters:
 *   NONE
 *
 * Return:
 *   NONE
 *
 ******************************************************************************/
static void spp_write_eir(void)
{
    uint8_t *pBuf;
    uint8_t *p;
    uint8_t length;
    uint16_t eir_length;

    pBuf = (uint8_t *)wiced_bt_get_buffer(WICED_EIR_BUF_MAX_SIZE);
    WICED_BT_TRACE("hci_control_write_eir %x\n", pBuf);

    if (!pBuf)
    {
        WICED_BT_TRACE("app_write_eir %x\n", pBuf);
    }
    else
    {
        p = pBuf;

        length = strlen((char *)wiced_bt_cfg_settings.device_name);

        *p++ = length + 1;
        *p++ = BT_EIR_COMPLETE_LOCAL_NAME_TYPE; /* EIR type full name */
        memcpy(p, wiced_bt_cfg_settings.device_name, length);
        p += length;

        *p++ = 2 + 1;                            /* Length of 16 bit services*/
        *p++ = BT_EIR_COMPLETE_16BITS_UUID_TYPE; /* 0x03 EIR type full list
                                                    of 16 bit service UUIDs */
        *p++ = UUID_SERVCLASS_SERIAL_PORT & 0xff;
        *p++ = (UUID_SERVCLASS_SERIAL_PORT >> 8) & 0xff;

        *p++ = 0; /* end of EIR Data is 0 */

        eir_length = (uint16_t)(p - pBuf);

        /* print EIR data */
        WICED_BT_TRACE_ARRAY(pBuf, MIN(p - pBuf, 100), "EIR :");
        wiced_bt_dev_write_eir(pBuf, eir_length);
    }
}

/*******************************************************************************
 * Function Name: spp_init
 *******************************************************************************
 * Summary:
 *   Initializes SPP profile, sdp records and sets the device to discoverable,
 *   pairable and connectable mode.
 *
 * Parameters:
 *   NONE
 *
 * Return:
 *   NONE
 *
 ******************************************************************************/
static void spp_init(void)
{
    wiced_init_timer(&spp_tx_timer, spp_tx_ack_timeout, 0, WICED_MILLI_SECONDS_TIMER);

    spp_write_eir();

    /* Initialize SPP library */
    wiced_bt_spp_startup(&spp_reg);

    /* create SDP records */
    wiced_bt_sdp_db_init((uint8_t *)sdp_database, wiced_app_cfg_sdp_record_get_size());

    /* Allow peer to pair */
    wiced_bt_set_pairable_mode(WICED_TRUE, 0);

    /* This application will always configure device connectable
     * and discoverable
     */
    wiced_bt_dev_set_discoverability(BTM_GENERAL_DISCOVERABLE,
                                     WICED_BT_CFG_DEFAULT_INQUIRY_SCAN_INTERVAL,
                                     WICED_BT_CFG_DEFAULT_INQUIRY_SCAN_WINDOW);

    wiced_bt_dev_set_connectability(BTM_CONNECTABLE,
                                    WICED_BT_CFG_DEFAULT_PAGE_SCAN_INTERVAL,
                                    WICED_BT_CFG_DEFAULT_PAGE_SCAN_WINDOW);
}

/*******************************************************************************
 * Function Name: spp_connection_up_callback
 *******************************************************************************
 * Summary:
 *   SPP connection up callback
 *
 * Parameters:
 *   uint16_t handle : spp handle
 *   uint8_t* bda : connected device's BD address
 *
 * Return:
 *   NONE
 *
 ******************************************************************************/
void spp_connection_up_callback(uint16_t handle, uint8_t *bda)
{
    if (NULL != bda)
    {
        fprintf(stdout, "-------------------------------------------------------------\n");
        fprintf(stdout, "%s handle:%d address:%02X:%02X:%02X:%02X:%02X:%02X\n",
                __FUNCTION__, handle, bda[0], bda[1], bda[2], bda[3], bda[4], bda[5]);
        fprintf(stdout, "-------------------------------------------------------------\n");
        spp_handle = handle;
        spp_rx_bytes = 0;
    }
    else
    {
        WICED_BT_TRACE("%s bda is NULL\n", __FUNCTION__);
    }
}

/*******************************************************************************
 * Function Name: spp_connection_down_callback
 *******************************************************************************
 * Summary:
 *   SPP connection down callback
 *
 * Parameters:
 *   uint16_t handle : spp handle for which the spp should be disconnected
 *
 * Return:
 *   NONE
 *
 ******************************************************************************/
void spp_connection_down_callback(uint16_t handle)
{
    fprintf(stdout, "-------------------------------------------------------------\n");
    fprintf(stdout, "%s handle:%d rx_bytes:%d\n", __FUNCTION__, handle, spp_rx_bytes);
    fprintf(stdout, "-------------------------------------------------------------\n");
    spp_handle = 0;
    spp_send_offset = 0;
    spp_tx_retry_count = 0;
    if (wiced_is_timer_in_use(&spp_tx_timer))
    {
        wiced_stop_timer(&spp_tx_timer);
    }
}

/*******************************************************************************
 * Function Name: spp_rx_data_callback
 *******************************************************************************
 * Summary:
 *   Prints the Data received from SPP client
 *
 * Parameters:
 *   NONE
 *
 * Return:
 *   wiced_bool_t
 *
 ******************************************************************************/
wiced_bool_t spp_rx_data_callback(uint16_t handle, uint8_t *p_data, uint32_t data_len)
{
    wiced_bool_t ret = WICED_FALSE;

    if (NULL != p_data)
    {
        spp_rx_bytes += data_len;

        fprintf(stdout, "%s handle:%d len:%d %02x-%02x, total rx %d\n",
                __FUNCTION__, handle, data_len, p_data[0], p_data[data_len - 1], spp_rx_bytes);
        fprintf(stdout, "data: ");
        for (int i = 0; i < data_len; i++)
        {
            fprintf(stdout, " %c ", p_data[i]);
        }
        fprintf(stdout, "\n");
        ret = WICED_TRUE;
    }
    else
    {
        WICED_BT_TRACE("%s: p_data is NULL\n", __FUNCTION__);
    }
    return ret;
}

/*******************************************************************************
 * Function Name: spp_send_sample_data
 *******************************************************************************
 * Summary:
 *   Test function which sends large data to SPP client
 *
 * Parameters:
 *   NONE
 *
 * Return:
 *   NONE
 *
 ******************************************************************************/
void spp_send_sample_data(void)
{
    int i;
    wiced_bool_t ret;

    WICED_BT_TRACE("spp_send_sample_data entry, spp_handle = %d\n", spp_handle);

    while ((spp_handle != 0) && (spp_send_offset != SPP_TOTAL_DATA_TO_SEND))
    {
        int bytes_to_send = spp_send_offset + SPP_MAX_PAYLOAD < SPP_TOTAL_DATA_TO_SEND ? SPP_MAX_PAYLOAD : SPP_TOTAL_DATA_TO_SEND - spp_send_offset;
        ret = wiced_bt_spp_can_send_more_data(spp_handle);
        if (!ret)
        {
            WICED_BT_TRACE(" ! return from wiced_bt_spp_can_send_more_data\n");
            break;
        }
        for (i = 0; i < bytes_to_send; i++)
        {
            spp_send_buffer[i] = spp_send_offset + i;
        }
        WICED_BT_TRACE("spp_send_sample_data: wiced_bt_spp_send_session_data\n");
        ret = wiced_bt_spp_send_session_data(spp_handle, spp_send_buffer, bytes_to_send);
        if (ret != WICED_TRUE)
        {
            WICED_BT_TRACE(" ! return from wiced_bt_spp_send_session_data\n");
            break;
        }
        spp_send_offset += bytes_to_send;
        spp_tx_retry_count = 0;
    }
    /* Check if we were able to send everything */
    if (spp_send_offset < SPP_TOTAL_DATA_TO_SEND)
    {
        if (spp_tx_retry_count >= MAX_TX_RETRY)
        {
            WICED_BT_TRACE("Reached max tx retries! Terminating transfer!\n");
            WICED_BT_TRACE("Make sure peer device is providing us credits\n");
            spp_send_offset = 0;
        }
        else
        {
            WICED_BT_TRACE("wiced_start_timer spp_tx_timer %d\n", spp_send_offset);
            wiced_start_timer(&spp_tx_timer, TX_RETRY_TIMEOUT);
            spp_tx_retry_count++;
        }
    }
    else
    {
        WICED_BT_TRACE("sent %d bytes of data\n", SPP_TOTAL_DATA_TO_SEND);
        spp_send_offset = 0;
    }
}

/*******************************************************************************
 * Function Name: spp_tx_ack_timeout
 *******************************************************************************
 * Summary:
 *   The timeout function is called to retry the trasnfer of data if transfer
 *   of big data over spp failed for any reason
 *
 * Parameters:
 *   WICED_TIMER_PARAM_TYPE arg
 *
 * Return:
 *   NONE
 *
 ******************************************************************************/
static void spp_tx_ack_timeout(WICED_TIMER_PARAM_TYPE arg)
{
    spp_send_sample_data();
}

/*******************************************************************************
 * Function Name: spp_write_nvram
 *******************************************************************************
 * Summary:
 *   Write NVRAM function is called to store information in the NVRAM.
 *
 * Parameters:
 *   int nvram_id : Volatile Section Identifier. Application can use
 *                  the VS ids from WICED_NVRAM_VSID_START to
 *                  WICED_NVRAM_VSID_END
 *   int data_len : Length of the data to be written to the NVRAM
 *   void *p_data : Pointer to the data to be written to the NVRAM
 *
 * Return:
 *   int : number of bytes written, 0 on error
 *
 ******************************************************************************/
static int spp_write_nvram(int nvram_id, int data_len, void *p_data)
{
    wiced_result_t result;
    int bytes_written = 0;

    if (NULL != p_data)
    {
        bytes_written = wiced_hal_write_nvram(nvram_id, data_len, (uint8_t *)p_data, &result);
        WICED_BT_TRACE("NVRAM ID:%d written :%d bytes result:%d\n", nvram_id, bytes_written, result);
    }
    return (bytes_written);
}

/*******************************************************************************
 * Function Name: spp_read_nvram
 *******************************************************************************
 * Summary:
 *   Read data from the NVRAM and return in the passed buffer
 *
 * Parameters:
 *   int nvram_id : Volatile Section Identifier. Application can use
 *                  the VS ids from WICED_NVRAM_VSID_START to
 *                  WICED_NVRAM_VSID_END
 *   int data_len : Length of the data to be read from the NVRAM
 *   void *p_data : Pointer to the data to be read out from the NVRAM
 *
 * Return:
 *   int : number of bytes read, 0 on error
 *
 ******************************************************************************/
static int spp_read_nvram(int nvram_id, void *p_data, int data_len)
{
    uint16_t read_bytes = 0;
    wiced_result_t result;

    if ((NULL != p_data) && (data_len >= sizeof(wiced_bt_device_link_keys_t)))
    {
        read_bytes = wiced_hal_read_nvram(nvram_id, sizeof(wiced_bt_device_link_keys_t), p_data, &result);
        WICED_BT_TRACE("NVRAM ID:%d read out of %d bytes:%d result:%d\n",
                       nvram_id, sizeof(wiced_bt_device_link_keys_t), read_bytes, result);
    }
    return (read_bytes);
}

/*******************************************************************************
 *      UTILITY FUNCTION DEFINITIONS
 ******************************************************************************/

/******************************************************************************
 * Function Name: spp_print_bd_address()
 ******************************************************************************
 * Summary:
 *   This is the utility function that prints the address of the Bluetooth
 *   device
 *
 * Parameters:
 *   wiced_bt_device_address_t bdadr                : Bluetooth address
 *
 * Return:
 *   void
 *
 ******************************************************************************/
static void spp_print_bd_address(wiced_bt_device_address_t bdadr)
{
    WICED_BT_TRACE("%02X:%02X:%02X:%02X:%02X:%02X\n",
                   bdadr[0], bdadr[1], bdadr[2], bdadr[3], bdadr[4], bdadr[5]);
}

/******************************************************************************
 * Function Name: spp_get_bt_event_name
 ******************************************************************************
 * Summary:
 *   The function converts the wiced_bt_management_evt_t enum value to its
 *   corresponding string literal. This will help the programmer to debug
 *   easily with log traces without navigating through the source code.
 *
 * Parameters:
 *   wiced_bt_management_evt_t event: Bluetooth management event type
 *
 * Return:
 *   const char *: String for wiced_bt_management_evt_t
 *
 ******************************************************************************/
static const char *spp_get_bt_event_name(wiced_bt_management_evt_t event)
{
    switch ((int)event)
    {
        CASE_RETURN_STR(BTM_ENABLED_EVT)
        CASE_RETURN_STR(BTM_DISABLED_EVT)
        CASE_RETURN_STR(BTM_POWER_MANAGEMENT_STATUS_EVT)
        CASE_RETURN_STR(BTM_PIN_REQUEST_EVT)
        CASE_RETURN_STR(BTM_USER_CONFIRMATION_REQUEST_EVT)
        CASE_RETURN_STR(BTM_PASSKEY_NOTIFICATION_EVT)
        CASE_RETURN_STR(BTM_PASSKEY_REQUEST_EVT)
        CASE_RETURN_STR(BTM_KEYPRESS_NOTIFICATION_EVT)
        CASE_RETURN_STR(BTM_PAIRING_IO_CAPABILITIES_BR_EDR_REQUEST_EVT)
        CASE_RETURN_STR(BTM_PAIRING_IO_CAPABILITIES_BR_EDR_RESPONSE_EVT)
        CASE_RETURN_STR(BTM_PAIRING_IO_CAPABILITIES_BLE_REQUEST_EVT)
        CASE_RETURN_STR(BTM_PAIRING_COMPLETE_EVT)
        CASE_RETURN_STR(BTM_ENCRYPTION_STATUS_EVT)
        CASE_RETURN_STR(BTM_SECURITY_REQUEST_EVT)
        CASE_RETURN_STR(BTM_SECURITY_FAILED_EVT)
        CASE_RETURN_STR(BTM_SECURITY_ABORTED_EVT)
        CASE_RETURN_STR(BTM_READ_LOCAL_OOB_DATA_COMPLETE_EVT)
        CASE_RETURN_STR(BTM_REMOTE_OOB_DATA_REQUEST_EVT)
        CASE_RETURN_STR(BTM_PAIRED_DEVICE_LINK_KEYS_UPDATE_EVT)
        CASE_RETURN_STR(BTM_PAIRED_DEVICE_LINK_KEYS_REQUEST_EVT)
        CASE_RETURN_STR(BTM_LOCAL_IDENTITY_KEYS_UPDATE_EVT)
        CASE_RETURN_STR(BTM_LOCAL_IDENTITY_KEYS_REQUEST_EVT)
        CASE_RETURN_STR(BTM_BLE_SCAN_STATE_CHANGED_EVT)
        CASE_RETURN_STR(BTM_BLE_ADVERT_STATE_CHANGED_EVT)
        CASE_RETURN_STR(BTM_SMP_REMOTE_OOB_DATA_REQUEST_EVT)
        CASE_RETURN_STR(BTM_SMP_SC_REMOTE_OOB_DATA_REQUEST_EVT)
        CASE_RETURN_STR(BTM_SMP_SC_LOCAL_OOB_DATA_NOTIFICATION_EVT)
        CASE_RETURN_STR(BTM_SCO_CONNECTED_EVT)
        CASE_RETURN_STR(BTM_SCO_DISCONNECTED_EVT)
        CASE_RETURN_STR(BTM_SCO_CONNECTION_REQUEST_EVT)
        CASE_RETURN_STR(BTM_SCO_CONNECTION_CHANGE_EVT)
        CASE_RETURN_STR(BTM_BLE_CONNECTION_PARAM_UPDATE)
    }

    return NULL;
}

/* END OF FILE [] */
