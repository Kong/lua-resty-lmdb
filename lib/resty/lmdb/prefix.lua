local _M = {}


local ffi = require("ffi")
local table_new = require("table.new")
require("resty.lmdb.cdefs")
local transaction = require("resty.lmdb.transaction")
local base = require("resty.core.base")


local C = ffi.C
-- DEFAULT_OPS_SIZE must be >= 2,
-- see the function comment for ngx_lua_resty_lmdb_ffi_prefix
local DEFAULT_OPS_SIZE = 512
local DEFAULT_DB = transaction.DEFAULT_DB
local NGX_ERROR = ngx.ERROR
local NGX_AGAIN = ngx.AGAIN


local ffi_string = ffi.string
local ffi_new = ffi.new
local get_dbi = transaction.get_dbi
local err_ptr = base.get_errmsg_ptr()
local get_string_buf = base.get_string_buf
local get_string_buf_size = base.get_string_buf_size
local assert = assert
local math_max = math.max

function _M.page(start, prefix, db, page_size)
    if not page_size then
        page_size = DEFAULT_OPS_SIZE
    end

    assert(page_size >= 1, "page_size must be at least 1")

    -- the function ngx_lua_resty_lmdb_ffi_prefix requires at least 2 operations
    -- special handling for the case when page_size is less than 2
    local query_size = math_max(page_size, 2)

    local value_buf_size = get_string_buf_size()
    local ops = ffi_new("ngx_lua_resty_lmdb_operation_t[?]", query_size)

    local dbi, err = get_dbi(false, db or DEFAULT_DB)
    if err then
        return nil, "unable to open DB for access: " .. err

    elseif not dbi then
        return nil, "DB " .. db .. " does not exist"
    end

    ops[0].dbi = dbi

::again::
    ops[0].opcode = C.NGX_LMDB_OP_PREFIX
    ops[0].key.data = start
    ops[0].key.len = #start

    ops[1].opcode = C.NGX_LMDB_OP_PREFIX
    ops[1].key.data = prefix
    ops[1].key.len = #prefix

    local buf = get_string_buf(value_buf_size, false)
    local ret = C.ngx_lua_resty_lmdb_ffi_prefix(ops, query_size,
                    buf, value_buf_size, err_ptr)
    if ret == NGX_ERROR then
        return nil, ffi_string(err_ptr[0])
    end

    if ret == NGX_AGAIN then
        value_buf_size = value_buf_size * 2
        goto again
    end

    if ret == 0 then
        -- unlikely case
        return {}, false
    end

    assert(ret > 0)

    -- special handling for the case when page_size is less than 2
    if ret > page_size then
        -- we then blindly use ret == page_size to indicate there are more keys
        ret = page_size
    end

    local res = table_new(ret, 0)

    for i = 1, ret do
        local cop = ops[i - 1]

        assert(cop.opcode == C.NGX_LMDB_OP_PREFIX)

        local pair = {
            key = ffi_string(cop.key.data, cop.key.len),
            value = ffi_string(cop.value.data, cop.value.len),
        }

        res[i] = pair
    end

    -- if ret == page_size, then it is possible there are more keys
    return res, ret == page_size
end


return _M
