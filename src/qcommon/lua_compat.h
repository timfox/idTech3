#ifndef LUA_COMPAT_H
#define LUA_COMPAT_H

#include <lua.h>
#include <lauxlib.h>
#include <lualib.h>

#if !defined(LUA_VERSION_NUM)
#error "LUA_VERSION_NUM is required"
#endif

#if LUA_VERSION_NUM < 501
#error "Lua 5.1+ is required"
#endif

#ifndef LUA_OK
#define LUA_OK 0
#endif

#if LUA_VERSION_NUM >= 502
#define ID3_LUA_GETGLOBAL(L, N) lua_getglobal((L), (N))
#else
#define ID3_LUA_GETGLOBAL(L, N) lua_getfield((L), LUA_GLOBALSINDEX, (N))
#endif

#if LUA_VERSION_NUM >= 502
#define ID3_LUA_SETGLOBAL(L, N) lua_setglobal((L), (N))
#else
#define ID3_LUA_SETGLOBAL(L, N) lua_setfield((L), LUA_GLOBALSINDEX, (N))
#endif

#if LUA_VERSION_NUM >= 502
#define ID3_LUA_PUSH_GLOBAL_TABLE(L) lua_pushglobaltable((L))
#else
#define ID3_LUA_PUSH_GLOBAL_TABLE(L) lua_pushvalue((L), LUA_GLOBALSINDEX)
#endif

#if LUA_VERSION_NUM >= 502
#define ID3_LUA_ABSINDEX(L, IDX) lua_absindex((L), (IDX))
#else
#define ID3_LUA_ABSINDEX(L, IDX) ((IDX) > 0 || (IDX) <= LUA_REGISTRYINDEX ? (IDX) : lua_gettop((L)) + (IDX) + 1)
#endif

#if LUA_VERSION_NUM >= 502
#define ID3_LUA_RAWLEN(L, IDX) lua_rawlen((L), (IDX))
#else
#define ID3_LUA_RAWLEN(L, IDX) lua_objlen((L), (IDX))
#endif

#if LUA_VERSION_NUM >= 502
#define ID3_LUA_TONUMBERX(L, IDX, ISNUM) lua_tonumberx((L), (IDX), (ISNUM))
#define ID3_LUA_TOINTEGERX(L, IDX, ISNUM) lua_tointegerx((L), (IDX), (ISNUM))
#else
#define ID3_LUA_TONUMBERX(L, IDX, ISNUM) (((ISNUM) ? (*(ISNUM) = lua_isnumber((L), (IDX))) : 0), lua_tonumber((L), (IDX)))
#define ID3_LUA_TOINTEGERX(L, IDX, ISNUM) (((ISNUM) ? (*(ISNUM) = lua_isnumber((L), (IDX))) : 0), lua_tointeger((L), (IDX)))
#endif

#if LUA_VERSION_NUM >= 503
#define ID3_LUA_ISINTEGER(L, IDX) lua_isinteger((L), (IDX))
#else
#define ID3_LUA_ISINTEGER(L, IDX) (0)
#endif

#if LUA_VERSION_NUM >= 502
#define ID3_LUA_NEWLIB(L, LREG) luaL_newlib((L), (LREG))
#else
#define ID3_LUA_NEWLIB(L, LREG) (lua_newtable((L)), luaL_register((L), NULL, (LREG)))
#endif

#if LUA_VERSION_NUM >= 504
#define ID3_LUA_RESUME(L, FROM, NARGS, NRES_OUT) lua_resume((L), (FROM), (NARGS), (NRES_OUT))
#elif LUA_VERSION_NUM >= 502
#define ID3_LUA_RESUME(L, FROM, NARGS, NRES_OUT) ((void)(NRES_OUT), lua_resume((L), (FROM), (NARGS)))
#else
#define ID3_LUA_RESUME(L, FROM, NARGS, NRES_OUT) ((void)(NRES_OUT), lua_resume((L), (NARGS)))
#endif

#endif
