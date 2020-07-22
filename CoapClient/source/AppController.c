
/* own header files */
#include "XdkAppInfo.h"
#undef BCDS_MODULE_ID  /* Module ID define before including Basics package*/
#define BCDS_MODULE_ID XDK_APP_MODULE_ID_APP_CONTROLLER
#include "AppController.h"
/* system header files */
#include <stdio.h>
#include <string.h>
/* additional interface header files */
#include "BCDS_BSP_Board.h"
#include "BCDS_CmdProcessor.h"
#include "FreeRTOS.h"

/* for wifi */
#include "BCDS_WlanConnect.h"
#include "BCDS_NetworkConfig.h"
#include "BCDS_ServalPal.h"
#include "BCDS_ServalPalWiFi.h"

/* for coap-client */
#include "Serval_Coap.h"
#include "Serval_CoapClient.h"
#include "Serval_Network.h"
#include "XDK_ServalPAL.h"
//#include "timers.h"
#include "XDK_Sensor.h"
#include "XDK_Utils.h"
#include "BCDS_Assert.h"
#include "task.h"

/* --------------------------------------------------------------------------- |
 * HANDLES ******************************************************************* |
 * -------------------------------------------------------------------------- */
static xTaskHandle coapHandle = NULL;
static CmdProcessor_T * AppCmdProcessor;/**< Handle to store the main Command processor handle to be used by run-time event driven threads */

/*----------------------------------MySensor.c-------------------------------*/
#define APP_TEMPERATURE_OFFSET_CORRECTION   (-3459)
//correction temperature
#define APP_CURRENT_RATED_TRANSFORMATION_RATIO	  (0)

//callback function for accelerometer and light
//those function can be interupted
static void AccelAppCallback(void *param1, uint32_t param2){
	BCDS_UNUSED(param1);
	BCDS_UNUSED(param2);
}

static void LightAppCallback(void *param1, uint32_t param2){
	BCDS_UNUSED(param1);
	BCDS_UNUSED(param2);
}



//setup sensor , enable all sensors
Sensor_Setup_T SensorSetup ={
	.CmdProcessorHandle = NULL,
	.Enable = {
		.Accel = true,
		.Mag = true,
		.Gyro = true,
		.Humidity = true,
		.Temp = true,
		.Pressure = true,
		.Light = true,
		.Noise = false,
	},
	//configuration for all sensors
	.Config = {
		.Accel = {
			.Type = SENSOR_ACCEL_BMA280,
			.IsRawData = false,
			.IsInteruptEnabled = true,
			.Callback = AccelAppCallback,
		},
		.Gyro = {
			.Type = SENSOR_GYRO_BMG160,
			.IsRawData = false,
		},
		.Mag = {
			.IsRawData = false,
		},
		.Light = {
			.IsInteruptEnabled = true,
			.Callback = LightAppCallback,
		},
		.Temp ={
			.OffsetCorrection = APP_TEMPERATURE_OFFSET_CORRECTION, //(temperature value of sensor -3459) grad
		},
	},
};/* Sensor setup parameters */

//method for setup sensor
//return retcode
static Retcode_T MySensorSetup(void){
	SensorSetup.CmdProcessorHandle = AppCmdProcessor;
	Retcode_T retcode = Sensor_Setup(&SensorSetup);
	return retcode;
}
//method for enable sensor
//return retcode
static Retcode_T MySensorEnable(void){
	Retcode_T retcode = Sensor_Enable();
	return retcode;
}

//get sensor value and return reference
static Sensor_Value_T* GetSensorValues(void){
	static Sensor_Value_T sensorValue;

	memset(&sensorValue, 0x00, sizeof(sensorValue));

	Sensor_GetData(&sensorValue);

	return &sensorValue;
}

// structure for sensor values
struct
{
	unsigned RH;      //Humidity
	unsigned Pressure; //pressure
	unsigned Temp;  //temperature
	unsigned Light; // light
	unsigned Noise; //noise

	//accelometer
	struct {
		unsigned X;
		unsigned Y;
		unsigned Z;
	} Accel;
//magnetometer
	struct
	{
		unsigned X;
		unsigned Y;
		unsigned Z;
		unsigned R;
	}
	Mag;
//gyroscope
	struct {
		unsigned X;
		unsigned Y;
		unsigned Z;
	}
	Gyro;
};

Ip_Address_T ip;
Ip_Port_T serverPort;

void networkSetup(void)
{
    WlanConnect_SSID_T connectSSID = (WlanConnect_SSID_T) "XDK";
    WlanConnect_PassPhrase_T connectPassPhrase = (WlanConnect_PassPhrase_T) "!Gty54321";
    WlanConnect_Init();
    NetworkConfig_SetIpDhcp(0);
    WlanConnect_WPA(connectSSID, connectPassPhrase, NULL);
    printf("connected to WIFI network \n\r");
}

retcode_t responseCallback(CoapSession_T *coapSession, Msg_T *msg_ptr, retcode_t status)
{
    (void) coapSession;
    CoapParser_T parser;

    const uint8_t *payload;
    uint8_t payload_length;
    CoapParser_setup(&parser, msg_ptr);
    CoapParser_getPayload(&parser, &payload, &payload_length);
    printf("Coap Answer to Request: %s \n\r", payload);
    printf("responseCallback received \n\r");
    return status;
}
Retcode_T sendingCallback(Callable_T *callable_ptr, Retcode_T status)
{
    (void) callable_ptr;
    printf("sendingCallback success \n\r");
    return status;
}
/**/
void serializeRequest(Msg_T *msg_ptr, uint8_t requestCode,   char*payload_ptr)
{
    CoapSerializer_T serializer;
    CoapSerializer_setup(&serializer, msg_ptr, REQUEST);
    CoapSerializer_setCode(&serializer, msg_ptr, requestCode);
    CoapSerializer_setConfirmable(msg_ptr, false);
    CoapSerializer_serializeToken(&serializer, msg_ptr, NULL, 0);
    // call this after all options have been serialized (in this case: none)
    CoapSerializer_setEndOfOptions(&serializer, msg_ptr);
    // serialize the payload
    uint8_t resource[255] = { 0 };
    uint8_t resourceLength = strlen(payload_ptr);
    memcpy(resource, payload_ptr, resourceLength + 1);
    CoapSerializer_serializePayload(&serializer, msg_ptr, resource, resourceLength);
    printf("serializeRequest done \n\r");
}

//Post environmental values to Coap Server
static Retcode_T PostSensorValuesPart1(Sensor_Value_T* sensorValue) {
	Retcode_T retcode = RETCODE_OK;

	if (RETCODE_OK == retcode) {
		//payload
		char body[512] = "{\"values\":[";
		//payload length
		unsigned len = sizeof("{\"values\":[") - 1;

		char buf[64] = "";
		unsigned buf_len = 0;

		char url[128];
		memset(url, 0x00, sizeof(url));
		sprintf(url, "/update~~~~~");

//		if (SensorSetup.Enable.Accel) {
//			buf_len = sprintf(buf, "{\"accel.x\":%ld},",
//					(long int) sensorValue->Accel.X);
//			strcpy(&body[len], buf);
//			len += buf_len;
//			buf_len = sprintf(buf, "{\"accel.y\":%ld},",
//
//			(long int) sensorValue->Accel.Y);
//			strcpy(&body[len], buf);
//			len += buf_len;
//			buf_len = sprintf(buf, "{\"accel.z\":%ld},",
//
//			(long int) sensorValue->Accel.Z);
//			strcpy(&body[len], buf);
//			len += buf_len;
//		}
//
//		if (SensorSetup.Enable.Mag) {
//			buf_len = sprintf(buf, "{\"Mag.x\":%ld},",
//
//			(long int) sensorValue->Mag.X);
//			strcpy(&body[len], buf);
//			len += buf_len;
//			buf_len = sprintf(buf, "{\"Mag.y\":%ld},",
//					(long int) sensorValue->Mag.Y);
//			strcpy(&body[len], buf);
//			len += buf_len;
//			buf_len = sprintf(buf, "{\"Mag.z\":%ld},",
//					(long int) sensorValue->Mag.Z);
//			strcpy(&body[len], buf);
//			len += buf_len;
//			buf_len = sprintf(buf, "{\"Mag.R\":%ld},",
//					(long int) sensorValue->Mag.R);
//			strcpy(&body[len], buf);
//			len += buf_len;
//		}
//		if (SensorSetup.Enable.Gyro) {
//			buf_len = sprintf(buf, "{\"Gyro.x\":%ld},",
//					(long int) sensorValue->Gyro.X);
//			strcpy(&body[len], buf);
//			len += buf_len;
//			buf_len = sprintf(buf, "{\"Gyro.y\":%ld},",
//					(long int) sensorValue->Gyro.Y);
//			strcpy(&body[len], buf);
//			len += buf_len;
//			buf_len = sprintf(buf, "{\"Gyro.z\":%ld},",
//					(long int) sensorValue->Gyro.Z);
//			strcpy(&body[len], buf);
//			len += buf_len;
//		}

		if (SensorSetup.Enable.Humidity) {
			buf_len = sprintf(buf, "{\"humidity\":%ld},",
					(long int) sensorValue->RH);
			strcpy(&body[len], buf);
			len += buf_len;
		}
		if (SensorSetup.Enable.Pressure) {
			buf_len = sprintf(buf, "{\"pressure\":%ld},",
					(long int) sensorValue->Pressure);
			strcpy(&body[len], buf);
			len += buf_len;
		}
		//temperature need to be
		// (sensorValue.Temp /= 1000)
		if (SensorSetup.Enable.Temp) {
			buf_len = sprintf(buf, "{\"temp\":%ld},",
					(long int) sensorValue->Temp);
			strcpy(&body[len], buf);
			len += buf_len;
		}
		if (SensorSetup.Enable.Light) {
			buf_len = sprintf(buf, "{\"light\":%d},",
					(unsigned int) sensorValue->Light);
			strcpy(&body[len], buf);
			len += buf_len;
		}
//				if (SensorSetup.Enable.Noise){
//					buf_len = sprintf(buf, "{\"id\":%d,\"Noise\":%f},",
//						ids.Noise,
//						sensorValue->Noise
//					);
//					strcpy(&body[len], buf);
//					len+=buf_len;
//				}
		// delete last comma
		body[len - 1] = 0x00;
		len -= 1;
		strcpy(&body[len], "]}");
		len += sizeof("]}") - 1;
		puts("request send.");
//数据读取完毕
//—————————————————————————————————————sensor completed reading
// payload in char "body", payload length in unsigned "len"

		Msg_T *msg_ptr;
		char*payload= (char*) malloc(512 * sizeof(char));
		strcpy(payload, body); //拷贝到payload
		CoapClient_initReqMsg(&ip, serverPort, &msg_ptr); //coap初始化信息
		serializeRequest(msg_ptr, Coap_Codes[COAP_POST /*COAP_GET*/], payload);  //coap post数据
		Callable_T *alpCallable_ptr = Msg_defineCallback(msg_ptr,
				(CallableFunc_T) sendingCallback);
		CoapClient_request(msg_ptr, alpCallable_ptr, &responseCallback);
		  free(payload);
	//	  msg_ptr=NULL;
	//alpCallable_ptr=NULL;
		vTaskDelay(1000);
//vtaskdelay(3000);

	//	printf(body);
	}
	return retcode;
}

//part 2
//post accel. Mag. Gyro. values to Coap Server
//static Retcode_T PostSensorValuesPart2(Sensor_Value_T* sensorValue) {
//	Retcode_T retcode = RETCODE_OK;
//
//	if (RETCODE_OK == retcode) {
//		//payload
//		char body[512] = "{\"values\":[";
//		//payload length
//		unsigned len = sizeof("{\"values\":[") - 1;
//
//		char buf[64] = "";
//		unsigned buf_len = 0;
//
//		char url[128];
//		memset(url, 0x00, sizeof(url));
//		sprintf(url, "/update~~~~~");
//
//		if (SensorSetup.Enable.Accel) {
//			buf_len = sprintf(buf, "{\"accel.x\":%ld},",
//					(long int) sensorValue->Accel.X);
//			strcpy(&body[len], buf);
//			len += buf_len;
//			buf_len = sprintf(buf, "{\"accel.y\":%ld},",
//
//			(long int) sensorValue->Accel.Y);
//			strcpy(&body[len], buf);
//			len += buf_len;
//			buf_len = sprintf(buf, "{\"accel.z\":%ld},",
//
//			(long int) sensorValue->Accel.Z);
//			strcpy(&body[len], buf);
//			len += buf_len;
//		}
//
//		if (SensorSetup.Enable.Mag) {
//			buf_len = sprintf(buf, "{\"Mag.x\":%ld},",
//
//			(long int) sensorValue->Mag.X);
//			strcpy(&body[len], buf);
//			len += buf_len;
//			buf_len = sprintf(buf, "{\"Mag.y\":%ld},",
//					(long int) sensorValue->Mag.Y);
//			strcpy(&body[len], buf);
//			len += buf_len;
//			buf_len = sprintf(buf, "{\"Mag.z\":%ld},",
//					(long int) sensorValue->Mag.Z);
//			strcpy(&body[len], buf);
//			len += buf_len;
//			buf_len = sprintf(buf, "{\"Mag.R\":%ld},",
//					(long int) sensorValue->Mag.R);
//			strcpy(&body[len], buf);
//			len += buf_len;
//		}
//		if (SensorSetup.Enable.Gyro) {
//			buf_len = sprintf(buf, "{\"Gyro.x\":%ld},",
//					(long int) sensorValue->Gyro.X);
//			strcpy(&body[len], buf);
//			len += buf_len;
//			buf_len = sprintf(buf, "{\"Gyro.y\":%ld},",
//					(long int) sensorValue->Gyro.Y);
//			strcpy(&body[len], buf);
//			len += buf_len;
//			buf_len = sprintf(buf, "{\"Gyro.z\":%ld},",
//					(long int) sensorValue->Gyro.Z);
//			strcpy(&body[len], buf);
//			len += buf_len;
//		}
//
//
//		// delete last comma
//		body[len - 1] = 0x00;
//		len -= 1;
//		strcpy(&body[len], "]}");
//		len += sizeof("]}") - 1;
//		puts("request send.");
////数据读取完毕
//		Msg_T *msg_ptr;
//		//char*payload = "this_is_a_default_message";
//		//char*payload;
//		char*payload= (char*) malloc(512 * sizeof(char));
//		strcpy(payload, body); //拷贝到payload
//
////preparing Coap Client
//		//initiat request message, define ip, port,
//		/*This function is called to initiate a request from CoAP client. This function checks for any previous
//		 active transaction with the given server and if not provides the message to be filled for sending the
//		 request.*/
//		CoapClient_initReqMsg(&ip, serverPort, &msg_ptr);
//
//		serializeRequest(msg_ptr, Coap_Codes[COAP_POST /*COAP_GET*/], payload);  //coap post数据
//		Callable_T *alpCallable_ptr = Msg_defineCallback(msg_ptr,
//				(CallableFunc_T) sendingCallback);
//		CoapClient_request(msg_ptr, alpCallable_ptr, &responseCallback);
//		free(payload); //
//		free(msg_ptr);
//		free(alpCallable_ptr);
//		vTaskDelay(1000);
//
//	}
//	return retcode;
//}
/*------------------------------Main Controller-------------------------------------------*/
#define APP_CONTROLLER_TX_DELAY UINT32_C(100)

static void AppControllerFire(void* pvParameters)

{
	BCDS_UNUSED(pvParameters);
	Retcode_T retcode = RETCODE_OK;
	while (1) {
		retcode = PostSensorValuesPart1(GetSensorValues()); //post Environmental values to Coap Server
		if (RETCODE_OK != retcode) {
			Retcode_RaiseError(retcode);
		}
		vTaskDelay(pdMS_TO_TICKS(APP_CONTROLLER_TX_DELAY)); //post accel. Mag. Gyro. values to Coap Server
//		retcode = PostSensorValuesPart2(GetSensorValues());
//		if (RETCODE_OK != retcode) {
//			Retcode_RaiseError(retcode);
//		}
//		vTaskDelay(pdMS_TO_TICKS(APP_CONTROLLER_TX_DELAY));
	}
}

Retcode_T coapReceiveCallback(Msg_T *msg_ptr, Retcode_T status)
{

    CoapParser_T parser;
    const uint8_t *payload;
    uint8_t payload_length;
    CoapParser_setup(&parser, msg_ptr);
    CoapParser_getPayload(&parser, &payload, &payload_length);
    printf("Coap Answer to Request: %s \n\r", payload);
    printf("coapReceiveCallback received \n\r");

    return status;
}


/* --------------------------------------------------------------------------- |
 * EXECUTING FUNCTIONS ******************************************************* |
 * -------------------------------------------------------------------------- */

static void AppControllerEnable(void * param1, uint32_t param2)
{
    BCDS_UNUSED(param1);
    BCDS_UNUSED(param2);
    Retcode_T retcode = RETCODE_OK;

    if (RETCODE_OK == retcode)
    {
        retcode = ServalPAL_Enable();
    }

    if (RETCODE_OK == retcode) {
    		retcode = MySensorEnable();
    	}

    vTaskDelay(5000);
    serverPort = Ip_convertIntToPort((uint16_t) 5683);
    // Server's IP-string
    Ip_convertStringToAddr("192.168.0.93", &ip);
    CoapClient_startInstance();
    // if no response to your request is printed, try a delay or wrap the function in a

    if (RETCODE_OK == retcode) {
		if (pdPASS
				!= xTaskCreate(AppControllerFire,
						(const char * const ) "AppController",
						TASK_STACK_SIZE_APP_CONTROLLER, NULL,
						TASK_PRIO_APP_CONTROLLER, &coapHandle)) {
			retcode = RETCODE(RETCODE_SEVERITY_ERROR, RETCODE_OUT_OF_RESOURCES);
		}
	}

    if (RETCODE_OK != retcode) {
    		printf("AppControllerEnable : Failed \r\n");
    		Retcode_RaiseError(retcode);
    		assert(0); /* To provide LED indication for the user */
    	}

    	Utils_PrintResetCause();
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
	if (RETCODE_OK == retcode)
	{
		retcode = MySensorSetup();
	}

    CoapClient_initialize();

    if (RETCODE_OK == retcode){
    		retcode = CmdProcessor_Enqueue(AppCmdProcessor, AppControllerEnable, NULL, UINT32_C(0));
    	}


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

	if (cmdProcessorHandle == NULL) {
		printf("AppController_Init : Command processor handle is NULL \r\n");
		retcode = RETCODE(RETCODE_SEVERITY_ERROR, RETCODE_NULL_POINTER);
	} else {
		AppCmdProcessor = (CmdProcessor_T *) cmdProcessorHandle;
		retcode = CmdProcessor_Enqueue(AppCmdProcessor, AppControllerSetup,
				NULL, UINT32_C(0));
	}

	if (RETCODE_OK != retcode) {
		Retcode_RaiseError(retcode);
		assert(0); /* To provide LED indication for the user */
	}
}

/****************************************************************************/
