# vim:set ft= ts=4 sw=4 et:

use Test::Nginx::Socket::Lua;
use Cwd qw(cwd);

repeat_each(2);

plan tests => repeat_each() * blocks() * 5;

my $pwd = cwd();

our $MainConfig = qq{
    lmdb_environment_path /tmp/test.mdb;
    lmdb_map_size 5m;
    lmdb_encryption_key_data "12345678900987654321123456789002";
    lmdb_encryption_type "EVP_chacha20_poly1305";
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
            ngx.say(l.get("test_not_exist"))
        }
    }
--- request
GET /t
--- response_body
nilunable to open DB for access: MDB_CRYPTO_FAIL: Page encryption or decryption failed
nilunable to open DB for access: MDB_CRYPTO_FAIL: Page encryption or decryption failed
nilunable to open DB for access: MDB_CRYPTO_FAIL: Page encryption or decryption failed
--- no_error_log
[error]
[warn]
[crit]


