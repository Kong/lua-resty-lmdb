#include <ngx_lua_resty_lmdb_module.h>


int ngx_lua_resty_lmdb_ffi_env_info(ngx_lua_resty_lmdb_ffi_status_t *lst,
                                    const char **err)
{
    ngx_lua_resty_lmdb_conf_t      *lcf;
    MDB_stat                        mst;
    MDB_envinfo                     mei;

    lcf = (ngx_lua_resty_lmdb_conf_t *) ngx_get_conf(ngx_cycle->conf_ctx,
                                                     ngx_lua_resty_lmdb_module);

    if (lcf == NULL || lcf->env == NULL) {
        *err = "no LMDB environment defined";
        return NGX_ERROR;
    }

    if (mdb_env_stat(lcf->env, &mst)) {
        *err = "mdb_env_stat() failed";
        return NGX_ERROR;
    }

    if (mdb_env_info(lcf->env, &mei)) {
        *err = "mdb_env_info() failed";
        return NGX_ERROR;
    }

    lst->map_size           = mei.me_mapsize;
    lst->page_size          = mst.ms_psize;
    lst->max_readers        = mei.me_maxreaders;
    lst->num_readers        = mei.me_numreaders;
    lst->in_use_pages       = mst.ms_branch_pages + mst.ms_leaf_pages
                                + mst.ms_overflow_pages;
    lst->allocated_pages    = mei.me_last_pgno + 1;
    lst->entries            = mst.ms_entries;

    return NGX_OK;
}
