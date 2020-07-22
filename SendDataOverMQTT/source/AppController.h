


/* header definition ******************************************************** */
#ifndef APPCONTROLLER_H_
#define APPCONTROLLER_H_

/* local interface declaration ********************************************** */
#include "XDK_Utils.h"


//#define WLAN_SSID                           "Aniu"
//#define WLAN_PSK                            "11111111"

/**
 * @brief Gives control to the Application controller.
 *
 * @param[in] cmdProcessorHandle
 * Handle of the main command processor which shall be used based on the application needs
 *
 * @param[in] param2
 * Unused
 */
void AppController_Init(void * cmdProcessorHandle, uint32_t param2);

#endif /* APPCONTROLLER_H_ */

/** ************************************************************************* */
