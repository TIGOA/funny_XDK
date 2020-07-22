/*
 * Copyright (C) Bosch Connected Devices and Solutions GmbH.
 * All Rights Reserved. Confidential.
 *
 * Distribution only to people who need to know this information in
 * order to do their job.(Need-to-know principle).
 * Distribution to persons outside the company, only if these persons
 * signed a non-disclosure agreement.
 * Electronic transmission, e.g. via electronic mail, must be made in
 * encrypted form.
 */
/*----------------------------------------------------------------------------*/

/**
 *  @file
 *
 *  @brief Configuration header for the AppController.c file.
 *
 */

/* header definition ******************************************************** */
#ifndef APPCONTROLLER_H_
#define APPCONTROLLER_H_

/* local interface declaration ********************************************** */
#include "XDK_Utils.h"

/* local type and macro definitions */

/* local module global variable declarations */

/* local inline function definitions */

/* local type and macro definitions */


/**
 * WLAN_CONNECT_WPA_SSID is the SSID of the WIFI network you want to connect to.
 */

void AppController_Init(void * cmdProcessorHandle, uint32_t param2);

#endif /* APPCONTROLLER_H_ */

/** ************************************************************************* */
