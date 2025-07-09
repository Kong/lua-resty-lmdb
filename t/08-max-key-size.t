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

=== TEST 1: key size is 512 set() / get()
--- http_config eval: $::HttpConfig
--- main_config eval: $::MainConfig
--- config
    location = /t {
        content_by_lua_block {
            local l = require("resty.lmdb")

            local key = string.rep("a", 512)
            ngx.say(l.set(key, "value"))
            ngx.say(l.get(key))
        }
    }
--- request
GET /t
--- response_body
true
value
--- no_error_log
[error]
[warn]
[crit]


=== TEST 2: key size is 511 set() / get()
--- http_config eval: $::HttpConfig
--- main_config eval: $::MainConfig
--- config
    location = /t {
        content_by_lua_block {
            local l = require("resty.lmdb")

            local key = string.rep("a", 511)
            ngx.say(l.set(key, "value"))
            ngx.say(l.get(key))
        }
    }
--- request
GET /t
--- response_body
true
value
--- no_error_log
[error]
[warn]
[crit]


=== TEST 3: key size is 1024 set() / get()
--- http_config eval: $::HttpConfig
--- main_config eval: $::MainConfig
--- config
    location = /t {
        content_by_lua_block {
            local l = require("resty.lmdb")

            local key = string.rep("a", 1024)
            ngx.say(l.set(key, "value"))
            ngx.say(l.get(key))
        }
    }
--- request
GET /t
--- response_body
true
value
--- no_error_log
[error]
[warn]
[crit]


=== TEST 4: key size is 448 set() / get()
--- http_config eval: $::HttpConfig
--- main_config eval: $::MainConfig
--- config
    location = /t {
        content_by_lua_block {
            local l = require("resty.lmdb")

            local key = string.rep("a", 448)
            ngx.say(l.set(key, "value"))
            ngx.say(l.get(key))
        }
    }
--- request
GET /t
--- response_body
true
value
--- no_error_log
[error]
[warn]
[crit]


=== TEST 5: different keys are ok
--- http_config eval: $::HttpConfig
--- main_config eval: $::MainConfig
--- config
    location = /t {
        content_by_lua_block {
            local l = require("resty.lmdb")

            local key1 = string.rep("a", 512)
            ngx.say(l.set(key1, "value1"))
            ngx.say(l.get(key1))

            local key2 = string.rep("b", 512)
            ngx.say(l.set(key2, "value2"))
            ngx.say(l.get(key2))
        }
    }
--- request
GET /t
--- response_body
true
value1
true
value2
--- no_error_log
[error]
[warn]
[crit]


=== TEST 6: key size is 480 set() / get() with keep_prefix
--- http_config eval: $::HttpConfig
--- main_config eval: $::MainConfig
--- config
    location = /t {
        content_by_lua_block {
            local l = require("resty.lmdb")

            local key = string.rep("a", 480)
            ngx.say(l.set(key, "value", true))
            ngx.say(l.get(key, true))
        }
    }
--- request
GET /t
--- response_body
true
value
--- no_error_log
[error]
[warn]
[crit]


=== TEST 7: key size is 479 set() / get() with keep_prefix
--- http_config eval: $::HttpConfig
--- main_config eval: $::MainConfig
--- config
    location = /t {
        content_by_lua_block {
            local l = require("resty.lmdb")

            local key = string.rep("a", 479)
            ngx.say(l.set(key, "value", true))
            ngx.say(l.get(key, true))
        }
    }
--- request
GET /t
--- response_body
true
value
--- no_error_log
[error]
[warn]
[crit]


=== TEST 8: key size is 1024 set() / get() with keep_prefix
--- http_config eval: $::HttpConfig
--- main_config eval: $::MainConfig
--- config
    location = /t {
        content_by_lua_block {
            local l = require("resty.lmdb")

            local key = string.rep("a", 1024)
            ngx.say(l.set(key, "value", true))
            ngx.say(l.get(key, true))
        }
    }
--- request
GET /t
--- response_body
true
value
--- no_error_log
[error]
[warn]
[crit]


=== TEST 9: key size is 416 set() / get() with keep_prefix
--- http_config eval: $::HttpConfig
--- main_config eval: $::MainConfig
--- config
    location = /t {
        content_by_lua_block {
            local l = require("resty.lmdb")

            local key = string.rep("a", 416)
            ngx.say(l.set(key, "value", true))
            ngx.say(l.get(key, true))
        }
    }
--- request
GET /t
--- response_body
true
value
--- no_error_log
[error]
[warn]
[crit]


=== TEST 10: different keys are ok with keep_prefix
--- http_config eval: $::HttpConfig
--- main_config eval: $::MainConfig
--- config
    location = /t {
        content_by_lua_block {
            local l = require("resty.lmdb")

            local key1 = string.rep("a", 480)
            ngx.say(l.set(key1, "value1", true))
            ngx.say(l.get(key1, true))

            local key2 = string.rep("b", 480)
            ngx.say(l.set(key2, "value2", true))
            ngx.say(l.get(key2, true))
        }
    }
--- request
GET /t
--- response_body
true
value1
true
value2
--- no_error_log
[error]
[warn]
[crit]


=== TEST 11: directly get value via normalized key with keep_prefix
--- http_config eval: $::HttpConfig
--- main_config eval: $::MainConfig
--- config
    location = /t {
        content_by_lua_block {
            local l = require("resty.lmdb")
            local resty_sha256 = assert(require("resty.sha256").new())

            local sha256 = function(str)
                resty_sha256:reset()
                resty_sha256:update(str)
                return resty_sha256:final()
            end

            local key = string.rep("a", 480)
            ngx.say(l.set(key, "value", true))
            ngx.say(l.get(key, true))

            local normalized_key = string.sub(key, 1, 479) .. assert(sha256(key))
            ngx.say(l.get(normalized_key))
        }
    }
--- request
GET /t
--- response_body
true
value
value
--- no_error_log
[error]
[warn]
[crit]


=== TEST 12: get values via prefix with keep_prefix
--- http_config eval: $::HttpConfig
--- main_config eval: $::MainConfig
--- config
    location = /t {
        content_by_lua_block {
            local l = require("resty.lmdb")

            local prefix = string.rep("a", 240)

            local key1 = prefix .. string.rep("b", 240)
            ngx.say(l.set(key1, "value1", true))
            ngx.say(l.get(key1, true))

            local key2 = prefix .. string.rep("c", 240)
            ngx.say(l.set(key2, "value2", true))
            ngx.say(l.get(key2, true))

            local key3 = string.rep("d", 480)
            ngx.say(l.set(key3, "value3", true))
            ngx.say(l.get(key3, true))

            local key4 = string.rep("a", 239)
            ngx.say(l.set(key4, "value4", true))
            ngx.say(l.get(key4, true))

            for k, v in l.prefix(prefix) do
                ngx.say(value: ", v)
            end
        }
    }
--- request
GET /t
--- response_body
true
value1
true
value2
true
value3
true
value4
value: value1
value: value2
--- no_error_log
[error]
[warn]
[crit]
