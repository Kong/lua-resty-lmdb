#include <ngx_lua_resty_lmdb_module.h>


/*
 * This function is the FFI call used for range lookups.
 * It is very similar to `ngx_lua_resty_lmdb_ffi_execute` above,
 * except we can only specify one key as the starting point at a time.
 *
 * The `ops[0]` will be the key to lookup, this function returns all keys
 * >= `ops[0].key` and up to `n` will be returned at a time.
 *
 * Returns:
 * * >= 0        - number of keys found. If return < `n`, then it is the last
 *                 key in the map (no more keys afterward the last result)
 * * `NGX_ERROR` - an error occurred, *err will contain the error string
 * * `NGX_AGAIN` - `buf_len` is not enough, try again with larger `buf`
 */
int ngx_lua_resty_lmdb_ffi_range(ngx_lua_resty_lmdb_operation_t *ops,
    size_t n, u_char *buf, size_t buf_len, const char **err)
{
    ngx_lua_resty_lmdb_conf_t      *lcf;
    size_t                          i;
    MDB_txn                        *txn;
    int                             rc;
    MDB_val                         key;
    MDB_val                         value;
    MDB_cursor                     *cur;

    ngx_lua_resty_lmdb_assert(n >= 1);
    ngx_lua_resty_lmdb_assert(ops[0].opcode == NGX_LMDB_OP_PREFIX);

    lcf = (ngx_lua_resty_lmdb_conf_t *) ngx_get_conf(ngx_cycle->conf_ctx,
                                                     ngx_lua_resty_lmdb_module);

    if (lcf == NULL || lcf->env == NULL) {
        *err = "no LMDB environment defined";
        return NGX_ERROR;
    }

    txn = lcf->ro_txn;
    rc = mdb_txn_renew(txn);
    if (rc != 0) {
        *err = mdb_strerror(rc);
        return NGX_ERROR;
    }

    rc = mdb_cursor_open(txn, ops[0].dbi, &cur);
    if (rc != 0) {
        *err = mdb_strerror(rc);
        mdb_txn_reset(txn);

        return NGX_ERROR;
    }

    /* we always have at least one ops slot as asserted above */

    for (i = 0; i < n; i++) {
        key.mv_size = ops[i].key.len;
        key.mv_data = ops[i].key.data;

        rc = mdb_cursor_get(cur, &key, &value, i == 0 ? MDB_SET_RANGE : MDB_NEXT);
        if (rc == 0) {
            /* key found, copy result into buf */
            if (key.mv_size + value.mv_size > buf_len) {
                mdb_cursor_close(cur);
                mdb_txn_reset(txn);

                return NGX_AGAIN;
            }

            ops[i].key.data = buf;
            ops[i].key.len = key.mv_size;
            buf = ngx_cpymem(buf, key.mv_data, key.mv_size);

            ops[i].value.data = buf;
            ops[i].value.len = value.mv_size;
            buf = ngx_cpymem(buf, value.mv_data, value.mv_size);

            ops[i].opcode = NGX_LMDB_OP_PREFIX;

            buf_len -= key.mv_size + value.mv_size;

        } else if (rc == MDB_NOTFOUND) {
            mdb_cursor_close(cur);
            mdb_txn_reset(txn);

            return i;

        } else {
            *err = mdb_strerror(rc);

            mdb_cursor_close(cur);
            mdb_txn_reset(txn);

            return NGX_ERROR;
        }
    }

    mdb_cursor_close(cur);
    mdb_txn_reset(txn);

    return i;
}
