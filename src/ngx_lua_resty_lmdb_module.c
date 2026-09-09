#include <ngx_lua_resty_lmdb_module.h>


#define NGX_LUA_RESTY_LMDB_FILE_MODE        0600
#define NGX_LUA_RESTY_LMDB_DIR_MODE         0700

/* 126 is the default number of max readers in LMDB */
#define NGX_LUA_RESTY_LMDB_DEFAULT_READERS         126
#define NGX_LUA_RESTY_LMDB_MAX_READERS_REDUNDANCY  16

#define NGX_LUA_RESTY_LMDB_VALIDATION_KEY  "validation_tag"

#define NGX_LUA_RESTY_LMDB_MAX_BUF_LEN      512

/*
 * AES-256-GCM and ChaCha20-Poly1305 produce a 16-byte authentication tag.
 */
#define NGX_LUA_RESTY_LMDB_ENC_TAG_LEN      16


#ifndef NGX_LUA_RESTY_LMDB_DIGEST_CONSTANT
#define NGX_LUA_RESTY_LMDB_DIGEST_CONSTANT "konglmdb"
#endif


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


static int ngx_lua_resty_lmdb_digest_key(ngx_str_t *passwd, MDB_val *key);
static int ngx_lua_resty_lmdb_cipher(const MDB_val *src, MDB_val *dst,
                                     const MDB_val *key, int encdec);


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

    { ngx_string("lmdb_encryption_key"),
      NGX_MAIN_CONF|NGX_DIRECT_CONF|NGX_CONF_TAKE1,
      ngx_conf_set_str_slot,
      0,
      offsetof(ngx_lua_resty_lmdb_conf_t, key_file),
      NULL },

    { ngx_string("lmdb_encryption_mode"),
      NGX_MAIN_CONF|NGX_DIRECT_CONF|NGX_CONF_TAKE1,
      ngx_conf_set_str_slot,
      0,
      offsetof(ngx_lua_resty_lmdb_conf_t, encryption_mode),
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
     *     conf->cipher = NULL;
     */

    lcf->max_databases = NGX_CONF_UNSET_SIZE;
    lcf->map_size = NGX_CONF_UNSET_SIZE;

    return lcf;
}


static char *
ngx_lua_resty_lmdb_init_conf(ngx_cycle_t *cycle, void *conf)
{
    ngx_lua_resty_lmdb_conf_t     *lcf = conf;
    ngx_file_t                     file;
    ngx_file_info_t                fi;
    ngx_str_t                      key_data;
    MDB_val                        enc_key;
    size_t                         size;
    ssize_t                        n;
    u_char                        *buf;

    ngx_str_null(&key_data);

    ngx_conf_init_size_value(lcf->max_databases, 1);

    /* same as mdb.c DEFAULT_MAPSIZE */
    ngx_conf_init_size_value(lcf->map_size, 1048576);

    /* The default encryption mode is aes-256-gcm */
    if (lcf->encryption_mode.data == NULL) {
        ngx_str_set(&lcf->encryption_mode, "aes-256-gcm");
    }

    if (ngx_strcasecmp(
            lcf->encryption_mode.data, (u_char*)"aes-256-gcm") != 0 &&
        ngx_strcasecmp(
            lcf->encryption_mode.data, (u_char*)"chacha20-poly1305") != 0 ) {

        ngx_log_error(NGX_LOG_CRIT, cycle->log, 0,
                "invalid \"lmdb_encryption_mode\": \"%V\"",
                &lcf->encryption_mode);

        return NGX_CONF_ERROR;
    }

    if (lcf->key_file.data != NULL) {
        if (ngx_conf_full_name(cycle, &lcf->key_file, 1) != NGX_OK) {
            ngx_log_error(NGX_LOG_CRIT, cycle->log, 0,
                          "search \"%V\" failed", &lcf->key_file);
            return NGX_CONF_ERROR;
        }

        ngx_memzero(&file, sizeof(ngx_file_t));
        file.name = lcf->key_file;
        file.log = cycle->log;

        file.fd = ngx_open_file(file.name.data, NGX_FILE_RDONLY,
                                NGX_FILE_OPEN, 0);

        if (file.fd == NGX_INVALID_FILE) {
            ngx_log_error(NGX_LOG_CRIT, cycle->log, ngx_errno,
                          ngx_open_file_n " \"%V\" failed", &file.name);
            return NGX_CONF_ERROR;
        }

        if (ngx_fd_info(file.fd, &fi) == NGX_FILE_ERROR) {
            ngx_log_error(NGX_LOG_CRIT, cycle->log, ngx_errno,
                          ngx_fd_info_n " \"%V\" failed", &file.name);
            ngx_close_file(file.fd);
            return NGX_CONF_ERROR;
        }

        size = ngx_file_size(&fi);

        if (size > NGX_LUA_RESTY_LMDB_MAX_BUF_LEN) {
            ngx_log_error(NGX_LOG_CRIT, cycle->log, 0,
                          "\"%V\" must be less than %d bytes",
                          &file.name, NGX_LUA_RESTY_LMDB_MAX_BUF_LEN);
            ngx_close_file(file.fd);
            return NGX_CONF_ERROR;
        }

        buf = ngx_pcalloc(cycle->pool, size);
        if (buf == NULL) {
            ngx_log_error(NGX_LOG_CRIT, cycle->log, 0,
                          "allocate key memory failed");
            ngx_close_file(file.fd);
            return NGX_CONF_ERROR;
        }

        n = ngx_read_file(&file, buf, size, 0);

        if (n == NGX_ERROR) {
            ngx_log_error(NGX_LOG_CRIT, cycle->log, ngx_errno,
                          ngx_read_file_n " \"%V\" failed", &file.name);
            ngx_close_file(file.fd);
            return NGX_CONF_ERROR;
        }

        if ((size_t) n != size) {
            ngx_log_error(NGX_LOG_CRIT, cycle->log, 0,
                          ngx_read_file_n " \"%V\" returned only "
                          "%z bytes instead of %uz", &file.name, n, size);
            ngx_close_file(file.fd);
            return NGX_CONF_ERROR;
        }

        key_data.data = buf;
        key_data.len = size;

        ngx_close_file(file.fd);
    }

    if (key_data.data != NULL) {
        lcf->cipher = EVP_get_cipherbyname((char *)lcf->encryption_mode.data);

        if (lcf->cipher == NULL ) {
            ngx_log_error(NGX_LOG_CRIT, cycle->log, 0,
                "init \"lmdb_encryption\": \"%V\" failed",
                &lcf->encryption_mode);

            ngx_explicit_memzero(key_data.data, key_data.len);
            return NGX_CONF_ERROR;
        }

        /*
         * Derive the actual encryption key once, here, and never keep the
         * raw key file contents around: every later mdb_env_set_encrypt()
         * call (master re-opens, and every worker after fork) reuses this
         * cached digest instead of re-reading/re-hashing the passphrase.
         */
        enc_key.mv_data = lcf->enc_key;
        enc_key.mv_size = sizeof(lcf->enc_key);

        if (ngx_lua_resty_lmdb_digest_key(&key_data, &enc_key) != 0) {
            ngx_log_error(NGX_LOG_CRIT, cycle->log, 0,
                          "unable to derive LMDB encryption key");
            ngx_explicit_memzero(key_data.data, key_data.len);
            return NGX_CONF_ERROR;
        }

        ngx_explicit_memzero(key_data.data, key_data.len);
    }

    return NGX_CONF_OK;
}


static ngx_int_t
ngx_lua_resty_lmdb_create_env(ngx_cycle_t *cycle,
                              ngx_lua_resty_lmdb_conf_t *lcf)
{
    int                        rc;
    MDB_val                    enckey;

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

    if (lcf->cipher == NULL) {
        return NGX_OK;
    }

    /* setup lmdb encryption, using the key derived in init_conf() */

    enckey.mv_data = lcf->enc_key;
    enckey.mv_size = sizeof(lcf->enc_key);

    rc = mdb_env_set_encrypt(lcf->env, ngx_lua_resty_lmdb_cipher,
                             &enckey, NGX_LUA_RESTY_LMDB_ENC_TAG_LEN);
    if (rc != 0) {
        ngx_log_error(NGX_LOG_CRIT, cycle->log, 0,
                      "unable to set LMDB encryption key: %s",
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
    size_t                     readers;
    ngx_core_conf_t           *ccf;

    if (ngx_lua_resty_lmdb_create_env(cycle, lcf) != NGX_OK) {
        return NGX_ERROR;
    }

    /* Set max readers depending on the number of worker processes */
    ccf = (ngx_core_conf_t *) ngx_get_conf(cycle->conf_ctx, ngx_core_module);
    readers = (size_t) ccf->worker_processes + NGX_LUA_RESTY_LMDB_MAX_READERS_REDUNDANCY;

    if (readers < NGX_LUA_RESTY_LMDB_DEFAULT_READERS) {
      readers = NGX_LUA_RESTY_LMDB_DEFAULT_READERS;
    }

    rc = mdb_env_set_maxreaders(lcf->env, readers);
    if (rc != 0) {
        ngx_log_error(NGX_LOG_CRIT, cycle->log, 0,
                      "unable to set max readers for LMDB environment: %s",
                      mdb_strerror(rc));
        mdb_env_close(lcf->env);
        lcf->env = NULL;

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

        if (ngx_lua_resty_lmdb_create_env(cycle, lcf) != NGX_OK) {
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

    rc = mdb_dbi_open(txn, NULL, MDB_CREATE, &dbi);
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
        ngx_log_error(NGX_LOG_NOTICE, cycle->log, 0,
                      "LMDB validation tag does not exist, assuming empty database");

        mdb_txn_abort(txn);
        return NGX_DECLINED;

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

    ngx_lua_resty_lmdb_assert(lcf->validation_tag.data);

    rc = mdb_txn_begin(lcf->env, NULL, 0, &txn);
    if (rc != 0) {
        ngx_log_error(NGX_LOG_CRIT, cycle->log, 0,
                      "unable to open LMDB transaction: %s",
                      mdb_strerror(rc));
        return NGX_ERROR;
    }

    ngx_lua_resty_lmdb_assert(txn);

    rc = mdb_dbi_open(txn, NULL, MDB_CREATE, &dbi);
    if (rc != 0) {
        ngx_log_error(NGX_LOG_ERR, cycle->log, 0,
                      "unable to open LMDB database: %s",
                      mdb_strerror(rc));
        goto failed;
    }

    /* set tag value to lmdb db */

    key.mv_size = sizeof(NGX_LUA_RESTY_LMDB_VALIDATION_KEY) - 1;
    key.mv_data = NGX_LUA_RESTY_LMDB_VALIDATION_KEY;

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
                      "unable to commit validation tag into LMDB: %s",
                      mdb_strerror(rc));
        return NGX_ERROR;
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
    ngx_int_t                  rc;
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

    rc = ngx_lua_resty_lmdb_validate(cycle, lcf);

    if (rc != NGX_OK) {
        ngx_lua_resty_lmdb_close_file(cycle, lcf);

        ngx_log_error((rc == NGX_DECLINED ? NGX_LOG_NOTICE : NGX_LOG_WARN),
                      cycle->log, 0,
                      "LMDB validation tag mismatch, wiping the database");

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


static int ngx_lua_resty_lmdb_digest_key(ngx_str_t *passwd, MDB_val *key)
{
    unsigned int    size;
    int             rc;
    EVP_MD_CTX     *mdctx = EVP_MD_CTX_new();

    rc = EVP_DigestInit_ex(mdctx, EVP_sha256(), NULL);
    if (rc) {
        rc = EVP_DigestUpdate(mdctx,
                NGX_LUA_RESTY_LMDB_DIGEST_CONSTANT,
                sizeof(NGX_LUA_RESTY_LMDB_DIGEST_CONSTANT));
    }

    if (rc) {
        rc = EVP_DigestUpdate(mdctx, passwd->data, passwd->len);
    }

    if (rc) {
        rc = EVP_DigestFinal_ex(mdctx, key->mv_data, &size);
    }

    EVP_MD_CTX_free(mdctx);

    return rc == 0;
}


static int
ngx_lua_resty_lmdb_cipher(const MDB_val *src, MDB_val *dst,
                          const MDB_val *key, int encdec)
{
    ngx_lua_resty_lmdb_conf_t  *lcf;

    u_char                      iv[12];
    int                         ivl, outl, rc;
    mdb_size_t                 *ptr;
    EVP_CIPHER_CTX             *ctx = EVP_CIPHER_CTX_new();

    lcf = (ngx_lua_resty_lmdb_conf_t *) ngx_get_conf(ngx_cycle->conf_ctx,
                                                     ngx_lua_resty_lmdb_module);

    ngx_lua_resty_lmdb_assert(lcf->cipher != NULL);

    ptr = key[1].mv_data;
    ivl = ptr[0] & 0xffffffff;
    ngx_memcpy(iv, &ivl, sizeof(int));
    ngx_memcpy(iv + sizeof(int), ptr + 1, sizeof(mdb_size_t));

    rc = EVP_CipherInit_ex(ctx, lcf->cipher, NULL, key[0].mv_data, iv, encdec);
    if (rc) {
        EVP_CIPHER_CTX_set_padding(ctx, 0);
    }

    if (rc && !encdec) {
        rc = EVP_CIPHER_CTX_ctrl(ctx, EVP_CTRL_AEAD_SET_TAG,
                                 key[2].mv_size, key[2].mv_data);
    }

    if (rc) {
        rc = EVP_CipherUpdate(ctx, dst->mv_data, &outl,
                              src->mv_data, src->mv_size);
    }

    if (rc) {
        rc = EVP_CipherFinal_ex(ctx, key[2].mv_data, &outl);
    }

    if (rc && encdec) {
        rc = EVP_CIPHER_CTX_ctrl(ctx, EVP_CTRL_AEAD_GET_TAG,
                                 key[2].mv_size, key[2].mv_data);
    }

    EVP_CIPHER_CTX_free(ctx);

    return rc == 0;
}
