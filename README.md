# lua-resty-lmdb

This module allows OpenResty applications to use the LMDB (Lightning Memory-Mapped Database)
inside the Nginx worker process. It has two parts, a core module built into Nginx that
controls the life cycle of the database environment, and a FFI based Lua binding for
interacting with the module to access/change data.

## Copyright and license

Copyright (c) 2021 Kong, Inc.

Licensed under the Apache License, Version 2.0 <LICENSE or
[https://www.apache.org/licenses/LICENSE-2.0](https://www.apache.org/licenses/LICENSE-2.0)>.
Files in the project may not be copied, modified, or distributed except according to those terms.
