# vim:set ft= ts=4 sw=4 et:

use Test::Nginx::Socket::Lua;
use Cwd qw(cwd);

repeat_each(1);
no_shuffle();
plan tests => repeat_each() * blocks() * 5;

my $pwd = cwd();

our $MainConfig = qq{
    lmdb_environment_path /tmp/test-exit-worker-$$.mdb;
    lmdb_map_size 5m;
    lmdb_max_databases 2;
};

our $HttpConfig = qq{
    lua_package_path "$pwd/lib/?.lua;$pwd/lib/?/init.lua;$pwd/../lua-resty-core/lib/?.lua;$pwd/../lua-resty-core/lib/?/init.lua;$pwd/../lua-resty-lrucache/lib/?.lua;$pwd/../lua-resty-lrucache/lib/?/init.lua;$pwd/../lua-resty-string/lib/?.lua;$pwd/../lua-resty-string/lib/?/init.lua;;";
};

no_long_string();
run_tests();

__DATA__

=== TEST 1: exit_worker can write after module exit hooks begin
--- main_config eval: $::MainConfig
--- http_config eval
$::HttpConfig . q{
    init_worker_by_lua_block {
        local lmdb = require("resty.lmdb")
        assert(lmdb.set("marker", "stale", "exit_worker_lifecycle"))
        assert(lmdb.db_drop(false, "exit_worker_lifecycle"))
    }

    exit_worker_by_lua_block {
        local lmdb = require("resty.lmdb")
        local ok, err = lmdb.set("marker", "persisted",
                                 "exit_worker_lifecycle")
        if not ok then
            ngx.log(ngx.ERR, "failed to persist exit_worker marker: ", err)
        end
    }
}
--- config
    location = /t {
        return 200 "ready\n";
    }
--- request
GET /t
--- response_body
ready
--- no_error_log
[error]
[warn]
[crit]

=== TEST 2: exit_worker value survives worker shutdown
--- main_config eval: $::MainConfig
--- http_config eval: $::HttpConfig
--- config
    location = /t {
        content_by_lua_block {
            local lmdb = require("resty.lmdb")
            local value, err = lmdb.get("marker", "exit_worker_lifecycle")
            ngx.say(value or "missing", ",", err or "ok")
        }
    }
--- request
GET /t
--- response_body
persisted,ok
--- no_error_log
[error]
[warn]
[crit]
