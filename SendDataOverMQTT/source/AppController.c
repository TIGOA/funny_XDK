/* own header files */
#include "XdkAppInfo.h"
#undef BCDS_MODULE_ID  /* Module ID define before including Basics package*/
#define BCDS_MODULE_ID XDK_APP_MODULE_ID_APP_CONTROLLER

/* own header files */
#include "AppController.h"

/* system header files */
#include <stdio.h>

/* additional interface header files */
#include "BCDS_BSP_Board.h"
//#include "BatteryMonitor.h"
#include "BCDS_CmdProcessor.h"
#include "BCDS_WlanNetworkConfig.h"
#include "BCDS_WlanNetworkConnect.h"
#include "BCDS_Assert.h"
#include "XDK_Utils.h"
#include "XDK_WLAN.h"
#include "XDK_MQTT.h"
#include "XDK_Sensor.h"
#include "XDK_SNTP.h"
#include "XDK_ServalPAL.h"
#include "FreeRTOS.h"
#include "task.h"

/* constant definitions ***************************************************** */
//timeout constant
// Timeout in milli-second to await successful publication

#define MQTT_CONNECT_TIMEOUT_IN_MS                  UINT32_C(60000)/**< Macro for MQTT connection timeout in milli-second */

#define MQTT_SUBSCRIBE_TIMEOUT_IN_MS                UINT32_C(20000)/**< Macro for MQTT subscription timeout in milli-second */

#define MQTT_PUBLISH_TIMEOUT_IN_MS                  UINT32_C(1000)/**< Macro for MQTT publication timeout in milli-second */

#define APP_MQTT_DATA_PUBLISH_PERIODICITY   UINT32_C(1000)
/**
 * APP_MQTT_DATA_PUBLISH_PERIODICITY is time for MQTT to publish the sensor data
 */

#define APP_TEMPERATURE_OFFSET_CORRECTION               (-3459)/**< Macro for static temperature offset correction. Self heating, temperature correction factor */
#define APP_MQTT_DATA_BUFFER_SIZE                   UINT32_C(512)/**< macro for data size of incoming subscribed and published messages */


/* wlan setup macros********************************************************** */
//home router
#define WLAN_SSID                           "XDK"
#define WLAN_PSK                            "!Gty54321"
/**
 * WLAN_STATIC_IP is a boolean. If "true" then static IP will be assigned and if "false" then DHCP is used.
 */
#define WLAN_STATIC_IP                      false
/**
 * WLAN_IP_ADDR is the WIFI router WPA/WPA2 static IPv4 IP address (unused if WLAN_STATIC_IP is false)
 * Make sure to update the WLAN_IP_ADDR constant according to your required WIFI network,
 * if WLAN_STATIC_IP is "true".
 */
#define WLAN_IP_ADDR                        XDK_NETWORK_IPV4(0, 0, 0, 0)
/**
 * WLAN_GW_ADDR is the WIFI router WPA/WPA2 static IPv4 gateway address (unused if WLAN_STATIC_IP is false)
 * Make sure to update the WLAN_GW_ADDR constant according to your required WIFI network,
 * if WLAN_STATIC_IP is "true".
 */
#define WLAN_GW_ADDR                        XDK_NETWORK_IPV4(0, 0, 0, 0)
/**
 * WLAN_DNS_ADDR is the WIFI router WPA/WPA2 static IPv4 DNS address (unused if WLAN_STATIC_IP is false)
 * Make sure to update the WLAN_DNS_ADDR constant according to your required WIFI network,
 * if WLAN_STATIC_IP is "true".
 */
#define WLAN_DNS_ADDR                       XDK_NETWORK_IPV4(0, 0, 0, 0)
/**
 * WLAN_MASK is the WIFI router WPA/WPA2 static IPv4 mask address (unused if WLAN_STATIC_IP is false)
 * Make sure to update the WLAN_MASK constant according to your required WIFI network,
 * if WLAN_STATIC_IP is "true".
 */
#define WLAN_MASK                           XDK_NETWORK_IPV4(0, 0, 0, 0)

/* MQTT server configurations *********************************************** */
/**
 * APP_MQTT_BROKER_HOST_URL is the MQTT broker host address URL.
 */
#define APP_MQTT_BROKER_HOST_URL            "test.mosquitto.org"
/**
 * APP_MQTT_BROKER_HOST_PORT is the MQTT broker host port.
 */
#define APP_MQTT_BROKER_HOST_PORT           UINT16_C(8883)
/**
 * APP_MQTT_CLIENT_ID is the device name
 */
#define APP_MQTT_CLIENT_ID                  "myXDK"
/**
 * APP_MQTT_TOPIC is the topic to subscribe and publish
 */
#define APP_MQTT_TOPIC                      "BCDS/XDK110/EnvironmentData"
/**
 * APP_MQTT_SECURE_ENABLE is a macro to enable MQTT with security
 */
#define APP_MQTT_SECURE_ENABLE              1




//setup wlan
static WLAN_Setup_T WLANSetupInfo =
        {
                .IsEnterprise = false,
                .IsHostPgmEnabled = false,
                .SSID = WLAN_SSID,
                .Username = WLAN_PSK,
                .Password = WLAN_PSK,
                .IsStatic = WLAN_STATIC_IP,
                .IpAddr = WLAN_IP_ADDR,
                .GwAddr = WLAN_GW_ADDR,
                .DnsAddr = WLAN_DNS_ADDR,
                .Mask = WLAN_MASK,
        };/**< WLAN setup parameters */
//enable all sensors
// setup all sensor configurations
static Sensor_Setup_T SensorSetup =
        {
                .CmdProcessorHandle = NULL,
                .Enable =
                        {
                                .Accel = true,
                                .Mag = true,
                                .Gyro = true,
                                .Humidity = true,
                                .Temp = true,
                                .Pressure = true,
                                .Light = true,
                                .Noise = false,
                        },
                .Config =
                        {
                                .Accel =
                                        {
                                                .Type = SENSOR_ACCEL_BMA280,
                                                .IsRawData = false,
                                                .IsInteruptEnabled = false,
                                                .Callback = NULL,
                                        },
                                .Gyro =
                                        {
                                                .Type = SENSOR_GYRO_BMG160,
                                                .IsRawData = false,
                                        },
                                .Mag =
                                        {
                                                .IsRawData = false
                                        },
                                .Light =
                                        {
                                                .IsInteruptEnabled = false,
                                                .Callback = NULL,
                                        },
                                .Temp =
                                        {
                                                .OffsetCorrection = APP_TEMPERATURE_OFFSET_CORRECTION,
                                        },
                        },
        };/**< Sensor setup parameters */

//setup mqtt
static MQTT_Setup_T MqttSetupInfo =
        {
                .MqttType = MQTT_TYPE_SERVALSTACK,
                .IsSecure = APP_MQTT_SECURE_ENABLE,
        };/**< MQTT setup parameters */

//setup mqtt connection
static MQTT_Connect_T MqttConnectInfo =
        {
                .ClientId = APP_MQTT_CLIENT_ID,   //"myXDK"
                .BrokerURL = APP_MQTT_BROKER_HOST_URL,  //"test mosquitto"
                .BrokerPort = APP_MQTT_BROKER_HOST_PORT,  //8883
                .CleanSession = true,
                .KeepAliveInterval = 300,
        };/**< MQTT connect parameters */

//static void AppMQTTSubscribeCB(MQTT_SubscribeCBParam_T param);
//subscribe infomation
// subscribe to only on topic
// equivalent = print all received sensor date
//static MQTT_Subscribe_T MqttSubscribeInfo =
//        {
//                .Topic = APP_MQTT_TOPIC,
//                .QoS = 1UL,
//                .IncomingPublishNotificationCB = AppMQTTSubscribeCB,
//        };/**< MQTT subscribe parameters */

// publish on one topic

static MQTT_Publish_T MqttPublishInfo =
        {
                .Topic = APP_MQTT_TOPIC,
                .QoS = 1UL,
                .Payload = NULL,
                .PayloadLength = 0UL,
        };/**< MQTT publish parameters */

//static uint32_t AppIncomingMsgCount = 0UL;/**< Incoming message count */
//
//static char AppIncomingMsgTopicBuffer[APP_MQTT_DATA_BUFFER_SIZE];/**< Incoming message topic buffer */
//
//static char AppIncomingMsgPayloadBuffer[APP_MQTT_DATA_BUFFER_SIZE];/**< Incoming message payload buffer */

static CmdProcessor_T * AppCmdProcessor;/**< Handle to store the main Command processor handle to be used by run-time event driven threads */

static xTaskHandle AppControllerHandle = NULL;/**< OS thread handle for Application controller to be used by run-time blocking threads */


// subscribe function
// receive message and print it out

//static void AppMQTTSubscribeCB(MQTT_SubscribeCBParam_T param)
//{
//    AppIncomingMsgCount++;
//    memset(AppIncomingMsgTopicBuffer, 0, sizeof(AppIncomingMsgTopicBuffer));
//    memset(AppIncomingMsgPayloadBuffer, 0, sizeof(AppIncomingMsgPayloadBuffer));
//
//    if (param.PayloadLength < sizeof(AppIncomingMsgPayloadBuffer))
//    {
//        strncpy(AppIncomingMsgPayloadBuffer, (const char *) param.Payload, param.PayloadLength);
//    }
//    else
//    {
//        strncpy(AppIncomingMsgPayloadBuffer, (const char *) param.Payload, (sizeof(AppIncomingMsgPayloadBuffer) - 1U));
//    }
//    if (param.TopicLength < (int) sizeof(AppIncomingMsgTopicBuffer))
//    {
//        strncpy(AppIncomingMsgTopicBuffer, param.Topic, param.TopicLength);
//    }
//    else
//    {
//        strncpy(AppIncomingMsgTopicBuffer, param.Topic, (sizeof(AppIncomingMsgTopicBuffer) - 1U));
//    }
//
//    printf("AppMQTTSubscribeCB : #%d, Incoming Message:\r\n"
//            "\tTopic: %s\r\n"
//            "\tPayload: \r\n\"\"\"\r\n%s\r\n\"\"\"\r\n", (int) AppIncomingMsgCount,
//            AppIncomingMsgTopicBuffer, AppIncomingMsgPayloadBuffer);
//}

/**
 * @brief This will validate the WLAN network connectivity
 *
 * If there is no connectivity then it will scan for the given network and try to reconnect
 *
 * @return  RETCODE_OK on success, or an error code otherwise.
 */
static Retcode_T AppControllerValidateWLANConnectivity(void)
{
    Retcode_T retcode = RETCODE_OK;
    WlanNetworkConnect_IpStatus_T nwStatus;

    nwStatus = WlanNetworkConnect_GetIpStatus();
    if (WLANNWCT_IPSTATUS_CT_AQRD != nwStatus)
    {
        if (RETCODE_OK == retcode)
        {
            retcode = MQTT_ConnectToBroker(&MqttConnectInfo, MQTT_CONNECT_TIMEOUT_IN_MS);
            if (RETCODE_OK != retcode)
            {
                printf("AppControllerFire : MQTT connection to the broker failed \n\r");
            }
        }
//        if (RETCODE_OK == retcode)
//        {
//            retcode = MQTT_SubsribeToTopic(&MqttSubscribeInfo, MQTT_SUBSCRIBE_TIMEOUT_IN_MS);
//            if (RETCODE_OK != retcode)
//            {
//                printf("AppControllerFire : MQTT subscribe failed \n\r");
//            }
//        }
    }
    return retcode;
}

/**
 * @brief Responsible for controlling the send data over MQTT application control flow.
 *
 * - Connect to MQTT broker
 * - Subscribe to MQTT topic
 * - Read environmental sensor data
 * - Publish data periodically for a MQTT topic
 *
 * @param[in] pvParameters
 * Unused
 */
static void AppControllerFire(void* pvParameters)
{

    BCDS_UNUSED(pvParameters);
    Retcode_T retcode = RETCODE_OK;
    //
    Sensor_Value_T sensorValue;
    char publishBuffer[APP_MQTT_DATA_BUFFER_SIZE];
    const char *publishDataFormat =
    		"{ \"accelerometer\"  : { \"x\" : \"%ld\",  \"y\" : \"%ld\", \"z\" : \"%ld\"}, "
            "\"gyro\" : { \"x\" : \"%ld\",  \"y\" : \"%ld\", \"z\" : \"%ld\"},"
            "\"magnetometer\"  : { \"x\": \"%ld\",  \"y\" : \"%ld\", \"z\" : \"%ld\", \"r\" : \"%ld\"}, "
            "\"humidity\" : \"%ld\","
            "\"pressure\" : \"%ld\","
            "\"light\" : \"%ld\","
    		"\"temperature\" : \"%ld\","
    		"\"id\" : \"%ld\"";

    memset(&sensorValue, 0x00, sizeof(sensorValue));

//connect to Broker
    if (RETCODE_OK == retcode)
    {
        retcode = MQTT_ConnectToBroker(&MqttConnectInfo, MQTT_CONNECT_TIMEOUT_IN_MS);
        if (RETCODE_OK != retcode)
        {
            printf("AppControllerFire : MQTT connection to the broker failed \n\r");
        }
    }


    if (RETCODE_OK != retcode)
    {
        /* We raise error and still proceed to publish data periodically */
        Retcode_RaiseError(retcode);
        vTaskSuspend(NULL);
    }

    /* A function that implements a task must not exit or attempt to return to
     its caller function as there is nothing to return to. */
    while (1)
    {
        /* Resetting / clearing the necessary buffers / variables for re-use */
        retcode = RETCODE_OK;

        /* Check whether the WLAN network connection is available */
        retcode = AppControllerValidateWLANConnectivity();

        //Read environmental sensor data
        if (RETCODE_OK == retcode)
        {
            retcode = Sensor_GetData(&sensorValue);
        }
        // put those sensor values into package

		if (RETCODE_OK == retcode) {
			//define length, put value and value type into it
			int32_t length = snprintf((char *) publishBuffer,
			APP_MQTT_DATA_BUFFER_SIZE, publishDataFormat,
					(long int) sensorValue.Accel.X,
					(long int) sensorValue.Accel.Y,
					(long int) sensorValue.Accel.Z,
					(long int) sensorValue.Gyro.X,
					(long int) sensorValue.Gyro.Y,
					(long int) sensorValue.Gyro.Z, (long int) sensorValue.Mag.X,
					(long int) sensorValue.Mag.Y, (long int) sensorValue.Mag.Z,
					(long int) sensorValue.Mag.R, (long int) sensorValue.RH,
					(long int) sensorValue.Pressure,
					(unsigned int) sensorValue.Light,
					(long int) (sensorValue.Temp /= 1000),
					(APP_MQTT_CLIENT_ID));

			//define payload and length
			MqttPublishInfo.Payload = publishBuffer;
			MqttPublishInfo.PayloadLength = length;

			// Publish data periodically for a MQTT topic
			retcode = MQTT_PublishToTopic(&MqttPublishInfo,
			MQTT_PUBLISH_TIMEOUT_IN_MS);

			if (RETCODE_OK != retcode) {
				printf("AppControllerFire : MQTT publish failed \n\r");
			}

//			//Subscribe data periodically to a MQTT topic
//			retcode = MQTT_SubsribeToTopic(&MqttSubscribeInfo,
//			MQTT_SUBSCRIBE_TIMEOUT_IN_MS);
//
//			if (RETCODE_OK != retcode) {
//				printf("AppControllerFire : MQTT subscribe failed \n\r");
//			}

		}
		if (RETCODE_OK != retcode) {
			Retcode_RaiseError(retcode);
		}
		vTaskDelay(APP_MQTT_DATA_PUBLISH_PERIODICITY);
	}
}

/**
 * @brief To enable the necessary modules for the application
 * - WLAN
 * - ServalPAL
 * - MQTT
 * - Sensor
 *
 * @param[in] param1
 * Unused
 *
 * @param[in] param2
 * Unused
 */
static void AppControllerEnable(void * param1, uint32_t param2)
{
    BCDS_UNUSED(param1);
    BCDS_UNUSED(param2);

    Retcode_T retcode = WLAN_Enable();


    if (RETCODE_OK == retcode)
    {
        retcode = ServalPAL_Enable();
    }

    //enable mqtt
    if (RETCODE_OK == retcode)
    {
        retcode = MQTT_Enable();
    }

    // enable sensor
    if (RETCODE_OK == retcode)
    {
        retcode = Sensor_Enable();
    }
    if (RETCODE_OK == retcode)
    {
        if (pdPASS != xTaskCreate(AppControllerFire, (const char * const ) "AppController", TASK_STACK_SIZE_APP_CONTROLLER, NULL, TASK_PRIO_APP_CONTROLLER, &AppControllerHandle))
        {
            retcode = RETCODE(RETCODE_SEVERITY_ERROR, RETCODE_OUT_OF_RESOURCES);
        }
    }
    if (RETCODE_OK != retcode)
    {
        printf("AppControllerEnable : Failed \r\n");
        Retcode_RaiseError(retcode);
        assert(0); /* To provide LED indication for the user */
    }
    Utils_PrintResetCause();

}

/**
 * @brief To setup the necessary modules for the application
 * - WLAN
 * - ServalPAL
 * - MQTT
 * - Sensor
 *
 * @param[in] param1
 * Unused
 *
 * @param[in] param2
 * Unused
 */
static void AppControllerSetup(void * param1, uint32_t param2)
{
    BCDS_UNUSED(param1);
    BCDS_UNUSED(param2);
    Retcode_T retcode = WLAN_Setup(&WLANSetupInfo);
    if (RETCODE_OK == retcode)
    {
        retcode = ServalPAL_Setup(AppCmdProcessor);
    }

    if (RETCODE_OK == retcode)
    {
        retcode = MQTT_Setup(&MqttSetupInfo);
    }
    if (RETCODE_OK == retcode)
    {
        SensorSetup.CmdProcessorHandle = AppCmdProcessor;
        retcode = Sensor_Setup(&SensorSetup);
    }
    if (RETCODE_OK == retcode)
    {
        retcode = CmdProcessor_Enqueue(AppCmdProcessor, AppControllerEnable, NULL, UINT32_C(0));
    }

    if (RETCODE_OK != retcode)
    {
        printf("AppControllerSetup : Failed \r\n");
        Retcode_RaiseError(retcode);
        assert(0); /* To provide LED indication for the user */
    }
}

/* global functions ********************************************************* */

/** Refer interface header for description */
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

/**@} */
/** ************************************************************************* */
