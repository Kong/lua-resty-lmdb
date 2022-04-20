#include <stdio.h>
#include <ngx_lua_resty_lmdb_module.h>
#include <openssl/bio.h>
#include <openssl/evp.h>
#include <openssl/des.h>
#include <stdlib.h>
#include <memory.h>
#include <sys/param.h>
#include "lmdb.h"

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

    { ngx_string("lmdb_encryption_key_file"),
      NGX_MAIN_CONF|NGX_DIRECT_CONF|NGX_CONF_TAKE1,
      ngx_conf_set_str_slot,
      0,
      offsetof(ngx_lua_resty_lmdb_conf_t, key_file),
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
     *     conf->key_file = NULL;
     *     conf->env = NULL;
     */

    lcf->max_databases = NGX_CONF_UNSET_SIZE;
    lcf->map_size = NGX_CONF_UNSET_SIZE;
    // lcf->key_file.data = "12345678900987654321123456789009";
    //lcf->key_file.len =  NGX_CONF_UNSET_SIZE;
    return lcf;
}


static char *
ngx_lua_resty_lmdb_init_conf(ngx_cycle_t *cycle, void *conf)
{
    ngx_lua_resty_lmdb_conf_t *lcf = conf;

    ngx_conf_init_size_value(lcf->max_databases, 1);
    /* same as mdb.c DEFAULT_MAPSIZE */
    ngx_conf_init_size_value(lcf->map_size, 1048576);
    //ngx_conf_init_value(lcf->key_file.data, "12345678900987654321123456789009");

    return NGX_CONF_OK;
}


static ngx_int_t ngx_lua_resty_lmdb_init(ngx_cycle_t *cycle) {
    /* ngx_lua_resty_lmdb_conf_t *lcf;

    lcf = (ngx_lua_resty_lmdb_conf_t *) ngx_get_conf(cycle->conf_ctx,
                                                     ngx_lua_resty_lmdb_module); */

    return NGX_OK;
}

static EVP_CIPHER *cipher;

// static int mcf_str2key(const char *passwd, MDB_val *key)
// {
// 	unsigned int size;
// 	EVP_MD_CTX *mdctx = EVP_MD_CTX_new();
// 	EVP_DigestInit_ex(mdctx, EVP_sha256(), NULL);
// 	EVP_DigestUpdate(mdctx, "Just a Constant", sizeof("Just a Constant"));
// 	EVP_DigestUpdate(mdctx, passwd, strlen(passwd));
// 	EVP_DigestFinal_ex(mdctx, key->mv_data, &size);
// 	EVP_MD_CTX_free(mdctx);
// 	return 0;
// }

static int lmcf_encfunc(const MDB_val *src, MDB_val *dst, const MDB_val *key, int encdec)
{
	unsigned char iv[12];
	int ivl, outl, rc;
	mdb_size_t *ptr;
    EVP_CIPHER_CTX *ctx = EVP_CIPHER_CTX_new();

	ptr = key[1].mv_data;
	ivl = ptr[0] & 0xffffffff;
	memcpy(iv, &ivl, 4);
	memcpy(iv+4, ptr+1, sizeof(mdb_size_t));

	EVP_CipherInit_ex(ctx, cipher, NULL, key[0].mv_data, iv, encdec);
	EVP_CIPHER_CTX_set_padding(ctx, 0);
	if (!encdec) {
		EVP_CIPHER_CTX_ctrl(ctx, EVP_CTRL_AEAD_SET_TAG, key[2].mv_size, key[2].mv_data);
	}
	rc = EVP_CipherUpdate(ctx, dst->mv_data, &outl, src->mv_data, src->mv_size);
	if (rc)
		rc = EVP_CipherFinal_ex(ctx, key[2].mv_data, &outl);
	if (rc && encdec) {
		EVP_CIPHER_CTX_ctrl(ctx, EVP_CTRL_AEAD_GET_TAG, key[2].mv_size, key[2].mv_data);
	}
	return rc == 0;
}

static ngx_int_t ngx_lua_resty_lmdb_init_worker(ngx_cycle_t *cycle)
{
    ngx_lua_resty_lmdb_conf_t *lcf;
    int                        rc;

    lcf = (ngx_lua_resty_lmdb_conf_t *) ngx_get_conf(cycle->conf_ctx,
                                                     ngx_lua_resty_lmdb_module);

    if (lcf == NULL || lcf->env_path == NULL) {
        return NGX_OK;
    }

    rc = mdb_env_create(&lcf->env);
    if (rc != 0) {
        ngx_log_error(NGX_LOG_CRIT, cycle->log, 0,
                      "unable to create LMDB environment");
        return NGX_ERROR;
    }
    
    rc = mdb_env_set_mapsize(lcf->env, lcf->map_size);
    if (rc != 0) {
        ngx_log_error(NGX_LOG_CRIT, cycle->log, 0,
                      "unable to set map size for LMDB");
        return NGX_ERROR;
    }

    rc = mdb_env_set_maxdbs(lcf->env, lcf->max_databases);
    if (rc != 0) {
        ngx_log_error(NGX_LOG_CRIT, cycle->log, 0,
                      "unable to set maximum DB count for LMDB");
        return NGX_ERROR;
    }

    if (lcf->key_file.data) {

        u_char *key = lcf->key_file.data;

        MDB_val enckey;
        enckey.mv_data = &key[0];
        enckey.mv_size = 32;

        cipher = (EVP_CIPHER *)EVP_aes_256_gcm();
        //cipher = (EVP_CIPHER *)EVP_chacha20_poly1305();
        rc = mdb_env_set_encrypt(lcf->env, lmcf_encfunc, &enckey, AES_256_GCM_BLOCK_SIZE);
        if (rc != 0) {
            ngx_log_error(NGX_LOG_CRIT, cycle->log, 0, "unable to set LMDB encryption key: %s", mdb_strerror(rc));
            return NGX_ERROR;
        }
    }


    rc = mdb_env_open(lcf->env, (const char *) lcf->env_path->name.data, 0, 0600);
    if (rc != 0) {
        ngx_log_error(NGX_LOG_CRIT, cycle->log, 0,
                      "unable to open LMDB environment: %s", mdb_strerror(rc));
        return NGX_ERROR;
    }
    //---mdb_txn_begin(MDB_env *env, MDB_txn *parent, unsigned int flags, MDB_txn **ret)
    rc = mdb_txn_begin(lcf->env, NULL, MDB_RDONLY, &lcf->ro_txn);
    if (rc != 0) {
        ngx_log_error(NGX_LOG_CRIT, cycle->log, 0,
                      "unable to open LMDB read only transaction: %s", mdb_strerror(rc));
        return NGX_ERROR;
    }

    mdb_txn_reset(lcf->ro_txn);

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
        mdb_txn_abort(lcf->ro_txn);
        mdb_env_close(lcf->env);
    }
}
