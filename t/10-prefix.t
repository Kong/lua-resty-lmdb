# vim:set ft= ts=4 sw=4 et:

use Test::Nginx::Socket::Lua;
use Cwd qw(cwd);

repeat_each(2);

plan tests => repeat_each() * blocks() * 5;

my $pwd = cwd();

our $MainConfig = qq{
    lmdb_environment_path /tmp/test.mdb;
    lmdb_map_size 5m;
};

our $HttpConfig = qq{
    lua_package_path "$pwd/lib/?.lua;;";
};

no_long_string();
#no_diff();

run_tests();

__DATA__

=== TEST 1: prefix() operation
--- http_config eval: $::HttpConfig
--- main_config eval: $::MainConfig
--- config
    location = /t {
        content_by_lua_block {
            local l = require("resty.lmdb")

            ngx.say(l.db_drop(true))
            ngx.say(l.set("test", "value"))
            ngx.say(l.set("test1", "value1"))
            ngx.say(l.set("test2", "value2"))
            ngx.say(l.set("test3", "value3"))
            ngx.say(l.set("u", "value4"))
            ngx.say(l.set("u1", "value5"))

            for k, v in l.prefix("tes") do
                ngx.say("key: ", k, " value: ", v)
            end
        }
    }
--- request
GET /t
--- response_body
true
true
true
true
true
true
true
key: test value: value
key: test1 value: value1
key: test2 value: value2
key: test3 value: value3
--- no_error_log
[error]
[warn]
[crit]



=== TEST 2: prefix() operation not found
--- http_config eval: $::HttpConfig
--- main_config eval: $::MainConfig
--- config
    location = /t {
        content_by_lua_block {
            local l = require("resty.lmdb")

            ngx.say(l.db_drop(true))
            ngx.say(l.set("test", "value"))

            for k, v in l.prefix("test1") do
                ngx.say("key: ", k, " value: ", v)
            end
        }
    }
--- request
GET /t
--- response_body
true
true
--- no_error_log
[error]
[warn]
[crit]



=== TEST 3: prefix() operation only 1 result
--- http_config eval: $::HttpConfig
--- main_config eval: $::MainConfig
--- config
    location = /t {
        content_by_lua_block {
            local l = require("resty.lmdb")

            ngx.say(l.db_drop(true))
            ngx.say(l.set("test", "value"))

            for k, v in l.prefix("test") do
                ngx.say("key: ", k, " value: ", v)
            end
        }
    }
--- request
GET /t
--- response_body
true
true
key: test value: value
--- no_error_log
[error]
[warn]
[crit]



=== TEST 4: prefix() operation 511-513 keys (edge cases)
--- http_config eval: $::HttpConfig
--- main_config eval: $::MainConfig
--- config
    location = /t {
        content_by_lua_block {
            local l = require("resty.lmdb")

            for i = 511, 513 do
                ngx.say(l.db_drop(true))

                for j = 1, i do
                    assert(l.set("test:" .. j, "value:" .. j))
                end

                local j = 0
                local found = {}
                for k, v in l.prefix("test:") do
                    j = j + 1
                    found[k] = v
                end

                ngx.say("j = ", j)

                for j = 1, i do
                    assert(found["test:" .. j] == "value:" .. j)
                end

                ngx.say("done")
            end
        }
    }
--- request
GET /t
--- response_body
true
j = 511
done
true
j = 512
done
true
j = 513
done
--- no_error_log
[error]
[warn]
[crit]



=== TEST 5: large values
--- http_config eval: $::HttpConfig
--- main_config eval: $::MainConfig
--- config
    location = /t {
        content_by_lua_block {
            local l = require("resty.lmdb")

            ngx.say(l.db_drop(true))
            for i = 1, 1024 do
                assert(l.set("test:" .. i, "value:" .. string.rep("x", i)))
            end

            local j = 0
            local found = {}
            for k, v in l.prefix("test:") do
                j = j + 1
                found[k] = v
            end

            ngx.say("j = ", j)

            for i = 1, 1024 do
                assert(found["test:" .. i] == "value:" .. string.rep("x", i))
            end

            ngx.say("done")
        }
    }
--- request
GET /t
--- response_body
true
j = 1024
done
--- no_error_log
[error]
[warn]
[crit]



=== TEST 6: prefix.page() operation
--- http_config eval: $::HttpConfig
--- main_config eval: $::MainConfig
--- config
    location = /t {
        content_by_lua_block {
            local l = require("resty.lmdb")

            ngx.say(l.db_drop(true))
            ngx.say(l.set("test", "value"))
            ngx.say(l.set("test1", "value1"))
            ngx.say(l.set("test2", "value2"))
            ngx.say(l.set("test3", "value3"))
            ngx.say(l.set("u", "value4"))
            ngx.say(l.set("u1", "value5"))

            local p = require("resty.lmdb.prefix")

            local res, err = p.page("test", "test")
            if not res then
                ngx.say("page errored: ", err)
            end

            for _, pair in ipairs(res) do
                ngx.say("key: ", pair.key, " value: ", pair.value)
            end
        }
    }
--- request
GET /t
--- response_body
true
true
true
true
true
true
true
key: test value: value
key: test1 value: value1
key: test2 value: value2
key: test3 value: value3
--- no_error_log
[error]
[warn]
[crit]



=== TEST 7: prefix.page() operation with custom page size
--- http_config eval: $::HttpConfig
--- main_config eval: $::MainConfig
--- config
    location = /t {
        content_by_lua_block {
            local l = require("resty.lmdb")

            ngx.say(l.db_drop(true))
            ngx.say(l.set("test", "value"))
            ngx.say(l.set("test1", "value1"))
            ngx.say(l.set("test2", "value2"))
            ngx.say(l.set("test3", "value3"))
            ngx.say(l.set("u", "value4"))
            ngx.say(l.set("u1", "value5"))

            local p = require("resty.lmdb.prefix")

            local res, err = p.page("test", "test", nil, 2)
            if not res then
                ngx.say("page errored: ", err)
            end

            ngx.say("FIRST PAGE")
            for _, pair in ipairs(res) do
                ngx.say("key: ", pair.key, " value: ", pair.value)
            end

            res, err = p.page("test1\x00", "test", nil, 2)
            if not res then
                ngx.say("page errored: ", err)
            end

            ngx.say("SECOND PAGE")
            for _, pair in ipairs(res) do
                ngx.say("key: ", pair.key, " value: ", pair.value)
            end
        }
    }
--- request
GET /t
--- response_body
true
true
true
true
true
true
true
FIRST PAGE
key: test value: value
key: test1 value: value1
SECOND PAGE
key: test2 value: value2
key: test3 value: value3
--- no_error_log
[error]
[warn]
[crit]



=== TEST 8: prefix.page() operation with invalid page size
--- http_config eval: $::HttpConfig
--- main_config eval: $::MainConfig
--- config
    location = /t {
        content_by_lua_block {
            local l = require("resty.lmdb")

            ngx.say(l.db_drop(true))
            local p = require("resty.lmdb.prefix")

            ngx.say(l.set("test", "value"))
            ngx.say(pcall(p.page, "test", "test", nil, 1))
        }
    }
--- request
GET /t
--- response_body
true
true
false.../lua-resty-lmdb/lua-resty-lmdb/lib/resty/lmdb/prefix.lua:34: 'page_size' can not be less than 2
--- no_error_log
[error]
[warn]
[crit]



=== TEST 9: prefix.page() operation with large page size [KAG-5874]
--- http_config eval: $::HttpConfig
--- main_config eval: $::MainConfig
--- config
    location = /t {
        content_by_lua_block {
            local l = require("resty.lmdb")

            ngx.say(l.db_drop(true))

            ngx.say(l.set("test", "value"))

            local inserted = { test = "value" }

            for i = 1, 2048 do
                -- string.rep with 120 makes sure each page goes just over the 1MB
                -- default buffer size and triggers a realloc
                assert(l.set(string.rep("test", 120) .. i, string.rep("value", 120)))
                inserted[string.rep("test", 120) .. i] = string.rep("value", 120)
            end

            ngx.say(l.set("u", "value4"))
            ngx.say(l.set("u1", "value5"))

            local p = require("resty.lmdb.prefix")

            local res, err = p.page("test", "test", nil, 1000)
            if not res then
                ngx.say("page errored: ", err)
            end

            ngx.say("FIRST PAGE")
            for _, pair in ipairs(res) do
                inserted[pair.key] = nil
            end

            res, err = p.page(res[#res].key .. "\x00", "test", nil, 1000)
            if not res then
                ngx.say("page errored: ", err)
            end
            ngx.say("SECOND PAGE")
            for _, pair in ipairs(res) do
                inserted[pair.key] = nil
            end

            res, err = p.page(res[#res].key .. "\x00", "test", nil, 1000)
            if not res then
                ngx.say("page errored: ", err)
            end
            ngx.say("THIRD PAGE")
            for _, pair in ipairs(res) do
                inserted[pair.key] = nil
            end

            ngx.say(next(inserted))
        }
    }
--- request
GET /t
--- response_body
true
true
true
true
FIRST PAGE
SECOND PAGE
THIRD PAGE
nil
--- no_error_log
[error]
[warn]
[crit]
