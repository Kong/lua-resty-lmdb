#ifndef _NGX_LUA_RESTY_LMDB_MODULE_H_INCLUDED_
#define _NGX_LUA_RESTY_LMDB_MODULE_H_INCLUDED_


#include <ngx_config.h>
#include <ngx_core.h>
#include <lmdb.h>
#include <openssl/evp.h>


struct ngx_lua_resty_lmdb_conf_s {
    ngx_path_t  *env_path;
    size_t       max_databases;
    size_t       map_size;
    MDB_env     *env;
    MDB_txn     *ro_txn;

    ngx_str_t    validation_tag;

    ngx_str_t    key_file;
    ngx_str_t    encryption_mode;

    const EVP_CIPHER  *cipher;

    /*
     * SHA-256 digest of the lmdb_encryption_key file contents, derived once
     * in ngx_lua_resty_lmdb_init_conf() and reused by every later
     * mdb_env_set_encrypt() call (master re-opens, and every worker after
     * fork), so the raw key file contents never need to be kept around.
     */
    u_char       enc_key[32];
};


typedef struct ngx_lua_resty_lmdb_conf_s ngx_lua_resty_lmdb_conf_t;


typedef enum {
    NGX_LMDB_OP_GET = 0,
    NGX_LMDB_OP_PREFIX,
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


typedef struct {
    size_t          map_size;        /**< Size of the data memory map */
    unsigned int    page_size;       /**< Size of a database page. */
    unsigned int    max_readers;     /**< max reader slots in the environment */
    unsigned int    num_readers;     /**< max reader slots used in the environment */
    unsigned int    allocated_pages; /**< number of pages allocated */
    size_t          in_use_pages;    /**< number of pages used */
    unsigned int    entries;         /**< the number of entries (key/value pairs) in the environment */
} ngx_lua_resty_lmdb_ffi_status_t;


extern ngx_module_t ngx_lua_resty_lmdb_module;


#ifdef NGX_LUA_USE_ASSERT
#include <assert.h>
#   define ngx_lua_resty_lmdb_assert(a)  assert(a)
#else
#   define ngx_lua_resty_lmdb_assert(a)
#endif


#endif /* _NGX_LUA_RESTY_LMDB_MODULE_H_INCLUDED_ */
