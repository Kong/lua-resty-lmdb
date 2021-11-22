# vim:set ft= ts=4 sw=4 et:

use Test::Nginx::Socket::Lua;
use Cwd qw(cwd);

repeat_each(2);

plan tests => repeat_each() * blocks() * 5;

my $pwd = cwd();

our $MainConfig = qq{
    lmdb_environment /tmp/test.mdb;
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
