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
