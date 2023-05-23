# vim:set ft= ts=4 sw=4 et:

# run this test after 00-init_mdb.t

use Test::Nginx::Socket::Lua;
use Cwd qw(cwd);

repeat_each(1);

plan tests => repeat_each() * blocks() * 6;

my $pwd = cwd();

# remove db for testing
system("rm -rf /tmp/test10-plain.mdb");

our $MainConfig1 = qq{
    lmdb_environment_path /tmp/test10-plain.mdb;
    lmdb_map_size 5m;
};

our $MainConfig2 = qq{
    lmdb_environment_path /tmp/test10-plain.mdb;
    lmdb_map_size 5m;
    lmdb_validation_tag 3.3;
};

our $MainConfig3 = qq{
    lmdb_environment_path /tmp/test10-plain.mdb;
    lmdb_map_size 5m;
    lmdb_validation_tag 3.4;
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
--- error_log
LMDB validation disabled
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


=== TEST 3: change validation_tag
--- http_config eval: $::HttpConfig
--- main_config eval: $::MainConfig3
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
3.4
nil
true
value
nil
--- error_log
LMDB validation failed
--- no_error_log
[emerg]
[error]
[crit]
