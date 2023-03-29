/*******************************************************************************
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
 *******************************************************************************/
/******************************************************************************
 * File Name: main.c
 *
 * Description: This is the source code for Linux SPP CE template project.
 *
 * Related Document: See README.md
 *
 *******************************************************************************/

/*******************************************************************************
 *                           INCLUDES
 *******************************************************************************/
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include "wiced_bt_trace.h"
#include "wiced_bt_ble.h"
#include "wiced_bt_types.h"
#include "wiced_bt_dev.h"
#include "wiced_bt_stack.h"
#include "wiced_memory.h"
#include "platform_linux.h"
#include "utils_arg_parser.h"
#include "wiced_bt_cfg.h"
#include "wiced_bt_spp.h"
#include "spp.h"

/*******************************************************************************
 *                               MACROS
 *******************************************************************************/
#define MAX_PATH (256)
#define LOCAL_IP_LEN (16)

/*USER INPUT COMMANDS*/
#define EXIT (0)
#define PRINT_MENU (1)
#define SEND_SAMPLE_DATA (2)
#define SEND_DATA (3)
#define SCAN_ERROR (0)

/*******************************************************************************
 *                               STRUCTURES AND ENUMERATIONS
 *******************************************************************************/

/******************************************************************************
 *                               GLOBAL VARIABLES
 ******************************************************************************/

/* Application name */
static char g_app_name[MAX_PATH];

static const char app_menu[] = "\n\
---------------------SPP MENU-----------------------\n\n\
    0.  Exit \n\
    1.  Print Menu \n\
    2.  Send Large Sample Data \n\
    3.  Send Data \n\
Choose option -> ";
uint8_t spp_bd_address[LOCAL_BDA_LEN] = {0x11, 0x12, 0x13, 0x21, 0x22, 0x23};

/****************************************************************************
 *                              FUNCTION DECLARATIONS
 ***************************************************************************/
uint32_t hci_control_proc_rx_cmd(uint8_t *p_buffer, uint32_t length);
void APPLICATION_START(void);

/******************************************************************************
 *                               FUNCTION DEFINITIONS
 ******************************************************************************/

/******************************************************************************
 * Function Name: hci_control_proc_rx_cmd()
 *******************************************************************************
 * Summary:
 *   Function to handle HCI receive (This will be called from
 *   porting layer - linux_tcp_server.c)
 *
 * Parameters:
 *   uint8_t* p_buffer    : rx buffer
 *   uint32_t length      : rx buffer length
 *
 * Return:
 *  status code
 *
 ******************************************************************************/
uint32_t
hci_control_proc_rx_cmd(uint8_t *p_buffer, uint32_t length)
{
    return 0;
}

/******************************************************************************
 * Function Name: APPLICATION_START()
 *******************************************************************************
 * Summary:
 *   BT stack initialization function wrapper
 *
 * Parameters:
 *   None
 *
 * Return:
 *      None
 *
 ******************************************************************************/
void APPLICATION_START(void)
{
    spp_application_start();
}

/******************************************************************************
 * Function Name: main()
 *******************************************************************************
 * Summary:
 *   Application entry function
 *
 * Parameters:
 *   int argc            : argument count
 *   char *argv[]        : list of arguments
 *
 * Return:
 *      None
 *
 ******************************************************************************/
int main(int argc, char *argv[])
{
    int filename_len = 0;
    char fw_patch_file[MAX_PATH];
    char hci_port[MAX_PATH];
    char peer_ip_addr[16] = "000.000.000.000";
    uint32_t hci_baudrate = 0;
    uint32_t patch_baudrate = 0;
    int btspy_inst = 0;
    uint8_t btspy_is_tcp_socket = 0;         /* Throughput calculation thread handler */
    pthread_t throughput_calc_thread_handle; /* Audobaud configuration GPIO bank and pin */
    cybt_controller_autobaud_config_t autobaud;
    char uart_cts_gpio_path[50] = {0x00}; /* UART CTS GPIO Path */
    char bt_regon_gpio_path[50] = {0x00}; /* BT REG ONOFF GPIO Path */
    memset(fw_patch_file, 0, MAX_PATH);
    memset(hci_port, 0, MAX_PATH);
    int choice = 0;
    int spp_buf_size = 0;

    if (PARSE_ERROR ==
        arg_parser_get_args(argc, argv, hci_port, spp_bd_address, &hci_baudrate,
                            &btspy_inst, peer_ip_addr, &btspy_is_tcp_socket,
                            fw_patch_file, &patch_baudrate, &autobaud))
    {
        return EXIT_FAILURE;
    }
    filename_len = strlen(argv[0]);
    if (filename_len >= MAX_PATH)
    {
        filename_len = MAX_PATH - 1;
    }

    cy_platform_bluetooth_init(fw_patch_file, hci_port, hci_baudrate,
                               patch_baudrate, &autobaud);

    fprintf(stdout, " Linux CE SPP project initialization complete...\n");

    for (;;)
    {
        fprintf(stdout, "%s", app_menu);
        fflush(stdin);
        if( SCAN_ERROR == scanf("%d", &choice) )
        {
           fprintf(stdout, "Invalid input received, Try again\n");
           while (getchar() != '\n');
           continue; 
        }

        switch (choice)
        {
        case EXIT:
            exit(EXIT_SUCCESS);
        case PRINT_MENU:
            break;
        case SEND_SAMPLE_DATA:
            if (0 != spp_handle)
            {
                spp_send_sample_data();
            }
            else
            {
                fprintf(stdout, "SPP not connected\n");
            }
            break;
        case SEND_DATA:
            if (0 != spp_handle)
            {
                wiced_bool_t ret = WICED_FALSE;
                fprintf(stdout,
                        "Enter the Data to be Sent (Enter less than 1007 i.e SPP_MAX_PAYLOAD):\n");
                spp_buf_size = scanf("%1007s", spp_send_buffer);
                if (spp_buf_size == 0)
                {
                    WICED_BT_TRACE("Error reading buffer to send\b");
                    continue;
                }
                ret = wiced_bt_spp_send_session_data(spp_handle, spp_send_buffer,
                                                   strlen(spp_send_buffer));
                if (ret != WICED_TRUE)
                {
                    WICED_BT_TRACE(" error return from wiced_bt_spp_send_session_data, ret = %x\n",
                                   ret);
                }
            }
            else
            {
                fprintf(stdout, "SPP not connected\n");
            }
            break;
        default:
            fprintf(stdout, "Invalid input received, Try again\n");
            break;
        }
    }
    return EXIT_SUCCESS;
}
