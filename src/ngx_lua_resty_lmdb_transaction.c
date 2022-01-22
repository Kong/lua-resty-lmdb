#include <ngx_lua_resty_lmdb_module.h>


int ngx_lua_resty_lmdb_ffi_execute(ngx_lua_resty_lmdb_operation_t *ops,
    size_t n, int need_write, u_char *buf, size_t buf_len, const char **err)
{
    ngx_lua_resty_lmdb_conf_t      *lcf;
    size_t                          i;
    MDB_txn                        *txn;
    int                             rc;
    MDB_val                         key;
    MDB_val                         value;

    lcf = (ngx_lua_resty_lmdb_conf_t *) ngx_get_conf(ngx_cycle->conf_ctx,
                                                     ngx_lua_resty_lmdb_module);

    if (lcf == NULL || lcf->env == NULL) {
        *err = "no LMDB environment defined";
        return NGX_ERROR;
    }

    if (need_write) {
        rc = mdb_txn_begin(lcf->env, NULL, 0, &txn);

    } else {
        txn = lcf->ro_txn;
        rc = mdb_txn_renew(txn);
    }
    if (rc != 0) {
        *err = mdb_strerror(rc);
        return NGX_ERROR;
    }

    for (i = 0; i < n; i++) {
        switch (ops[i].opcode) {
            case NGX_LMDB_OP_GET:
                key.mv_size = ops[i].key.len;
                key.mv_data = ops[i].key.data;

                rc = mdb_get(txn, ops[i].dbi, &key, &value);
                if (rc == 0) {
                    /* key found, copy result into buf */
                    if (value.mv_size > buf_len) {
                        if (need_write) {
                            mdb_txn_abort(txn);

                        } else {
                            mdb_txn_reset(txn);
                        }

                        return NGX_AGAIN;
                    }

                    ops[i].value.data = buf;
                    ops[i].value.len = value.mv_size;

                    buf = ngx_cpymem(buf, value.mv_data, value.mv_size);
                    buf_len -= value.mv_size;

                } else if (rc == MDB_NOTFOUND) {
                    ngx_str_null(&ops[i].value);

                } else {
                    *err = mdb_strerror(rc);
                    goto err;
                }

                break;

            case NGX_LMDB_OP_SET:
                ngx_lua_resty_lmdb_assert(need_write);

                key.mv_size = ops[i].key.len;
                key.mv_data = ops[i].key.data;
                value.mv_size = ops[i].value.len;
                value.mv_data = ops[i].value.data;

                rc = value.mv_data != NULL
                     ? mdb_put(txn, ops[i].dbi, &key, &value, ops[i].flags)
                     : mdb_del(txn, ops[i].dbi, &key, NULL);
                if (rc != 0) {
                    *err = mdb_strerror(rc);
                    goto err;

                }

                break;

            case NGX_LMDB_OP_DB_DROP:
                ngx_lua_resty_lmdb_assert(need_write);

                rc = mdb_drop(txn, ops[i].dbi, ops[i].flags);
                if (rc != 0) {
                    *err = mdb_strerror(rc);
                    goto err;

                }

                break;

            case NGX_LMDB_OP_DB_OPEN:
                /*
                 * write access is always required no matter
                 * the database already exists or not because the dbi has to be
                 * written into the shared map
                 */
                ngx_lua_resty_lmdb_assert(need_write);

                rc = mdb_dbi_open(txn, (const char *) ops[i].key.data,
                                  ops[i].flags, &ops[i].dbi);
                if (rc != 0) {
                    *err = mdb_strerror(rc);
                    goto err;

                }

                break;

            default:
                *err = "unknown opcode";
                goto err;
        }
    }

    if (need_write) {
        rc = mdb_txn_commit(txn);
        if (rc != 0) {
            *err = mdb_strerror(rc);
            goto err;
        }

    } else {
        mdb_txn_reset(txn);
    }

    return NGX_OK;

err:
    if (need_write) {
        mdb_txn_abort(txn);

    } else {
        mdb_txn_reset(txn);
    }

    return NGX_ERROR;
}
