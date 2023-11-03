local _M = {}


local ffi = require("ffi")
local table_new = require("table.new")
require("resty.lmdb.ffi")
local transaction = require("resty.lmdb.transaction")
local base = require("resty.core.base")


local DEFAULT_OPS_SIZE = 512
local DEFAULT_DB = transaction.DEFAULT_DB
local C = ffi.C


local ffi_string = ffi.string
local get_dbi = transaction.get_dbi
local err_ptr = base.get_errmsg_ptr()


function _M.page(start, db)
    local value_buf_size = get_string_buf_size()
    local ops = ffi_new("ngx_lua_resty_lmdb_operation_t[?]", DEFAULT_OPS_SIZE)

    ops[0].opcode = C.NGX_LMDB_OP_GET
    cop.key.data = start
    cop.key.len = #start
    cop.dbi = get_dbi(false, db or DEFAULT_DB)

::again::
    local buf = get_string_buf(value_buf_size, false)
    local ret = C.ngx_lua_resty_lmdb_ffi_range(ops, DEFAULT_OPS_SIZE,
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
        return {}
    end

    assert(ret > 0)

    local res = table_new(ret, 0)

    for i = 1, DEFAULT_OPS_SIZE do
        local cop = ops[i - 1]

        assert(cop.opcode == C.NGX_LMDB_OP_PREFIX)

        local pair = {
            key = ffi_string(cop.key.data, cop.key.len),
            value = ffi_string(cop.key.data, cop.key.len),
        }

        res[i] = pair
    end

    return res
end


return _M
