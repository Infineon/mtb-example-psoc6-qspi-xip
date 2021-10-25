/******************************************************************************
* File Name:   main.c
*
* Description: This is the source code for the PSoC 6 MCU: QSPI XIP Example
*              for ModusToolbox.
*
* Related Document: See README.md
*
*
*******************************************************************************
* Copyright 2019-2021, Cypress Semiconductor Corporation (an Infineon company) or
* an affiliate of Cypress Semiconductor Corporation.  All rights reserved.
*
* This software, including source code, documentation and related
* materials ("Software") is owned by Cypress Semiconductor Corporation
* or one of its affiliates ("Cypress") and is protected by and subject to
* worldwide patent protection (United States and foreign),
* United States copyright laws and international treaty provisions.
* Therefore, you may use this Software only as provided in the license
* agreement accompanying the software package from which you
* obtained this Software ("EULA").
* If no EULA applies, Cypress hereby grants you a personal, non-exclusive,
* non-transferable license to copy, modify, and compile the Software
* source code solely for use in connection with Cypress's
* integrated circuit products.  Any reproduction, modification, translation,
* compilation, or representation of this Software except as specified
* above is prohibited without the express written permission of Cypress.
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
* including Cypress's product in a High Risk Product, the manufacturer
* of such system or application assumes all risk of such use and in doing
* so agrees to indemnify Cypress against all liability.
*******************************************************************************/

#include "cy_pdl.h"
#include "cyhal.h"
#include "cybsp.h"
#include "cy_retarget_io.h"
#include "cy_serial_flash_qspi.h"
#include "cycfg_qspi_memslot.h"


/*******************************************************************************
* Macros
********************************************************************************/
#define PACKET_SIZE             (64u)     /* Memory Read/Write size */
#define NUM_BYTES_PER_LINE      (16u)     /* Used when array of data is printed on the console */
#define LED_TOGGLE_DELAY_MSEC   (1000u)   /* LED blink delay */
#define MEM_SLOT_NUM            (0u)      /* Slot number of the memory to use */
#define QSPI_BUS_FREQUENCY_HZ   (50000000lu)

/*******************************************************************************
* Global Variables
********************************************************************************/
/* String placed in external memory on programming */
const char *hi_word CY_SECTION(".cy_xip") __attribute__((used)) = "Hello from the external string!\n";

/*******************************************************************************
* Function declarations
*******************************************************************************/
CY_SECTION(".cy_xip_code") __attribute__((used))
void print_from_external_memory(const char *buf);

/********************************************************
* Function Name: print_from_external_memory
*********************************************************
* Summary:
*  Prints the passed string to a UART.
*  Executes from external memory.
*
* Parameters:
*   buf: The string to be printed
********************************************************/
void print_from_external_memory(const char *buf)
{
    printf("%s", buf);
}

/*******************************************************************************
* Function Name: check_status
****************************************************************************//**
* Summary:
*  Prints the message, indicates the non-zero status by turning the LED on, and
*  asserts the non-zero status.
*
* Parameters:
*  message - message to print if status is non-zero.
*  status - status for evaluation.
*
*******************************************************************************/
void check_status(char *message, uint32_t status)
{
    if(0u != status)
    {
        printf("\n================================================================================\n");
        printf("\nFAIL: %s\n", message);
        printf("Error Code: 0x%08lX\n", (unsigned long)status);
        printf("\n================================================================================\n");

        /* On failure, turn the LED ON */
        cyhal_gpio_write(CYBSP_USER_LED, CYBSP_LED_STATE_ON);
        while(true); /* Wait forever here when error occurs. */
    }
}

/*******************************************************************************
* Function Name: print_array
****************************************************************************//**
* Summary:
*  Prints the content of the buffer to the UART console.
*
* Parameters:
*  message - message to print before array output
*  buf - buffer to print on the console.
*  size - size of the buffer.
*
*******************************************************************************/
void print_array(char *message, uint8_t *buf, uint32_t size)
{
    printf("\n%s (%lu bytes):\n", message, (unsigned long)size);
    printf("-------------------------\n");

    for(uint32_t index = 0; index < size; index++)
    {
        printf("0x%02X ", buf[index]);

        if(0u == ((index + 1) % NUM_BYTES_PER_LINE))
        {
            printf("\n");
        }
    }
}

/*******************************************************************************
* Function Name: check_address
****************************************************************************//**
* Summary:
*  Checks the address of the passed function. If the address is not in the
*  external memory region (addr >= 0x18000000), then prints a failure message
*  and exits.
*
* Parameters:
*  message - message to print if address is not in external memory.
*  addr - address for evaluation.
*
*******************************************************************************/
void check_address(char *message, uint32_t addr)
{
    if(smifMemConfigs[MEM_SLOT_NUM]->baseAddress > addr)
    {
        printf("\n================================================================================\n");
        printf("FAIL: %s\n", message);
        printf("Address: 0x%lx", addr);
        printf("\n================================================================================\n");

        /* On failure, turn the LED ON */
        cyhal_gpio_write(CYBSP_USER_LED, CYBSP_LED_STATE_ON);
        while(true); /* Wait forever here when error occurs. */
    }
}
/*******************************************************************************
* Function Name: main
********************************************************************************
* Summary:
*  This is the main function for CM4 CPU. It does...
*     1. Initializes UART for console output and SMIF for interfacing a QSPI
*        flash.
*     2. Performs erase followed by write and verifies the written data by
*        reading it back.
*     3. Transitions the SMIF block into XIP mode and prints a string from
*        external memory and calls a function from external memory.
*
* Parameters:
*  void
*
* Return:
*  int
*
*******************************************************************************/
int main(void)
{
    uint32_t addr;
    cy_rslt_t result;

    /* Initialize the device and board peripherals */
    result = cybsp_init();
    CY_ASSERT(result == CY_RSLT_SUCCESS);

    /* Enable global interrupts */
    __enable_irq();

    /* Initialize retarget-io to use the debug UART port */
    cy_retarget_io_init(CYBSP_DEBUG_UART_TX, CYBSP_DEBUG_UART_RX, CY_RETARGET_IO_BAUDRATE);

    /* Initialize the User LED */
    cyhal_gpio_init(CYBSP_USER_LED, CYHAL_GPIO_DIR_OUTPUT, CYHAL_GPIO_DRIVE_STRONG, CYBSP_LED_STATE_OFF);

    /* \x1b[2J\x1b[;H - ANSI ESC sequence for clear screen */
    printf("\x1b[2J\x1b[;H");
    printf("*************** PSoC 6 MCU: External Flash Access in XIP Mode ***************\n\n");

    /* Initialize the QSPI block */
    result = cy_serial_flash_qspi_init(smifMemConfigs[MEM_SLOT_NUM], CYBSP_QSPI_D0, CYBSP_QSPI_D1,
                                       CYBSP_QSPI_D2, CYBSP_QSPI_D3, NC, NC, NC, NC, CYBSP_QSPI_SCK,
                                       CYBSP_QSPI_SS, QSPI_BUS_FREQUENCY_HZ);
    check_status("Serial Flash initialization failed", result);

    /* Initialize the transfer buffers */
    uint8_t txBuffer[PACKET_SIZE];
    uint8_t rxBuffer[PACKET_SIZE];

    /* Initialize tx buffer and rx buffer */
    for(uint32_t index = 0; index < PACKET_SIZE; index++)
    {
        txBuffer[index] = (uint8_t) (index & 0xFF);
        rxBuffer[index] = 0;
    }

    /* Set the address to transact with to the start of the second sector */
    uint32_t extMemAddress = 0x00;
    size_t sectorSize = cy_serial_flash_qspi_get_erase_size(extMemAddress);
    extMemAddress = sectorSize;

    printf("\n1. Total Flash Size: %u bytes.\n", cy_serial_flash_qspi_get_size());

    /* Erase before write */
    printf("\n1. Erasing %u bytes of memory.\n", sectorSize);
    result = cy_serial_flash_qspi_erase(extMemAddress, sectorSize);
    check_status("Erasing memory failed", result);

    /* Read after Erase to confirm that all data is 0xFF */
    printf("\n2. Reading after Erase. Ensure that the data read is 0xFF for each byte.\n");
    result = cy_serial_flash_qspi_read(extMemAddress, PACKET_SIZE, rxBuffer);
    check_status("Reading memory failed", result);
    print_array("Received Data", rxBuffer, PACKET_SIZE);

    /* Write the content of the txBuffer to the memory */
    printf("\n3. Writing data to memory.\n");
    result = cy_serial_flash_qspi_write(extMemAddress, PACKET_SIZE, txBuffer);
    check_status("Writing to memory failed", result);
    print_array("Written Data", txBuffer, PACKET_SIZE);

    /* Read back after Write for verification */
    printf("\n4. Reading back for verification.\n");
    result = cy_serial_flash_qspi_read(extMemAddress, PACKET_SIZE, rxBuffer);
    check_status("Reading memory failed", result);
    print_array("Received Data", rxBuffer, PACKET_SIZE);

    /* Check if the transmitted and received arrays are equal */
    check_status("Read data does not match with written data. Read/Write operation failed.",
            memcmp(txBuffer, rxBuffer, PACKET_SIZE));

    printf("\n================================================================================\n");
    printf("\nSUCCESS: Read data matches with written data!\n");
    printf("\n================================================================================\n");

    /* Put the device in XIP mode */
    printf("\n5. Entering XIP Mode.\n");
    cy_serial_flash_qspi_enable_xip(true);

    addr = (uint32_t)&hi_word;
    check_address("String not found in external memory.", addr);
    /* Print the string from external memory */
    printf("\nString in the external memory at address: 0x%lx", addr);
    printf("\n-------------------------------------------------------\n%s", hi_word);

    addr = (uint32_t)&print_from_external_memory;
    check_address("Function not found in external memory.", addr);
    /* Print by calling function that lives in external memory */
    printf("\nFunction call from external memory address: 0x%lx", addr);
    printf("\n-------------------------------------------------------");
    print_from_external_memory("\nHello from the external function!\n");

    printf("\n================================================================================\n");
    printf("\nSUCCESS: Data successfully accessed in XIP mode!\n");
    printf("\n================================================================================\n");

    for(;;)
    {
        cyhal_gpio_toggle(CYBSP_USER_LED);
        cyhal_system_delay_ms(LED_TOGGLE_DELAY_MSEC);
    }
}

/* [] END OF FILE */
