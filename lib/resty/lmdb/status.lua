local _M = {}


require("resty.lmdb.cdefs")
local ffi = require("ffi")
local base = require("resty.core.base")

local C = ffi.C
local ffi_string = ffi.string
local ffi_new = ffi.new
local tonumber = tonumber
local NGX_ERROR = ngx.ERROR

local err_ptr = base.get_errmsg_ptr()


function _M.get_env_info()
    local env_status = ffi_new("ngx_lua_resty_lmdb_ffi_status_t[1]")
    local ret = C.ngx_lua_resty_lmdb_ffi_env_info(env_status, err_ptr)
    if ret == NGX_ERROR then
        return nil, ffi_string(err_ptr[0])
    end

    assert(env_status[0] ~= nil)

    return {
        map_size        = tonumber(env_status[0].map_size),
        page_size       = tonumber(env_status[0].page_size),
        max_readers     = tonumber(env_status[0].max_readers),
        num_readers     = tonumber(env_status[0].num_readers),
        allocated_pages = tonumber(env_status[0].allocated_pages),
        in_use_pages    = tonumber(env_status[0].in_use_pages),
        entries         = tonumber(env_status[0].entries),
    }
end


return _M
