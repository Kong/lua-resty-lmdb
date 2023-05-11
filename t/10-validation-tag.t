# vim:set ft= ts=4 sw=4 et:

# run this test after 00-init_mdb.t

use Test::Nginx::Socket::Lua 'no_plan';
use Cwd qw(cwd);

repeat_each(1);

my $pwd = cwd();

our $MainConfig1 = qq{
    lmdb_environment_path /tmp/test10.mdb;
    lmdb_map_size 5m;
};

our $MainConfig2 = qq{
    lmdb_environment_path /tmp/test10.mdb;
    lmdb_map_size 5m;
    lmdb_validation_tag 3.3;
};

our $HttpConfig = qq{
    lua_package_path "$pwd/lib/?.lua;;";
};

no_long_string();
#no_diff();

no_shuffle();
run_tests();

__DATA__

=== TEST 1: no validation_tag
--- http_config eval: $::HttpConfig
--- main_config eval: $::MainConfig1
--- config
    location = /t {
        content_by_lua_block {
            local l = require("resty.lmdb")

            ngx.say(l.set("test", "value"))
            ngx.say(l.get("test"))
            ngx.say(l.get("test_not_exist"))

            ngx.say(l.get("validation_tag"))
        }
    }
--- request
GET /t
--- response_body
true
value
nil
nil
--- no_error_log
[error]
[warn]
[crit]


=== TEST 2: start and set validation_tag
--- http_config eval: $::HttpConfig
--- main_config eval: $::MainConfig2
--- config
    location = /t {
        content_by_lua_block {
            local l = require("resty.lmdb")

            ngx.say(l.get("validation_tag"))
            ngx.say(l.get("test"))

            ngx.say(l.set("test", "value"))
            ngx.say(l.get("test"))
            ngx.say(l.get("test_not_exist"))
        }
    }
--- request
GET /t
--- response_body
3.3
nil
true
value
nil
--- error_log
LMDB has no validation_tag
--- no_error_log
[emerg]
[error]
[crit]
