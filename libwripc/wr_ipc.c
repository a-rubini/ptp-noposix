#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <stdarg.h>
#include <poll.h>
//#include <inttypes.h>  -- now in ptpd-wrappers.h
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/un.h>

#include <wr_ipc.h>

#include "asm_helpers.h"

#ifdef DEBUG
#define DBG(...) dbg_printf(__func__, __VA_ARGS__)
#else
#define DBG(...)
#endif

#define WRIPC_SERVER 1
#define WRIPC_CLIENT 2

#define WRIPC_MAX_FUNCS 128
#define WRIPC_MAX_ARGS 16
#define WRIPC_MAX_HANDLES 16
#define WRIPC_MAX_EVENTS 128

#define WRIPC_MAX_CONNECTIONS 16

#define MSG_TYPE_CALL 1
#define MSG_TYPE_EVENT_SUBSCRIBE 2
#define MSG_TYPE_EVENT_NOTIFY 3
#define MSG_TYPE_ERROR 4
#define MSG_TYPE_CALL_ACK 5
#define MSG_TYPE_SUBSCRIBE_ACK 6

#define MAX_MESSAGE_SIZE 2048
#define REPLY_TIMEOUT 20000 // msec

#define ARG_TYPE_MASK 0xff

struct wripc_function {
	int in_use;
	char *name;
	void *ptr;
	int n_args;
	int arg_types[WRIPC_MAX_ARGS];
	int rval_type;
	int rval_size;
};

struct wripc_connection {
	int in_use;
	int cli_fd;
	uint32_t event_mask[WRIPC_MAX_EVENTS / 32];
};

struct wripc_event {
	int in_use;
	char *name;
	uint32_t id;
};

struct wripc_server_context {
	int fd;
	char *name;

	int num_functions;
	struct wripc_function *funcs;
	struct wripc_event *events;
	struct wripc_connection *conns;

	int current_event_id;

	//  int num_events;
	//struct wripc_event *evts;
};

struct wripc_client_context {
	int fd;
	char *srv_name;
};

struct handle_struct {
	int in_use;
	int type;
	struct wripc_server_context *srv;
	struct wripc_client_context *cli;
};

static struct handle_struct handle_map[WRIPC_MAX_HANDLES];


/*********************

   Common Functions

*********************/

#ifdef DEBUG
static void dbg_printf(const char *func, const char *fmt, ...)
{
	va_list ap;
	fprintf(stderr,"%s: ", func);
	va_start(ap, fmt);
	vfprintf(stderr, fmt, ap);
	va_end(ap);
}
#endif


static struct wripc_server_context *get_srv_context(wripc_handle_t handle)
{
	if(handle <0 || handle >= WRIPC_MAX_HANDLES)
		return NULL;
	if(!handle_map[handle].in_use || handle_map[handle].type !=WRIPC_SERVER)
		return NULL;

	return handle_map[handle].srv;
}


static struct wripc_client_context *get_cli_context(wripc_handle_t handle)
{
	if(handle <0 || handle >= WRIPC_MAX_HANDLES)
		return NULL;
	if(!handle_map[handle].in_use || handle_map[handle].type !=WRIPC_CLIENT)
		return NULL;

	return handle_map[handle].cli;
}

static wripc_handle_t alloc_handle(int type)
{
	int i;

	for(i = 0; i<WRIPC_MAX_HANDLES; i++)
		if(!handle_map[i].in_use)
		{
			handle_map[i].type = type;
			handle_map[i].in_use = 1;
			return (wripc_handle_t) i;
		}

	return -1;
}

static void free_handle(wripc_handle_t handle)
{
	if(handle >= 0 && handle <= WRIPC_MAX_HANDLES)
		handle_map[handle].in_use = 0;
}

static void *safe_zmalloc(size_t howmuch)
{
	void *p;

	p = malloc(howmuch);
	if(!p)
	{
		DBG("FATAL: not enough memory\n");
		exit(-1);
	}

	memset(p, 0, howmuch);
	return p;
}

/*********************

   Server-side

*********************/


wripc_handle_t wripc_create_server(const char *name)
{
	int fd;
	int rval;
	struct wripc_server_context *srv;
	wripc_handle_t handle;
	struct sockaddr_un sun;

	fd = socket(SOCK_STREAM, AF_UNIX, 0);
	if(fd < 0)
		return fd;

	sun.sun_family = AF_UNIX;
	strncpy(sun.sun_path, "/tmp/.wripc_", sizeof(sun.sun_path));
	strncat(sun.sun_path, name, sizeof(sun.sun_path));

	unlink(sun.sun_path);

	if((rval = bind (fd, (struct sockaddr *)&sun,
			 sizeof(struct sockaddr_un))) < 0)
	{
		perror("bind");
		close(fd);
		return rval;
	}

	if((rval = listen(fd, WRIPC_MAX_CONNECTIONS)) < 0)
	{
		close(fd);
		return rval;
	}

	srv = safe_zmalloc(sizeof(struct wripc_server_context));

	if(!srv)
	{
		close(fd);
		return -1;
	}

	handle = alloc_handle(WRIPC_SERVER);

	if(handle < 0)
	{
		close(fd);
		free(srv);
		return -1;
	}

	srv->fd = fd;
	srv->name = strdup(name);
	srv->num_functions = 0;

	srv->funcs = safe_zmalloc(WRIPC_MAX_FUNCS
				  * sizeof(struct wripc_function));
	srv->events = safe_zmalloc(WRIPC_MAX_FUNCS
				   * sizeof(struct wripc_event));
	srv->conns = safe_zmalloc(WRIPC_MAX_FUNCS
				  * sizeof(struct wripc_connection));

	handle_map[handle].srv = srv;

	fcntl(fd, F_SETFL, O_NONBLOCK);

	DBG("created server '%s'\n", name);
	return handle;
}


wripc_handle_t wripc_connect(const char *name)
{
	int fd;
	int rval;
	struct wripc_client_context *cli;
	wripc_handle_t handle;
	struct sockaddr_un sun;

	handle = alloc_handle(WRIPC_CLIENT);
	if(handle < 0)
		return -ENOMEM;

	cli = malloc(sizeof(struct wripc_client_context));
	if(!cli)
		return -ENOMEM;

	handle_map[handle].cli = cli;

	fd = socket(SOCK_STREAM, AF_UNIX, 0);
	if(fd < 0)
		return fd;

	sun.sun_family = AF_UNIX;
	strncpy(sun.sun_path, "/tmp/.wripc_", sizeof(sun.sun_path));
	strncat(sun.sun_path, name, sizeof(sun.sun_path));

	//unlink(sun.sun_path);

	if((rval = connect (fd, (struct sockaddr *)&sun,
			    sizeof(struct sockaddr_un))) < 0)
	{
		DBG("connect failed: %s\n", strerror(errno));
		close(fd);
		return rval;
	}

	cli->fd = fd;

	DBG("fd = %d\n", fd);
	return handle;
}


static struct wripc_function *find_function(struct wripc_server_context *srv,
					    const char *func_name)
{
	int i;

	for(i=0;i<WRIPC_MAX_FUNCS;i++)
		if(srv->funcs[i].in_use && !strcmp(func_name,
						   srv->funcs[i].name))
			return &srv->funcs[i];

	return NULL;
}

static struct wripc_function *new_function(struct wripc_server_context *srv,
					   const char *func_name)
{
	int i;

	if(find_function(srv, func_name) != NULL) // already registered
		return NULL;

	for(i=0;i<WRIPC_MAX_FUNCS;i++)
		if(!srv->funcs[i].in_use)
			return &srv->funcs[i];

	return NULL;
}

static int get_arg_size(uint32_t arg_type)
{
	switch(arg_type & ARG_TYPE_MASK)
	{
		case T_FLOAT:
		case T_INT32:
		case T_INT16:
		case T_INT8: return 4;
		case T_DOUBLE: return 8;
		case T_STRUCT_TYPE: return (arg_type >> 8);
		case T_VOID: return 0;
		default: return 0;
	}
}

int wripc_export(wripc_handle_t handle, int rval_type, const char *name,
		 void *func_ptr, int num_args,  ...)
{
	struct wripc_server_context *srv;
	struct wripc_function *func;
	va_list ap;
	int i;

	DBG("export '%s'\n", name);
	if(num_args > WRIPC_MAX_ARGS)
		return -EINVAL;

	srv = get_srv_context(handle);

	if(!srv) return -EINVAL;


	func = new_function(srv, name);
	if(!func) return -ENOSPC;

	func->name = strdup(name);

	func->rval_type = rval_type & ARG_TYPE_MASK;
	func->rval_size = get_arg_size(rval_type);
	func->ptr = func_ptr;
	func->n_args = num_args;


	va_start(ap, num_args);
	for(i=0; i<num_args; i++)
		func->arg_types[i] = va_arg(ap, int);
	va_end(ap);

	func->in_use = 1;

	return 0;
}

static struct wripc_connection *new_connection(struct wripc_server_context *srv)
{
	int i;

	for(i=0;i<WRIPC_MAX_CONNECTIONS; i++)
		if(!srv->conns[i].in_use)
		{
			srv->conns[i].in_use = 1;
			return  &srv->conns[i];
		}
	return NULL;
}

static void reply_error(struct wripc_connection *conn, int err_val)
{
	uint32_t buffer[3];

	buffer[0] = MSG_TYPE_ERROR;
	buffer[1] = 3;
	buffer[2] = err_val;

	send(conn->cli_fd, buffer, sizeof(buffer), 0);
}

static char *extract_string(uint32_t *ptr)
{
	int len = *ptr++;
	char * buf = malloc(len + 1);
	memcpy(buf, ptr, len);
	buf[len] = 0;
	return buf;
}


//extern int _do_call(void *func_ptr, void *args, int args_size);

// FIXME: optimize search
static inline struct wripc_function *lookup_function(
	struct wripc_server_context *srv, const char *func_name)
{
	int i;
	for(i=0;i < WRIPC_MAX_FUNCS; i++)
		if(srv->funcs[i].in_use)
			if(!strcmp(srv->funcs[i].name, func_name))
				return &srv->funcs[i];

	return NULL;
}

static inline int deserialize_string(uint32_t *stream, int current_pos,
				     int buf_size, uint32_t *dst)
{
	int length = stream[current_pos];
	int num_words = ((length + 4) >> 2) + 1;
	char *str_p;

	// printf("DeserializeString: cp %d nwords %d bsize %d \n",
	//        current_pos, num_words, buf_size);

	if( current_pos + num_words > buf_size)
		return -1;

	str_p = (char *)(stream + current_pos + 1);

	// printf("Str_P %s length %d\n", str_p);

	if(str_p[length])
		return -1;
	//for(i=0;i<num_words;i++) printf("%08x ", stream[current_pos+i]);
	//printf("\n");
	*dst = (uint32_t)&stream[current_pos + 1];

	return current_pos + num_words;
}

static inline int deserialize_struct(uint32_t *stream, int current_pos,
				     int buf_size, uint32_t *dst)
{
	int size = stream[current_pos];
	int num_words = ((size + 3) >> 2) + 1;

	//printf("bs %d cp %d nw %d\n", buf_size, current_pos, num_words);

	if( current_pos + num_words > buf_size)
		return -1;

	*dst = (uint32_t)&stream[current_pos + 1];

	return current_pos + num_words;
}

static inline int serialize_string(uint32_t *buffer, int current_pos,
				   int max_pos, char *str)
{
	int len;
	int num_words;

	len = strlen(str);
	num_words = 1 + ((len + 4) >> 2);

	if(current_pos + num_words >= max_pos) return -ENOMEM;

	buffer[current_pos] = len;
	memcpy(buffer + current_pos + 1, str, len+1);
	current_pos += num_words;

	return current_pos;
}

static inline int serialize_struct(uint32_t *buffer, int current_pos,
				   int max_pos, int size, void *ptr)
{
	int num_words = 1 + ((size + 3) >> 2);

	if(current_pos + num_words >= max_pos)
		return -ENOMEM;

	buffer[current_pos] = size;
	memcpy(buffer + current_pos + 1, ptr, size);

	return current_pos + num_words;
}


static inline int send_call_reply(struct wripc_connection *conn,
				  struct wripc_function *func, uint32_t *rval,
				  int buf_size)
{
	int total_size;
	uint32_t buf[MAX_MESSAGE_SIZE];

	buf[0] = MSG_TYPE_CALL_ACK;
	buf[1] = 0; // size will be put here later

	if(func->rval_type == T_VOID)
	{
		buf[2] = 0;
		total_size = 3;
	} else {
		switch(func->rval_type)
		{
		case T_INT8:
		case T_INT16:
		case T_INT32:
		case T_FLOAT:
		case T_DOUBLE:
			buf[2] = func->rval_size;
			memcpy(buf + 3, rval, func->rval_size);
			total_size = 3 + ((func->rval_size + 3) >> 2);
			break;
		case T_STRING:
			total_size = serialize_string(buf, 2,
						      MAX_MESSAGE_SIZE,
						      (char *)rval);
			if(total_size < 0) {
				reply_error(conn, WRIPC_ERROR_MALFORMED_PACKET);
				return 0;
			}
		case T_STRUCT_TYPE:
			//printf("ReturnsStruct: size %d\n", func->rval_size);
			total_size = serialize_struct(buf, 2,
						      MAX_MESSAGE_SIZE,
						      func->rval_size, rval);

			if(total_size < 0) {
				reply_error(conn, WRIPC_ERROR_MALFORMED_PACKET);
				return 0;
			}

			break;
		}
	}

	buf[1] = total_size << 2;

	DBG("about_to_send ts %d ", total_size * 4);

	int n_sent  = send(conn->cli_fd, buf, total_size * 4, 0);
	DBG("about_to_send ts %d n_sent %d", total_size * 4, n_sent);

	if(n_sent != total_size * 4)
		return -1;

	return 0;
}

static int handle_call(struct wripc_server_context *srv,
		       struct wripc_connection *conn, uint32_t *buf,
		       int buf_size)
{
	struct wripc_function *func;
	char *func_name;
	uint32_t arg_buf[WRIPC_MAX_ARGS * 2];
	uint32_t rval_buf[MAX_MESSAGE_SIZE];
	int arg_buf_pos;
	int num_args;
	int current_pos;
	int i;
	int direct_rval;

	func_name = extract_string(buf + 3);

	func = lookup_function(srv, func_name);

	num_args = buf[2];

	current_pos = 3 + 1+ ((strlen(func_name)+4)>>2);

	DBG("conn %x call %s nargs = %d\n", conn, func_name, num_args);

	if(!func)
	{
		reply_error(conn, WRIPC_ERROR_UNKNOWN_FUNCTION);
		return 0;
	}

	if(func->rval_type == T_INT8
	   || func->rval_type == T_INT16
	   || func->rval_type == T_INT32
	   || func->rval_type == T_VOID)
	{
		// these functions can return the value directly
		// (e.g. int add (a, b) { return a+b;  }
		direct_rval = 1;
		arg_buf_pos = 0;
	} else {
		direct_rval = 0;
		arg_buf_pos = 1;
	}


	for(i=0;i<num_args;i++)
	{
		int arg_type = buf[current_pos++];
		DBG("argtype  %d cp %d\n", arg_type, current_pos);
		switch(arg_type)
		{
		case T_INT8:
			arg_buf[arg_buf_pos++] = buf[current_pos++] & 0xff;
			break;
		case T_INT16:
			arg_buf[arg_buf_pos++] = buf[current_pos++] & 0xffff;
			break;
		case T_INT32:
			arg_buf[arg_buf_pos++] = buf[current_pos++]&0xffffffff;
			break;

		case T_DOUBLE:
		{

			arg_buf[arg_buf_pos++] = buf[current_pos++];
			arg_buf[arg_buf_pos++] = buf[current_pos++];

			break;
		}
		case T_FLOAT:
		{
			float tmp = (float) (*(double *) &buf[current_pos]);
			arg_buf[arg_buf_pos++] = *(uint32_t *)&tmp;

			current_pos += 2; break;
		}

		case T_STRING:
			current_pos =
				deserialize_string(buf, current_pos,
						   buf_size,
						   &arg_buf[arg_buf_pos]);
			DBG("DeserializeString, pos = %d string = %s\n",
			    current_pos, &arg_buf[arg_buf_pos]);
			    
			    arg_buf_pos++;

			if(current_pos < 0)
			{
				reply_error(conn, WRIPC_ERROR_MALFORMED_PACKET);
				return 0;
			}
			break;

		case T_STRUCT_TYPE:
			current_pos =
				deserialize_struct(buf, current_pos,
						   buf_size,
						   &arg_buf[arg_buf_pos++]);
			DBG("StructArg, pos= %d\n",current_pos);

			if(current_pos < 0)
			{
				reply_error(conn, WRIPC_ERROR_MALFORMED_PACKET);
				return 0;
			}

			break;
		default:
			reply_error(conn, WRIPC_ERROR_MALFORMED_PACKET);
		}
	}

	if(!direct_rval)
	{
		arg_buf[0] = (uint32_t) rval_buf;
		DBG("Rval_ptr: %x\n", arg_buf[0]);
		DBG("Func_ptr: %x\n", func->ptr);
		//DBG("String: %s\n", arg_buf[1]);
		_do_call(func->ptr, arg_buf, arg_buf_pos * 4);

	} else {
		DBG("Rval_ptr: %x\n", arg_buf[0]);
		DBG("Func_ptr: %x\n", func->ptr);

		rval_buf[0] = _do_call(func->ptr, arg_buf, arg_buf_pos * 4);

		DBG("Rval: %d\n", rval_buf[0]);
	}

	return send_call_reply(conn, func, rval_buf, MAX_MESSAGE_SIZE);
}

static int handle_client_request(struct wripc_server_context *srv,
				 struct wripc_connection *conn)
{
	uint32_t buffer[MAX_MESSAGE_SIZE];
	int n_recv;
	int rq_type;
	int size;

	for(;;)
	{
		n_recv = recv(conn->cli_fd, buffer, sizeof(buffer), 0);
		if(!n_recv) return 0;

		if(n_recv & 3)
			return -1; // non-integer number of words?

		rq_type = buffer[0];
		size = buffer[1];

		if(n_recv != size)
		{
			reply_error(conn, WRIPC_ERROR_INVALID_REQUEST);
			continue;
		}

		switch(rq_type)
		{
		case MSG_TYPE_CALL:
			return handle_call(srv, conn, buffer,  n_recv >> 2);

			break;
		}

	}

	return 0;
}

static int process_server(struct wripc_server_context *srv)
{
	struct pollfd pfd;
	struct sockaddr_un sun;
	int new_fd;
	int i;

	socklen_t slen;

	slen = sizeof(struct sockaddr_un);

	if((new_fd = accept(srv->fd,(struct sockaddr *) &sun, &slen)) > 0)
	{
		struct wripc_connection *conn;

		fcntl(new_fd, F_SETFL, O_NONBLOCK);

		DBG("got connection: fd = %d.\n", new_fd);

		conn = new_connection(srv);
		if(conn)
			conn->cli_fd = new_fd;
		else {
			DBG("too many connections active, refusing...\n");
			close(new_fd);
		}
	}



	for(i=0;i<WRIPC_MAX_CONNECTIONS;i++)
	{
		struct wripc_connection *conn = &srv->conns[i];
		if(conn->in_use)
		{
			pfd.fd = conn->cli_fd;
			pfd.events = POLLIN | POLLHUP | POLLERR ;
			pfd.revents = 0;

			if(poll(&pfd, 1, 0) > 0)
			{
				if(pfd.revents & (POLLERR | POLLHUP))
				{
					DBG("poll returned error, "
					    "killing connection (fd = %d)\n",
					    conn->cli_fd);
					conn->in_use = 0;
					close(conn->cli_fd);
				} else if(pfd.revents & POLLIN) {

					int rv = handle_client_request(srv,
								       conn);

					DBG("handle request returned %d\n", rv);

					if(rv < 0)
					{
						DBG("Request failed. "
						    "Closing connection");
						conn->in_use = 0;
						close(conn->cli_fd);
					}
				}
			}
		}
	}
	return 0;
}

int wripc_process(wripc_handle_t handle)
{
	struct wripc_server_context *srv;

	srv = get_srv_context(handle);
	if(srv) return process_server(srv);

	return 0;
}


void wripc_close(wripc_handle_t handle)
{
	struct wripc_server_context *srv;
	struct wripc_client_context *cli;
	int i;

	srv = get_srv_context(handle);
	cli = get_cli_context(handle);

	if(!srv && !cli) return;

	if(srv)
	{
		for(i=0; i<WRIPC_MAX_CONNECTIONS;i++)
			if(srv->conns[i].in_use)
				close(srv->conns[i].cli_fd);

		free(srv->conns);
		free(srv->funcs);
		free(srv->events);
		close(srv->fd);
		free(srv);
	}

	if(cli)
	{
		close(cli->fd);
		free(cli);
	}


	free_handle(handle);
}

static int recv_with_timeout(int fd, void *buf, size_t len, int timeout)\
{
	struct pollfd pfd;

	pfd.fd = fd;
	pfd.events = POLLIN | POLLHUP;
	pfd.revents = 0;

	if(poll(&pfd, 1, REPLY_TIMEOUT) <= 0)
		return WRIPC_ERROR_TIMEOUT;

	return recv(fd, buf, len, 0);
}

int wripc_call(wripc_handle_t handle, const char *name, void *rval,
	       int num_args, ...)
{
	uint32_t buffer[MAX_MESSAGE_SIZE];
	int current_pos;
	int name_len;
	int i;
	//int arg_types[WRIPC_MAX_ARGS];
	int rv, msg_size;
	va_list ap;

	struct wripc_client_context *cli = get_cli_context(handle);

	DBG("call %s handle %d ctx %x\n", name, handle, cli);

	if(!cli) return -EINVAL;
	if(num_args > WRIPC_MAX_ARGS)
		return WRIPC_ERROR_INVALID_ARG;

	name_len = strlen(name);
	if(name_len > 128)
		return WRIPC_ERROR_INVALID_ARG;

	//DBG("dupa?");

	buffer[0] = MSG_TYPE_CALL;
	buffer[1] = 0; // size will be put here later
	buffer[2] = num_args;
	buffer[3] = name_len;

	memcpy(buffer + 4, name, name_len);

	current_pos = ((name_len + 4) >> 2) + 4;

	va_start(ap, num_args);

	for(i = 0; i<num_args; i++)
	{
		uint32_t varg = va_arg(ap, uint32_t);
		int type = varg & ARG_TYPE_MASK;
		int struct_size = varg >> 8;

		if(current_pos >= MAX_MESSAGE_SIZE)
			return WRIPC_ERROR_NO_MEMORY;

		buffer[current_pos++] = type;

		switch(type)
		{
		case T_INT8:
			buffer[current_pos++] = va_arg(ap, uint32_t) & 0xff;
			break;
		case T_INT16:
			buffer[current_pos++] = va_arg(ap, uint32_t) & 0xffff;
			break;
		case T_INT32:
			buffer[current_pos++] = va_arg(ap, uint32_t);
			break;
		case T_FLOAT:
		case T_DOUBLE: {
			double tmp = va_arg(ap, double);


			buffer[current_pos++] = *(uint32_t *) (&tmp);
			buffer[current_pos++] = *(((uint32_t *) (&tmp)) + 1);
			break;
		}
		case T_STRING: {
//				DBG("serialize: T_STRING\n");
			int new_pos = serialize_string(buffer, current_pos,
						       MAX_MESSAGE_SIZE,
						       va_arg(ap, char *));
			if(new_pos < 0) return WRIPC_ERROR_NO_MEMORY;
			current_pos = new_pos;
			break;
		}

		case T_STRUCT_TYPE: {
			int new_pos;
			void *struct_ptr = va_arg(ap, void*);

			new_pos = serialize_struct(buffer, current_pos,
						   MAX_MESSAGE_SIZE,
						   struct_size, struct_ptr);
			if(new_pos < 0) return WRIPC_ERROR_NO_MEMORY;
			current_pos = new_pos;

			break;
		}

		default:
			DBG("invalid parameter type %d\n", type);
			return WRIPC_ERROR_INVALID_ARG;
		}
	}


	//for(i=0; i<current_pos; i++) fprintf(stderr,"%08x ", buffer[i]);
	//fprintf(stderr,"\n");

	msg_size = current_pos * sizeof(uint32_t);

	buffer[1] = msg_size; // store the message size

	rv = send(cli->fd, buffer, msg_size, 0);

	DBG("total size: %d, sent: %d\n", current_pos*4, rv);

	if(rv != msg_size)
		return rv;

	rv = recv_with_timeout(cli->fd, buffer, 4*MAX_MESSAGE_SIZE,
			       REPLY_TIMEOUT);

	if(rv <= 0 || rv != buffer[1]) return WRIPC_ERROR_MALFORMED_PACKET;

	if(buffer[0] == MSG_TYPE_ERROR) // oops. we received an error
		return (int) buffer[2];
	else if (buffer[0] == MSG_TYPE_CALL_ACK) // ack packet contains retval
	{
		int rval_size = buffer[2];
		if(rval_size)
			memcpy(rval, buffer + 3, rval_size);
		return 0;
	} else return WRIPC_ERROR_MALFORMED_PACKET;

	return 0;
};


static int do_subscribe(wripc_handle_t handle, int event_id, int onoff)
{
	uint32_t buf[4];

	struct wripc_client_context *cli = get_cli_context(handle);
	if(!cli)
		return WRIPC_ERROR_INVALID_ARG;

	if(event_id < 0 || event_id >= WRIPC_MAX_EVENTS)
		return WRIPC_ERROR_INVALID_ARG;

	buf[0] = MSG_TYPE_EVENT_SUBSCRIBE;
	buf[1] = 16;
	buf[2] = event_id;
	buf[3] = onoff;

	if( send(cli->fd, buf, sizeof(buf), 0) != sizeof(buf))
		return WRIPC_ERROR_NETWORK_FAIL;

	if(recv_with_timeout(cli->fd, buf, sizeof(buf), REPLY_TIMEOUT)
	   != sizeof(buf))
		return WRIPC_ERROR_MALFORMED_PACKET;

	if(buf[0] != MSG_TYPE_SUBSCRIBE_ACK || buf[1] != sizeof(buf)
	   || buf[2] != event_id)
		return WRIPC_ERROR_MALFORMED_PACKET;

	DBG("event %d", event_id);


	return 0;
}


int wripc_unsubscribe_event(wripc_handle_t handle, int event_id)
{
	return do_subscribe(handle, event_id, 0);
}

int wripc_subscribe_event(wripc_handle_t handle, int event_id)
{
	return do_subscribe(handle, event_id, 1);
}

__attribute__((constructor)) int wripc_init()
{
	//DBG("\n");
	memset(handle_map, 0, sizeof(handle_map));
	return 0;
}
