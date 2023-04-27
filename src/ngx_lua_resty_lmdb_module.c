#include <ngx_lua_resty_lmdb_module.h>


#define NGX_LUA_RESTY_LMDB_MAX_BUF_LEN      512
#define NGX_LUA_RESTY_LMDB_ENC_KEY_LEN      32


#define NGX_LUA_RESTY_LMDB_FILE_MODE        0600
#define NGX_LUA_RESTY_LMDB_DIR_MODE         0700


#ifndef NGX_LUA_RESTY_LMDB_DIGEST_CONSTANT
#define NGX_LUA_RESTY_LMDB_DIGEST_CONSTANT "konglmdb"
#endif


static ngx_str_t ngx_lua_resty_lmdb_file_names[] = {
    ngx_string("/"),
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
    size_t                         size;
    ssize_t                        n;
    u_char                        *buf;

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

        lcf->key_data.data = buf;
        lcf->key_data.len = size;

        ngx_close_file(file.fd);
    }

    if (lcf->key_data.data != NULL) {
        lcf->cipher = EVP_get_cipherbyname((char *)lcf->encryption_mode.data);

        if (lcf->cipher == NULL ) {
            ngx_log_error(NGX_LOG_CRIT, cycle->log, 0,
                "init \"lmdb_encryption\": \"%V\" failed",
                &lcf->encryption_mode);

            return NGX_CONF_ERROR;
        }
    }

    return NGX_CONF_OK;
}


static ngx_int_t
ngx_lua_resty_lmdb_create_env(ngx_cycle_t *cycle,
                              ngx_lua_resty_lmdb_conf_t *lcf,
                              ngx_flag_t is_master)
{
    int                        rc;
    MDB_val                    enckey;
    int                        block_size = 0;
    u_char                     keybuf[2048];


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
        return NGX_ERROR;
    }

    rc = mdb_env_set_maxdbs(lcf->env, lcf->max_databases);
    if (rc != 0) {
        ngx_log_error(NGX_LOG_CRIT, cycle->log, 0,
                      "unable to set maximum DB count for LMDB: %s",
                      mdb_strerror(rc));
        return NGX_ERROR;
    }

    if (lcf->cipher != NULL) {
        enckey.mv_data = keybuf;
        enckey.mv_size = NGX_LUA_RESTY_LMDB_ENC_KEY_LEN;

        rc = ngx_lua_resty_lmdb_digest_key(&lcf->key_data, &enckey);
        if (rc != 0) {
            ngx_log_error(NGX_LOG_CRIT, cycle->log, 0,
                "unable to set LMDB encryption key, string to key");
            return NGX_ERROR;
        }

        block_size = EVP_CIPHER_block_size(lcf->cipher);
        if (block_size == 0) {
            ngx_log_error(NGX_LOG_CRIT, cycle->log, 0,
                          "unable to set LMDB encryption key, block size");
            return NGX_ERROR;
        }

        rc = mdb_env_set_encrypt(lcf->env, ngx_lua_resty_lmdb_cipher,
                                 &enckey, block_size);
        if (rc != 0) {
            ngx_log_error(NGX_LOG_CRIT, cycle->log, 0,
                          "unable to set LMDB encryption key: %s",
                          mdb_strerror(rc));
            return NGX_ERROR;
        }

        /* worker will destroy secret data */
        if (is_master == 0) {
            ngx_explicit_memzero(lcf->key_data.data, lcf->key_data.len);
            ngx_explicit_memzero(lcf->encryption_mode.data, lcf->encryption_mode.len);

            ngx_str_null(&lcf->key_data);
            ngx_str_null(&lcf->encryption_mode);
        }
    }

    return NGX_OK;
}


static ngx_int_t
ngx_lua_resty_lmdb_remove_files(ngx_cycle_t *cycle, ngx_path_t *path)
{
    ngx_file_info_t   fi;

    u_char            name_buf[NGX_MAX_PATH];
    ngx_str_t        *names = ngx_lua_resty_lmdb_file_names + 1;
    ngx_str_t        *name;

    ngx_log_error(NGX_LOG_WARN, cycle->log, 0,
                  "LMDB database is corrupted or incompatible, removing");

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

        if (ngx_lua_resty_lmdb_remove_files(cycle, lcf->env_path) != NGX_OK) {
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
                ngx_log_error(NGX_LOG_CRIT, cycle->log, ngx_errno,
                              "chown(\"%s\", %d) failed",
                              name_buf, ccf->user);
                return NGX_ERROR;
            }
        }
    }

    return NGX_OK;
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

    return rc == 0;
}


int ngx_lua_resty_lmdb_ffi_env_info(ngx_lua_resty_lmdb_ffi_status_t *lst, const char **err)
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

    lst->map_size       = mei.me_mapsize;
    lst->page_size      = mst.ms_psize;
    lst->max_map_size   = mei.me_mapsize;
    lst->last_used_page = mei.me_last_pgno + 1;
    lst->last_txnid     = mei.me_last_txnid;
    lst->max_readers    = mei.me_maxreaders;
    lst->num_readers    = mei.me_numreaders;

    return NGX_OK;
}
