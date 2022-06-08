# lua-resty-lmdb

This module allows OpenResty applications to use the LMDB (Lightning Memory-Mapped Database)
inside the Nginx worker process. It has two parts, a core module built into Nginx that
controls the life cycle of the database environment, and a FFI based Lua binding for
interacting with the module to access/change data.

Table of Contents
=================

* [lua-resty-lmdb](#lua-resty-lmdb)
    * [APIs](#apis)
        * [resty.lmdb](#restylmdb)
            * [get](#get)
            * [set](#set)
        * [db\_drop](#db_drop)
        * [resty.lmdb.transaction](#restylmdbtransaction)
            * [reset](#reset)
            * [get](#get)
            * [set](#set)
            * [db\_open](#db_open)
            * [db\_drop](#db_drop)
            * [commit](#commit)
    * [Directives](#Directives)
        * [lmdb_encryption_key](#lmdb_encryption_key)
        * [lmdb_encryption_mode](#lmdb_encryption_mode)
    * [Copyright and license](#copyright-and-license)

## APIs

### resty.lmdb

#### get

**syntax:** *value, err = lmdb.get(key, db?)*

**context:** *any context **except** init_by_lua&#42;*

Gets the value corresponding to `key` from LMDB database `db`. If `db` is omitted,
it defaults to `"_default"`.

If the key does not exist, `nil` will be returned.

In case of error, `nil` and a string describing the error will be returned instead.

[Back to TOC](#table-of-contents)

#### set

**syntax:** *ok, err = lmdb.set(key, value, db?)*

**context:** *any context **except** init_by_lua&#42;*

Sets the value corresponding to `key` to `value` inside LMDB database `db`. If `db` is omitted,
it defaults to `"_default"`.

Setting a key's value to `nil` will remove that key from the corresponding database.

In case of error, `nil` and a string describing the error will be returned instead.

[Back to TOC](#table-of-contents)

### db\_drop

**syntax:** *ok, err = lmdb.db_drop(delele?, db?)*

**context:** *any context **except** init_by_lua&#42;*

Clears the contents of database `db`. If `delete` is `true`, then the database handle is also dropped.

In case of error, `nil` and a string describing the error will be returned instead.

[Back to TOC](#table-of-contents)

### resty.lmdb.transaction

**syntax:** *local txn = transaction.begin(hint?)*

**context:** *any context*

Creates a new LMDB transaction object. This does not actually starts the transaction, but only creates
a Lua table that stores the operations for execution later. If `hint` is provided then the Lua table holding
the operations will be pre-allocated to store `hint` operations.

[Back to TOC](#table-of-contents)

#### reset

**syntax:** *txn:reset()*

**context:** *any context*

Resets a transaction object. Removes all existing transactions and results (if any) from the object but
keeps the table's capacity. After this call the transaction can be reused as if it was a new transaction
returned by `transaction.begin()`.

[Back to TOC](#table-of-contents)

#### get

**syntax:** *txn:get(key, db?)*

**context:** *any context*

Appends a `get` operation in the transactions table. If `db` is omitted,
it defaults to `"_default"`. The output table contains the following
fields:

* `value`: Value for `key`, or `nil` if `key` is not found

[Back to TOC](#table-of-contents)

#### set

**syntax:** *txn:set(key, value, db?)*

**context:** *any context*

Appends a `set` operation in the transactions table. If `db` is omitted,
it defaults to `"_default"`. The output able contains the following
fields:

* `result`: Always `true` for successful transaction commits

[Back to TOC](#table-of-contents)

#### db\_open

**syntax:** *txn:db_open(create, db?)*

**context:** *any context*

Appends a `db_open` operation in the transactions table. If `db` is omitted,
it defaults to `"_default"`. This operation does not return anything
in case of successful transaction commits.

[Back to TOC](#table-of-contents)

#### db\_drop

**syntax:** *txn:db_drop(delete, db?)*

**context:** *any context*

Appends a `db_drop` operation in the transactions table. If `db` is omitted,
it defaults to `"_default"`. This operation does not return anything
in case of successful transaction commits.

[Back to TOC](#table-of-contents)

#### commit

**syntax:** *local res, err = txn:commit()*

**context:** *any context **except** init_by_lua&#42;*

Commits all operations currently inside the transactions table using a single LMDB
transaction. Since LMDB transaction exhibits ACID (atomicity, consistency, isolation, durability)
properties, this method will either commit all operations at once or fail without causing
side effects.

In case of successful transaction commit, `true` will be returned. Output value of each operation
can be accessed like this: `txn[3].value`. Please note that Lua table index starts at `1` and the
order corresponds to the order operations were appended into the transaction. So the first operation's
output will be inside `txn[1]` and second operation's result will be inside `txn[2]`.

In case of any error during the transaction, it will be rolled back and `nil` and
an string describing the reason of the failure will be returned instead. Accessing the output value
from the `txn` table when `commit()` returned an error is undefined.

[Back to TOC](#table-of-contents)

## Directives

### lmdb_encryption_key

**syntax:** *lmdb_encryption_key path/to/keyfile;*

**context:** *main*

Encrypt the lmdb database. Encryption is enabled only when the `lmdb_encryption_key` is set. The
content of keyfile will be used to derive a key to encrypt lmdb.

[Back to TOC](#table-of-contents)

### lmdb_encryption_mode

**syntax:** *lmdb_encryption_mode "aes-256-gcm";*

**context:** *main*

Set the lmdb database encryption mode. The default encryption mode is "aes-256-gcm". The optional encryption
modes are "chacha20-poly1305" and "aes-256-gcm". Note that `lmdb_encryption_mode` needs to be set only when
`lmdb_encryption_key` is set.

[Back to TOC](#table-of-contents)

## Copyright and license

Copyright (c) 2021-2022 Kong, Inc.

Licensed under the Apache License, Version 2.0 <LICENSE or
[https://www.apache.org/licenses/LICENSE-2.0](https://www.apache.org/licenses/LICENSE-2.0)>.
Files in the project may not be copied, modified, or distributed except according to those terms.

[Back to TOC](#table-of-contents)

