local _M = {}


local transaction = require("resty.lmdb.transaction")
local range = require("resty.lmdb.range")
local status = require("resty.lmdb.status")


local next = next
local error = error
local assert = assert
local range_page = range.page


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


function _M.prefix(prefix, db)
    local res, i, res_n, err_or_more
    local last = prefix

    return function()
        ::more::
        if not res then
            -- need to fetch more data
            res, err_or_more = range_page(last, db)
            if not res then
                return nil, err_or_more
            end

            res_n = #res
            if res_n == 0 or (i and res_n == 1) then
                return nil
            end

            if i then
                -- not the first call to prefix_page, skip the first key
                i = 2

            else
                -- first call to prefix_page
                i = 1
            end
        end

        assert(res_n > 0)

        if i > res_n then
            if err_or_more then
                last = res[i - 1].key
                res = nil
                goto more
            end

            -- more = false

            return nil
        end

        local key = res[i].key
        local value = res[i].value
        i = i + 1

        return key, value
    end
end


_M.get_env_info = status.get_env_info


return _M
