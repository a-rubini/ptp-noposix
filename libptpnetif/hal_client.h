
#ifndef __HAL_CLIENT_H
#define __HAL_CLIENT_H

#include "hal_exports.h"

int halexp_client_init();
int halexp_client_try_connect(int retries, int timeout);


#endif
