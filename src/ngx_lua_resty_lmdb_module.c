#include <ngx_lua_resty_lmdb_module.h>


#define NGX_LUA_RESTY_LMDB_FILE_MODE        0600
#define NGX_LUA_RESTY_LMDB_DIR_MODE         0700


#define NGX_LUA_RESTY_LMDB_VALIDATION_KEY  "validation_tag"
#define NGX_LUA_RESTY_LMDB_DEFAULT_DB      "_default"


static ngx_str_t ngx_lua_resty_lmdb_file_names[] = {
    ngx_string("/data.mdb"),
    ngx_string("/lock.mdb"),
    ngx_null_string,
};


static void *ngx_lua_resty_lmdb_create_conf(ngx_cycle_t *cycle);
static char *ngx_lua_resty_lmdb_init_conf(ngx_cycle_t *cycle, void *conf);
static ngx_int_t ngx_lua_resty_lmdb_init(ngx_cycle_t *cycle);
static ngx_int_t ngx_lua_resty_lmdb_init_worker(ngx_cycle_t *cycle);
static void ngx_lua_resty_lmdb_exit_worker(ngx_cycle_t *cycle);


static ngx_command_t  ngx_lua_resty_lmdb_commands[] = {

    { ngx_string("lmdb_environment_path"),
      NGX_MAIN_CONF|NGX_DIRECT_CONF|NGX_CONF_TAKE1,
      ngx_conf_set_path_slot,
      0,
      offsetof(ngx_lua_resty_lmdb_conf_t, env_path),
      NULL },

    { ngx_string("lmdb_max_databases"),
      NGX_MAIN_CONF|NGX_DIRECT_CONF|NGX_CONF_TAKE1,
      ngx_conf_set_size_slot,
      0,
      offsetof(ngx_lua_resty_lmdb_conf_t, max_databases),
      NULL },

    { ngx_string("lmdb_map_size"),
      NGX_MAIN_CONF|NGX_DIRECT_CONF|NGX_CONF_TAKE1,
      ngx_conf_set_size_slot,
      0,
      offsetof(ngx_lua_resty_lmdb_conf_t, map_size),
      NULL },

    { ngx_string("lmdb_validation_tag"),
      NGX_MAIN_CONF|NGX_DIRECT_CONF|NGX_CONF_TAKE1,
      ngx_conf_set_str_slot,
      0,
      offsetof(ngx_lua_resty_lmdb_conf_t, validation_tag),
      NULL },

      ngx_null_command
};


static ngx_core_module_t  ngx_lua_resty_lmdb_module_ctx = {
   ngx_string("lua_resty_lmdb"),
   ngx_lua_resty_lmdb_create_conf,
   ngx_lua_resty_lmdb_init_conf
};


ngx_module_t  ngx_lua_resty_lmdb_module = {
    NGX_MODULE_V1,
    &ngx_lua_resty_lmdb_module_ctx,        /* module context */
    ngx_lua_resty_lmdb_commands,           /* module directives */
    NGX_CORE_MODULE,                       /* module type */
    NULL,                                  /* init master */
    ngx_lua_resty_lmdb_init,               /* init module */
    ngx_lua_resty_lmdb_init_worker,        /* init process */
    NULL,                                  /* init thread */
    NULL,                                  /* exit thread */
    ngx_lua_resty_lmdb_exit_worker,        /* exit process */
    NULL,                                  /* exit master */
    NGX_MODULE_V1_PADDING
};


static void *
ngx_lua_resty_lmdb_create_conf(ngx_cycle_t *cycle)
{
    ngx_lua_resty_lmdb_conf_t  *lcf;

    lcf = ngx_pcalloc(cycle->pool, sizeof(ngx_lua_resty_lmdb_conf_t));
    if (lcf == NULL) {
        return NULL;
    }

    /*
     * set by ngx_pcalloc():
     *
     *     conf->env_path = NULL;
     *     conf->env = NULL;
     */

    lcf->max_databases = NGX_CONF_UNSET_SIZE;
    lcf->map_size = NGX_CONF_UNSET_SIZE;

    return lcf;
}


static char *
ngx_lua_resty_lmdb_init_conf(ngx_cycle_t *cycle, void *conf)
{
    ngx_lua_resty_lmdb_conf_t     *lcf = conf;

    ngx_conf_init_size_value(lcf->max_databases, 1);

    /* same as mdb.c DEFAULT_MAPSIZE */
    ngx_conf_init_size_value(lcf->map_size, 1048576);

    return NGX_CONF_OK;
}


static ngx_int_t
ngx_lua_resty_lmdb_create_env(ngx_cycle_t *cycle,
                              ngx_lua_resty_lmdb_conf_t *lcf,
                              ngx_flag_t is_master)
{
    int                        rc;

    rc = mdb_env_create(&lcf->env);
    if (rc != 0) {
        ngx_log_error(NGX_LOG_CRIT, cycle->log, 0,
                      "unable to create LMDB environment: %s",
                      mdb_strerror(rc));
        return NGX_ERROR;
    }

    rc = mdb_env_set_mapsize(lcf->env, lcf->map_size);
    if (rc != 0) {
        ngx_log_error(NGX_LOG_CRIT, cycle->log, 0,
                      "unable to set map size for LMDB: %s",
                      mdb_strerror(rc));
        goto failed;
    }

    rc = mdb_env_set_maxdbs(lcf->env, lcf->max_databases);
    if (rc != 0) {
        ngx_log_error(NGX_LOG_CRIT, cycle->log, 0,
                      "unable to set maximum DB count for LMDB: %s",
                      mdb_strerror(rc));
        goto failed;
    }

    return NGX_OK;

failed:

    mdb_env_close(lcf->env);
    lcf->env = NULL;

    return NGX_ERROR;
}


static ngx_int_t
ngx_lua_resty_lmdb_remove_files(ngx_cycle_t *cycle, ngx_lua_resty_lmdb_conf_t *lcf)
{
    ngx_file_info_t   fi;

    u_char            name_buf[NGX_MAX_PATH];
    ngx_str_t        *names = ngx_lua_resty_lmdb_file_names;
    ngx_str_t        *name;
    ngx_path_t       *path = lcf->env_path;

    if (ngx_file_info(path->name.data, &fi) == NGX_FILE_ERROR) {
        ngx_log_error(NGX_LOG_CRIT, cycle->log, ngx_errno,
                      ngx_file_info_n " \"%s\" failed", path->name.data);
        return NGX_ERROR;
    }

    if (ngx_is_dir(&fi)) {

        /* try to remove all lmdb files */
        for (name = names; name->len; name++) {
            ngx_snprintf(name_buf, NGX_MAX_PATH,
                         "%V%V%Z", &path->name, name);

            ngx_log_debug1(NGX_LOG_DEBUG_CORE, cycle->log, 0,
                           "lmdb file remove: \"%s\"", name_buf);

            if (ngx_delete_file(name_buf) == NGX_FILE_ERROR) {
                ngx_log_error(NGX_LOG_WARN, cycle->log, ngx_errno,
                              ngx_delete_file_n " \"%s\" failed",
                              name_buf);
            }
        }

        return NGX_OK;
    }

    ngx_lua_resty_lmdb_assert(!ngx_is_dir(&fi));

    /* try to delete the file */
    if (ngx_delete_file(path->name.data) == NGX_FILE_ERROR) {
        ngx_log_error(NGX_LOG_WARN, cycle->log, ngx_errno,
                      ngx_delete_file_n " \"%V\" failed", &path->name);
    }

    /* ensure lmdb directory exists */
    if (ngx_create_dir(
            path->name.data, NGX_LUA_RESTY_LMDB_DIR_MODE) == NGX_FILE_ERROR) {

        ngx_log_error(NGX_LOG_WARN, cycle->log, ngx_errno,
                      ngx_create_dir_n " \"%V\" failed", &path->name);
    }

    return NGX_OK;
}


static ngx_int_t
ngx_lua_resty_lmdb_open_file(ngx_cycle_t *cycle,
                             ngx_lua_resty_lmdb_conf_t *lcf,
                             ngx_flag_t is_master)
{
    int                        rc;
    int                        dead;

    if (ngx_lua_resty_lmdb_create_env(cycle, lcf, is_master) != NGX_OK) {
        return NGX_ERROR;
    }

    rc = mdb_env_open(lcf->env, (const char *) lcf->env_path->name.data,
                      0, NGX_LUA_RESTY_LMDB_FILE_MODE);

    /*
     * may be MDB_VERSION_MISMATCH or MDB_INVALID
     * try to remove the invalid LMDB files and open it again
     */

    if (is_master == 1 &&
        (rc == ENOTDIR || rc == MDB_VERSION_MISMATCH || rc == MDB_INVALID)) {

        mdb_env_close(lcf->env);
        lcf->env = NULL;

        ngx_log_error(NGX_LOG_WARN, cycle->log, 0,
                      "LMDB database is corrupted or incompatible, removing");

        if (ngx_lua_resty_lmdb_remove_files(cycle, lcf) != NGX_OK) {
            return NGX_ERROR;
        }

        if (ngx_lua_resty_lmdb_create_env(cycle, lcf, is_master) != NGX_OK) {
            return NGX_ERROR;
        }

        rc = mdb_env_open(lcf->env, (const char *) lcf->env_path->name.data,
                          0, NGX_LUA_RESTY_LMDB_FILE_MODE);
    }

    if (rc != 0) {
        ngx_log_error(NGX_LOG_CRIT, cycle->log, 0,
                      "unable to open LMDB environment: %s", mdb_strerror(rc));

        mdb_env_close(lcf->env);
        lcf->env = NULL;

        return NGX_ERROR;
    }

    rc = mdb_reader_check(lcf->env, &dead);
    if (rc != 0) {
        ngx_log_error(NGX_LOG_CRIT, cycle->log, 0,
                      "unable to check LMDB reader slots: %s", mdb_strerror(rc));
        /* this is not a fatal error */

    } else if (dead > 0) {
        ngx_log_error(NGX_LOG_WARN, cycle->log, 0,
                      "found and cleared %d stale readers from LMDB", dead);
    }

    rc = mdb_txn_begin(lcf->env, NULL, MDB_RDONLY, &lcf->ro_txn);
    if (rc != 0) {
        ngx_log_error(NGX_LOG_CRIT, cycle->log, 0,
                      "unable to open LMDB read only transaction: %s",
                      mdb_strerror(rc));

        mdb_env_close(lcf->env);
        lcf->env = NULL;

        return NGX_ERROR;
    }

    mdb_txn_reset(lcf->ro_txn);

    return NGX_OK;
}


static ngx_int_t
ngx_lua_resty_lmdb_close_file(ngx_cycle_t *cycle,
                              ngx_lua_resty_lmdb_conf_t *lcf)
{
    mdb_txn_abort(lcf->ro_txn);
    mdb_env_close(lcf->env);

    lcf->ro_txn = NULL;
    lcf->env = NULL;

    return NGX_OK;
}


static ngx_int_t
ngx_lua_resty_lmdb_verify_file_status(ngx_cycle_t *cycle,
                                      ngx_lua_resty_lmdb_conf_t *lcf)
{
    ngx_core_conf_t  *ccf;
    ngx_file_info_t   fi;

    u_char            name_buf[NGX_MAX_PATH];
    ngx_str_t        *names = ngx_lua_resty_lmdb_file_names;
    ngx_str_t        *name;

    ccf = (ngx_core_conf_t *) ngx_get_conf(cycle->conf_ctx, ngx_core_module);

    if (ccf->user == (ngx_uid_t) NGX_CONF_UNSET_UINT) {
        return NGX_OK;
    }

    /* check directory */

    ngx_snprintf(name_buf, NGX_MAX_PATH,
                 "%V%Z", &lcf->env_path->name);

    if (ngx_file_info(name_buf, &fi) == NGX_FILE_ERROR) {
        ngx_log_error(NGX_LOG_CRIT, cycle->log, ngx_errno,
                      ngx_file_info_n " \"%s\" failed", name_buf);
        return NGX_ERROR;
    }

    if (fi.st_uid != ccf->user) {
        if (chown((const char *) name_buf, ccf->user, -1) == -1) {
            ngx_log_error(NGX_LOG_WARN, cycle->log, ngx_errno,
                          "chown(\"%s\", %d) failed, "
                          "LMDB files/directory is not owned by the current Nginx user, "
                          "this may cause permission issues or security risks later",
                          name_buf, ccf->user);
        }
    }

    /* check files */

    for (name = names; name->len; name++) {
        ngx_snprintf(name_buf, NGX_MAX_PATH,
                     "%V%V%Z", &lcf->env_path->name, name);

        if (ngx_file_info(name_buf, &fi) == NGX_FILE_ERROR) {
            ngx_log_error(NGX_LOG_CRIT, cycle->log, ngx_errno,
                          ngx_file_info_n " \"%s\" failed", name_buf);
            return NGX_ERROR;
        }

        if (fi.st_uid != ccf->user) {
            if (chown((const char *) name_buf, ccf->user, -1) == -1) {
                ngx_log_error(NGX_LOG_WARN, cycle->log, ngx_errno,
                              "chown(\"%s\", %d) failed, "
                              "LMDB files/directory is not owned by the current Nginx user, "
                              "this may cause permission issues or security risks later",
                              name_buf, ccf->user);
            }
        }
    }

    return NGX_OK;
}


static ngx_int_t
ngx_lua_resty_lmdb_validate(ngx_cycle_t *cycle,
                            ngx_lua_resty_lmdb_conf_t *lcf)
{
    int                        rc;
    MDB_dbi                    dbi;
    MDB_val                    key;
    MDB_val                    value;
    MDB_txn                   *txn = NULL;

    /* check tag value in lmdb */

    if (lcf->validation_tag.data == NULL) {
        return NGX_OK;
    }

    ngx_log_debug1(NGX_LOG_DEBUG_CORE, cycle->log, 0,
                   "LMDB validation enabled, using validation tag: \"%V\"",
                   &lcf->validation_tag);

    ngx_lua_resty_lmdb_assert(lcf->validation_tag.data);

    rc = mdb_txn_begin(lcf->env, NULL, 0, &txn);
    if (rc != 0) {
        ngx_log_error(NGX_LOG_CRIT, cycle->log, 0,
                      "unable to open LMDB transaction: %s",
                      mdb_strerror(rc));
        return NGX_ERROR;
    }

    ngx_lua_resty_lmdb_assert(txn);

    rc = mdb_dbi_open(txn, NGX_LUA_RESTY_LMDB_DEFAULT_DB, MDB_CREATE, &dbi);
    if (rc != 0) {
        ngx_log_error(NGX_LOG_ERR, cycle->log, 0,
                      "unable to open LMDB database: %s",
                      mdb_strerror(rc));
        goto failed;
    }

    key.mv_size = sizeof(NGX_LUA_RESTY_LMDB_VALIDATION_KEY) - 1;
    key.mv_data = NGX_LUA_RESTY_LMDB_VALIDATION_KEY;

    rc = mdb_get(txn, dbi, &key, &value);
    if (rc == 0) {
        /* key found, compare with validation_tag value */
        if (lcf->validation_tag.len == value.mv_size &&
            ngx_strncmp(lcf->validation_tag.data,
                        value.mv_data, value.mv_size) == 0) {

            mdb_txn_abort(txn);
            return NGX_OK;
        }

        ngx_log_error(NGX_LOG_WARN, cycle->log, 0,
                      "LMDB validation tag \"%*s\" did not match configured tag \"%V\"",
                      value.mv_size, value.mv_data,
                      &lcf->validation_tag);

    } else if (rc == MDB_NOTFOUND) {
        ngx_log_error(NGX_LOG_WARN, cycle->log, 0,
                      "LMDB validation tag does not exist");

    } else {
        ngx_log_error(NGX_LOG_ERR, cycle->log, 0,
                      "unable to get LMDB validation tag: %s",
                      mdb_strerror(rc));
    }

failed:

    mdb_txn_abort(txn);
    return NGX_ERROR;
}


static ngx_int_t
ngx_lua_resty_lmdb_write_tag(ngx_cycle_t *cycle,
                             ngx_lua_resty_lmdb_conf_t *lcf)
{
    int                        rc;
    MDB_dbi                    dbi;
    MDB_val                    key;
    MDB_val                    value;
    MDB_txn                   *txn = NULL;

    ngx_str_t                  validation_key =
                                    ngx_string(NGX_LUA_RESTY_LMDB_VALIDATION_KEY);

    ngx_lua_resty_lmdb_assert(lcf->validation_tag.data);

    rc = mdb_txn_begin(lcf->env, NULL, 0, &txn);
    if (rc != 0) {
        ngx_log_error(NGX_LOG_CRIT, cycle->log, 0,
                      "unable to open LMDB transaction: %s",
                      mdb_strerror(rc));
        return NGX_ERROR;
    }

    ngx_lua_resty_lmdb_assert(txn);

    rc = mdb_dbi_open(txn, NGX_LUA_RESTY_LMDB_DEFAULT_DB, MDB_CREATE, &dbi);
    if (rc != 0) {
        ngx_log_error(NGX_LOG_ERR, cycle->log, 0,
                      "unable to open LMDB database: %s",
                      mdb_strerror(rc));
        goto failed;
    }

    /* set tag value to lmdb db */

    key.mv_size = validation_key.len;
    key.mv_data = validation_key.data;

    value.mv_size = lcf->validation_tag.len;
    value.mv_data = lcf->validation_tag.data;

    rc = mdb_put(txn, dbi, &key, &value, 0);
    if (rc != 0) {
        ngx_log_error(NGX_LOG_ERR, cycle->log, 0,
                      "unable to set LMDB validation tag: %s",
                      mdb_strerror(rc));
        goto failed;
    }

    rc = mdb_txn_commit(txn);
    if (rc != 0) {
        ngx_log_error(NGX_LOG_ERR, cycle->log, 0,
                      "unable to commit LMDB: %s",
                      mdb_strerror(rc));
        goto failed;
    }

    ngx_log_debug1(NGX_LOG_DEBUG_CORE, cycle->log, 0,
                   "set LMDB validation tag: \"%V\"",
                   &lcf->validation_tag);

    return NGX_OK;

failed:

    mdb_txn_abort(txn);
    return NGX_ERROR;
}


static ngx_int_t ngx_lua_resty_lmdb_init(ngx_cycle_t *cycle)
{
    ngx_lua_resty_lmdb_conf_t *lcf;

    lcf = (ngx_lua_resty_lmdb_conf_t *) ngx_get_conf(cycle->conf_ctx,
                                                     ngx_lua_resty_lmdb_module);

    if (lcf == NULL || lcf->env_path == NULL) {
        return NGX_OK;
    }

    /* ensure lmdb file is ok */

    if (ngx_lua_resty_lmdb_open_file(cycle, lcf, 1) != NGX_OK) {
        return NGX_ERROR;
    }

    /* check lmdb validation tag */

    if (ngx_lua_resty_lmdb_validate(cycle, lcf) != NGX_OK) {
        ngx_lua_resty_lmdb_close_file(cycle, lcf);

        ngx_log_error(NGX_LOG_WARN, cycle->log, 0,
                      "LMDB database tag mismatch, wiping the database");

        /* remove lmdb files to clean data */
        if (ngx_lua_resty_lmdb_remove_files(cycle, lcf) != NGX_OK) {
            return NGX_ERROR;
        }

        /* open lmdb file again */
        if (ngx_lua_resty_lmdb_open_file(cycle, lcf, 1) != NGX_OK) {
            return NGX_ERROR;
        }

        /* write tag into lmdb */
        if (ngx_lua_resty_lmdb_write_tag(cycle, lcf) != NGX_OK) {
            ngx_lua_resty_lmdb_close_file(cycle, lcf);
            return NGX_ERROR;
        }
    }

    if (ngx_lua_resty_lmdb_close_file(cycle, lcf) != NGX_OK)  {
        return NGX_ERROR;
    }

    /* change to proper permission */

    if (ngx_lua_resty_lmdb_verify_file_status(cycle, lcf) != NGX_OK) {
        return NGX_ERROR;
    }

    return NGX_OK;
}


static ngx_int_t ngx_lua_resty_lmdb_init_worker(ngx_cycle_t *cycle)
{
    ngx_lua_resty_lmdb_conf_t *lcf;

    lcf = (ngx_lua_resty_lmdb_conf_t *) ngx_get_conf(cycle->conf_ctx,
                                                     ngx_lua_resty_lmdb_module);

    if (lcf == NULL || lcf->env_path == NULL) {
        return NGX_OK;
    }

    if (ngx_lua_resty_lmdb_open_file(cycle, lcf, 0) != NGX_OK) {
        return NGX_ERROR;
    }

    return NGX_OK;
}


static void ngx_lua_resty_lmdb_exit_worker(ngx_cycle_t *cycle)
{
    ngx_lua_resty_lmdb_conf_t *lcf;

    lcf = (ngx_lua_resty_lmdb_conf_t *) ngx_get_conf(cycle->conf_ctx,
                                                     ngx_lua_resty_lmdb_module);

    if (lcf == NULL || lcf->env_path == NULL) {
        return;
    }

    if (lcf->env != NULL) {
        ngx_lua_resty_lmdb_close_file(cycle, lcf);
    }
}


