local ffi = require("ffi")
local base = require("resty.core.base")

local C = ffi.C
local ffi_string = ffi.string
local ffi_new = ffi.new
local tonumber = tonumber
local NGX_ERROR = ngx.ERROR

local err_ptr = base.get_errmsg_ptr()

local _M = {}

ffi.cdef([[
    typedef struct {
        size_t          map_size;        /**< Size of the data memory map */
        unsigned int    page_size;       /**< Size of a database page. */
        size_t          max_map_size;    /**< Size of the data memory map */
        unsigned int    last_used_page;  /**< page numbers of the last used pages */
        size_t          last_txnid;      /**< ID of the last committed transaction */
        unsigned int    max_readers;     /**< max reader slots in the environment */
        unsigned int    num_readers;     /**< max reader slots used in the environment */
    } ngx_lua_resty_lmdb_ffi_status_t;

    int ngx_lua_resty_lmdb_ffi_env_info(ngx_lua_resty_lmdb_ffi_status_t *lst, char **err);    
]])


function _M.get_env_info()
    local env_status = ffi_new("ngx_lua_resty_lmdb_ffi_status_t[1]")
    local ret = C.ngx_lua_resty_lmdb_ffi_env_info(env_status, err_ptr)
    if ret == NGX_ERROR then
        return nil, ffi_string(err_ptr[0])
    end

    assert(env_status[0] ~= nil)

    return {
        map_size       = tonumber(env_status[0].map_size),
        page_size      = tonumber(env_status[0].page_size),
        max_map_size   = tonumber(env_status[0].max_map_size),
        last_used_page = tonumber(env_status[0].last_used_page),
        last_txnid     = tonumber(env_status[0].last_txnid),
        max_readers    = tonumber(env_status[0].max_readers),
        num_readers    = tonumber(env_status[0].num_readers),
    }
end


return _M
