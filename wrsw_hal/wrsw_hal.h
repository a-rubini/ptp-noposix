#ifndef __WRSW_HAL_H
#define __WRSW_HAL_H

#include <inttypes.h>

typedef void (*hal_cleanup_callback_t)();

#define PORT_BUSY 1
#define PORT_OK 0
#define PORT_ERROR -1

int hal_parse_config();
int hal_check_running();


int hal_config_get_int(const char *name, int *value);
int hal_config_get_double(const char *name, double *value);
int hal_config_get_string(const char *name, char *value, int max_len);
int hal_config_iterate(const char *section, int index, char *subsection, int max_len);

int hal_init_ports();
void hal_update_ports();

int hal_init_wripc();
int hal_update_wripc();

int hal_add_cleanup_callback(hal_cleanup_callback_t cb);

int hal_port_start_lock(const char  *port_name, int priority);
int hal_port_check_lock(const char  *port_name);


#endif
