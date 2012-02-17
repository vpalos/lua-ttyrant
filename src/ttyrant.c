/* 
 * Lua-TTyrant
 *
 * TokyoTyrant API for Lua.
 * Copyright (C)2010 by Valeriu Palos. All rights reserved.
 * This library is under the MIT license (see doc/LICENSE).
 */

#include <ctype.h>
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
#define _self_xyz(L, F, C)  _self(L, 1, "__" #F, "Invalid «self», expected «" C "» instance!")
#define _self_rdb(L)        (TCRDB*)_self_xyz(L, rdb, "ttyrant")
#define _self_tdb(L)        (TCRDB*)_self_xyz(L, tdb, "ttyrant.table")
#define _self_any(L)        (TCRDB*)_self_xyz(L, any, "ttyrant or ttyrant.table")
#define _self_qry(L)        (RDBQRY*)_self_xyz(L, qry, "ttyrant.query")

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
        luaL_error(L, "Invalid «self» for %class:open(), expected «%s»!", class, class);
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
        lua_pushlightuserdata(L, db);
        lua_setfield(L, 2, "__any");    // instance.__any = <userdata>
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
static int _increment(lua_State* L, TCRDB* db) {

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

/*
 * Put kinds.
 */
#define PUT_NORMAL  0
#define PUT_NR      1
#define PUT_CAT     2
#define PUT_KEEP    3

/*
 * Store value at given key in db.
 */
static int _put(lua_State* L, int kind) {
   
    // initialize
    TCRDB*  db = _self_rdb(L);

    // extract
    size_t keysz, valuesz;
    const char* key = luaL_checklstring(L, 2, &keysz);
    const char* value = luaL_checklstring(L, 3, &valuesz);

    // store
    int result = 0;
    switch (kind) {
        case PUT_CAT:
            result = tcrdbputcat(db, key, keysz, value, valuesz);
            break;
        case PUT_KEEP:
            result = tcrdbputkeep(db, key, keysz, value, valuesz);
            break;
        default:
            result = tcrdbput(db, key, keysz, value, valuesz);
            break;
    }

    // result
    if (!result) {
        _failure(L, tcrdberrmsg(tcrdbecode(db)));
    }
    lua_pushboolean(L, 1);

    // ready
    return 1;
}

/*
 * Store tuple or extend an existing one in a table db.
 */
static int _table_put(lua_State* L, const char* error, int kind) {

    // db
    TCRDB* db = _self_tdb(L);

    // tuple check
    if (!lua_istable(L, 3)) {
        luaL_error(L, error);
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
    int result = 0;
    switch (kind) {
        case PUT_CAT:
            result = tcrdbtblputcat(db, key, keysz, tuple);
            break;
        case PUT_KEEP:
            result = tcrdbtblputkeep(db, key, keysz, tuple);
            break;
        default:
            result = tcrdbtblput(db, key, keysz, tuple);
            break;
    }
    if (!result) {
        _failure(L, tcrdberrmsg(tcrdbecode(db)));
    } else {
        lua_pushboolean(L, 1);
    }
    tcmapdel(tuple);

    // ready
    return 1;
}

/*----------------------------------------------------------------------------------------------------------*/

/*
 * Erase key(s) from any db.
 *
 * <boolean> = <any>:out(key1, key2, ...)
 * <boolean> = <any>:out{key1, key2, ...}
 */
static int luaF_any_out(lua_State* L) {

    // initialize
    TCRDB*  db = _self_any(L);
    TCLIST* items = NULL;
    int     status = 0;

    // table
    if (lua_istable(L, 2)) {
        items = _luatable2tclist(L, 2, 0);

    // item
    } else if (lua_gettop(L) == 2) {
        size_t keysz;
        const char* key = luaL_checklstring(L, 2, &keysz);
        status = tcrdbout(db, key, keysz);

    // list
    } else {
        items = _lualist2tclist(L, 2);
    }

    // clear items
    if (items) {
        TCLIST* result = tcrdbmisc(db, "outlist", 0, items);
        if (result) {
            status = 1;
            tclistdel(result);
        }
        tclistdel(items);
    }

    // result
    if (!status) {
        _failure(L, tcrdberrmsg(tcrdbecode(db)));
    }
    lua_pushboolean(L, 1);

    // ready
    return 1;
}

/*
 * Erase all records from any database.
 *
 * <boolean> = <any>:vanish()
 */
static int luaF_any_vanish(lua_State* L) {
    TCRDB* db = _self_any(L);
    if (!tcrdbvanish(db)) {
        _failure(L, tcrdberrmsg(tcrdbecode(db)));
    }
    lua_pushboolean(L, 1);
    return 1;
}

/*
 * Synchronize any database with the storage back-end (i.e. disk).
 *
 * <boolean> = <any>:sync()
 */
static int luaF_any_sync(lua_State* L) {
    TCRDB* db = _self_any(L);
    if (!tcrdbsync(db)) {
        _failure(L, tcrdberrmsg(tcrdbecode(db)));
    }
    lua_pushboolean(L, 1);
    return 1;
}

/*
 * Get the number of recods in a database.
 *
 * <number> = <any>:rnum()
 */
static int luaF_any_rnum(lua_State* L) {
    TCRDB* db = _self_any(L);
    uint64_t rnum = tcrdbrnum(db);
    if (!rnum) {
        int ecode = tcrdbecode(db);
        if (ecode != TTESUCCESS) {
            _failure(L, tcrdberrmsg(ecode));
        }
    }
    lua_pushinteger(L, rnum);
    return 1;
}

/*
 * Get size of a database.
 *
 * <number> = <any>:size()
 */
static int luaF_any_size(lua_State* L) {
    TCRDB* db = _self_any(L);
    uint64_t size = tcrdbsize(db);
    if (!size) {
        _failure(L, tcrdberrmsg(tcrdbecode(db)));
    }
    lua_pushinteger(L, size);
    return 1;
}

/*
 * Clone database into another file.
 *
 * <boolean> = <any>:copy(file)
 */
static int luaF_any_copy(lua_State* L) {
    TCRDB* db = _self_any(L);
    const char* file = luaL_checkstring(L, 2);
    uint64_t size = tcrdbsize(db);
    if (!tcrdbcopy(db, file)) {
        _failure(L, tcrdberrmsg(tcrdbecode(db)));
    }
    lua_pushboolean(L, 1);
    return 1;
}

/*
 * Get database status.
 *
 * <table> = <any>:stat()
 */
static int luaF_any_stat(lua_State* L) {

    // extract/execute
    TCRDB* db = _self_any(L);
    char* stats = tcrdbstat(db);
    if (!stats) {
        _failure(L, tcrdberrmsg(tcrdbecode(db)));
    }
    
    // parse
    TCLIST* list = tclistnew();
    char* p1 = stats;
    char* p2 = stats;
    while (*p2) {
        if (*p2 == '\t' || *p2 == '\n') {
            tclistpush(list, p1, p2 - p1);
            p1 = p2 + 1;
        }
        ++p2;
    }
    _tclist2luatable(L, list, 1);
    tclistdel(list);
    free(stats);
    
    // ready
    return 1;
}

/*
 * Iterate over all keys in db.
 */
static int _luaF_any_keys_iterator(lua_State* L) {
    
    // extract
    TCRDB* db = lua_touserdata(L, lua_upvalueindex(1));
    
    // execute
    int keysz = 0;
    void* key = tcrdbiternext(db, &keysz);
    if (!key) {
        lua_pushnil(L);
    } else {
        lua_pushlstring(L, key, keysz);
        free(key);
    }
    
    return 1;
}
static int luaF_any_keys(lua_State* L) {
    
    // extract
    TCRDB* db = _self_any(L);
    
    // cursor
    tcrdbiterinit(db);
    lua_pushlightuserdata(L, db);
    
    // iterator
    lua_pushcclosure(L, _luaF_any_keys_iterator, 1);
    
    // ready
    return 1;
}

/*----------------------------------------------------------------------------------------------------------*/

/*
 * Open a regular database.
 *
 * <object> = ttyrant:open({ host = 'localhost', port = 1978 })
 */
static int luaF_ttyrant_open(lua_State* L) {
    return _open(L, "ttyrant", "__rdb");
}

/*
 * Close a regular database.
 *
 * <boolean> = ttyrant:close()
 */
static int luaF_ttyrant_close(lua_State* L) {
    return _close(L, _self_rdb(L));
}

/*
 * Increment numeric value at key.
 *
 * <number> = ttyrant:increment(key, amount)
 */
static int luaF_ttyrant_increment(lua_State* L) {
    return _increment(L, _self_rdb(L));
}

/*
 * Store value(s) at given key(s) in db.
 *
 * <boolean> = ttyrant:put(key1, value1, key2, value2, ...)
 * <boolean> = ttyrant:put{ key1 = value1, key2 = value2, ... }
 *
 * Note: implemented explicitly since it translates multiple key-value
 *       pairs into single-shot operations (i.e. 'putlist') for speed.
 */
static int luaF_ttyrant_put(lua_State* L) {

    // initialize
    TCRDB*  db = _self_rdb(L);
    TCLIST* items = NULL;
    int     status = 0;

    // table
    if (lua_istable(L, 2)) {
        items = _luatable2tclist(L, 2, 1);

    // item
    } else if (lua_gettop(L) == 3) {
        size_t keysz, valuesz;
        const char* key = luaL_checklstring(L, 2, &keysz);
        const char* value = luaL_checklstring(L, 3, &valuesz);
        status = tcrdbput(db, key, keysz, value, valuesz);

    // list
    } else {
        items = _lualist2tclist(L, 2);
    }

    // clear items
    if (items) {
        TCLIST* result = tcrdbmisc(db, "putlist", 0, items);
        if (result) {
            status = 1;
            tclistdel(result);
        }
        tclistdel(items);
    }

    // result
    if (!status) {
        _failure(L, tcrdberrmsg(tcrdbecode(db)));
    }
    lua_pushboolean(L, 1);

    // ready
    return 1;
}

/*
 * Append value at given key in db.
 *
 * <boolean> = ttyrant:putcat(key, value)
 */
static int luaF_ttyrant_putcat(lua_State* L) {
    return _put(L, PUT_CAT);
}

/*
 * Store a value at given key in db only if it does not exist.
 *
 * <boolean> = ttyrant:putkeep(key, value)
 */
static int luaF_ttyrant_putkeep(lua_State* L) {
    return _put(L, PUT_KEEP);
}

/*
 * Append value at given key in db and crop final value to given width.
 *
 * <boolean> = ttyrant:putshl(key, value, width)
 */
static int luaF_ttyrant_putshl(lua_State* L) {

    // initialize
    TCRDB*  db = _self_rdb(L);

    // extract
    size_t keysz, valuesz;
    const char* key = luaL_checklstring(L, 2, &keysz);
    const char* value = luaL_checklstring(L, 3, &valuesz);
    int width = luaL_checkint(L, 4);

    // store
    if (!tcrdbputshl(db, key, keysz, value, valuesz, width)) {
        _failure(L, tcrdberrmsg(tcrdbecode(db)));
    }
    lua_pushboolean(L, 1);

    // ready
    return 1;
}

/*
 * Store a value at given key in db without waiting for a response.
 *
 * <boolean> = ttyrant:putnr(key, value)
 */
static int luaF_ttyrant_putnr(lua_State* L) {
    return _put(L, PUT_NR);
}

/*
 * Get value(s) at key(s) from db.
 *
 * <value> = ttyrant:get(key1, key2, ...)
 * <value> = ttyrant:get{key1, key2, ...}
 */
static int luaF_ttyrant_get(lua_State* L) {

    // initialize
    TCRDB*  db = _self_rdb(L);
    TCLIST* keys = NULL;
    TCLIST* items = NULL;
    char*   item = NULL;
    int     itemsz = 0;

    // input set
    if (lua_istable(L, 2)) {
        keys = _luatable2tclist(L, 2, 0);
    } else if (lua_gettop(L) == 2) {
        size_t keysz;
        const char* key = luaL_checklstring(L, 2, &keysz);
        item = tcrdbget(db, key, keysz, &itemsz);
    } else {
        keys = _lualist2tclist(L, 2);
    }

    // act on set
    if (keys) {
        items = tcrdbmisc(db, "getlist", 0, keys);
        tclistdel(keys);
    }

    // result set
    if (item) {
        lua_pushlstring(L, item, itemsz);
        free(item);
    } else if (items) {
        _tclist2luatable(L, items, 1);
        tclistdel(items);
    } else {
        _failure(L, tcrdberrmsg(tcrdbecode(db)));
    }

    // done
    return 1;
}

/*
 * Return the width of the value stored at given key.
 *
 * <number> = ttyrant:vsiz(key)
 */
static int luaF_ttyrant_vsiz(lua_State* L) {

    // initialize
    TCRDB*  db = _self_rdb(L);

    // extract
    size_t keysz;
    const char* key = luaL_checklstring(L, 2, &keysz);

    // store
    int vsiz = tcrdbvsiz(db, key, keysz);
    if (vsiz < 0) {
        _failure(L, tcrdberrmsg(tcrdbecode(db)));
    }
    lua_pushinteger(L, vsiz);

    // ready
    return 1;
}

/*----------------------------------------------------------------------------------------------------------*/

/*
 * Open a table database.
 *
 * <object> = ttyrant.table:open({ host = 'localhost', port = 1978 })
 */
static int luaF_ttyrant_table_open(lua_State* L) {
    return _open(L, "ttyrant.table", "__tdb");
}

/*
 * Close a table database.
 *
 * <boolean> = ttyrant.table:close()
 */
static int luaF_ttyrant_table_close(lua_State* L) {
    return _close(L, _self_tdb(L));
}

/*
 * Increment '_num' column of tuple at key.
 *
 * <number> = ttyrant.table:add(key, amount)
 */
static int luaF_ttyrant_table_increment(lua_State* L) {
    return _increment(L, _self_tdb(L));
}

/*
 * Create new tuple in the table db using given column values.
 *
 * <boolean> = ttyrant.table:put(key, {})
 */
static int luaF_ttyrant_table_put(lua_State* L) {
    return _table_put(L, "Invalid value for «ttyrant.table:put()», expected a table/tuple!", PUT_NORMAL);
}

/*
 * Append given column values an existing tuple in the table db.
 *
 * <boolean> = ttyrant.table:putcat(key, {})
 */
static int luaF_ttyrant_table_putcat(lua_State* L) {
    return _table_put(L, "Invalid value for «ttyrant.table:putcat()», expected a table/tuple!", PUT_CAT);
}

/*
 * Create new tuple in the table db using given column values only if it does not exist.
 *
 * <boolean> = ttyrant.table:putkeep(key, {})
 */
static int luaF_ttyrant_table_putkeep(lua_State* L) {
    return _table_put(L, "Invalid value for «ttyrant.table:putkeep()», expected a table/tuple!", PUT_KEEP);
}

/*
 * Get tuple from db.
 *
 * <table> = ttyrant.table:get(key)
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
 * Create an index on a column.
 *
 * <boolean> = ttyrant.table:setindex(column, type, keep)
 */
static int luaF_ttyrant_table_setindex(lua_State* L) {

    // db
    TCRDB* db = _self_tdb(L);

    // column
    const char* column = luaL_checkstring(L, 2);
    int keep = lua_toboolean(L, 4);

    // nominal indicator table
    static const char* const type_names[] = {
        "LEXICAL",
        "DECIMAL",
        "TOKEN",
        "QGRAM",
        "OPT",
        "VOID",
        NULL
    };

    // scalar indicator table
    static const int type_values[] = {
        RDBITLEXICAL,
        RDBITDECIMAL,
        RDBITTOKEN,
        RDBITQGRAM,
        RDBITOPT,
        RDBITVOID,
        0
    };

    // extract indicator
    const char* type = luaL_checkstring(L, 3);
    char* p = (char*)type - 1;
    while (*(++p)) if (islower(*p)) *p = toupper(*p);
    
    // make "RDBIT" prefix optional
    if (strstr(type, "RDBIT") == type) {
        type += 5; // strlen("RDBIT") == 5
    }

    // extract operator
    lua_pushstring(L, type);
    int index = luaL_checkoption(L, -1, NULL, type_names);
    lua_pop(L, 1);

    // execute
    if (!tcrdbtblsetindex(db, column, type_values[index] | (keep ? RDBITKEEP : 0))) {
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
 * <object> = ttyrant.query:new()
 */
static int luaF_ttyrant_query_new(lua_State* L) {

    // instance
    if (!lua_istable(L, 1)) {
        luaL_error(L, "Invalid «self» for ttyrant.query:new(), expected «ttyrant.query»!");
    }
    TCRDB* db = _self(L, 2, "__tdb", "Invalid «ttyrant.table» instance for ttyrant.query:new()!");
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
 * <boolean> = ttyrant.query:delete()
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
 * <boolean> = ttyrant.query:addcond(column, operator, expression)
 */
static int luaF_ttyrant_query_addcond(lua_State* L) {

    // instance
    RDBQRY* qry = _self_qry(L);

    // column
    const char* column = luaL_checkstring(L, 2);

    // nominal indicator table
    static const char* const operator_names[] = {
        "STREQ",
        "STRINC",
        "STRBW",
        "STREW",
        "STRAND",
        "STROR",
        "STROREQ",
        "STRRX",
        "NUMEQ",
        "NUMGT",
        "NUMGE",
        "NUMLT",
        "NUMLE",
        "NUMBT",
        "NUMOREQ",
        "FTSPH",
        "FTSAND",
        "FTSOR",
        "FTSEX",
        "NEGATE",
        "NOIDX",
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

    // extract indicator
    const char* indicator = luaL_checkstring(L, 3);
    char* p = (char*)indicator - 1;
    while (*(++p)) if (islower(*p)) *p = toupper(*p);
    
    // make "RDBQC" prefix optional
    if (strstr(indicator, "RDBQC") == indicator) {
        indicator += 5; // strlen("RDBQC") == 5
    }

    // extract operator
    lua_pushstring(L, indicator);
    int operator = luaL_checkoption(L, -1, NULL, operator_names);
    lua_pop(L, 1);

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
        tcrdbqryaddcond(qry, column, operator_values[operator], expression);
    }

    // ready
    lua_pushboolean(L, expression != NULL);
    return 1;
}

/*
 * Limit the query result set.
 *
 * <boolean> = ttyrant.query:setlimit(limit[, offset])
 */
static int luaF_ttyrant_query_setlimit(lua_State* L) {

    // instance
    RDBQRY* qry = _self_qry(L);

    // limits
    int limit = luaL_checkint(L, 2);
    int offset = lua_tointeger(L, 3);

    // apply
    tcrdbqrysetlimit(qry, limit, offset);

    // ready
    lua_pushboolean(L, 1);
    return 1;
}

/*
 * Choose a search ordering.
 *
 * <boolean> = ttyrant.query:setorder(column, method)
 */
static int luaF_ttyrant_query_setorder(lua_State* L) {

    // instance
    RDBQRY* qry = _self_qry(L);

    // column
    const char* column = luaL_checkstring(L, 2);

    // nominal indicator table
    static const char* const method_names[] = {
        "STRASC",
        "STRDESC",
        "NUMASC",
        "NUMDESC",
        NULL
    };

    // scalar indicator table
    static const int method_values[] = {
        RDBQOSTRASC,
        RDBQOSTRDESC,
        RDBQONUMASC,
        RDBQONUMDESC,
        0
    };

    // extract indicator
    const char* method = luaL_checkstring(L, 3);
    char* p = (char*)method - 1;
    while (*(++p)) if (islower(*p)) *p = toupper(*p);
    if (strstr(method, "RDBQO") == method) {
        method += 5; // strlen("RDBQO") == 5
    }

    // extract method
    lua_pushstring(L, method);
    int sort = luaL_checkoption(L, -1, NULL, method_names);
    lua_pop(L, 1);

    // execute
    tcrdbqrysetorder(qry, column, method_values[sort]);

    // ready
    lua_pushboolean(L, 1);
    return 1;
}

/*
 * Get the list of keys corresponding to the query object.
 *
 * <table> = ttyrant.query:search()
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

/*
 * Remove all tuples corresponding to the query object.
 *
 * <table> = ttyrant.query:search_out()
 */
static int luaF_ttyrant_query_search_out(lua_State* L) {

    // instance
    RDBQRY* qry = _self_qry(L);

    // execute
    lua_pushboolean(L, tcrdbqrysearchout(qry));

    // ready
    return 1;
}

/*
 * Get the values of tuples which correspond to the query object.
 *
 * <table> = ttyrant.query:search_get()
 */
static int luaF_ttyrant_query_search_get(lua_State* L) {

    // instance
    RDBQRY* qry = _self_qry(L);

    // execute
    TCLIST* items = tcrdbqrysearchget(qry);


    // initialize
    lua_newtable(L);
    char* item;
    int itemsz;
    TCLIST* columns;
    int count = tclistnum(items);

    // traverse
    for (; count > 0; count--) {

        // tuple
        item = tclistshift(items, &itemsz);
        columns = tcstrsplit2(item + 1, itemsz);
        free(item);

        // key/values
        item = tclistshift(columns, &itemsz);
        lua_pushlstring(L, item, itemsz);
        free(item);
        _tclist2luatable(L, columns, 1);

        // store
        tclistdel(columns);
        lua_settable(L, -3);
    }
    tclistdel(items);

    // ready
    return 1;
}

/*
 * Count the tuples which correspond to the query object.
 *
 * <table> = ttyrant.query:search_count()
 */
static int luaF_ttyrant_query_search_count(lua_State* L) {

    // instance
    RDBQRY* qry = _self_qry(L);

    // execute
    lua_pushinteger(L, tcrdbqrysearchcount(qry));

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
        { "increment",      luaF_ttyrant_increment },
        { "put",            luaF_ttyrant_put },
        { "putcat",         luaF_ttyrant_putcat },
        { "putkeep",        luaF_ttyrant_putkeep },
        { "putshl",         luaF_ttyrant_putshl },
        { "putnr",          luaF_ttyrant_putnr },
        { "get",            luaF_ttyrant_get },
        { "vsiz",           luaF_ttyrant_vsiz },
        { "out",            luaF_any_out },
        { "vanish",         luaF_any_vanish },
        { "sync",           luaF_any_sync },
        { "rnum",           luaF_any_rnum },
        { "size",           luaF_any_size },
        { "copy",           luaF_any_copy },
        { "stat",           luaF_any_stat },
        { "keys",           luaF_any_keys },
        { NULL, NULL }
    };

    // table registry
    static const luaL_Reg ttyrant_table[] = {
        { "open",           luaF_ttyrant_table_open },
        { "close",          luaF_ttyrant_table_close },
        { "increment",      luaF_ttyrant_table_increment },
        { "put",            luaF_ttyrant_table_put },
        { "putcat",         luaF_ttyrant_table_putcat },
        { "putkeep",        luaF_ttyrant_table_putkeep },
        { "get",            luaF_ttyrant_table_get },
        { "setindex",       luaF_ttyrant_table_setindex },
        { "out",            luaF_any_out },
        { "vanish",         luaF_any_vanish },
        { "sync",           luaF_any_sync },
        { "rnum",           luaF_any_rnum },
        { "size",           luaF_any_size },
        { "copy",           luaF_any_copy },
        { "stat",           luaF_any_stat },
        { "keys",           luaF_any_keys },
        { NULL, NULL }
    };

    // query registry
    static const luaL_Reg ttyrant_query[] = {
        { "new",            luaF_ttyrant_query_new },
        { "delete",         luaF_ttyrant_query_delete },
        { "addcond",        luaF_ttyrant_query_addcond },
        { "setlimit",       luaF_ttyrant_query_setlimit },
        { "setorder",       luaF_ttyrant_query_setorder },
        { "search",         luaF_ttyrant_query_search },
        { "search_get",     luaF_ttyrant_query_search_get },
        { "search_out",     luaF_ttyrant_query_search_out },
        { "search_count",   luaF_ttyrant_query_search_count },
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
