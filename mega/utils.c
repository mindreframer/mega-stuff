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

#include "utils.h"
#include <string.h>
#include <nettle/yarrow.h>
#include <glib/gstdio.h>
#ifdef G_OS_WIN32
#include <windows.h>
#include <wincrypt.h>
#endif

/**
 * mega_format_hex:
 * @data: (element-type guchar) (array length=len) (transfer none):
 * @len:
 *
 * Format binary buffer into string representation. Several formats are
 * supported:
 *
 *   - hexadecimal packed (eg. 0100FF12)
 *   - C like hexadecimal byte prepresentation (eg. 0x01 0x00 0xFF 0x12)
 *   - C string escape sequence (eg. "\x01\x00\xFF\x12")
 *
 * Returns: Formatted string.
 */
gchar* mega_format_hex(const guchar* data, gsize len, MegaHexFormat fmt)
{
  gsize i;
  GString* str;

  g_return_val_if_fail(data != NULL, NULL);
  
  str = g_string_sized_new(64);
  
  if (fmt == MEGA_HEX_FORMAT_PACKED)
  {
    for (i = 0; i < len; i++)
      g_string_append_printf(str, "%02X", (guint)data[i]);
  }
  else if (fmt == MEGA_HEX_FORMAT_C)
  {
    for (i = 0; i < len; i++)
      g_string_append_printf(str, "%s0x%02X", i ? " " : "", (guint)data[i]);
  }
  else if (fmt == MEGA_HEX_FORMAT_STRING)
  {
    g_string_append(str, "\"");
    for (i = 0; i < len; i++)
      g_string_append_printf(str, "\\x%02X", (guint)data[i]);
    g_string_append(str, "\"");
  }
    
  return g_string_free(str, FALSE);
}

/**
 * mega_base64urlencode:
 * @data: (element-type guint8) (array length=len) (transfer none): Buffer data.
 * @len: Buffer length.
 *
 * Encode buffer to Base64 and replace + with -, / with _ and remove trailing =.
 *
 * Returns: (transfer full): UBase64 encoded string.
 */
gchar* mega_base64urlencode(const guchar* data, gsize len)
{
  gint i, shl;
  gchar *sh, *she, *p;

  g_return_val_if_fail(data != NULL, NULL);
  g_return_val_if_fail(len > 0, NULL);

  sh = g_base64_encode(data, len);
  shl = strlen(sh);

  she = g_malloc0(shl + 1), p = she;
  for (i = 0; i < shl; i++)
  {
    if (sh[i] == '+')
      *p = '-';
    else if (sh[i] == '/')
      *p = '_';
    else if (sh[i] == '=')
      continue;
    else
      *p = sh[i];
    p++;
  }

  *p = '\0';

  g_free(sh);
  return she;
}

/**
 * mega_base64urldecode:
 * @str: UBase64 encoded string.
 * @len: (out): Decoded data length.
 *
 * Decode string encoded with #mega_base64urlencode.
 *
 * Returns: (transfer full) (element-type guint8) (array length=len): Decoded data.
 */
guchar* mega_base64urldecode(const gchar* str, gsize* len)
{
  GString* s;
  gint i;

  g_return_val_if_fail(str != NULL, NULL);
  g_return_val_if_fail(len != NULL, NULL);

  s = g_string_new(str);

  for (i = 0; i < s->len; i++)
  {
    if (s->str[i] == '-')
      s->str[i] = '+';
    else if (s->str[i] == '_')
      s->str[i] = '/';
  }

  gint eqs = (s->len * 3) & 0x03;
  for (i = 0; i < eqs; i++)
    g_string_append_c(s, '=');

  guchar* data = g_base64_decode(s->str, len);

  g_string_free(s, TRUE);

  return data;
}

// randomness

G_LOCK_DEFINE_STATIC(yarrow);
static gboolean yarrow_ready = FALSE;
static struct yarrow256_ctx yarrow_ctx;

/**
 * mega_randomness: (skip)
 * @buffer: Buffer.
 * @len: Buffer length.
 *
 * Fill buffer with random data.
 */
void mega_randomness(guchar* buffer, gsize len)
{
  guchar buf[YARROW256_SEED_FILE_SIZE];

  G_LOCK(yarrow);

  if (!yarrow_ready)
  {
    yarrow256_init(&yarrow_ctx, 0, NULL);

#ifdef G_OS_WIN32
    HCRYPTPROV hProvider;
    if (!CryptAcquireContextW(&hProvider, NULL, NULL, PROV_RSA_FULL, CRYPT_VERIFYCONTEXT))
      g_error("Failed to seed random generator");
    if (!CryptGenRandom(hProvider, sizeof(buf), buf))
      g_error("Failed to seed random generator");

    CryptReleaseContext(hProvider, 0); 
#else
    FILE* f = g_fopen("/dev/urandom", "r");
    if (!f)
      g_error("Failed to seed random generator");

    if (fread(buf, 1, sizeof(buf), f) != sizeof(buf))
      g_error("Failed to seed random generator");

    fclose(f);
#endif

    yarrow256_seed(&yarrow_ctx, YARROW256_SEED_FILE_SIZE, buf);
  }

  yarrow256_random(&yarrow_ctx, len, buffer);

  G_UNLOCK(yarrow);
}

// for use in nettle funcs

void mega_randomness_nettle(gpointer ctx, guint len, guchar* buffer)
{
  mega_randomness(buffer, len);
}
