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
 * File Name: spp.h
 *
 * Description: This is the include file for Linux SPP CE.
 *
 * Related Document: See README.md
 *
 *****************************************************************************/

#ifndef __APP_SPP_H__
#define __APP_SPP_H__

/******************************************************************************
 *          INCLUDES
 *****************************************************************************/
#include <stdio.h>
#include "wiced_bt_cfg.h"
#include "wiced_bt_trace.h"

/******************************************************************************
 *          MACROS
 *****************************************************************************/
#define HDLR_SPP                                ( 0x10001 )
#define SPP_RFCOMM_SCN                          ( 2 )
#define SPP_MAX_PAYLOAD                         ( 1007 )
#define LOCAL_BDA_LEN                           ( 6 )

/******************************************************************************
 *          VARIABLE DEFINITIONS
 *****************************************************************************/
extern uint8_t spp_send_buffer[SPP_MAX_PAYLOAD];
extern uint16_t spp_handle;
extern uint8_t spp_bd_address[];

/******************************************************************************
 *          FUNCTION PROTOTYPES
 *****************************************************************************/
void spp_application_start( );

void spp_send_sample_data( void );

#endif /* __APP_SPP_H__ */
