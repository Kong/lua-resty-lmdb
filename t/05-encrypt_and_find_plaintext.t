# vim:set ft= ts=4 sw=4 et:

use Test::Nginx::Socket::Lua;
use Cwd qw(cwd);

repeat_each(2);

plan tests => repeat_each() * blocks() * 5;

my $pwd = cwd();

our $MainConfig = qq{
    lmdb_environment_path /tmp/test5.mdb;
    lmdb_map_size 5m;
    lmdb_encryption_key /etc/hostname;
    lmdb_encryption_mode "chacha20-poly1305";
};

our $MainConfig1 = qq{
    lmdb_environment_path /tmp/test6.mdb;
    lmdb_map_size 5m;
};

our $HttpConfig = qq{
    lua_package_path "$pwd/lib/?.lua;;";
};

no_shuffle();
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

            ngx.say(l.set("test", "encrypted"))

            local file1 = io.input("/tmp/test5.mdb/data.mdb")
            local str = io.read("*a")

            local _, q = string.find(str, 'test')
            if q == nil then
                ngx.say("can not find plaintxt key")
            else
                ngx.say("can find plaintxt key")
            end

            local _, p = string.find(str, 'encrypted')
            if p == nil then
                ngx.say("can not find plaintxt value")
            else
                ngx.say("can find plaintxt value")
            end

            io.close(file1);

            ngx.say(l.get("test"))
            ngx.say(l.get("test_not_exist"))
        }
    }
--- request
GET /t
--- response_body
true
can not find plaintxt key
can not find plaintxt value
encrypted
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



=== TEST 8: clear using set()
--- http_config eval: $::HttpConfig
--- main_config eval: $::MainConfig
--- config
    location = /t {
        content_by_lua_block {
            local l = require("resty.lmdb")

            ngx.say(l.set("test", "value"))
            ngx.say(l.set("test", nil))
            ngx.say(l.get("test"))
        }
    }
--- request
GET /t
--- response_body
true
true
nil
--- no_error_log
[error]
[warn]
[crit]



=== TEST 9: db_drop()
--- http_config eval: $::HttpConfig
--- main_config eval: $::MainConfig
--- config
    location = /t {
        content_by_lua_block {
            local l = require("resty.lmdb")

            ngx.say(l.set("test", "value"))
            ngx.say(l.db_drop())
            ngx.say(l.get("test"))
        }
    }
--- request
GET /t
--- response_body
true
true
nil
--- no_error_log
[error]
[warn]
[crit]



=== TEST 10: simple set() / get()
--- http_config eval: $::HttpConfig
--- main_config eval: $::MainConfig1
--- config
    location = /t {
        content_by_lua_block {
            local l = require("resty.lmdb")

            ngx.say(l.set("test", "unenc"))

            local file1 = io.input("/tmp/test6.mdb/data.mdb")
            local str = io.read("*a")

            local _, q = string.find(str, 'test')
            if q == nil then
                ngx.say("can not find plaintxt key")
            else
                ngx.say("can find plaintxt key")
            end

            local _, p = string.find(str, 'unenc')
            if p == nil then
                ngx.say("can not find plaintxt value")
            else
                ngx.say("can find plaintxt value")
            end

            io.close(file1);

            ngx.say(l.get("test"))
            ngx.say(l.get("test_not_exist"))
        }
    }
--- request
GET /t
--- response_body
true
can find plaintxt key
can find plaintxt value
unenc
nil
--- no_error_log
[error]
[warn]
[crit]
