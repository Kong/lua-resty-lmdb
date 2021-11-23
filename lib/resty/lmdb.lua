local _M = {}


local bit = require("bit")
local ffi = require("ffi")
local base = require("resty.core.base")
local table_new = require("table.new")
local table_clear = require("table.clear")


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
local error = error
local setmetatable = setmetatable
local assert = assert
local math_max = math.max
local type = type
local band = bit.band
local C = ffi.C
local ffi_string = ffi.string
local ffi_new = ffi.new
local ffi_cast = ffi.cast
local MIN_OPS_N = 16
local DEFAULT_VALUE_BUF_SIZE = 4096
local NGX_ERROR = ngx.ERROR
local NGX_AGAIN = ngx.AGAIN
local NGX_OK = ngx.OK
local _TXN_MT = {}
_TXN_MT.__index = _TXN_MT


local CACHED_DBI = {}
local MDB_CREATE = 0x40000
local DEFAULT_DB = "_default"


function _TXN_MT:reset()
    table_clear(self)

    self.n = 0
    self.write = false
    self.ops_capacity = 0
    self.ops = nil
end


function _TXN_MT:get(key, db)
    local n = self.n + 1

    self[n] = {
        opcode = "GET",
        key = key,
        db = db or DEFAULT_DB,
    }

    self.n = n
end


function _TXN_MT:set(key, value, db)
    local n = self.n + 1

    self[n] = {
        opcode = "SET",
        key = key,
        value = value,
        db = db or DEFAULT_DB,
    }

    self.n = n
    self.write = true
end


function _TXN_MT:db_open(db, create)
    assert(type(create) == "boolean")

    local n = self.n + 1

    self[n] = {
        opcode = "DB_OPEN",
        db = db or DEFAULT_DB,
        flags = create and MDB_CREATE or 0,
    }

    self.n = n
    self.write = create
end


function _TXN_MT:db_drop(db, delete)
    assert(type(delete) == "boolean")

    local n = self.n + 1

    self[n] = {
        opcode = "DB_DROP",
		db = db or DEFAULT_DB,
        flags = delete,
    }

    self.n = n
    self.write = true
end


local get_dbi
function _TXN_MT:commit(txn)
    local value_buf_size = DEFAULT_VALUE_BUF_SIZE
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
            local dbi, err = get_dbi(lop.db)
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
            local dbi, err = get_dbi(lop.db, true)
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

        elseif lop.opcode == "DB_OPEN" then
            cop.opcode = C.NGX_LMDB_OP_DB_OPEN
            cop.key.data = lop.db
            cop.key.len = #lop.db
            cop.flags = lop.flags

        elseif lop.opcode == "DB_DROP" then
            local dbi, err = get_dbi(lop.db, false)
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


do
    function _M.begin(hint)
        hint = hint or 4

        local txn = table_new(hint, 3)

        txn.n = 0
        txn.write = false
        txn.ops_capacity = 0
        txn.ops = nil

        return setmetatable(txn, _TXN_MT)
    end

    local CACHED_TXN = _M.begin(1)
    local CACHED_TXN_DBI = _M.begin(1)

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


    function _M.db_drop(db, delete)
        delete = not not delete

        CACHED_TXN:reset()
        CACHED_TXN:db_drop(db, delete)
        local res, err = CACHED_TXN:commit()
        if not res then
            return nil, err
        end

        return true
    end


    function _M.get_dbi(db, create)
        create = not not create

        local dbi = CACHED_DBI[db]
        if dbi then
            return dbi
        end

        CACHED_TXN_DBI:reset()
        CACHED_TXN_DBI:db_open(db, create)
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

return _M
