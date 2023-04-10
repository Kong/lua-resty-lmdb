local _M = {}


local ffi = require("ffi")
local base = require("resty.core.base")
local table_new = require("table.new")


ffi.cdef([[
typedef unsigned int 	MDB_dbi;


typedef enum {
    NGX_LMDB_OP_GET = 0,
    NGX_LMDB_OP_SET,
    NGX_LMDB_OP_DB_OPEN,
    NGX_LMDB_OP_DB_DROP,
} ngx_lua_resty_lmdb_operation_e;


typedef struct {
    ngx_lua_resty_lmdb_operation_e  opcode;
    ngx_str_t                       key;   /* GET, SET */
    ngx_str_t                       value; /* GET, SET */
    MDB_dbi                         dbi;   /* ALL OPS */
    unsigned int                    flags; /* SET, DROP */
} ngx_lua_resty_lmdb_operation_t;


int ngx_lua_resty_lmdb_ffi_execute(ngx_lua_resty_lmdb_operation_t *ops,
    size_t n, int need_write, unsigned char *buf, size_t buf_len, char **err);
]])


local err_ptr = base.get_errmsg_ptr()
local get_string_buf = base.get_string_buf
local get_string_buf_size = base.get_string_buf_size
local setmetatable = setmetatable
local assert = assert
local math_max = math.max
local type = type
local C = ffi.C
local ffi_string = ffi.string
local ffi_new = ffi.new
local MIN_OPS_N = 16
local DEFAULT_VALUE_BUF_SIZE = 16 * 1024 -- 16KB
base.set_string_buf_size(DEFAULT_VALUE_BUF_SIZE)
local NGX_ERROR = ngx.ERROR
local NGX_AGAIN = ngx.AGAIN
local NGX_OK = ngx.OK
local _TXN_MT = {}
_TXN_MT.__index = _TXN_MT


local CACHED_DBI = {}
local MDB_CREATE = 0x40000
local DEFAULT_DB = "_default"


function _M.begin(hint)
    hint = hint or 4

    local txn = table_new(hint, 4)

    txn.n = 0
    txn.write = false
    txn.ops_capacity = 0
    txn.ops = nil

    return setmetatable(txn, _TXN_MT)
end


local normalize_key
do
    local resty_sha256 = assert(require("resty.sha256").new())

    -- lmdb has 511 bytes limitation for key
    local MAX_KEY_SIZE = 511

    local sha256 = function(str)
        resty_sha256:reset()
        resty_sha256:update(str)
        return resty_sha256:final()
    end

    normalize_key = function(key)
        if key and #key > MAX_KEY_SIZE then
            return assert(sha256(key))
        end

        return key
    end
end


local get_dbi
do
    local CACHED_TXN_DBI = _M.begin(1)
    function _M.get_dbi(create, db)
        local dbi = CACHED_DBI[db]
        if dbi then
            return dbi
        end

        CACHED_TXN_DBI:reset()
        CACHED_TXN_DBI:db_open(create, db)
        local res, err = CACHED_TXN_DBI:commit()
        if not res then
            return nil, err
        end

        dbi = CACHED_TXN_DBI[1].result

        -- dbi already cached by commit()

        return dbi
    end
    get_dbi = _M.get_dbi
end


function _TXN_MT:reset()
    self.n = 0
    self.write = false
end


function _TXN_MT:_new_op()
    local n = self.n + 1
    self.n = n

    local op = self[n]
    if not op then
        op = table_new(0, 4)
        self[n] = op
    end

    return op
end


function _TXN_MT:get(key, db)
    local op = self:_new_op()
    op.opcode = "GET"
    op.key = normalize_key(key)
    op.db = db or DEFAULT_DB
end


function _TXN_MT:set(key, value, db)
    local op = self:_new_op()
    op.opcode = "SET"
    op.key = normalize_key(key)
    op.value = value
    op.db = db or DEFAULT_DB
    op.flags = 0

    self.write = true
end


function _TXN_MT:db_open(create, db)
    assert(type(create) == "boolean")

    local op = self:_new_op()
    op.opcode = "DB_OPEN"
    op.db = db or DEFAULT_DB
    op.flags = create and MDB_CREATE or 0

    self.write = true
end


function _TXN_MT:db_drop(delete, db)
    assert(type(delete) == "boolean")

    local op = self:_new_op()

    op.opcode = "DB_DROP"
    op.db = db or DEFAULT_DB
    op.flags = delete

    self.write = true
end


function _TXN_MT:commit()
    local value_buf_size = get_string_buf_size()
    local ops

    if self.ops_capacity >= self.n then
        ops = self.ops

    else
        self.ops_capacity = math_max(self.n, MIN_OPS_N)
        ops = ffi_new("ngx_lua_resty_lmdb_operation_t[?]", self.ops_capacity)
        self.ops = ops
    end

    for i = 1, self.n do
        local lop = self[i]
        local cop = ops[i - 1]

        if lop.opcode == "GET" then
            local dbi, err = get_dbi(false, lop.db)
            if err then
                return nil, "unable to open DB for access: " .. err

            elseif not dbi then
                return nil, "DB " .. lop.db .. " does not exist"
            end

            cop.opcode = C.NGX_LMDB_OP_GET
            cop.key.data = lop.key
            cop.key.len = #lop.key
            cop.dbi = dbi

        elseif lop.opcode == "SET" then
            local dbi, err = get_dbi(true, lop.db)
            if err then
                return nil, "unable to open DB for access: " .. err

            elseif not dbi then
                return nil, "DB " .. lop.db .. " does not exist"
            end

            cop.opcode = C.NGX_LMDB_OP_SET
            cop.key.data = lop.key
            cop.key.len = #lop.key
            local val = cop.value
            if lop.value == nil then
                val.data = nil
                val.len = 0

            else
                val.data = lop.value
                val.len = #lop.value
            end
            cop.dbi = dbi
            cop.flags = lop.flags

        elseif lop.opcode == "DB_OPEN" then
            cop.opcode = C.NGX_LMDB_OP_DB_OPEN
            cop.key.data = lop.db
            cop.key.len = #lop.db
            cop.flags = lop.flags

        elseif lop.opcode == "DB_DROP" then
            local dbi, err = get_dbi(false, lop.db)
            if err then
                return nil, "unable to open DB for access: " .. err

            elseif not dbi then
                return nil, "DB " .. lop.db .. " does not exist"
            end

            cop.opcode = C.NGX_LMDB_OP_DB_DROP
            cop.flags = lop.flags
            cop.dbi = dbi

        else
            assert(false)
        end
    end

::again::
    local ret = C.ngx_lua_resty_lmdb_ffi_execute(ops, self.n, self.write,
                    get_string_buf(value_buf_size, false), value_buf_size, err_ptr)
    if ret == NGX_ERROR then
        return nil, ffi_string(err_ptr[0])
    end

    if ret == NGX_AGAIN then
        value_buf_size = value_buf_size * 2
        goto again
    end

    assert(ret == NGX_OK)

    for i = 1, self.n do
        local cop = ops[i - 1]
        local lop = self[i]

        if cop.opcode == C.NGX_LMDB_OP_GET then
            if cop.value.data ~= nil then
                lop.result = ffi_string(cop.value.data, cop.value.len)

            else
                lop.result = nil
            end

        elseif cop.opcode == C.NGX_LMDB_OP_SET then
            -- Set does not return flags
            lop.result = true

        elseif cop.opcode == C.NGX_LMDB_OP_DB_OPEN then
            -- cache the DBi
            lop.result = cop.dbi
            CACHED_DBI[lop.db] = cop.dbi

        elseif cop.opcode == C.NGX_LMDB_OP_DB_DROP then
            -- remove cached DBi
            lop.result = true

            if lop.flags then
                -- delete == true
                CACHED_DBI[lop.db] = nil
            end
        end
    end

    return true
end


return _M
