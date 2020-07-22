/*----------------------------------------------------------------------------
Coap Server
 --------------------------------------------------------------------------- |
 INCLUDES & DEFINES ******************************************************** |
 -------------------------------------------------------------------------- */

/* own header files */
#include "XdkAppInfo.h"
#undef BCDS_MODULE_ID  /* Module ID define before including Basics package*/
#define BCDS_MODULE_ID XDK_APP_MODULE_ID_APP_CONTROLLER

/* system header files */
#include <stdio.h>

/* additional interface header files */
#include "BCDS_CmdProcessor.h"
#include "FreeRTOS.h"

/* for wifi */
#include "BCDS_WlanConnect.h"
#include "BCDS_NetworkConfig.h"
#include "BCDS_ServalPal.h"
#include "BCDS_ServalPalWiFi.h"

/* for coap-server */
#include "Serval_Coap.h"
#include "Serval_CoapServer.h"

/* --------------------------------------------------------------------------- |
 * HANDLES ******************************************************************* |
 * -------------------------------------------------------------------------- */

static CmdProcessor_T * AppCmdProcessor;

void networkSetup(void)
{
//home router
    WlanConnect_SSID_T connectSSID = (WlanConnect_SSID_T) "aniu";
    WlanConnect_PassPhrase_T connectPassPhrase =
            (WlanConnect_PassPhrase_T) "!Gty54321";

//	//hotspot
//    WlanConnect_SSID_T connectSSID = (WlanConnect_SSID_T) "Aniu";
//      WlanConnect_PassPhrase_T connectPassPhrase =
//              (WlanConnect_PassPhrase_T) "11111111";


    WlanConnect_Init();
    NetworkConfig_SetIpDhcp(0);
    WlanConnect_WPA(connectSSID, connectPassPhrase, NULL);
    printf("networkSetup success \r\n");

}

void createCoapResponse(Msg_T *msg_ptr, char const *payload_ptr, uint8_t responseCode)
{
    CoapSerializer_T serializer;
    CoapSerializer_setup(&serializer, msg_ptr, RESPONSE);
    CoapSerializer_setCode(&serializer, msg_ptr, responseCode);
    CoapSerializer_setConfirmable(msg_ptr, false);
    CoapSerializer_reuseToken(&serializer, msg_ptr);

    CoapOption_T uriOption;
    uriOption.OptionNumber = Coap_Options[COAP_URI_PATH];
    uriOption.value = (uint8_t*) "test";
    uriOption.length = 4;
    CoapSerializer_serializeOption(&serializer, msg_ptr, &uriOption);

    CoapSerializer_setEndOfOptions(&serializer, msg_ptr);
    // serialize the payload
    uint8_t resource[30] = { 0 };
    uint8_t resourceLength = strlen(payload_ptr);
    memcpy(resource, payload_ptr, resourceLength + 1);
    CoapSerializer_serializePayload(&serializer, msg_ptr, resource, resourceLength);
    printf("createCoapResponse success \r\n");

}

Retcode_T sendingCallback(Callable_T *callable_ptr, Retcode_T status)
{
    (void) callable_ptr;
    (void) status;
    printf("sendingCallback success \r\n");
    return RC_OK;
}

void sendCoapResponse(Msg_T *message, char const* payloard_ptr)
{
    createCoapResponse(message, payloard_ptr, Coap_Codes[COAP_CONTENT]);
    Callable_T *alpCallable_ptr = Msg_defineCallback(message,
            (CallableFunc_T) sendingCallback);
    CoapServer_respond(message, alpCallable_ptr);
    printf("sendCoapResponse success \r\n");

}

void parseCoapRequest(Msg_T *msg_ptr, uint8_t *code)
{
    CoapParser_T parser;
    CoapParser_setup(&parser, msg_ptr);
    *code = CoapParser_getCode(msg_ptr);
    const uint8_t* payload;
    uint8_t payloadlen;
    CoapParser_getPayload(&parser, &payload, &payloadlen);
    printf("Incoming Coap request: %s \n\r", payload);
}

Retcode_T coapReceiveCallback(Msg_T *msg_ptr, Retcode_T status)
{

    printf("Request received!\n"); //Printf to ensure that the callback function is called

    uint8_t code = 0;
    parseCoapRequest(msg_ptr, &code);
    if (code == Coap_Codes[COAP_POST])
    {
        sendCoapResponse(msg_ptr, "Hello Client, POST received");
    }
    else if (code == Coap_Codes[COAP_GET])
    {
        sendCoapResponse(msg_ptr, "Hello Client, GET Received!");
    }
    return status;
}

/* --------------------------------------------------------------------------- |
 * BOOTING- AND SETUP FUNCTIONS ********************************************** |
 * -------------------------------------------------------------------------- */

static void AppControllerEnable(void * param1, uint32_t param2)
{
    BCDS_UNUSED(param1);
    BCDS_UNUSED(param2);
    Retcode_T retcode = RETCODE_OK;

    vTaskDelay(5000);
    if (RETCODE_OK == retcode)
    {
        retcode = ServalPAL_Enable();
    }
    Ip_Port_T serverPort = Ip_convertIntToPort((uint16_t) 5683);

    CoapServer_startInstance(serverPort, (CoapAppReqCallback_T) &coapReceiveCallback);

    NetworkConfig_IpSettings_T myIp;
    NetworkConfig_GetIpSettings(&myIp);
    vTaskDelay(5000);
    printf("The IP was retrieved: %u.%u.%u.%u \n\r",
            (unsigned int) (NetworkConfig_Ipv4Byte(myIp.ipV4, 3)),
            (unsigned int) (NetworkConfig_Ipv4Byte(myIp.ipV4, 2)),
            (unsigned int) (NetworkConfig_Ipv4Byte(myIp.ipV4, 1)),
            (unsigned int) (NetworkConfig_Ipv4Byte(myIp.ipV4, 0)));
}

static void AppControllerSetup(void * param1, uint32_t param2)
{
    BCDS_UNUSED(param1);
    BCDS_UNUSED(param2);
    Retcode_T retcode = RETCODE_OK;

    /* Setup the necessary modules required for the application */
    networkSetup();
    if (RETCODE_OK == retcode)
    {
        retcode = ServalPAL_Setup(AppCmdProcessor);
    }
    CoapServer_initialize();
    retcode = CmdProcessor_Enqueue(AppCmdProcessor, AppControllerEnable, NULL, UINT32_C(0));
    if (RETCODE_OK != retcode)
    {
        printf("AppControllerSetup : Failed \r\n");
        Retcode_RaiseError(retcode);
        assert(0); /* To provide LED indication for the user */
    }
}

void AppController_Init(void * cmdProcessorHandle, uint32_t param2)
{
    BCDS_UNUSED(param2);

    Retcode_T retcode = RETCODE_OK;

    if (cmdProcessorHandle == NULL)
    {
        printf("AppController_Init : Command processor handle is NULL \r\n");
        retcode = RETCODE(RETCODE_SEVERITY_ERROR, RETCODE_NULL_POINTER);
    }
    else
    {
        AppCmdProcessor = (CmdProcessor_T *) cmdProcessorHandle;
        retcode = CmdProcessor_Enqueue(AppCmdProcessor, AppControllerSetup, NULL, UINT32_C(0));
    }

    if (RETCODE_OK != retcode)
    {
        Retcode_RaiseError(retcode);
        assert(0); /* To provide LED indication for the user */
    }
}

/****************************************************************************/
