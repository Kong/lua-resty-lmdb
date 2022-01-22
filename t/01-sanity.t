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

our $HttpConfigWithInit = qq{
    lua_package_path "$pwd/lib/?.lua;;";

    init_by_lua_block {
        local l = require("resty.lmdb")

        local res, err = l.set("test", "value")

        package.loaded.res = res
        package.loaded.err = err
    }
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



=== TEST 2: clear using set()
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



=== TEST 3: db_drop()
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



=== TEST 4: db_drop(delete = true)
--- http_config eval: $::HttpConfig
--- main_config eval: $::MainConfig
--- config
    location = /t {
        content_by_lua_block {
            local l = require("resty.lmdb")

            ngx.say(l.set("test", "value"))
            ngx.say(l.db_drop(true))
            ngx.say(l.db_drop(true))
            ngx.say(l.get("test"))
        }
    }
--- request
GET /t
--- response_body
true
true
nilunable to open DB for access: MDB_NOTFOUND: No matching key/data pair found
nilunable to open DB for access: MDB_NOTFOUND: No matching key/data pair found
--- no_error_log
[error]
[warn]
[crit]



=== TEST 5: works fine when not enabled
--- http_config eval: $::HttpConfig
--- config
    location = /t {
        content_by_lua_block {
            ngx.say("good")
        }
    }
--- request
GET /t
--- response_body
good
--- no_error_log
[error]
[warn]
[crit]



=== TEST 6: does not crash when called in init_by_lua
--- http_config eval: $::HttpConfigWithInit
--- main_config eval: $::MainConfig
--- config
    location /t {
        content_by_lua_block {
            ngx.say(package.loaded.res)
            ngx.say(package.loaded.err)
        }
    }
--- request
GET /t
--- response_body
nil
unable to open DB for access: no LMDB environment defined
--- no_error_log
[error]
[warn]
[crit]
