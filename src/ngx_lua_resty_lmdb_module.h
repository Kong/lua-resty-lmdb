#ifndef _NGX_LUA_RESTY_LMDB_MODULE_H_INCLUDED_
#define _NGX_LUA_RESTY_LMDB_MODULE_H_INCLUDED_


#include <ngx_config.h>
#include <ngx_core.h>
#include <lmdb.h>
#include <chacha8.h>


struct ngx_lua_resty_lmdb_conf_s {
    ngx_path_t  *env_path;
    ngx_path_t  *key_file;
    size_t       max_databases;
    size_t       map_size;
    MDB_env     *env;
    MDB_txn     *ro_txn;
};


typedef struct ngx_lua_resty_lmdb_conf_s ngx_lua_resty_lmdb_conf_t;


typedef enum {
    NGX_LMDB_OP_GET = 0,
    NGX_LMDB_OP_SET,
    NGX_LMDB_OP_DB_OPEN,
    NGX_LMDB_OP_DB_DROP,
} ngx_lua_resty_lmdb_operation_e;


struct ngx_lua_resty_lmdb_operation_s {
    ngx_lua_resty_lmdb_operation_e  opcode;
    ngx_str_t                       key;   /* GET, SET */
    ngx_str_t                       value; /* GET, SET */
    MDB_dbi                         dbi;   /* ALL OPS */
    unsigned int                    flags; /* SET, DROP */
};


typedef struct ngx_lua_resty_lmdb_operation_s ngx_lua_resty_lmdb_operation_t;


extern ngx_module_t ngx_lua_resty_lmdb_module;


#ifdef NGX_LUA_USE_ASSERT
#include <assert.h>
#   define ngx_lua_resty_lmdb_assert(a)  assert(a)
#else
#   define ngx_lua_resty_lmdb_assert(a)
#endif

/* cheats - internal OpenSSL 1.1 structures */
typedef struct evp_cipher_ctx_st {
    const EVP_CIPHER *cipher;
    ENGINE *engine;             /* functional reference if 'cipher' is
                                 * ENGINE-provided */
    int encrypt;                /* encrypt or decrypt */
    int buf_len;                /* number we have left */
    unsigned char oiv[EVP_MAX_IV_LENGTH]; /* original iv */
    unsigned char iv[EVP_MAX_IV_LENGTH]; /* working iv */
    unsigned char buf[EVP_MAX_BLOCK_LENGTH]; /* saved partial block */
    int num;                    /* used by cfb/ofb/ctr mode */
    /* FIXME: Should this even exist? It appears unused */
    void *app_data;             /* application stuff */
    int key_len;                /* May change for variable length cipher */
    unsigned long flags;        /* Various flags */
    void *cipher_data;          /* per EVP data */
    int final_used;
    int block_mask;
    unsigned char final[EVP_MAX_BLOCK_LENGTH]; /* possible final block */
} EVP_CIPHER_CTX;

#define	CHACHA_KEY_SIZE	32
#define CHACHA_CTR_SIZE	16
#define CHACHA_BLK_SIZE	64
#define POLY1305_BLOCK_SIZE	16

typedef struct {
    union {
        double align;   /* this ensures even sizeof(EVP_CHACHA_KEY)%8==0 */
        unsigned int d[CHACHA_KEY_SIZE / 4];
    } key;
    unsigned int  counter[CHACHA_CTR_SIZE / 4];
    unsigned char buf[CHACHA_BLK_SIZE];
    unsigned int  partial_len;
} EVP_CHACHA_KEY;

typedef struct {
    EVP_CHACHA_KEY key;
    unsigned int nonce[12/4];
    unsigned char tag[POLY1305_BLOCK_SIZE];
    unsigned char tls_aad[POLY1305_BLOCK_SIZE];
    struct { uint64_t aad, text; } len;
    int aad, mac_inited, tag_len, nonce_len;
    size_t tls_payload_length;
} EVP_CHACHA_AEAD_CTX;


#endif /* _NGX_LUA_RESTY_LMDB_MODULE_H_INCLUDED_ */
