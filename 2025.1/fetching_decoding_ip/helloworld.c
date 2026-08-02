/******************************************************************************
* Copyright (C) 2023 Advanced Micro Devices, Inc. All Rights Reserved.
* SPDX-License-Identifier: MIT
******************************************************************************/
/*
 * helloworld.c: simple test application
 *
 * This application configures UART 16550 to baud rate 9600.
 * PS7 UART (Zynq) is not initialized by this application, since
 * bootrom/bsp configures it to baud rate 115200
 *
 * ------------------------------------------------
 * | UART TYPE   BAUD RATE                        |
 * ------------------------------------------------
 *   uartns550   9600
 *   uartlite    Configurable only in HW design
 *   ps7_uart    115200 (configured by bootrom/bsp)
 */

#include <stdio.h>
#include "xfetching_decoding_ip.h"
#include "xparameters.h"
#include "xscugic.h"
#include "xil_exception.h"

#define LOG_CODE_RAM_SIZE 15
//size in words
#define CODE_RAM_SIZE     (1<<LOG_CODE_RAM_SIZE)

// GIC device ID and the IP's fabric interrupt ID: both are only defined
// in xparameters.h once the interrupt port is wired to IRQ_F2P in Vivado
// and the platform is rebuilt from the new .xsa.
#define INTC_DEVICE_ID       XPAR_SCUGIC_SINGLE_DEVICE_ID
#define IP_INTR_ID           XPAR_FABRIC_FETCHING_DECODING_IP_0_INTERRUPT_INTR

XFetching_decoding_ip_Config *cfg_ptr;
XFetching_decoding_ip         ip;
XScuGic                       intc;
volatile int                  ip_done = 0;

word_type code_ram[CODE_RAM_SIZE]={
#include "test_op_imm_0_text.hex"
};

void ip_interrupt_handler(void *CallbackRef){
  XFetching_decoding_ip_InterruptGlobalDisable(&ip);
  XFetching_decoding_ip_InterruptClear(&ip, 1);
  ip_done = 1;
  XFetching_decoding_ip_InterruptGlobalEnable(&ip);
}

int setup_interrupt_system(){
  XScuGic_Config *intc_cfg = XScuGic_LookupConfig(INTC_DEVICE_ID);
  if (XScuGic_CfgInitialize(&intc, intc_cfg, intc_cfg->CpuBaseAddress) != XST_SUCCESS)
    return XST_FAILURE;

  Xil_ExceptionInit();
  Xil_ExceptionRegisterHandler(XIL_EXCEPTION_ID_INT,
    (Xil_ExceptionHandler)XScuGic_InterruptHandler, &intc);
  Xil_ExceptionEnable();

  XScuGic_Connect(&intc, IP_INTR_ID,
    (Xil_ExceptionHandler)ip_interrupt_handler, &ip);
  XScuGic_Enable(&intc, IP_INTR_ID);
  return XST_SUCCESS;
}

int main(){
  cfg_ptr = XFetching_decoding_ip_LookupConfig(XPAR_FETCHING_DECODING_IP_0_BASEADDR);
  XFetching_decoding_ip_CfgInitialize(&ip, cfg_ptr);

  if (setup_interrupt_system() != XST_SUCCESS){
    printf("interrupt system setup failed\n");
    return XST_FAILURE;
  }

  XFetching_decoding_ip_InterruptGlobalEnable(&ip);
  XFetching_decoding_ip_InterruptEnable(&ip, 1); // enable on ap_done

  XFetching_decoding_ip_Set_start_pc(&ip, 0);
  XFetching_decoding_ip_Write_code_ram_Words(&ip, 0, code_ram, CODE_RAM_SIZE);
  XFetching_decoding_ip_Start(&ip);

  while (!ip_done);

  printf("%d fetched and decoded instructions\n",
    (int)XFetching_decoding_ip_Get_nb_instruction(&ip));
}
