local ffi = require("ffi")
local base = require("resty.core.base")

local C = ffi.C
local ffi_string = ffi.string
local ffi_new = ffi.new
local NGX_ERROR = ngx.ERROR

local err_ptr = base.get_errmsg_ptr()

local _M = {}


ffi.cdef([[
    typedef struct {
        size_t          map_size;        /**< Size of the data memory map */
        unsigned int    page_size;       /**< Size of a database page. */
        size_t          max_map_size;    /**< Size of the data memory map */
        unsigned int    used_pages;
        size_t          last_txnid;      /**< ID of the last committed transaction */
        unsigned int    max_readers;     /**< max reader slots in the environment */
        unsigned int    num_readers;     /**< max reader slots used in the environment */
    } ngx_lua_resty_lmdb_ffi_statu_t;

    int ngx_lua_resty_lmdb_ffi_env_info(ngx_lua_resty_lmdb_ffi_statu_t *lst, const char **err);    
]])

local transaction = require("resty.lmdb.transaction")


do
    local CACHED_TXN = transaction.begin(1)


    function _M.get(key, db)
        CACHED_TXN:reset()
        CACHED_TXN:get(key, db)
        local res, err = CACHED_TXN:commit()
        if not res then
            return nil, err
        end

        return CACHED_TXN[1].result
    end


    function _M.set(key, value, db)
        CACHED_TXN:reset()
        CACHED_TXN:set(key, value, db)
        local res, err = CACHED_TXN:commit()
        if not res then
            return nil, err
        end

        return true
    end


    function _M.db_drop(delete, db)
        delete = not not delete

        CACHED_TXN:reset()
        CACHED_TXN:db_drop(delete, db)
        local res, err = CACHED_TXN:commit()
        if not res then
            return nil, err
        end

        return true
    end
end


function _M.get_env_info()
    local env_status = ffi_new("ngx_lua_resty_lmdb_ffi_statu_t[1]")
    local ret = C.ngx_lua_resty_lmdb_ffi_env_info(env_status, err_ptr)
    if ret == NGX_ERROR then
        return nil, ffi_string(err_ptr[0])
    end

    return {
        map_size = env_status.map_size,
        page_size = env_status.page_size,
        max_map_size = env_status.max_map_size,
        used_pages = env_status.used_pages,
        last_txnid = env_status.last_txnid,
        max_readers = env_status.max_readers,
        num_readers = env_status.num_readers,
    }
end


return _M
