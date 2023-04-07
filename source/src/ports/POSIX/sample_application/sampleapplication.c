/*******************************************************************************
 * Copyright (c) 2012, Rockwell Automation, Inc.
 * All rights reserved.
 *
 ******************************************************************************/

#include <string.h>
#include <stdlib.h>
#include <stdbool.h>

#include "opener_api.h"
#include "appcontype.h"
#include "trace.h"
#include "cipidentity.h"
#include "ciptcpipinterface.h"
#include "cipqos.h"
#include "nvdata.h"
#if defined(OPENER_ETHLINK_CNTRS_ENABLE) && 0 != OPENER_ETHLINK_CNTRS_ENABLE
  #include "cipethernetlink.h"
  #include "ethlinkcbs.h"
#endif

#define DEMO_APP_INPUT_ASSEMBLY_NUM                100 //0x064
#define DEMO_APP_OUTPUT_ASSEMBLY_NUM               150 //0x096
#define DEMO_APP_CONFIG_ASSEMBLY_NUM               151 //0x097
#define DEMO_APP_HEARTBEAT_INPUT_ONLY_ASSEMBLY_NUM  152 //0x098
#define DEMO_APP_HEARTBEAT_LISTEN_ONLY_ASSEMBLY_NUM 153 //0x099
#define DEMO_APP_EXPLICT_ASSEMBLY_NUM              154 //0x09A

// LCU Assemblies
#define LCU_APP_INPUT_ASSEMBLY_NUM                 102 //0x066
#define LCU_APP_OUTPUT_ASSEMBLY_NUM                101 //0x065
#define LCU_APP_CONFIG_ASSEMBLY_NUM                103 //0x067
#define LCU_APP_HEARTBEAT_INPUT_ONLY_ASSEMBLY_NUM  254 //0x0FE

/* global variables for demo application (4 assembly data fields)  ************/

EipUint8 g_assembly_data064[32]; /* Input */
EipUint8 g_assembly_data096[32]; /* Output */
EipUint8 g_assembly_data097[10]; /* Config */
EipUint8 g_assembly_data09A[32]; /* Explicit */
EipUint8 g_assembly_data_lcu_to_tcms[126]; /* Cip input message from LCU to TCMS (1008 bits) */
EipUint8 g_assembly_data_tcms_to_lcu[233]; /* Cip output message from TCMS to LCU (1864 bits) */
EipUint8 g_assembly_data_config[10];      /* Config */

/* local functions */

/* global functions called by the stack */
EipStatus ApplicationInitialization(void) {
  printf("HZY LCU CIP Application Initialization\n");
  /* create 3 assembly object instances*/
  /*INPUT*/
  CreateAssemblyObject( DEMO_APP_INPUT_ASSEMBLY_NUM, g_assembly_data064,
                        sizeof(g_assembly_data064) );

  /*OUTPUT*/
  CreateAssemblyObject( DEMO_APP_OUTPUT_ASSEMBLY_NUM, g_assembly_data096,
                        sizeof(g_assembly_data096) );

  /*CONFIG*/
  CreateAssemblyObject( DEMO_APP_CONFIG_ASSEMBLY_NUM, g_assembly_data097,
                        sizeof(g_assembly_data097) );

  /*Heart-beat output assembly for Input only connections */
  CreateAssemblyObject(DEMO_APP_HEARTBEAT_INPUT_ONLY_ASSEMBLY_NUM, NULL, 0);

  /*Heart-beat output assembly for Listen only connections */
  CreateAssemblyObject(DEMO_APP_HEARTBEAT_LISTEN_ONLY_ASSEMBLY_NUM, NULL, 0);

  /* assembly for explicit messaging */
  CreateAssemblyObject( DEMO_APP_EXPLICT_ASSEMBLY_NUM, g_assembly_data09A,
                        sizeof(g_assembly_data09A) );

  /* LCU Cip input message from LCU to TCMS */
  CreateAssemblyObject( LCU_APP_INPUT_ASSEMBLY_NUM, &g_assembly_data_lcu_to_tcms[0],
                        sizeof(g_assembly_data_lcu_to_tcms) );

  /* LCU Cip output message from TCMS to LCU */
  CreateAssemblyObject( LCU_APP_OUTPUT_ASSEMBLY_NUM, &g_assembly_data_tcms_to_lcu[0],
                        sizeof(g_assembly_data_tcms_to_lcu) );

  /* LCU Cip Config if any*/
  CreateAssemblyObject( LCU_APP_CONFIG_ASSEMBLY_NUM, &g_assembly_data_config[0],
                        sizeof(g_assembly_data_config) );

  /* LCU Heart-beat output assembly for Input only connections */
  CreateAssemblyObject(LCU_APP_HEARTBEAT_INPUT_ONLY_ASSEMBLY_NUM, 0, 0);

  /* ConfigureExclusiveOwnerConnectionPoint(0, DEMO_APP_OUTPUT_ASSEMBLY_NUM,
                                         DEMO_APP_INPUT_ASSEMBLY_NUM,
                                         DEMO_APP_CONFIG_ASSEMBLY_NUM);
  ConfigureInputOnlyConnectionPoint(0,
                                    DEMO_APP_HEARTBEAT_INPUT_ONLY_ASSEMBLY_NUM,
                                    DEMO_APP_INPUT_ASSEMBLY_NUM,
                                    DEMO_APP_CONFIG_ASSEMBLY_NUM);
  ConfigureListenOnlyConnectionPoint(0,
                                     DEMO_APP_HEARTBEAT_LISTEN_ONLY_ASSEMBLY_NUM,
                                     DEMO_APP_INPUT_ASSEMBLY_NUM,
                                     DEMO_APP_CONFIG_ASSEMBLY_NUM); */

  /* LCU MPU Master connection */
  ConfigureExclusiveOwnerConnectionPoint(0, 
                                         LCU_APP_OUTPUT_ASSEMBLY_NUM,
                                         LCU_APP_INPUT_ASSEMBLY_NUM,
                                         LCU_APP_CONFIG_ASSEMBLY_NUM);

  /* LCU MPU Slave connection */
  ConfigureInputOnlyConnectionPoint(0, 
                                    LCU_APP_HEARTBEAT_INPUT_ONLY_ASSEMBLY_NUM,
                                    LCU_APP_INPUT_ASSEMBLY_NUM,
                                    LCU_APP_CONFIG_ASSEMBLY_NUM);

  /* For NV data support connect callback functions for each object class with
   *  NV data.
   */
  InsertGetSetCallback(GetCipClass(kCipQoSClassCode), NvQosSetCallback,
                       kNvDataFunc);
  InsertGetSetCallback(GetCipClass(kCipTcpIpInterfaceClassCode),
                       NvTcpipSetCallback,
                       kNvDataFunc);

#if defined(OPENER_ETHLINK_CNTRS_ENABLE) && 0 != OPENER_ETHLINK_CNTRS_ENABLE
  /* For the Ethernet Interface & Media Counters connect a PreGetCallback and
   *  a PostGetCallback.
   * The PreGetCallback is used to fetch the counters from the hardware.
   * The PostGetCallback is utilized by the GetAndClear service to clear
   *  the hardware counters after the current data have been transmitted.
   */
  {
    CipClass *p_eth_link_class = GetCipClass(kCipEthernetLinkClassCode);
    InsertGetSetCallback(p_eth_link_class,
                         EthLnkPreGetCallback,
                         kPreGetFunc);
    InsertGetSetCallback(p_eth_link_class,
                         EthLnkPostGetCallback,
                         kPostGetFunc);
    /* Specify the attributes for which the callback should be executed. */
    for (int idx = 0; idx < OPENER_ETHLINK_INSTANCE_CNT; ++idx)
    {
      CipAttributeStruct *p_eth_link_attr;
      CipInstance *p_eth_link_inst =
        GetCipInstance(p_eth_link_class, idx + 1);
      OPENER_ASSERT(p_eth_link_inst);

      /* Interface counters attribute */
      p_eth_link_attr = GetCipAttribute(p_eth_link_inst, 4);
      p_eth_link_attr->attribute_flags |= (kPreGetFunc | kPostGetFunc);
      /* Media counters attribute */
      p_eth_link_attr = GetCipAttribute(p_eth_link_inst, 5);
      p_eth_link_attr->attribute_flags |= (kPreGetFunc | kPostGetFunc);
    }
  }
#endif

  return kEipStatusOk;
}

void HandleApplication(void) {
  /* check if application needs to trigger an connection */
}

void CheckIoConnectionEvent(unsigned int output_assembly_id,
                            unsigned int input_assembly_id,
                            IoConnectionEvent io_connection_event) {
  /* maintain a correct output state according to the connection state*/

  (void) output_assembly_id; /* suppress compiler warning */
  (void) input_assembly_id; /* suppress compiler warning */
  (void) io_connection_event; /* suppress compiler warning */
}

EipStatus AfterAssemblyDataReceived(CipInstance *instance) {
  EipStatus status = kEipStatusOk;
  // printf("Assembly Data Received. instance number=%d\n", pa_pstInstance->instance_number);

  /*handle the data received e.g., update outputs of the device */
  switch (instance->instance_number) {
    case DEMO_APP_OUTPUT_ASSEMBLY_NUM:
      /* Data for the output assembly has been received.
       * Mirror it to the inputs */
      memcpy( &g_assembly_data064[0], &g_assembly_data096[0],
              sizeof(g_assembly_data064) );
      break;
    case DEMO_APP_EXPLICT_ASSEMBLY_NUM:
      /* do something interesting with the new data from
       * the explicit set-data-attribute message */
      break;
    case DEMO_APP_CONFIG_ASSEMBLY_NUM:
      /* Add here code to handle configuration data and check if it is ok
       * The demo application does not handle config data.
       * However in order to pass the test we accept any data given.
       * EIP_ERROR
       */
      status = kEipStatusOk;
      break;
    case LCU_APP_OUTPUT_ASSEMBLY_NUM:
      printf("LCU_APP_OUTPUT_ASSEMBLY Data Received. instance number=%d, data=%s\n", pa_pstInstance->instance_number, &g_assembly_data_tcms_to_lcu[0]);
      // TODO: write into redis
      break;
    default:
      OPENER_TRACE_INFO(
        "Unknown assembly instance ind AfterAssemblyDataReceived");
      break;
  }
  return status;
}

EipBool8 BeforeAssemblyDataSend(CipInstance *pa_pstInstance) {
  /*update data to be sent e.g., read inputs of the device */
  /*In this sample app we mirror the data from out to inputs on data receive
   * therefore we need nothing to do here. Just return true to inform that
   * the data is new.
   */
  switch (pa_pstInstance->instance_number) {
    case DEMO_APP_EXPLICT_ASSEMBLY_NUM: {
      /* do something interesting with the existing data
       * for the explicit get-data-attribute message */
      char *str = "Hello World";
      int len = strlen(str);
      memcpy(&g_assembly_data09A[0], str, 32);
      break;
    }
    case LCU_APP_INPUT_ASSEMBLY_NUM: {
      // TODO: read from redis
      printf("LCU_APP_INPUT_ASSEMBLY Data Sent. instance number=%d, data=%s\n", pa_pstInstance->instance_number, &g_assembly_data_lcu_to_tcms[0]);
      break;
    }
  }
  
  return true;
}

EipStatus ResetDevice(void) {
  /* add reset code here*/
  CloseAllConnections();
  CipQosUpdateUsedSetQosValues();
  return kEipStatusOk;
}

EipStatus ResetDeviceToInitialConfiguration(void) {
  /*rest the parameters */
  g_tcpip.encapsulation_inactivity_timeout = 120;
  CipQosResetAttributesToDefaultValues();
  /*than perform device reset*/
  ResetDevice();
  return kEipStatusOk;
}

void *
CipCalloc(size_t number_of_elements,
          size_t size_of_element) {
  return calloc(number_of_elements, size_of_element);
}

void CipFree(void *data) {
  free(data);
}

void RunIdleChanged(EipUint32 run_idle_value) {
  OPENER_TRACE_INFO("Run/Idle handler triggered\n");
  if( (0x0001 & run_idle_value) == 1 ) {
    CipIdentitySetExtendedDeviceStatus(kAtLeastOneIoConnectionInRunMode);
  } else {
    CipIdentitySetExtendedDeviceStatus(
      kAtLeastOneIoConnectionEstablishedAllInIdleMode);
  }
  (void) run_idle_value;
}

