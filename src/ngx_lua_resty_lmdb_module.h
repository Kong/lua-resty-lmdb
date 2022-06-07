#ifndef _NGX_LUA_RESTY_LMDB_MODULE_H_INCLUDED_
#define _NGX_LUA_RESTY_LMDB_MODULE_H_INCLUDED_


#include <ngx_config.h>
#include <ngx_core.h>
#include <lmdb.h>


struct ngx_lua_resty_lmdb_conf_s {
    ngx_path_t  *env_path;
    size_t       max_databases;
    size_t       map_size;
    MDB_env     *env;
    MDB_txn     *ro_txn;

    ngx_str_t    key_file;
    ngx_str_t    key_data;
    ngx_str_t    encryption_mode;
};


typedef struct ngx_lua_resty_lmdb_conf_s ngx_lua_resty_lmdb_conf_t;


typedef enum {
    NGX_LMDB_OP_GET = 0,
    NGX_LMDB_OP_SET,
    NGX_LMDB_OP_DB_OPEN,
    NGX_LMDB_OP_DB_DROP
} ngx_lua_resty_lmdb_operation_e;


struct ngx_lua_resty_lmdb_operation_s {
    ngx_lua_resty_lmdb_operation_e  opcode;
    ngx_str_t                       key;   /* GET, SET */
    ngx_str_t                       value; /* GET, SET */
    MDB_dbi                         dbi;   /* ALL OPS */
    unsigned int                    flags; /* SET, DROP */
};


typedef struct ngx_lua_resty_lmdb_operation_s ngx_lua_resty_lmdb_operation_t;


extern ngx_module_t ngx_lua_resty_lmdb_module;


#ifdef NGX_LUA_USE_ASSERT
#include <assert.h>
#   define ngx_lua_resty_lmdb_assert(a)  assert(a)
#else
#   define ngx_lua_resty_lmdb_assert(a)
#endif


#endif /* _NGX_LUA_RESTY_LMDB_MODULE_H_INCLUDED_ */
