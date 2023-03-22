# vim:set ft= ts=4 sw=4 et:

use Test::Nginx::Socket::Lua;
use Cwd qw(cwd);

repeat_each(2);

plan tests => repeat_each() * blocks() * 5;

my $pwd = cwd();

our $MainConfig = qq{
    lmdb_environment_path html/test.mdb;
    lmdb_max_databases 3;
    lmdb_map_size 5m;
};

our $MainConfig2 = qq{
    lmdb_environment_path html/test.mdb;
    lmdb_max_databases 3;
    lmdb_map_size 10m;
};

our $HttpConfig = qq{
    lua_package_path "$pwd/lib/?.lua;;";
};

no_long_string();
#no_diff();

run_tests();

__DATA__

=== TEST 1: simple get env_info with 5M LMDB Map
--- http_config eval: $::HttpConfig
--- main_config eval: $::MainConfig
--- config
    location = /t {
        content_by_lua_block {
            local l = require("resty.lmdb")
            local info = l.info()
            ngx.say(info["map_size"])
            ngx.say(info["page_size"])
            ngx.say(info["map_size"]/info["page_size"]==info["max_pages"])
        }
    }
--- request
GET /t
--- response_body
5242880
4096
true
--- no_error_log
[error]
[warn]
[crit]


=== TEST 2: simple get env_info with 10M LMDB Map
--- http_config eval: $::HttpConfig
--- main_config eval: $::MainConfig2
--- config
    location = /t {
        content_by_lua_block {
            local l = require("resty.lmdb")
            local info = l.get_env_info()
            ngx.say(info["map_size"])
            ngx.say(info["page_size"])
            ngx.say(info["map_size"]/info["page_size"]==info["max_pages"])
        }
    }
--- request
GET /t
--- response_body
10485760
4096
true
--- no_error_log
[error]
[warn]
[crit]



=== TEST 3: simple get env_info after set() / get()
--- http_config eval: $::HttpConfig
--- main_config eval: $::MainConfig
--- config
    location = /t {
        content_by_lua_block {
            local l = require("resty.lmdb")
            local info = l.get_env_info()

            ngx.say(info["map_size"])
            ngx.say(info["page_size"])
            ngx.say(info["map_size"]/info["page_size"]==info["max_pages"])

            local old_used_pages = info["last_used_page"]
            local old_last_txnid = info["last_txnid"]
            local a = string.rep("Abcdef", 2000)
            ngx.say(l.set("test", a))
            ngx.say(l.get("test_not_exist"))

            local info = l.get_env_info()
            ngx.say(info["map_size"])
            ngx.say(info["page_size"])
            ngx.say(info["map_size"]/info["page_size"]==info["max_pages"])
            
            local used_pages =  info["last_used_page"]
            local last_txnid = info["last_txnid"]
            ngx.say(used_pages > old_used_pages)
            ngx.say(last_txnid > old_last_txnid)
        }
    }
--- request
GET /t
--- response_body
5242880
4096
true
true
nil
5242880
4096
true
true
true
--- no_error_log
[error]
[warn]
[crit]



=== TEST 4: simple get env_info after transaction set() / get()
--- http_config eval: $::HttpConfig
--- main_config eval: $::MainConfig
--- config
    location = /t {
        content_by_lua_block {
            local txn = require("resty.lmdb.transaction")
            local l = require("resty.lmdb")

            l.set("testbalabala", "aaaa")
            ngx.say(l.db_drop(false))

            local info = l.get_env_info()
            ngx.say(info["map_size"])
            ngx.say(info["page_size"])
            ngx.say(info["map_size"]/info["page_size"]==info["max_pages"])
            local old_used_pages = info["last_used_page"]
            local old_last_txnid = info["last_txnid"]

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

            local info = l.get_env_info()
            ngx.say(info["map_size"])
            ngx.say(info["page_size"])
            ngx.say(info["map_size"]/info["page_size"]==info["max_pages"])
            local used_pages =  info["last_used_page"]
            local last_txnid = info["last_txnid"]
            ngx.say(last_txnid > old_last_txnid)
        }
    }
--- request
GET /t
--- response_body
true
5242880
4096
true
true
1: nil
2: true
3: value
4: true
5: nil
nil
5242880
4096
true
true

--- no_error_log
[error]
[warn]
[crit]
