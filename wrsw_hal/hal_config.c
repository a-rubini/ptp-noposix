#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <lua.h>
#include <lauxlib.h>
#include <hw/trace.h>

#define HAL_CONFIG_FILE "/wr/etc/wrsw_hal.conf"

static lua_State *cfg_file = NULL;

int hal_parse_config()
{
	TRACE(TRACE_INFO, "Parsing wrsw_hal configuration file: %s", HAL_CONFIG_FILE);
	
  cfg_file = lua_open();

  luaL_openlibs(cfg_file);
 
  if (luaL_dofile(cfg_file, HAL_CONFIG_FILE)!=0) 
 		TRACE(TRACE_ERROR, "Error parsing the configuration file: %s", lua_tostring(cfg_file,-1));

	luaL_dostring(cfg_file, "\
	function get_var(name)\
	local t = _G\
	for w in name:gmatch(\"([%w_]+)\\.?\") do\
	t = t[w]\
	end\
	return t\
	end");

	return 0;
}

static int global_get_var(const char *name)
{
  lua_getglobal(cfg_file, "get_var");
  lua_pushstring(cfg_file, name);
	
  if(lua_pcall(cfg_file, 1, 1, 0) != 0) return -1;
	return 0;
}

int hal_config_get_int(const char *name, int *value)
{
	if(global_get_var(name) < 0) return -1;
	if(!lua_isnumber(cfg_file, -1)) return -1;
	*value = (int)lua_tonumber(cfg_file, -1);
	return 0;
}

int hal_config_get_double(const char *name, double *value)
{
	if(global_get_var(name) < 0) return -1;
	if(!lua_isnumber(cfg_file, -1)) return -1;
	*value = (double)lua_tonumber(cfg_file, -1);
	return 0;
}

int hal_config_get_string(const char *name, char *value, int max_len)
{
	if(global_get_var(name) < 0) return -1;
	if(!lua_isstring(cfg_file, -1)) return -1;
	ptpd_wrap_strncpy(value, lua_tostring(cfg_file, -1), max_len);
	return 0;
}

int hal_config_iterate(const char *section, int index, char *subsection, int max_len)
{
	int i = 0;
	if(global_get_var(section) < 0) return -1;
	
  lua_pushnil(cfg_file);  /* first key */
  while (lua_next(cfg_file, -2) != 0) {
   /* uses 'key' (at index -2) and 'value' (at index -1) */
      char *key_type = lua_typename(cfg_file, lua_type(cfg_file, -1));

			if(!strcmp(key_type, "table") && i == index)
			{
				ptpd_wrap_strncpy(subsection, lua_tostring(cfg_file, -2), max_len);
				return 1;
			}
			i++;
//      const char *value_type = cfg_file, lua_type(cfg_file, -1)));
       /* removes 'value'; keeps 'key' for next iteration */
     lua_pop(cfg_file, 1);
  }
	
	return 0;
}
