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
