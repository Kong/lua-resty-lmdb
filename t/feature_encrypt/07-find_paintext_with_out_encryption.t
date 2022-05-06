# vim:set ft= ts=4 sw=4 et:

use Test::Nginx::Socket::Lua;
use Cwd qw(cwd);

repeat_each(2);

plan tests => repeat_each() * blocks() * 5;

my $pwd = cwd();

our $MainConfig = qq{
    lmdb_environment_path /tmp/test8.mdb;
    lmdb_map_size 5m;
};

our $HttpConfig = qq{
    lua_package_path "$pwd/lib/?.lua;;";
};


no_long_string();
#no_diff();
no_shuffle();

run_tests();

__DATA__

=== TEST 1: simple set() / get()
--- http_config eval: $::HttpConfig
--- main_config eval: $::MainConfig
--- config
    location = /t {
        content_by_lua_block {
            local l = require("resty.lmdb")

            ngx.say(l.set("test", "unenc"))
            local file1 = io.input("/tmp/test8.mdb/data.mdb")  
            local str = io.read("*a")
            local _,q,p
            _, q = string.find(str, 'test')
            if q == nil then ngx.say("can not find plaintxt key") else ngx.say("can find plaintxt key") end
            _, p = string.find(str, 'unenc')
            if p == nil then ngx.say("can not find plaintxt value") else ngx.say("can find plaintxt value") end
            local ret = io.close(file1);
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
