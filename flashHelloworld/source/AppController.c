/*----------------------------------------------------------------------------*/

/* --------------------------------------------------------------------------- |
 * INCLUDES & DEFINES ******************************************************** |
 * -------------------------------------------------------------------------- */

/* own header files */
#include "XdkAppInfo.h"
#undef BCDS_MODULE_ID  /* Module ID define before including Basics package*/
#define BCDS_MODULE_ID XDK_APP_MODULE_ID_APP_CONTROLLER

/* system header files */
#include <stdio.h>

/* additional interface header files */
#include "BCDS_CmdProcessor.h"
#include "FreeRTOS.h"
#include "timers.h"

/* --------------------------------------------------------------------------- |
 * HANDLES ******************************************************************* |
 * -------------------------------------------------------------------------- */

static CmdProcessor_T * AppCmdProcessor;/**< Handle to store the main Command processor handle to be used by run-time event driven threads */

/* --------------------------------------------------------------------------- |
 * VARIABLES ***************************************************************** |
 * -------------------------------------------------------------------------- */

/* constant definitions */
#define TIMER_AUTORELOAD_ON pdTRUE
#define TIMER_AUTORELOAD_OFF pdFALSE
#define SECONDS(x) ((portTickType) (x * 1000) / portTICK_RATE_MS)

/* --------------------------------------------------------------------------- |
 * EXECUTING FUNCTIONS ******************************************************* |
 * -------------------------------------------------------------------------- */

// Function that prints "Hello World" on the serial console.
void helloWorld(void * pvParameters ){
  // Task code goes here.
  printf("Hello world \n\r");
}

// Function that creates the Hello World timer task.
void createHelloWorldTimerTask(void){
  xTimerHandle xHandle;

  // Create the "Hello World" timer task running every second
  // Validated for portMAX_DELAY to assist the task to wait infinitely
  // Since ticks cannot be 0 in the FreeRTOS timer, ticks is assigned to 1
  xHandle = xTimerCreate((char * const) "HelloWorldTimerTask", SECONDS(1), TIMER_AUTORELOAD_ON, NULL, helloWorld);

  // Check whether timer was successfully created
  if( xHandle == NULL ){
    // The timer was not created
  }
  else{
    // Start the timer
    xTimerStart(xHandle, 0);
  }
}

/* --------------------------------------------------------------------------- |
 * BOOTING- AND SETUP FUNCTIONS ********************************************** |
 * -------------------------------------------------------------------------- */

static void AppControllerEnable(void * param1, uint32_t param2)
{
    BCDS_UNUSED(param1);
    BCDS_UNUSED(param2);

    /* Enable necessary modules for the application and check their return values */
    createHelloWorldTimerTask();
}

static void AppControllerSetup(void * param1, uint32_t param2)
{
    BCDS_UNUSED(param1);
    BCDS_UNUSED(param2);
    Retcode_T retcode = RETCODE_OK;

    /* Setup the necessary modules required for the application */

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

/** ************************************************************************* */
