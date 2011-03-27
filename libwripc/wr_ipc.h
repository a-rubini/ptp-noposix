#ifndef __WR_IPC_H
#define __WR_IPC_H

//#include <inttypes.h>  -- now in ptpd-wrappers.h

#define T_INT32 1
#define T_INT16 2
#define T_INT8 3
#define T_STRING 4
#define T_FLOAT 5
#define T_DOUBLE 6
#define T_STRUCT_TYPE 7
#define T_STRUCT(type) (T_STRUCT_TYPE | (sizeof(type) << 8))
#define T_VOID 8


#define A_INT8(x) T_INT8,(x)
#define A_INT16(x) T_INT16,(x)
#define A_INT32(x) T_INT32,(x)
#define A_STRING(x) T_STRING,(x)
#define A_FLOAT(x) T_FLOAT,(x)
#define A_DOUBLE(x) T_DOUBLE,(x)
#define A_STRUCT(x) T_STRUCT(x),&x

#define WRIPC_ERROR_INVALID_REQUEST -1
#define WRIPC_ERROR_UNKNOWN_FUNCTION -2
#define WRIPC_ERROR_MALFORMED_PACKET -3
#define WRIPC_ERROR_INVALID_ARG -4
#define WRIPC_ERROR_NO_MEMORY -5
#define WRIPC_ERROR_TIMEOUT -6
#define WRIPC_ERROR_NETWORK_FAIL -7

typedef int wripc_handle_t;

wripc_handle_t wripc_create_server(const char *name);
wripc_handle_t wripc_connect(const char *name);

void wripc_close(wripc_handle_t h);

int wripc_export (wripc_handle_t handle, int rval_type, const char *name, void *func_ptr, int num_args,  ...);
int wripc_call   (wripc_handle_t handle, const char *name, void *rval, int num_args, ...);

int wripc_process(wripc_handle_t handle);

int wripc_publish_event(wripc_handle_t handle, int event_id, void *buf, int buf_len);
int wripc_subscribe_event(wripc_handle_t handle, int event_id);
int wripc_receive_event(wripc_handle_t handle, int *event_id, void *buf, int buf_len);

#endif
