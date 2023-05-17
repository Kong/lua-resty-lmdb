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

=== TEST 1: simple set() / get()
--- http_config eval: $::HttpConfig
--- main_config eval: $::MainConfig
--- config
    location = /t {
        content_by_lua_block {
            local l = require("resty.lmdb")
            ngx.say(l.set("test", "value"))
            ngx.say(l.get("test"))
            ngx.say(l.get("test_not_exist"))
        }
    }
--- request
GET /t
--- response_body
true
value
nil
--- no_error_log
[error]
[warn]
[crit]



=== TEST 2: single key edge cases set() / get() (16359)
--- http_config eval: $::HttpConfig
--- main_config eval: $::MainConfig
--- config
    location = /t {
        content_by_lua_block {
            local l = require("resty.lmdb")
            local v = ("a"):rep(16359)
            l.set("a", v)
            ngx.say(v == l.get("a"))
        }
    }
--- request
GET /t
--- response_body
true
--- no_error_log
[error]
[warn]
[crit]



=== TEST 3: single key edge cases set() / get() (16360)
--- http_config eval: $::HttpConfig
--- main_config eval: $::MainConfig
--- config
    location = /t {
        content_by_lua_block {
            local l = require("resty.lmdb")
            local v = ("a"):rep(16360)
            l.set("a", v)
            ngx.say(v == l.get("a"))
        }
    }
--- request
GET /t
--- response_body
true
--- no_error_log
[error]
[warn]
[crit]



=== TEST 4: single key edge cases set() / get() (16361)
--- http_config eval: $::HttpConfig
--- main_config eval: $::MainConfig
--- config
    location = /t {
        content_by_lua_block {
            local l = require("resty.lmdb")
            local v = ("a"):rep(16361)
            l.set("a", v)
            ngx.say(v == l.get("a"))
        }
    }
--- request
GET /t
--- response_body
true
--- no_error_log
[error]
[warn]
[crit]



=== TEST 5: three keys key edge cases set() / get() (5442)
--- http_config eval: $::HttpConfig
--- main_config eval: $::MainConfig
--- config
    location = /t {
        content_by_lua_block {
            local l = require("resty.lmdb")
            local v = ("a"):rep(5442)
            l.set("a", v)
            l.set("b", v)
            l.set("c", v)
            ngx.say(v == l.get("a"))
            ngx.say(v == l.get("b"))
            ngx.say(v == l.get("c"))
        }
    }
--- request
GET /t
--- response_body
true
true
true
--- no_error_log
[error]
[warn]
[crit]



=== TEST 6: three keys edge cases set() / get() (5443)
--- http_config eval: $::HttpConfig
--- main_config eval: $::MainConfig
--- config
    location = /t {
        content_by_lua_block {
            local l = require("resty.lmdb")
            local v = ("a"):rep(5443)
            l.set("a", v)
            l.set("b", v)
            l.set("c", v)
            ngx.say(v == l.get("a"))
            ngx.say(v == l.get("b"))
            ngx.say(v == l.get("c"))
        }
    }
--- request
GET /t
--- response_body
true
true
true
--- no_error_log
[error]
[warn]
[crit]



=== TEST 7: three keys edge cases set() / get() (5444)
--- http_config eval: $::HttpConfig
--- main_config eval: $::MainConfig
--- config
    location = /t {
        content_by_lua_block {
            local l = require("resty.lmdb")
            local v = ("a"):rep(5444)
            l.set("a", v)
            l.set("b", v)
            l.set("c", v)
            ngx.say(v == l.get("a"))
            ngx.say(v == l.get("b"))
            ngx.say(v == l.get("c"))
        }
    }
--- request
GET /t
--- response_body
true
true
true
--- no_error_log
[error]
[warn]
[crit]
