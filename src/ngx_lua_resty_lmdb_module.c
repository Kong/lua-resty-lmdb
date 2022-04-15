#include <stdio.h>
#include <ngx_lua_resty_lmdb_module.h>
#include <openssl/bio.h>
#include <openssl/evp.h>
#include <string.h>
#include <memory.h>
#include <sys/param.h>


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
      ngx_conf_set_path_slot,
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

// chacha 8 enc
// static int qencfunc(const MDB_val *src, MDB_val *dst, const MDB_val *key, int encdec)
// {
// 	chacha8(src->mv_data, src->mv_size, key[0].mv_data, key[1].mv_data, dst->mv_data);
// 	return 0;
// }

int DesEncode(char *inbuf, char *outbuf, int len)
{
	int ret = 0;

	ret = EVP_EncodeBlock(outbuf, inbuf, len);
	if(ret > 0)
	{
		printf("====base64_encode_outLen====%ld\n", strlen(outbuf));
		printf("%s\n\n", outbuf);
		return 0;
	}
	
	return 1;
}


int DesDecode(char *inbuf, char *outbuf, int *len)
{
	int ret = 0;

	ret = EVP_DecodeBlock(outbuf, inbuf, *len);
	if (ret > 0)
	{
		*len = ret - ret%4;
		printf("====base64_decode_outbuf====%s\n,len==:%d\n", outbuf, ret);
		return 0;
	}
	
	return 1;	
}

int DesEncrypt(const unsigned char *key,
								   char *inbuf, int inlen,
								   char *outbuf, int *outlen)
{		
	EVP_CIPHER_CTX *ctx = NULL;
	ctx = EVP_CIPHER_CTX_new();
	EVP_EncryptInit_ex(ctx, EVP_aes_256_ecb(), NULL, key, NULL);
 
	if (!EVP_EncryptUpdate(ctx, outbuf, outlen, inbuf, inlen))
	{
		EVP_CIPHER_CTX_free(ctx);
		return 1;
	}
	
	int tmplen = 0;
	if (!EVP_EncryptFinal_ex(ctx, outbuf + *outlen, &tmplen))
	{
		EVP_CIPHER_CTX_free(ctx);
		return 1;
	}
 
	*outlen += tmplen;
	EVP_CIPHER_CTX_free(ctx);
	printf("====encrype_outbuf====%s\n,len==:%d\n", outbuf, *outlen);

	return 0;
}

int DesDecrypt(const unsigned char *key,
								   char *inbuf, int *inlen,
								   char *outbuf, int *outlen)
{
	EVP_CIPHER_CTX *ctx = NULL;
	
	ctx = EVP_CIPHER_CTX_new();
	EVP_DecryptInit_ex(ctx, EVP_aes_256_ecb(), NULL, key, NULL);
 
	if (!EVP_DecryptUpdate(ctx, outbuf, outlen, inbuf, *inlen))
	{
		return 1;
	}

	int tmplen = 0;
	
	if (!EVP_DecryptFinal_ex(ctx, outbuf + *outlen, &tmplen))
	{
		EVP_CIPHER_CTX_free(ctx);
		return 1;
	}
	*outlen += tmplen;
	//
	EVP_CIPHER_CTX_free(ctx);
	outbuf[*outlen] = '\0';
	printf("====decode_outbuf====%s\n,len==:%d\n", outbuf, *outlen);
	return 0;
}

int abupEncryAndEncode(const unsigned char *key, char *inbuf, int inlen,
								   char *outbuf, int *outlen)
{
	int ret = 0;
	int out1len = 0;

	char *out1buf = malloc(10024);		
	if(out1buf)
	{
		memset(out1buf, 0x00, 10024);
	}
	ret = DesEncrypt(key, inbuf, inlen, out1buf, &out1len);
	if(ret)
	{
		return 1;
	}
	ret = DesEncode(out1buf, outbuf, out1len);
    free(out1buf);
    out1buf = NULL;
	if(ret)
	{
		return 1;
	}
	*outlen = strlen(outbuf);
	printf("==encrypt_encode_ok==\n");
	return 0;
}

int abupDecryAndDecode(const unsigned char *key,
								   char *inbuf, int *inlen,
								   char *outbuf, int *outlen)
{
	int ret = 0;
	
	char *out1buf = malloc(10024);
	if(out1buf)
	{
		memset(out1buf, 0x00, 10024);
	}
	
	ret = DesDecode(inbuf, out1buf, inlen);//
	if(ret)
	{
		return 1;
	}
	printf("==decode_ok==%d\n",*inlen);
	
	ret = DesDecrypt(key, out1buf, inlen, outbuf, outlen);
    free(out1buf);
    out1buf = NULL;
	if(ret)
	{
		return 1;
	}
	return 0;
}

//--me_encfunc(&in, &out, enckeys, 0)
int encfunc(const MDB_val *src, MDB_val *dst, const MDB_val *key, int encdec)
{
	char *key_t = key->mv_data;         
	char *inbuf = src->mv_data;

	printf("============================================================================\n");		
	printf("-------key_t-----%s         ",key_t);
    printf("          -----------------\n");

    printf("============================================================================\n");		
	printf("-------src->mv_data-----%s         ",src->mv_data);
    printf("        -----------------\n");

//-----------chacha8(src->mv_data,      src->mv_size,   key[0].mv_data,         key[1].mv_data,     dst->mv_data);
//-----------chacha8(const void* data,  size_t length,  const uint8_t* key,     const uint8_t* iv,  char* cipher) 


	char *outbuf = malloc(10024);
	if(outbuf)
	{
		memset(outbuf, 0x00, 10024);
	}
	
	char *tmpbuf = malloc(10024);
	if(tmpbuf)
	{
		memset(tmpbuf, 0x00, 10024);
	}
	int outLen = 0;int tmpLen = 0; 	

	printf("============================================================================\n");		
	printf("-------inbuf-----%s         ",inbuf);
    printf("        -----------------\n");	
	if(encdec == 1)
    {
        abupEncryAndEncode(key_t,inbuf, strlen(inbuf), outbuf, &outLen);
        dst->mv_data = outbuf;
        return 0;
    }
    else if(encdec == 0)
    {
        abupDecryAndDecode(key_t,outbuf,&outLen,tmpbuf,&tmpLen);
        
        // printf("====inbuf=%s,inLen=%ld\n",inbuf,strlen(inbuf));


        // DesEncode(inbuf, outbuf, strlen(inbuf));

        // DesEncrypt(key_t, inbuf, strlen(inbuf), outbuf, &outLen);

        // DesDecrypt(key_t, outbuf, &outLen, tmpbuf, &tmpLen);

        // DesDecode(outbuf, tmpbuf, &tmpLen);

        dst->mv_data = tmpbuf;
        printf("============================================================================\n");
        //printf("--------------%s------------\n",data,"\n------------------------");
        printf("----dst->data-------------%s         ",dst->mv_data);
        printf("                 -----------------\n");	
        printf("----src->data-------------%s         ",src->mv_data);
        printf("                 -----------------\n");

        // free(outbuf);
        // outbuf = NULL;
        // free(tmpbuf);
        // tmpbuf = NULL;

        return 0;
    }

    else
    {
        printf("no encdec");
        return 1;
    }
}

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

    return lcf;
}


static char *
ngx_lua_resty_lmdb_init_conf(ngx_cycle_t *cycle, void *conf)
{
    ngx_lua_resty_lmdb_conf_t *lcf = conf;

    ngx_conf_init_size_value(lcf->max_databases, 1);
    /* same as mdb.c DEFAULT_MAPSIZE */
    ngx_conf_init_size_value(lcf->map_size, 1048576);

    return NGX_CONF_OK;
}


static ngx_int_t ngx_lua_resty_lmdb_init(ngx_cycle_t *cycle) {
    /* ngx_lua_resty_lmdb_conf_t *lcf;

    lcf = (ngx_lua_resty_lmdb_conf_t *) ngx_get_conf(cycle->conf_ctx,
                                                     ngx_lua_resty_lmdb_module); */

    return NGX_OK;
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

// test use
    MDB_val enckey;
    char ekey[] = {1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16,
		17, 18, 19, 20, 21, 22, 23, 24, 25, 26, 27, 28, 29, 30, 31, 32};
    enckey.mv_data = ekey;
	enckey.mv_size = sizeof(ekey);
    rc = mdb_env_set_encrypt(lcf->env, encfunc, &enckey, 0);
    if (rc != 0) {
        ngx_log_error(NGX_LOG_CRIT, cycle->log, 0, "unable to set LMDB encryption key: %s", mdb_strerror(rc));
        return NGX_ERROR;
    }

    
    // if (rc != 0) {
    //     ngx_log_error(NGX_LOG_CRIT, cycle->log, 0,
    //                "unable to set LMDB encryption key: %s", mdb_strerror(rc));
    //     return NGX_ERROR;
    // }

    // if (lcf->key_file != NULL) {
    //     FILE *fp;

    //     fp = fopen((const char *) lcf->key_file->name.data, "r");
    //     if (fp == NULL) {
    //         ngx_log_error(NGX_LOG_CRIT, cycle->log, 0,
    //                       "unable to read LMDB encryption key from: %s",
    //                       (const char *) lcf->key_file->name.data);
    //         return NGX_ERROR;
    //     }

    //     char   key[32];
    //     fread(key, sizeof(char), 32, fp);

    //     fclose(fp);

    //     MDB_val enckey;
    //     enckey.mv_data = &key[0];
    //     enckey.mv_size = 32;

	// /** @brief Set encryption on an environment.
	//  *
	//  * This must be called before #mdb_env_open().
	//  * It implicitly sets #MDB_REMAP_CHUNKS on the env.
	//  * @param[in] env An environment handle returned by #mdb_env_create().
	//  * @param[in] func An #MDB_enc_func function.
	//  * @param[in] key The encryption key.
	//  * @param[in] size The size of authentication data in bytes, if any.
	//  * Set this to zero for unauthenticated encryption mechanisms.
	//  * @return A non-zero error value on failure and 0 on success.
	//  */

    //     rc = mdb_env_set_encrypt(lcf->env, ngx_lua_resty_lmdb_enc_func, &enckey, 0);
    //     if (rc != 0) {
    //         ngx_log_error(NGX_LOG_CRIT, cycle->log, 0,
    //                       "unable to set LMDB encryption key: %s", mdb_strerror(rc));
    //         return NGX_ERROR;
    //     }
    // }


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
