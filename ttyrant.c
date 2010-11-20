/*
 * ttyrant.c: TokyoTyrant API binding for Lua.
 *
 * Copyright (C)2010 by Valeriu Palos. All rights reserved.
 * This library is under the BSD license (see README file).
 */

#include <lua.h>
#include <lauxlib.h>
#include <stdlib.h>
#include <string.h>
#include <tcrdb.h>

/*----------------------------------------------------------------------------------------------------------*/

/*
 * Error macros.
 */
#define _failure(L, ...)  { lua_pushnil(L); \
                            lua_pushstring(L, ##__VA_ARGS__); \
                            return 2; }

/*
 * Self-extraction macros.
 */
#define _self_any(L, F, C)  _self(L, 1, "__" #F, "Invalid «self», expected «" C "» instance!")
#define _self_rdb(L)        (TCRDB*)_self_any(L, rdb, "ttyrant")
#define _self_tdb(L)        (TCRDB*)_self_any(L, tdb, "ttyrant.table")
#define _self_qry(L)        (RDBQRY*)_self_any(L, qry, "ttyrant.query")

/*
 * Extract 'self' userdata from '__xyz' field of first parameter.
 */
static void* _self(lua_State* L, int level, const char* __xyz, const char* error) {

    // extract
    void* self = NULL;
    if (lua_istable(L, level)) {
        lua_getfield(L, level, __xyz);
        self = lua_touserdata(L, -1);
        lua_pop(L, 1);
    }

    // check
    if (!self) {
        luaL_error(L, error);
    }

    // ready
    return self;
}

/*
 * Turn a Lua list (of parameters) into a TCLIST object,
 * starting from the 'index' position in the given stack.
 */
static TCLIST* _lualist2tclist(lua_State* L, int index) {

    // initialize
    size_t itemsz;
    const char* item;
    int items = lua_gettop(L);
    TCLIST* list = tclistnew2(items - index + 1);

    // traverse
    for (; index <= items; index++) {
        item = luaL_checklstring(L, index, &itemsz);
        tclistpush(list, item, itemsz);
    }

    // done
    return list;
}

/*
 * Turn a Lua table found at the 'index' position in the given
 * stack into a TCLIST object as key1, value1, key2... if keys
 * is 1, or as value1, value2... if keys is 0.
 */
static TCLIST* _luatable2tclist(lua_State* L, int index, int keys) {

    // initialize
    size_t keysz;
    const char* key;
    size_t valsz;
    const char* val;
    TCLIST* list = tclistnew();

    // traverse
    lua_pushnil(L);
    while (lua_next(L, index)) {

        // key
        if (keys) {
            char skey[64];
            key = NULL;
            if (lua_type(L, -2) == LUA_TSTRING) {
                key = luaL_checklstring(L, -2, &keysz);
            } else {
                int skeysz = snprintf(skey, 64, "%i", luaL_checkint(L, -2));
                if (skeysz < 0) {
                    break;
                } else {
                    key = skey;
                    keysz = skeysz;
                }
            }
            tclistpush(list, key, keysz);
        }

        // value
        val = luaL_checklstring(L, -1, &valsz);
        tclistpush(list, val, valsz);
        lua_pop(L, 1);
    }

    // ready
    return list;
}

/*
 * Assemble the items of the given TCLIST object into a Lua table
 * at the top of the given Lua stack. Elements are taken in order
 * from start to end as key1, value1, key2... if keys is 1, or as
 * value1, value2... if keys is 0.
 */
static int _tclist2luatable(lua_State* L, TCLIST* items, int keys) {

    // initialize
    lua_newtable(L);
    int itemsz;
    char* item;
    int pairs = tclistnum(items) / (keys ? 2 : 1);
    int index = 1;

    // traverse
    for (; pairs > 0; pairs--) {

        // key
        if (keys) {
            item = tclistshift(items, &itemsz);
            lua_pushlstring(L, item, itemsz);
            free(item);
        } else {
            lua_pushinteger(L, index++);
        }

        // value
        item = tclistshift(items, &itemsz);
        lua_pushlstring(L, item, itemsz);
        free(item);

        // store
        lua_settable(L, -3);
    }

    // ready
    return 1;
}


/*
 * Open a database.
 */
static int _open(lua_State* L, const char* class, const char* __xyz) {

    // instance
    if (!lua_istable(L, 1)) {
        luaL_error(L, "Invalid «self» given to %class:open(), expected «%s»!", class, class);
    } else if (lua_gettop(L) < 2) {
        lua_newtable(L);
    }

    // metatable
    lua_pushvalue(L, 1);
    lua_setfield(L, 1, "__index");      // self.__index = self
    lua_pushvalue(L, 1);
    lua_setmetatable(L, 2);             // setmetatable(instance, self)

    // options
    lua_getfield(L, 2, "host");
    lua_getfield(L, 2, "port");
    const char* host = luaL_checkstring(L, -2);
    int port = luaL_checkinteger(L, -1);
    lua_pop(L, 2);

    // open db
    TCRDB* db = tcrdbnew();
    if (!tcrdbopen(db, host, port)) {
        _failure(L, tcrdberrmsg(tcrdbecode(db)));
    } else {
        lua_pushlightuserdata(L, db);
        lua_setfield(L, 2, __xyz);      // instance.__xyz = <userdata>
    }

    // ready
    return 1;
}

/*
 * Close a database.
 */
static int _close(lua_State* L, TCRDB* db) {
    if (!tcrdbclose(db)) {
        _failure(L, tcrdberrmsg(tcrdbecode(db)));
    }
    tcrdbdel(db);
    lua_pushboolean(L, 1);
    return 1;
}

/*
 * Increment value at key or '_num' column of tuple at key.
 */
static int _add(lua_State* L, TCRDB* db) {

    // arguments
    size_t keysz;
    const char* key = luaL_checklstring(L, 2, &keysz);
    double amount = luaL_checknumber(L, 3);

    // increment
    double result = tcrdbadddouble(db, key, keysz, amount);
    if (result == INT_MIN) {
        _failure(L, tcrdberrmsg(tcrdbecode(db)));
    }

    // sum
    lua_pushnumber(L, result);

    // ready
    return 1;
}

/*----------------------------------------------------------------------------------------------------------*/

/*
 * Open a regular database.
 *
 * ttyrant:open()
 */
static int luaF_ttyrant_open(lua_State* L) {
    return _open(L, "ttyrant", "__rdb");
}

/*
 * Close a database (any type).
 *
 * ttyrant:close()
 */
static int luaF_ttyrant_close(lua_State* L) {
    return _close(L, _self_rdb(L));
}

/*
 * Increment numeric value at key.
 *
 * # = ttyrant:add(key, amount)
 */
static int luaF_ttyrant_add(lua_State* L) {
    return _add(L, _self_rdb(L));
}

/*
 * Store value(s) at key(s) in db.
 *
 * true|false = ttyrant:put(key1, value1, key2, value2, ...)
 * true|false = ttyrant:put{ key1 = value1, key2 = value2, ... }
 */
static int luaF_ttyrant_put(lua_State* L) {

    // initialize
    TCRDB* db = _self_rdb(L);
    TCLIST* items = NULL;
    TCLIST* result = NULL;

    // table
    if (lua_istable(L, 2)) {
        items = _luatable2tclist(L, 2, 1);
        result = tcrdbmisc(db, "putlist", 0, items);
        if (!result) {
            _failure(L, tcrdberrmsg(tcrdbecode(db)));
        }

    // item
    } else if (lua_gettop(L) == 3) {
        size_t keysz, valsz;
        const char* key = luaL_checklstring(L, 2, &keysz);
        const char* val = luaL_checklstring(L, 3, &valsz);
        if (!tcrdbput(db, key, keysz, val, valsz)) {
            _failure(L, tcrdberrmsg(tcrdbecode(db)));
        }

    // list
    } else {
        items = _lualist2tclist(L, 2);
        result = tcrdbmisc(db, "putlist", 0, items);
        if (!result) {
            _failure(L, tcrdberrmsg(tcrdbecode(db)));
        }
    }

    // finish
    if (items) {
        tclistdel(items);
        tclistdel(result);
    }
    lua_pushboolean(L, 1);
    return 1;
}

/*
 * Get value(s) at key(s) from db.
 *
 * value = ttyrant:get(key1, key2, ...)
 * value = ttyrant:get{key1, key2, ...}
 */
static int luaF_ttyrant_get(lua_State* L) {

    // initialize
    TCRDB* db = _self_rdb(L);
    TCLIST* keys = NULL;
    TCLIST* items = NULL;

    // keys table
    if (lua_istable(L, 2)) {
        keys = _luatable2tclist(L, 2, 0);
        items = tcrdbmisc(db, "getlist", 0, keys);
        if (!items) {
            _failure(L, tcrdberrmsg(tcrdbecode(db)));
        }
        _tclist2luatable(L, items, 1);

    // single key
    } else if (lua_gettop(L) == 2) {
        size_t keysz;
        int valsz;
        const char* key = luaL_checklstring(L, 2, &keysz);
        char* val = tcrdbget(db, key, keysz, &valsz);
        if (!val) {
            _failure(L, tcrdberrmsg(tcrdbecode(db)));
        }
        lua_pushlstring(L, val, valsz);
        free(val);

    // keys list
    } else {
        keys = _lualist2tclist(L, 2);
        items = tcrdbmisc(db, "getlist", 0, keys);
        if (!items) {
            _failure(L, tcrdberrmsg(tcrdbecode(db)));
        }
        _tclist2luatable(L, items, 1);
    }

    // done
    if (items) {
        tclistdel(keys);
        tclistdel(items);
    }
    return 1;
}

/*
 * Erase key(s) from db.
 *
 * true|false = ttyrant:out(key1, key2, ...)
 */
static int luaF_ttyrant_out(lua_State* L) {

    // initialize
    TCRDB* db = _self_rdb(L);
    TCLIST* items = NULL;
    TCLIST* result = NULL;

    // item
    if (lua_gettop(L) == 2) {
        size_t keysz;
        const char* key = luaL_checklstring(L, 2, &keysz);
        if (!tcrdbout(db, key, keysz)) {
            _failure(L, tcrdberrmsg(tcrdbecode(db)));
        }

    // list
    } else {
        items = _lualist2tclist(L, 2);
        result = tcrdbmisc(db, "outlist", 0, items);
        if (!result) {
            _failure(L, tcrdberrmsg(tcrdbecode(db)));
        }
    }

    // finish
    if (items) {
        tclistdel(items);
        tclistdel(result);
    }
    lua_pushboolean(L, 1);
    return 1;
}

/*----------------------------------------------------------------------------------------------------------*/

/*
 * Open a regular database.
 *
 * ttyrant.table:open()
 */
static int luaF_ttyrant_table_open(lua_State* L) {
    return _open(L, "ttyrant.table", "__tdb");
}

/*
 * Close a database (any type).
 *
 * ttyrant.table:close()
 */
static int luaF_ttyrant_table_close(lua_State* L) {
    return _close(L, _self_tdb(L));
}

/*
 * Increment '_num' column of tuple at key.
 *
 * # = ttyrant.table:add(key, amount)
 */
static int luaF_ttyrant_table_add(lua_State* L) {
    return _add(L, _self_tdb(L));
}

/*
 * Store tuple in db.
 *
 * true|false = ttyrant.table:put(key, {})
 */
static int luaF_ttyrant_table_put(lua_State* L) {

    // db
    TCRDB* db = _self_tdb(L);

    // tuple check
    if (!lua_istable(L, 3)) {
        luaL_error(L, "Invalid value given to «ttyrant.table:put()», expected a table/tuple!");
    }

    // key
    size_t keysz;
    const char* key = luaL_checklstring(L, 2, &keysz);

    // assemble tuple
    TCMAP* tuple = tcmapnew();
    lua_pushnil(L);
    while (lua_next(L, 3)) {

        // key
        ssize_t colsz;
        const char* col = NULL;
        char scol[64];
        if (lua_type(L, -2) == LUA_TSTRING) {
            col = luaL_checklstring(L, -2, &colsz);
        } else {
            colsz = snprintf(scol, 64, "%li", (long int)luaL_checkinteger(L, -2));
            if (colsz < 0) {
                _failure(L, tcrdberrmsg(tcrdbecode(db)));
            }
        }

        // value
        size_t valsz;
        const char* val = luaL_checklstring(L, -1, &valsz);

        // push
        tcmapput(tuple, col ? col : scol, colsz, val, valsz);
        lua_pop(L, 1);
    }

    // store
    if (!tcrdbtblput(db, key, keysz, tuple)) {
        _failure(L, tcrdberrmsg(tcrdbecode(db)));
    } else {
        lua_pushboolean(L, 1);
    }
    tcmapdel(tuple);

    // ready
    return 1;
}

/*
 * Get tuple from db.
 *
 * {} = ttyrant.table:get(key)
 */
static int luaF_ttyrant_table_get(lua_State* L) {

    // db
    TCRDB* db = _self_tdb(L);

    // tools
    int colsz;
    const char* col = NULL;
    int valsz;
    const char* val = NULL;
    size_t keysz;
    const char* key = luaL_checklstring(L, 2, &keysz);

    // retrieve
    TCMAP* tuple = tcrdbtblget(db, key, keysz);
    if (!tuple) {
        _failure(L, tcrdberrmsg(tcrdbecode(db)));
    } else {
        lua_newtable(L);
        tcmapiterinit(tuple);
        while ((col = tcmapiternext(tuple, &colsz)) != NULL) {
            val = tcmapget(tuple, col, colsz, &valsz);
            lua_pushlstring(L, col, colsz);
            lua_pushlstring(L, val, valsz);
            lua_settable(L, -3);
        }
    }
    tcmapdel(tuple);

    // ready
    return 1;
}

/*
 * Erase tuple from db.
 *
 * true|false = ttyrant.table:out(key)
 */
static int luaF_ttyrant_table_out(lua_State* L) {

    // db
    TCRDB* db = _self_tdb(L);

    // key
    size_t keysz;
    const char* key = luaL_checklstring(L, 2, &keysz);

    // erase
    if (!tcrdbtblout(db, key, keysz)) {
        _failure(L, tcrdberrmsg(tcrdbecode(db)));
    }

    // ready
    lua_pushboolean(L, 1);
    return 1;
}

/*----------------------------------------------------------------------------------------------------------*/

/*
 * Create a query object.
 *
 * ttyrant.query:new()
 */
static int luaF_ttyrant_query_new(lua_State* L) {

    // instance
    if (!lua_istable(L, 1)) {
        luaL_error(L, "Invalid «self» given to ttyrant.query:new(), expected «ttyrant.query»!");
    }
    TCRDB* db = _self(L, 2, "__tdb", "Invalid «ttyrant.table» instance given to ttyrant.query:new()!");
    lua_newtable(L);

    // metatable
    lua_pushvalue(L, 1);
    lua_setfield(L, 1, "__index");      // self.__index = self
    lua_pushvalue(L, 1);
    lua_setmetatable(L, 3);             // setmetatable(instance, self)

    // spawn query
    RDBQRY* qry = tcrdbqrynew(db);
    if (!qry) {
        _failure(L, tcrdberrmsg(tcrdbecode(db)));
    } else {
        lua_pushlightuserdata(L, qry);
        lua_setfield(L, 3, "__qry");    // instance.__qry = <userdata>
    }

    // ready
    return 1;
}

/*
 * Destroy a query object.
 *
 * ttyrant.query:delete()
 */
static int luaF_ttyrant_query_delete(lua_State* L) {

    // instance
    RDBQRY* qry = _self_qry(L);

    // release
    tcrdbqrydel(qry);

    // ready
    lua_pushboolean(L, 1);
    return 1;
}

/*
 * Add a filtering rule to a query object.
 *
 * ttyrant.query:add_condition()
 */
static int luaF_ttyrant_query_add_condition(lua_State* L) {

    // instance
    RDBQRY* qry = _self_qry(L);

    // column
    const char* column = luaL_checkstring(L, 2);

    // nominal indicator table
    static const char* const operator_names[] = {
        "RDBQCSTREQ",
        "RDBQCSTRINC",
        "RDBQCSTRBW",
        "RDBQCSTREW",
        "RDBQCSTRAND",
        "RDBQCSTROR",
        "RDBQCSTROREQ",
        "RDBQCSTRRX",
        "RDBQCNUMEQ",
        "RDBQCNUMGT",
        "RDBQCNUMGE",
        "RDBQCNUMLT",
        "RDBQCNUMLE",
        "RDBQCNUMBT",
        "RDBQCNUMOREQ",
        "RDBQCFTSPH",
        "RDBQCFTSAND",
        "RDBQCFTSOR",
        "RDBQCFTSEX",
        "RDBQCNEGATE",
        "RDBQCNOIDX",
        NULL
    };

    // scalar indicator table
    static const int operator_values[] = {
        RDBQCSTREQ,
        RDBQCSTRINC,
        RDBQCSTRBW,
        RDBQCSTREW,
        RDBQCSTRAND,
        RDBQCSTROR,
        RDBQCSTROREQ,
        RDBQCSTRRX,
        RDBQCNUMEQ,
        RDBQCNUMGT,
        RDBQCNUMGE,
        RDBQCNUMLT,
        RDBQCNUMLE,
        RDBQCNUMBT,
        RDBQCNUMOREQ,
        RDBQCFTSPH,
        RDBQCFTSAND,
        RDBQCFTSOR,
        RDBQCFTSEX,
        RDBQCNEGATE,
        RDBQCNOIDX,
        0
    };

    // get indicator
    int operator = luaL_checkoption(L, 3, NULL, operator_names);

    // operand expression
    char buffer[64];
    const char* expression = NULL;
    if (lua_type(L, 4) == LUA_TSTRING) {
        expression = luaL_checkstring(L, 4);
    } else {
        if (snprintf(buffer, 64, "%f", luaL_checknumber(L, 4)) >= 0) {
            expression = buffer;
        }
    }

    // execute
    if (expression) {
        tcrdbqryaddcond(qry, column, operator, expression);
    }

    // ready
    lua_pushboolean(L, expression != NULL);
    return 1;
}

/*
 * Perform a search using the given query object.
 *
 * ttyrant.query:search()
 */
static int luaF_ttyrant_query_search(lua_State* L) {

    // instance
    RDBQRY* qry = _self_qry(L);

    // execute
    TCLIST* items = tcrdbqrysearch(qry);
    _tclist2luatable(L, items, 0);
    tclistdel(items);


    // ready
    return 1;
}

/*----------------------------------------------------------------------------------------------------------*/

/*
 * Setup defaults.
 */
#define _defaults(L)  { lua_pushstring(L, "localhost"); \
                        lua_setfield(L, -2, "host"); \
                        lua_pushinteger(L, 1978); \
                        lua_setfield(L, -2, "port"); }

/*
 * Entry point.
 */
int luaopen_ttyrant(lua_State* L) {

    // rdb registry
    static const luaL_Reg ttyrant[] = {
        { "open",           luaF_ttyrant_open },
        { "close",          luaF_ttyrant_close },
        { "add",            luaF_ttyrant_add },
        { "put",            luaF_ttyrant_put },
        { "get",            luaF_ttyrant_get },
        { "out",            luaF_ttyrant_out },
        { NULL, NULL }
    };

    // table registry
    static const luaL_Reg ttyrant_table[] = {
        { "open",           luaF_ttyrant_table_open },
        { "close",          luaF_ttyrant_table_close },
        { "add",            luaF_ttyrant_table_add },
        { "put",            luaF_ttyrant_table_put },
        { "get",            luaF_ttyrant_table_get },
        { "out",            luaF_ttyrant_table_out },
        { NULL, NULL }
    };

    // query registry
    static const luaL_Reg ttyrant_query[] = {
        { "new",            luaF_ttyrant_query_new },
        { "delete",         luaF_ttyrant_query_delete },
        { "add_condition",  luaF_ttyrant_query_add_condition },
        { "search",         luaF_ttyrant_query_search },
        { NULL, NULL }
    };

    // publish
    luaL_register(L, "ttyrant", ttyrant);
    _defaults(L);
    luaL_register(L, "ttyrant.table", ttyrant_table);
    _defaults(L);
    lua_pop(L, 1);
    luaL_register(L, "ttyrant.query", ttyrant_query);
    lua_pop(L, 1);

    // ready
    return 1;
}