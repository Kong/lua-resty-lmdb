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
            local info = l.get_env_info()
            ngx.say(info["map_size"])
            ngx.say(info["page_size"])
            ngx.say(info["max_readers"])
            ngx.say(info["num_readers"])
            ngx.say(info["allocated_pages"])
            ngx.say(info["in_use_pages"])
            ngx.say(info["entries"])
        }
    }
--- request
GET /t
--- response_body_like chomp
\d+
\d+
\d+
\d+
\d+
\d+
\d+

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
        }
    }
--- request
GET /t
--- response_body
10485760
4096
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

            local old_in_use_pages = info["in_use_pages"]
            local a = string.rep("Abcdef", 5000)
            ngx.say(l.set("test", a))
            ngx.say(l.get("test_not_exist"))

            local info = l.get_env_info()
            ngx.say(info["map_size"])
            ngx.say(info["page_size"])

            local in_use_pages =  info["in_use_pages"]
            ngx.say(in_use_pages > old_in_use_pages)
        }
    }
--- request
GET /t
--- response_body
5242880
4096
true
nil
5242880
4096
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
            local cjson = require("cjson")

            l.set("testbalabala", "aaaa")
            ngx.say(l.db_drop(false))

            local info = l.get_env_info()
            ngx.say(info["map_size"])
            ngx.say(info["page_size"])
            local old_in_use_pages = info["in_use_pages"]
            local old_allocated_pages = info["allocated_pages"]
            ngx.say("old_entries: ", info["entries"])
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
                        ngx.say("entries: ", info["entries"])
            local in_use_pages =  info["in_use_pages"]
            local allocated_pages = info["allocated_pages"]
            ngx.say(in_use_pages > old_in_use_pages)
            ngx.say(allocated_pages > old_allocated_pages)
        }
    }
--- request
GET /t
--- response_body
true
5242880
4096
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
