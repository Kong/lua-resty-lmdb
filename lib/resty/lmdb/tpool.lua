local newtab   = require("table.new")
local cleartab = require("table.clear")


local _M = {}


local pool = {
    c = 0,
    [0] = 0,
    }


local MAX_POOL_SIZE     = 10 * 1000    -- 10K
local MAX_RELEASE_COUNT = MAX_POOL_SIZE * 100


function _M.fetch(narr, nrec)
    local len = pool[0]

    if len > 0 then
        local obj = pool[len]
        pool[len] = nil
        pool[0] = len - 1
        return obj
    end

    return newtab(narr, nrec)
end


function _M.release(obj)
    if not obj then
        return
    end

    cleartab(obj)

    do
        local cnt = pool.c + 1
        if cnt >= MAX_RELEASE_COUNT then
            pool = {
                c = 0,
                [0] = 0,
            }
            return
        end
        pool.c = cnt
    end

    local len = pool[0] + 1

    if len > MAX_POOL_SIZE then
        -- discard it simply
        return
    end

    pool[len] = obj
    pool[0] = len
end


return _M
