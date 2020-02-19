# lua-resty-lmdb

This module allows OpenResty applications to use the LMDB (Lightning Memory-Mapped Database)
inside the Nginx worker process. It has two parts, a core module built into Nginx that
controls the life cycle of the database environment, and a FFI based Lua binding for
interacting with the module to access/change data.
