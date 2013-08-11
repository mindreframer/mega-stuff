/*
 *  megatools - Mega.co.nz client library and tools
 *  Copyright (C) 2013  Ondřej Jirman <megous@megous.com>
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License along
 *  with this program; if not, write to the Free Software Foundation, Inc.,
 *  51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
 */

/**
 * MegaAesKey:
 *
 * 128 bit (16 byte) AES key is used to perform various kinds of encryption and
 * decryption within Mega. MegaAesKey represents this key and simplifies it's 
 * use within the application.
 *
 * The binary key's data needs to be loaded into MegaAesKey object before any of
 * the encryption/decryption methods can be called. 
 *
 * There are several ways to load a key, modelled after how it's stored on Mega
 * or within the application:
 * 
 * - From it's binary form (16 byte buffer)
 * - From Mega's modified URL friendly UBase64 formatted string (22 byte string)
 * - Decrypted by other AES key from binary form or UBase64 formatted string
 * - Derived from a plain text password
 * - Generated randomly
 *
 * Operations that can be performed with a key:
 *
 * (Unless otherwise stated all operations work on a data which are sized to the
 * multiples of 16 bytes.)
 * 
 * - Modeless encryption/decryption of a binary buffer using #mega_aes_key_encrypt_raw
 *   or #mega_aes_key_decrypt_raw
 *
 * - Modeless encryption/decryption to/from UBase64 encoded data using
 *   #mega_aes_key_encrypt or #mega_aes_key_decrypt
 *
 * - CBC mode encryption/decryption with zero IV to/from UBase64 encoded data
 *   using #mega_aes_key_encrypt_cbc or #mega_aes_key_decrypt_cbc.
 *
 * - Zero terminated string CBC mode encryption to UBase64 encoded data. The
 *   input string is automatically zero padded to multiples of 16 bytes.
 *
 * - CTR mode encryption/decryption initialized with 8 byte nonce and 64bit
 *   block position using #mega_aes_key_encrypt_ctr
 *   (also used for decryption, because it's a symetric operation).
 *
 * - Transform username to a UBase64 encoded hash used for session
 *   setup/authentication in Mega.
 */

#include "mega-aes-key.h"
#include "utils.h"

#include <string.h>
#include <nettle/aes.h>
#include <nettle/cbc.h>
#include <nettle/ctr.h>

struct _MegaAesKeyPrivate
{
  gboolean loaded;

  guchar key[16];

  struct aes_ctx enc_key;
  struct aes_ctx dec_key;
};

/**
 * mega_aes_key_new:
 *
 * Create new #MegaAesKey object.
 *
 * Returns: #MegaAesKey object.
 */
MegaAesKey* mega_aes_key_new(void)
{
  return g_object_new(MEGA_TYPE_AES_KEY, NULL);
}

/**
 * mega_aes_key_new_generated:
 *
 * Create new #MegaAesKey object with random key.
 *
 * Returns: #MegaAesKey object.
 */
MegaAesKey* mega_aes_key_new_generated(void)
{
  MegaAesKey* key = mega_aes_key_new();

  mega_aes_key_generate(key);

  return key;
}

/**
 * mega_aes_key_new_from_password:
 * @password: Password
 *
 * Create new #MegaAesKey object from password.
 *
 * Returns: #MegaAesKey object.
 */
MegaAesKey* mega_aes_key_new_from_password(const gchar* password)
{
  MegaAesKey* key = mega_aes_key_new();

  mega_aes_key_generate_from_password(key, password);

  return key;
}

/**
 * mega_aes_key_new_from_binary:
 * @data: (element-type guint8) (array fixed-size=16): 16 byte AES key buffer
 *
 * Create new #MegaAesKey object from binary data.
 *
 * Returns: #MegaAesKey object.
 */
MegaAesKey* mega_aes_key_new_from_binary(const guchar* data)
{
  MegaAesKey* key = mega_aes_key_new();

  mega_aes_key_load_binary(key, data);

  return key;
}

/**
 * mega_aes_key_new_from_ubase64:
 * @data: UBase64 encoded 16 byte AES key data
 *
 * Create new #MegaAesKey object preloaded with #data.
 *
 * Returns: #MegaAesKey object.
 */
MegaAesKey* mega_aes_key_new_from_ubase64(const gchar* data)
{
  MegaAesKey* key = mega_aes_key_new();

  mega_aes_key_load_ubase64(key, data);

  return key;
}

/**
 * mega_aes_key_new_from_enc_binary:
 * @data: (element-type guint8) (array fixed-size=16): 16 byte AES key buffer
 * @dec_key: Decryption key
 *
 * Create new #MegaAesKey object by decrypting 16 byte data with #dec_key.
 *
 * Returns: #MegaAesKey object.
 */
MegaAesKey* mega_aes_key_new_from_enc_binary(const guchar* data, MegaAesKey* dec_key)
{
  MegaAesKey* key = mega_aes_key_new();

  mega_aes_key_load_enc_binary(key, data, dec_key);

  return key;
}

/**
 * mega_aes_key_new_from_enc_ubase64:
 * @data: UBase64 encoded 16 byte AES key data
 * @dec_key: Decryption key
 *
 * Create new #MegaAesKey object by decrypting UBase64 encoded #data with
 * #dec_key.
 *
 * Returns: #MegaAesKey object.
 */
MegaAesKey* mega_aes_key_new_from_enc_ubase64(const gchar* data, MegaAesKey* dec_key)
{
  MegaAesKey* key = mega_aes_key_new();

  mega_aes_key_load_enc_ubase64(key, data, dec_key);

  return key;
}

// loaders

/**
 * mega_aes_key_load_binary:
 * @aes_key: a #MegaAesKey
 * @data: (element-type guint8) (array fixed-size=16): 16 byte AES key buffer
 *
 * Initialize key from #data.
 */
void mega_aes_key_load_binary(MegaAesKey* aes_key, const guchar* data)
{
  g_return_if_fail(MEGA_IS_AES_KEY(aes_key));
  g_return_if_fail(data != NULL);

  memcpy(aes_key->priv->key, data, 16);

  aes_set_encrypt_key(&aes_key->priv->enc_key, 16, data);
  aes_set_decrypt_key(&aes_key->priv->dec_key, 16, data);

  aes_key->priv->loaded = TRUE;
}

/**
 * mega_aes_key_load_ubase64:
 * @aes_key: a #MegaAesKey
 * @data: UBase64 encoded 16 byte AES key data
 *
 * Initialize key from #data.
 *
 * Returns: TRUE on success.
 */
gboolean mega_aes_key_load_ubase64(MegaAesKey* aes_key, const gchar* data)
{
  gsize len;
  guchar* key;

  g_return_val_if_fail(MEGA_IS_AES_KEY(aes_key), FALSE);
  g_return_val_if_fail(data != NULL, FALSE);

  key = mega_base64urldecode(data, &len);
  if (key == NULL || len != 16)
  {
    g_free(key);
    return FALSE;
  }

  mega_aes_key_load_binary(aes_key, key);

  g_free(key);
  return TRUE;
}

/**
 * mega_aes_key_load_enc_binary:
 * @aes_key: a #MegaAesKey
 * @data: (element-type guint8) (array fixed-size=16): 16 byte AES key buffer
 * @dec_key: Decryption key
 *
 * Initialize key from #data.
 */
void mega_aes_key_load_enc_binary(MegaAesKey* aes_key, const guchar* data, MegaAesKey* dec_key)
{
  guchar plain_key[16];

  g_return_if_fail(MEGA_IS_AES_KEY(aes_key));
  g_return_if_fail(data != NULL);
  g_return_if_fail(MEGA_IS_AES_KEY(dec_key));

  mega_aes_key_decrypt_raw(dec_key, data, plain_key, 16);

  mega_aes_key_load_binary(aes_key, plain_key);
}

/**
 * mega_aes_key_load_enc_ubase64:
 * @aes_key: a #MegaAesKey
 * @data: UBase64 encoded 16 byte AES key data
 * @dec_key: Decryption key
 *
 * Initialize key from #data.
 *
 * Returns: TRUE on success.
 */
gboolean mega_aes_key_load_enc_ubase64(MegaAesKey* aes_key, const gchar* data, MegaAesKey* dec_key)
{
  gsize len;
  guchar* key;

  g_return_val_if_fail(MEGA_IS_AES_KEY(aes_key), FALSE);
  g_return_val_if_fail(data != NULL, FALSE);
  g_return_val_if_fail(MEGA_IS_AES_KEY(dec_key), FALSE);

  key = mega_base64urldecode(data, &len);
  if (key == NULL || len != 16)
  {
    g_free(key);
    return FALSE;
  }

  mega_aes_key_load_enc_binary(aes_key, key, dec_key);

  g_free(key);
  return TRUE;
}

/**
 * mega_aes_key_encrypt_raw:
 * @aes_key: a #MegaAesKey
 * @plain: (in) (element-type guint8) (array length=len): Plaintext input data
 * @cipher: (out caller-allocates) (element-type guint8) (array length=len): Ciphertext
 * @len: (in): 16 byte aligned length of plaintext data.
 *
 * Encrypt plaintext blocks using AES key
 */
void mega_aes_key_encrypt_raw(MegaAesKey* aes_key, const guchar* plain, guchar* cipher, gsize len)
{
  gsize off;

  g_return_if_fail(MEGA_IS_AES_KEY(aes_key));
  g_return_if_fail(plain != NULL);
  g_return_if_fail(cipher != NULL);
  g_return_if_fail(len % AES_BLOCK_SIZE == 0);

  aes_encrypt(&aes_key->priv->enc_key, len, cipher, plain);
}

/**
 * mega_aes_key_decrypt_raw:
 * @aes_key: a #MegaAesKey
 * @cipher: (element-type guint8) (array length=len): Ciphertext
 * @plain: (element-type guint8) (array length=len) (out caller-allocates): Plaintext output data
 * @len: 16 byte aligned length of ciphertext and plaintext data.
 *
 * Decrypt ciphertext blocks using AES key
 */
void mega_aes_key_decrypt_raw(MegaAesKey* aes_key, const guchar* cipher, guchar* plain, gsize len)
{
  gsize off;

  g_return_if_fail(MEGA_IS_AES_KEY(aes_key));
  g_return_if_fail(cipher != NULL);
  g_return_if_fail(plain != NULL);
  g_return_if_fail(len % AES_BLOCK_SIZE == 0);

  aes_decrypt(&aes_key->priv->dec_key, len, plain, cipher);
}

/**
 * mega_aes_key_generate:
 * @aes_key: a #MegaAesKey
 *
 * Initialize key with random data.
 */
void mega_aes_key_generate(MegaAesKey* aes_key)
{
  guchar rand_key[AES_BLOCK_SIZE];

  g_return_if_fail(MEGA_IS_AES_KEY(aes_key));

  mega_randomness(rand_key, sizeof(rand_key));

  mega_aes_key_load_binary(aes_key, rand_key);
}

/**
 * mega_aes_key_generate_from_password:
 * @aes_key: a #MegaAesKey
 * @password: 
 *
 * Initialize key from plaintext password string. (Mega.co.nz algorithm)
 */
void mega_aes_key_generate_from_password(MegaAesKey* aes_key, const gchar* password)
{
  g_return_if_fail(MEGA_IS_AES_KEY(aes_key));
  g_return_if_fail(password != NULL);

  guchar pkey[AES_BLOCK_SIZE] = {0x93, 0xC4, 0x67, 0xE3, 0x7D, 0xB0, 0xC7, 0xA4, 0xD1, 0xBE, 0x3F, 0x81, 0x01, 0x52, 0xCB, 0x56};
  gint off, r;
  gint len;

  len = strlen(password);

  for (r = 65536; r--; )
  {
    for (off = 0; off < len; off += AES_BLOCK_SIZE)
    {
      struct aes_ctx k;
      guchar key[AES_BLOCK_SIZE] = {0};
      strncpy(key, password + off, AES_BLOCK_SIZE);

      aes_set_encrypt_key(&k, AES_BLOCK_SIZE, key);
      aes_encrypt(&k, AES_BLOCK_SIZE, pkey, pkey);  
    }
  }

  mega_aes_key_load_binary(aes_key, pkey);
}

/**
 * mega_aes_key_make_username_hash:
 * @aes_key: a #MegaAesKey
 * @username: E-mail
 *
 * Generate username hash (uh paraemter for 'us' API call) used for authentication to Mega.co.nz.
 *
 * Returns: Username hash string
 */
gchar* mega_aes_key_make_username_hash(MegaAesKey* aes_key, const gchar* username)
{
  gchar* username_lower;

  g_return_val_if_fail(MEGA_IS_AES_KEY(aes_key), NULL);
  g_return_val_if_fail(username != NULL, NULL);

  username_lower = g_ascii_strdown(username, -1);

  gint l, i;
  guchar hash[AES_BLOCK_SIZE] = {0}, oh[8];

  for (i = 0, l = strlen(username_lower); i < l; i++) 
    hash[i % AES_BLOCK_SIZE] ^= username_lower[i];

  for (i = 16384; i--; ) 
  {
    aes_encrypt(&aes_key->priv->enc_key, AES_BLOCK_SIZE, hash, hash);  
  }

  memcpy(oh, hash, 4);
  memcpy(oh + 4, hash + 8, 4);

  g_free(username_lower);

  return mega_base64urlencode(oh, 8);
}

/**
 * mega_aes_key_encrypt:
 * @aes_key: a #MegaAesKey
 * @plain: (in) (element-type guint8) (array length=len): Plaintext input data
 * @len: (in): 16 byte aligned length of plaintext data.
 *
 * Encrypt binary data into ubase64 encoded string.
 *
 * Returns: UBase64 encoded ciphertext.
 */
gchar* mega_aes_key_encrypt(MegaAesKey* aes_key, const guchar* plain, gsize len)
{
  guchar* cipher;
  gchar* str;

  g_return_val_if_fail(MEGA_IS_AES_KEY(aes_key), NULL);
  g_return_val_if_fail(plain != NULL, NULL);
  g_return_val_if_fail((len % AES_BLOCK_SIZE) == 0, NULL);
  g_return_val_if_fail(len > 0, NULL);

  cipher = g_malloc0(len);
  mega_aes_key_encrypt_raw(aes_key, plain, cipher, len);
  str = mega_base64urlencode(cipher, len);
  g_free(cipher);
  return str;
}

/**
 * mega_aes_key_decrypt:
 * @aes_key: a #MegaAesKey
 * @cipher: UBase64 encoded ciphertext.
 *
 * Decrypt UBase64 encoded 16-byte aligned ciphertext into binary plaintext data.
 *
 * Returns: (transfer full): Binary plaintext data.
 */
GBytes* mega_aes_key_decrypt(MegaAesKey* aes_key, const gchar* cipher)
{
  gsize cipherlen = 0;
  guchar* cipher_raw;
  guchar* plain;

  g_return_val_if_fail(MEGA_IS_AES_KEY(aes_key), NULL);
  g_return_val_if_fail(cipher != NULL, NULL);

  cipher_raw = mega_base64urldecode(cipher, &cipherlen);
  if (cipher_raw == NULL)
    return NULL;

  if (cipherlen == 0)
  {
    g_free(cipher_raw);
    return NULL;
  }

  if (cipherlen % AES_BLOCK_SIZE != 0)
  {
    g_free(cipher_raw);
    return NULL;
  }

  plain = g_malloc0(cipherlen);
  mega_aes_key_decrypt_raw(aes_key, cipher_raw, plain, cipherlen);
  g_free(cipher_raw);

  return g_bytes_new_take(plain, cipherlen);
}

/**
 * mega_aes_key_encrypt_cbc_raw:
 * @aes_key: a #MegaAesKey
 * @plain: (in) (element-type guint8) (array length=len): Plaintext input data
 * @cipher: (out caller-allocates) (element-type guint8) (array length=len): Ciphertext
 * @len: (in): 16 byte aligned length of plaintext data.
 *
 * Encrypt plaintext blocks using AES key in CBC mode with zero IV
 */
void mega_aes_key_encrypt_cbc_raw(MegaAesKey* aes_key, const guchar* plain, guchar* cipher, gsize len)
{
  guchar iv[AES_BLOCK_SIZE] = {0};

  g_return_if_fail(MEGA_IS_AES_KEY(aes_key));
  g_return_if_fail(plain != NULL);
  g_return_if_fail(cipher != NULL);
  g_return_if_fail((len % AES_BLOCK_SIZE) == 0);
  g_return_if_fail(len > 0);

  cbc_encrypt(&aes_key->priv->enc_key, (nettle_crypt_func*)aes_encrypt, AES_BLOCK_SIZE, iv, len, cipher, plain);
}

/**
 * mega_aes_key_decrypt_cbc_raw:
 * @aes_key: a #MegaAesKey
 * @cipher: (element-type guint8) (array length=len): Ciphertext
 * @plain: (element-type guint8) (array length=len) (out caller-allocates): Plaintext output data
 * @len: 16 byte aligned length of ciphertext and plaintext data.
 *
 * Decrypt ciphertext blocks using AES key in CBC mode with zero IV
 */
void mega_aes_key_decrypt_cbc_raw(MegaAesKey* aes_key, const guchar* cipher, guchar* plain, gsize len)
{
  guchar iv[AES_BLOCK_SIZE] = {0};

  g_return_if_fail(MEGA_IS_AES_KEY(aes_key));
  g_return_if_fail(cipher != NULL);
  g_return_if_fail(plain != NULL);
  g_return_if_fail((len % AES_BLOCK_SIZE) == 0);
  g_return_if_fail(len > 0);

  cbc_decrypt(&aes_key->priv->dec_key, (nettle_crypt_func*)aes_decrypt, AES_BLOCK_SIZE, iv, len, plain, cipher);
}

/**
 * mega_aes_key_encrypt_cbc:
 * @aes_key: a #MegaAesKey
 * @plain: (in) (element-type guint8) (array length=len): Plaintext input data
 * @len: (in): 16 byte aligned length of plaintext data.
 *
 * Encrypt plaintext blocks using AES key in CBC mode with zero IV into UBase64
 * ciphertext.
 *
 * Returns: UBase64 encoded ciphertext.
 */
gchar* mega_aes_key_encrypt_cbc(MegaAesKey* aes_key, const guchar* plain, gsize len)
{
  guchar* cipher;
  gchar* str;

  g_return_val_if_fail(MEGA_IS_AES_KEY(aes_key), NULL);
  g_return_val_if_fail(plain != NULL, NULL);
  g_return_val_if_fail((len % AES_BLOCK_SIZE) == 0, NULL);
  g_return_val_if_fail(len > 0, NULL);

  cipher = g_malloc0(len);
  mega_aes_key_encrypt_cbc_raw(aes_key, plain, cipher, len);
  str = mega_base64urlencode(cipher, len);
  g_free(cipher);

  return str;
}

/**
 * mega_aes_key_decrypt_cbc:
 * @aes_key: a #MegaAesKey
 * @cipher: UBase64 encoded ciphertext.
 *
 * Decrypt UBase64 encoded ciphertext blocks using AES key in CBC mode with zero IV.
 *
 * Returns: (transfer full): UBase64 encoded ciphertext.
 */
GBytes* mega_aes_key_decrypt_cbc(MegaAesKey* aes_key, const gchar* cipher)
{
  guchar* cipher_raw;
  guchar* plain;
  gsize cipherlen = 0;

  g_return_val_if_fail(MEGA_IS_AES_KEY(aes_key), NULL);
  g_return_val_if_fail(cipher != NULL, NULL);

  cipher_raw = mega_base64urldecode(cipher, &cipherlen);
  if (cipher_raw == NULL)
    return NULL;

  if (cipherlen % AES_BLOCK_SIZE != 0)
  {
    g_free(cipher_raw);
    return NULL;
  }

  plain = g_malloc0(cipherlen + 1);
  mega_aes_key_decrypt_cbc_raw(aes_key, cipher_raw, plain, cipherlen);
  g_free(cipher_raw);

  return g_bytes_new_take(plain, cipherlen);
}

/**
 * mega_aes_key_encrypt_string_cbc:
 * @aes_key: a #MegaAesKey
 * @str: Zero terminated string.
 *
 * Zero pad zero terminated string to align data to a AES block size, and
 * encrypt into UBase64 encoded ciphertext.
 *
 * Returns: UBase64 encoded ciphertext.
 */
gchar* mega_aes_key_encrypt_string_cbc(MegaAesKey* aes_key, const gchar* str)
{
  gsize len = 0;
  gsize len_padded;
  gchar* plain;
  gchar* cipher;

  g_return_val_if_fail(MEGA_IS_AES_KEY(aes_key), NULL);
  g_return_val_if_fail(str != NULL, NULL);

  // calculate paded size
  len = len_padded = strlen(str) + 1;
  if (len % AES_BLOCK_SIZE)
    len_padded += AES_BLOCK_SIZE - (len % AES_BLOCK_SIZE);

  plain = g_malloc0(len_padded);
  memcpy(plain, str, len);
  cipher = mega_aes_key_encrypt_cbc(aes_key, plain, len_padded);
  g_free(plain);

  return cipher;
}

/**
 * mega_aes_key_decrypt_string_cbc:
 * @aes_key: a #MegaAesKey
 * @cipher: UBase64 encoded ciphertext.
 *
 * Decrypt UBase64 encoded ciphertext blocks using AES key in CBC mode with zero IV.
 *
 * Returns: Zero terminated string.
 */
gchar* mega_aes_key_decrypt_string_cbc(MegaAesKey* aes_key, const gchar* cipher)
{
  gsize size;
  GBytes* bytes;

  g_return_val_if_fail(MEGA_IS_AES_KEY(aes_key), NULL);
  g_return_val_if_fail(cipher != NULL, NULL);

  bytes = mega_aes_key_decrypt_cbc(aes_key, cipher);
  if (!bytes)
    return NULL;

  return g_bytes_unref_to_data(bytes, &size);
}

/**
 * mega_aes_key_encrypt_ctr:
 * @aes_key: a #MegaAesKey
 * @nonce: (element-type guint8) (array fixed-size=8) (transfer none): 8-byte nonce buffer
 * @position: Counter value (block index)
 * @from: (in) (element-type guint8) (array length=len): Plaintext input data
 * @to: (out caller-allocates) (element-type guint8) (array length=len): Ciphertext
 * @len: (in): 16 byte aligned length of plaintext data.
 *
 * Encrypt plaintext blocks using AES key in CTR mode.
 */
void mega_aes_key_encrypt_ctr(MegaAesKey* aes_key, guchar* nonce, guint64 position, const guchar* from, guchar* to, gsize len)
{
  g_return_if_fail(MEGA_IS_AES_KEY(aes_key));
  g_return_if_fail(nonce != NULL);
  g_return_if_fail(from != NULL);
  g_return_if_fail(to != NULL);
  g_return_if_fail(len > 0);

  // for ctr
  union 
  {
    guchar iv[16];
    struct 
    {
      guchar nonce[8];
      guint64 position;
    };
  } ctr;

  memcpy(ctr.nonce, nonce, 8);
  ctr.position = GUINT64_TO_BE(position);

  ctr_crypt(&aes_key->priv->enc_key, (nettle_crypt_func*)aes_encrypt, AES_BLOCK_SIZE, ctr.iv, len, to, from);
}

/**
 * mega_aes_key_is_loaded:
 * @aes_key: a #MegaAesKey
 *
 * Check if key was successfully loaded. This is useful, when you've created key
 * with #mega_aes_key_new_from_ubase64, and you want to check if it was created
 * correctly.
 *
 * Returns: TRUE if loaded correctly.
 */
gboolean mega_aes_key_is_loaded(MegaAesKey* aes_key)
{
  g_return_val_if_fail(MEGA_IS_AES_KEY(aes_key), FALSE);

  return aes_key->priv->loaded;
}

/**
 * mega_aes_key_get_binary:
 * @aes_key: a #MegaAesKey
 *
 * Get 16 byte AES key data.
 *
 * Returns: (element-type guint8) (array fixed-size=16) (transfer full): Key
 * data
 */
guchar* mega_aes_key_get_binary(MegaAesKey* aes_key)
{
  g_return_val_if_fail(MEGA_IS_AES_KEY(aes_key), NULL);
  g_return_val_if_fail(aes_key->priv->loaded, NULL);

  return g_memdup(aes_key->priv->key, 16);
}

/**
 * mega_aes_key_get_ubase64:
 * @aes_key: a #MegaAesKey
 *
 * Get UBase64 encoded key data.
 *
 * Returns: UBase64 encoded string.
 */
gchar* mega_aes_key_get_ubase64(MegaAesKey* aes_key)
{
  g_return_val_if_fail(MEGA_IS_AES_KEY(aes_key), NULL);
  g_return_val_if_fail(aes_key->priv->loaded, NULL);

  return mega_base64urlencode(aes_key->priv->key, 16);
}

/**
 * mega_aes_key_get_enc_binary:
 * @aes_key: a #MegaAesKey
 * @enc_key: Encryption key.
 *
 * Get 16 byte AES key data encrypted with #enc_key.
 *
 * Returns: (element-type guint8) (array fixed-size=16) (transfer full): Key
 * data
 */
guchar* mega_aes_key_get_enc_binary(MegaAesKey* aes_key, MegaAesKey* enc_key)
{
  guchar cipher_key[16];

  g_return_val_if_fail(MEGA_IS_AES_KEY(aes_key), NULL);
  g_return_val_if_fail(MEGA_IS_AES_KEY(enc_key), NULL);
  g_return_val_if_fail(aes_key->priv->loaded, NULL);

  mega_aes_key_encrypt_raw(enc_key, aes_key->priv->key, cipher_key, 16);

  return g_memdup(cipher_key, 16);
}

/**
 * mega_aes_key_get_enc_ubase64:
 * @aes_key: a #MegaAesKey
 * @enc_key: Encryption key.
 *
 * Get UBase64 encoded key data encrypted with #enc_key.
 *
 * Returns: UBase64 encoded string.
 */
gchar* mega_aes_key_get_enc_ubase64(MegaAesKey* aes_key, MegaAesKey* enc_key)
{
  g_return_val_if_fail(MEGA_IS_AES_KEY(aes_key), NULL);
  g_return_val_if_fail(MEGA_IS_AES_KEY(enc_key), NULL);
  g_return_val_if_fail(aes_key->priv->loaded, NULL);

  return mega_aes_key_encrypt(enc_key, aes_key->priv->key, 16);
}

// {{{ GObject type setup

G_DEFINE_TYPE(MegaAesKey, mega_aes_key, G_TYPE_OBJECT);

static void mega_aes_key_init(MegaAesKey *aes_key)
{
  aes_key->priv = G_TYPE_INSTANCE_GET_PRIVATE(aes_key, MEGA_TYPE_AES_KEY, MegaAesKeyPrivate);
}

static void mega_aes_key_class_init(MegaAesKeyClass *klass)
{
  GObjectClass *gobject_class = G_OBJECT_CLASS(klass);

  g_type_class_add_private(klass, sizeof(MegaAesKeyPrivate));
}

// }}}
