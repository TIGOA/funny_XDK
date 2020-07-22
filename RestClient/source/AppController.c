/* own header files */
#include "XdkAppInfo.h"

#undef BCDS_MODULE_ID  /* Module ID define before including Basics package*/
#define BCDS_MODULE_ID XDK_APP_MODULE_ID_APP_CONTROLLER

#include "AppController.h"
//system header
#include <stdio.h>
#include <string.h>
//for wifi
#include "XDK_WLAN.h"
#include "XDK_ServalPAL.h"
#include "XDK_HTTPRestClient.h"
#include "BCDS_BSP_Board.h"

#include "BCDS_WlanNetworkConfig.h"
#include "BCDS_WlanNetworkConnect.h"

#include "XDK_Sensor.h"
#include "XDK_Utils.h"
#include "BCDS_Assert.h"
/* additional interface header files */
#include "BCDS_CmdProcessor.h"
#include "FreeRTOS.h"
#include "task.h"



static xTaskHandle AppControllerHandle = NULL; /**< OS thread handle for Application controller */

static CmdProcessor_T * AppCmdProcessor; /**< Handle to store the main Command processor handle to be reused by ServalPAL thread */


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

/*--------------------------------HTTP Client---------------------------------------------*/
//setup wlan , connect to home router
static WLAN_Setup_T WLANSetupInfo ={
	.IsEnterprise = false,
	.IsHostPgmEnabled = false,

	//home wifi
	.SSID = "XDK",          // wlan ssid
	.Username = "!Gty54321", /* wlan password and username are the same*/
	.Password = "!Gty54321",

//	//hotspot
//	.SSID = "Aniu",          // wlan ssid
//		.Username = "11111111", /* wlan password and username are the same*/
//		.Password = "11111111",

	.IsStatic = false,
	.IpAddr = XDK_NETWORK_IPV4(0, 0, 0, 0),
	.GwAddr = XDK_NETWORK_IPV4(0, 0, 0, 0),
	.DnsAddr = XDK_NETWORK_IPV4(0, 0, 0, 0),
	.Mask = XDK_NETWORK_IPV4(0, 0, 0, 0),
};/* WLAN setup parameters */


// HTTP_SECURE_ENABLE is Set to Use HTTP With Security
//#define HTTP_SECURE_ENABLE              UINT32_C(1)
//setup rest client
static HTTPRestClient_Setup_T HTTPRestClientSetupInfo ={
	.IsSecure = false,
};

//define the downloaded package size from server
#define REQUEST_MAX_DOWNLOAD_SIZE       UINT32_C(2048)

//setup rest client, configuration
//define destination server URL and Port
static HTTPRestClient_Config_T HTTPRestClientConfigInfo ={
	.IsSecure = false,
	//echo server, for testing
	.DestinationServerUrl = "postman-echo.com",
	.DestinationServerPort = UINT16_C(80),
	.RequestMaxDownloadSize = REQUEST_MAX_DOWNLOAD_SIZE,
}; /* HTTP rest client configuration parameters */


//define POST path
//define GET path
#define DEST_POST_PATH                  "/post"
#define DEST_GET_PATH                  "/get"
//define headers
#define POST_REQUEST_CUSTOM_HEADER_0    ""
#define POST_REQUEST_CUSTOM_HEADER_1    ""

//define Post information
//initiate payload , payload length and headers

static HTTPRestClient_Post_T HTTPRestClientPostInfo ={
				.Payload = "",
				.PayloadLength = 0,
				.Url = DEST_POST_PATH,

				.RequestCustomHeader0 = POST_REQUEST_CUSTOM_HEADER_0,
				.RequestCustomHeader1 = POST_REQUEST_CUSTOM_HEADER_1,
};


#define APP_RESPONSE_FROM_HTTP_SERVER_POST_TIMEOUT	  UINT32_C(1000)/**< Timeout for completion of HTTP rest client POST */
//#define APP_RESPONSE_FROM_HTTP_SERVER_GET_TIMEOUT	  UINT32_C(1000)/**< Timeout for completion of HTTP rest client GET */

// ==============================HTTP GET RESQUEST ============================

/**
 * @brief   The HTTP GET request response callback.

 * @param [in] responseContent
 * Pointer to the GET request response
 *
 * @param [in] responseContentLen
 * Length of the GET request response
 *
 * @param [in] isLastMessage
 * Boolean to represent if it is the last part of the ongoing message
 */
//
//
//static void HTTPRestClientGetCB(const char* responseContent, uint32_t responseContentLen, bool isLastMessage);
//
//// rest client Get information setup
//static HTTPRestClient_Get_T HTTPRestClientGetInfo =
//        {
//                .Url = DEST_GET_PATH,
//                .GetCB = HTTPRestClientGetCB,
//                .GetOffset = 0UL,
//        }; /**< HTTP rest client GET parameters */
///** Refer function definition for description */
//static void HTTPRestClientGetCB(const char* responseContent, uint32_t responseContentLen, bool isLastMessage)
//{
//    BCDS_UNUSED(isLastMessage);
//
//    if ((NULL != responseContent) && (0UL != responseContentLen))
//    {
//        printf("HTTPRestClientGetCB: HTTP GET response: %.*s\r\n", (int) responseContentLen, responseContent);
//    }
//}


// ========================================================================
//connectivity check
static Retcode_T AppControllerValidateWLANConnectivity(void){
	Retcode_T retcode = RETCODE_OK;
	WlanNetworkConnect_IpStatus_T nwStatus;
	nwStatus = WlanNetworkConnect_GetIpStatus();

	if (WLANNWCT_IPSTATUS_CT_AQRD != nwStatus){
		retcode = WLAN_Reconnect();
	}
	return retcode; //wlan connected !

}

/*
 * @brief To setup the necessary modules for the application
 * - WLAN
 * - ServalPAL
 * - SNTP (if HTTPS)
 * - HTTP rest client
 */
//put every setup methods into one HttpClientSetup method
// call this method one time in AppControllerSetup method !

static Retcode_T HttpClientSetup(void){
	Retcode_T retcode = WLAN_Setup(&WLANSetupInfo);
	if (RETCODE_OK == retcode){
		retcode = ServalPAL_Setup(AppCmdProcessor);
	}
	if (RETCODE_OK == retcode){
		retcode = HTTPRestClient_Setup(&HTTPRestClientSetupInfo);
	}
	return retcode;
}

//put every enable methods into one HttpClientEnable method
// call this method one time in AppControllerEnable method !

static Retcode_T HttpClientEnable(void){
	Retcode_T retcode = WLAN_Enable();
	if (RETCODE_OK == retcode){
		retcode = ServalPAL_Enable();
	}
	if (RETCODE_OK == retcode){
		retcode = HTTPRestClient_Enable();
	}
	return retcode;
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
}
//define the ids for display sequence
ids = {

	.Accel = {
		.X = 1,
		.Y = 2,
		.Z = 3,
	},

	.Mag = {
		.X = 4,
		.Y = 5,
		.Z = 6,
		.R = 7,
	},

	.Gyro = {
		.X = 8,
		.Y = 9,
		.Z = 10
	},
	.RH = 11,
	.Pressure = 12,
	.Temp = 13,
	.Light =14,
	.Noise = 15
};

//method for post sensor values in one cycle
static Retcode_T PostSensorValues(Sensor_Value_T* sensorValue)
{
	Retcode_T retcode = RETCODE_OK;

	/* Check whether the WLAN network connection is available */
	retcode = AppControllerValidateWLANConnectivity();

	if (RETCODE_OK == retcode){
		//payload
		char body[512] = "{\"values\":[";
		//payload length
		unsigned len = sizeof("{\"values\":[") - 1;

		char buf[64] = "";
		unsigned buf_len = 0;

		char url[128];
		memset(url, 0x00, sizeof(url));

		sprintf(url, "/update~~~~~");

		if (SensorSetup.Enable.Accel){
					 buf_len = sprintf(buf, "{\"id\":%d,\"accel.x\":%ld},",
						ids.Accel.X,
						(long int) sensorValue->Accel.X
					);
					strcpy(&body[len], buf);
					len+=buf_len;
					buf_len = sprintf(buf, "{\"id\":%d,\"accel.y\":%ld},",
						ids.Accel.Y,
						(long int) sensorValue->Accel.Y
					);
					strcpy(&body[len], buf);
					len+=buf_len;
					buf_len = sprintf(buf, "{\"id\":%d,\"accel.z\":%ld},",
						ids.Accel.Z,
						(long int) sensorValue->Accel.Z
					);
					strcpy(&body[len], buf);
					len+=buf_len;
				}

				if (SensorSetup.Enable.Mag){
					buf_len = sprintf(buf, "{\"id\":%d,\"Mag.x\":%ld},",
						ids.Mag.X,
						(long int)sensorValue->Mag.X
					);
					strcpy(&body[len], buf);
					len+=buf_len;
					buf_len = sprintf(buf, "{\"id\":%d,\"Mag.y\":%ld},",
						ids.Mag.Y,
						(long int)sensorValue->Mag.Y
					);
					strcpy(&body[len], buf);
					len+=buf_len;
					buf_len = sprintf(buf, "{\"id\":%d,\"Mag.z\":%ld},",
						ids.Mag.Z,
						(long int)sensorValue->Mag.Z
					);
					strcpy(&body[len], buf);
					len+=buf_len;
					buf_len = sprintf(buf, "{\"id\":%d,\"Mag.R\":%ld},",
						ids.Mag.R,
						(long int)sensorValue->Mag.R
					);
					strcpy(&body[len], buf);
					len+=buf_len;
				}
				if (SensorSetup.Enable.Gyro){
					buf_len = sprintf(buf, "{\"id\":%d,\"Gyro.x\":%ld},",
						ids.Gyro.X,
						(long int)sensorValue->Gyro.X
					);
					strcpy(&body[len], buf);
					len+=buf_len;
					buf_len = sprintf(buf, "{\"id\":%d,\"Gyro.y\":%ld},",
						ids.Gyro.Y,
						(long int)sensorValue->Gyro.Y
					);
					strcpy(&body[len], buf);
					len+=buf_len;
					buf_len = sprintf(buf, "{\"id\":%d,\"Gyro.z\":%ld},",
						ids.Gyro.Z,
						(long int)sensorValue->Gyro.Z
					);
					strcpy(&body[len], buf);
					len+=buf_len;
				}


				if (SensorSetup.Enable.Humidity){
					buf_len = sprintf(buf, "{\"id\":%d,\"humidity\":%ld},",
						ids.RH,
						(long int) sensorValue->RH
					);
					strcpy(&body[len], buf);
					len+=buf_len;
				}
				if (SensorSetup.Enable.Pressure){
					buf_len = sprintf(buf, "{\"id\":%d,\"pressure\":%ld},",
						ids.Pressure,
						(long int) sensorValue->Pressure
					);
					strcpy(&body[len], buf);
					len+=buf_len;
				}

				//temperature need to be
				// (sensorValue.Temp /= 1000)
				if (SensorSetup.Enable.Temp){
					buf_len = sprintf(buf, "{\"id\":%d,\"temp\":%ld},",
						ids.Temp,
						(long int) sensorValue->Temp
					);
					strcpy(&body[len], buf);
					len+=buf_len;
				}

				if (SensorSetup.Enable.Light){
					buf_len = sprintf(buf, "{\"id\":%d,\"light\":%d},",
						ids.Light,
						(unsigned int)sensorValue->Light
					);
					strcpy(&body[len], buf);
					len+=buf_len;
				}
				if (SensorSetup.Enable.Noise){
					buf_len = sprintf(buf, "{\"id\":%d,\"Noise\":%f},",
						ids.Noise,
						sensorValue->Noise
					);
					strcpy(&body[len], buf);
					len+=buf_len;
				}
		// delete last comma
		body[len-1] = 0x00;
		len-=1;
		strcpy(&body[len], "]}");
		len += sizeof("]}") - 1;

		puts("request send.");

		HTTPRestClientPostInfo.Payload = body;
		HTTPRestClientPostInfo.PayloadLength = len;

		retcode = HTTPRestClient_Post(&HTTPRestClientConfigInfo, &HTTPRestClientPostInfo, APP_RESPONSE_FROM_HTTP_SERVER_POST_TIMEOUT);
	}
	return retcode;
}


/*------------------------------Main Controller-------------------------------------------*/
#define APP_CONTROLLER_TX_DELAY UINT32_C(100)

static void AppControllerFire(void* pvParameters){
	BCDS_UNUSED(pvParameters);

	Retcode_T retcode = RETCODE_OK;


	while (1){
		retcode = PostSensorValues(GetSensorValues());
//		if (RETCODE_OK == retcode) {
//			//delay 1 secounds
//			vTaskDelay(UINT32_C(1000));
//			/* Do a HTTP rest client GET */
//			retcode = HTTPRestClient_Get(&HTTPRestClientConfigInfo,
//					&HTTPRestClientGetInfo,
//					APP_RESPONSE_FROM_HTTP_SERVER_GET_TIMEOUT);
//		}
		if (RETCODE_OK != retcode){
			Retcode_RaiseError(retcode);
		}
		vTaskDelay(pdMS_TO_TICKS(APP_CONTROLLER_TX_DELAY));
	}
}

static void AppControllerEnable(void * param1, uint32_t param2) {
	BCDS_UNUSED(param1);
	BCDS_UNUSED(param2);

	Retcode_T retcode = RETCODE_OK;

	retcode = HttpClientEnable();
	if (RETCODE_OK == retcode) {
		retcode = MySensorEnable();
	}

	if (RETCODE_OK == retcode) {
		if (pdPASS
				!= xTaskCreate(AppControllerFire,
						(const char * const ) "AppController",
						TASK_STACK_SIZE_APP_CONTROLLER, NULL,
						TASK_PRIO_APP_CONTROLLER, &AppControllerHandle)) {
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

static void AppControllerSetup(void * param1, uint32_t param2){
	BCDS_UNUSED(param1);
	BCDS_UNUSED(param2);
	Retcode_T retcode = RETCODE_OK;

	retcode = HttpClientSetup();
	if (RETCODE_OK == retcode){
		retcode = MySensorSetup();
	}

	if (RETCODE_OK == retcode){
		retcode = CmdProcessor_Enqueue(AppCmdProcessor, AppControllerEnable, NULL, UINT32_C(0));
	}
	if (RETCODE_OK != retcode){
		printf("AppControllerSetup : Failed \r\n");
		Retcode_RaiseError(retcode);
		assert(0); /* To provide LED indication for the user */
	}
}

void AppController_Init(void * cmdProcessorHandle, uint32_t param2){
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
