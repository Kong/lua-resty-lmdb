# vim:set ft= ts=4 sw=4 et:

use Test::Nginx::Socket::Lua;
use Cwd qw(cwd);

repeat_each(2);

plan tests => repeat_each() * blocks() * 5;

my $pwd = cwd();

our $HttpConfig = qq{
    lua_package_path "$pwd/lib/?.lua;;";
};

no_long_string();
#no_diff();

run_tests();

__DATA__

=== TEST 1: no LMDB environment defined
--- http_config eval: $::HttpConfig
--- config
    location = /t {
        content_by_lua_block {
            local l = require("resty.lmdb")

            ngx.say(l.db_drop(false))
            ngx.say(l.get("test"))
        }
    }
--- request
GET /t
--- response_body
nilunable to open DB for access: no LMDB environment defined
nilunable to open DB for access: no LMDB environment defined
--- no_error_log
[error]
[warn]
[crit]
