local ffi = require("ffi")
local base = require("resty.core.base")


local DEFAULT_VALUE_BUF_SIZE = 512 * 2048 -- 1MB
base.set_string_buf_size(DEFAULT_VALUE_BUF_SIZE)


ffi.cdef([[
typedef unsigned int 	MDB_dbi;


typedef enum {
    NGX_LMDB_OP_GET = 0,
    NGX_LMDB_OP_PREFIX,
    NGX_LMDB_OP_SET,
    NGX_LMDB_OP_DB_OPEN,
    NGX_LMDB_OP_DB_DROP
} ngx_lua_resty_lmdb_operation_e;


typedef struct {
    ngx_lua_resty_lmdb_operation_e  opcode;
    ngx_str_t                       key;   /* GET, SET */
    ngx_str_t                       value; /* GET, SET */
    MDB_dbi                         dbi;   /* ALL OPS */
    unsigned int                    flags; /* SET, DROP */
} ngx_lua_resty_lmdb_operation_t;

typedef struct {
    size_t          map_size;        /**< Size of the data memory map */
    unsigned int    page_size;       /**< Size of a database page. */
    unsigned int    max_readers;     /**< max reader slots in the environment */
    unsigned int    num_readers;     /**< max reader slots used in the environment */
    unsigned int    allocated_pages; /**< number of pages allocated */
    size_t          in_use_pages;    /**< number of pages currently in-use */
    unsigned int    entries;         /**< the number of entries (key/value pairs) in the environment */
} ngx_lua_resty_lmdb_ffi_status_t;

int ngx_lua_resty_lmdb_ffi_env_info(ngx_lua_resty_lmdb_ffi_status_t *lst, char **err);


int ngx_lua_resty_lmdb_ffi_execute(ngx_lua_resty_lmdb_operation_t *ops,
    size_t n, int need_write, u_char *buf, size_t buf_len, char **err);
int ngx_lua_resty_lmdb_ffi_range(ngx_lua_resty_lmdb_operation_t *ops,
    size_t n, u_char *buf, size_t buf_len, char **err);
]])
