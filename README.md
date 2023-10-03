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
            * [get_env_info](#get_env_info)
        * [db\_drop](#db_drop)
        * [resty.lmdb.transaction](#restylmdbtransaction)
            * [reset](#reset)
            * [get](#get)
            * [set](#set)
            * [db\_open](#db_open)
            * [db\_drop](#db_drop)
            * [commit](#commit)
    * [Directives](#Directives)
        * [lmdb_environment_path](#lmdb_environment_path)
        * [lmdb_max_databases](#lmdb_max_databases)
        * [lmdb_map_size](#lmdb_map_size)
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

#### get_env_info

**syntax:** *status, err = lmdb.get_env_info()*

**context:** *any context **except** init_by_lua&#42;*

Get the LMDB database runtime information. `status` table struct as below.
```
{
    "page_size": 4096,
    "max_readers":126,
    "num_readers": 1,
    "allocated_pages": 2,
    "in_use_pages": 0,
    "entries": 0,
    "map_size": 10485760 # in bytes
}
```

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

### lmdb_environment_path

**syntax:** *lmdb_environment_path path;*

**context:** *main*

Set the directory in which the LMDB database files reside.

[Back to TOC](#table-of-contents)

### lmdb_max_databases

**syntax:** *lmdb_max_databases number;*

**context:** *main*

Set the maximum number of named databases, the default value is `1`.

[Back to TOC](#table-of-contents)

### lmdb_map_size

**syntax:** *lmdb_map_size number;*

**context:** *main*

Set the size of the memory map, the default value is `1048576`(1MB).

[Back to TOC](#table-of-contents)

## Copyright and license

Copyright (c) 2021-2022 Kong, Inc.

Licensed under the Apache License, Version 2.0 <LICENSE or
[https://www.apache.org/licenses/LICENSE-2.0](https://www.apache.org/licenses/LICENSE-2.0)>.
Files in the project may not be copied, modified, or distributed except according to those terms.

[Back to TOC](#table-of-contents)


## TEST COMMIT

## TEST COMMIT 2 using keyless



