local _M = {}


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
end


return _M
