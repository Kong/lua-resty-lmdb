# vim:set ft= ts=4 sw=4 et:

use Test::Nginx::Socket::Lua;
use Cwd qw(cwd);

repeat_each(2);

plan tests => repeat_each() * blocks() * 5;

my $pwd = cwd();

our $MainConfig = qq{
    lmdb_environment_path /tmp/test.mdb;
    lmdb_max_databases 3;
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
            local txn = require("resty.lmdb.transaction")
            local l = require("resty.lmdb")

            ngx.say(l.db_drop(false))

            local t = txn.begin()
            t:get("not_found")
            t:set("test", "value")
            t:get("test")
            t:set("test", nil)
            t:get("test")
            assert(t.n == 5)
            assert(t.write == true)
            ngx.say(t:commit())
            assert(t.ops_capacity == 16)

            for i = 1, 5 do
                ngx.say(i, ": ", t[i].result)
            end

            ngx.say(l.get("test"))
        }
    }
--- request
GET /t
--- response_body
true
true
1: nil
2: true
3: value
4: true
5: nil
nil
--- no_error_log
[error]
[warn]
[crit]



=== TEST 2: transaction isolation
--- http_config eval: $::HttpConfig
--- main_config eval: $::MainConfig
--- config
    location = /t {
        content_by_lua_block {
            local txn = require("resty.lmdb.transaction")
            local l = require("resty.lmdb")

            ngx.say(l.db_drop(false))

            local t = txn.begin()
            t:set("test", "value")

            ngx.say(l.get("test"))
            ngx.say(t:commit())
            ngx.say(l.get("test"))
        }
    }
--- request
GET /t
--- response_body
true
nil
true
value
--- no_error_log
[error]
[warn]
[crit]



=== TEST 3: transaction failure aborts the entire transaction
--- http_config eval: $::HttpConfig
--- main_config eval: $::MainConfig
--- config
    location = /t {
        content_by_lua_block {
            local txn = require("resty.lmdb.transaction")
            local l = require("resty.lmdb")

            ngx.say(l.db_drop(false))

            local t = txn.begin()
            t:set("test", "value")
            t:get("test", "not_exist") -- this will fail

            ngx.say(t:commit())
            ngx.say(l.get("test"))
        }
    }
--- request
GET /t
--- response_body
true
nilunable to open DB for access: MDB_NOTFOUND: No matching key/data pair found
nil
--- no_error_log
[error]
[warn]
[crit]



=== TEST 4: full update with drop_db
--- http_config eval: $::HttpConfig
--- main_config eval: $::MainConfig
--- config
    location = /t {
        content_by_lua_block {
            local txn = require("resty.lmdb.transaction")
            local l = require("resty.lmdb")

            ngx.say(l.set("test", "value"))
            ngx.say(l.set("test1", "value1"))

            local t = txn.begin()
            t:db_drop(false)
            t:set("test", "new")
            t:set("test1", "new1")

            ngx.say(t:commit())

            ngx.say(l.get("test"))
            ngx.say(l.get("test1"))
        }
    }
--- request
GET /t
--- response_body
true
true
true
new
new1
--- no_error_log
[error]
[warn]
[crit]
