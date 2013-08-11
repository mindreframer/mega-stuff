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

#ifndef __MEGA_HTTP_IO_STREAM_H__
#define __MEGA_HTTP_IO_STREAM_H__

#include <mega/megatypes.h>

#define MEGA_TYPE_HTTP_IO_STREAM            (mega_http_io_stream_get_type())
#define MEGA_HTTP_IO_STREAM(obj)            (G_TYPE_CHECK_INSTANCE_CAST((obj), MEGA_TYPE_HTTP_IO_STREAM, MegaHttpIOStream))
#define MEGA_HTTP_IO_STREAM_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST((klass),  MEGA_TYPE_HTTP_IO_STREAM, MegaHttpIOStreamClass))
#define MEGA_IS_HTTP_IO_STREAM(obj)         (G_TYPE_CHECK_INSTANCE_TYPE((obj), MEGA_TYPE_HTTP_IO_STREAM))
#define MEGA_IS_HTTP_IO_STREAM_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE((klass),  MEGA_TYPE_HTTP_IO_STREAM))
#define MEGA_HTTP_IO_STREAM_GET_CLASS(obj)  (G_TYPE_INSTANCE_GET_CLASS((obj),  MEGA_TYPE_HTTP_IO_STREAM, MegaHttpIOStreamClass))

typedef struct _MegaHttpIOStreamClass MegaHttpIOStreamClass;
typedef struct _MegaHttpIOStreamPrivate MegaHttpIOStreamPrivate;

struct _MegaHttpIOStream
{
  GIOStream parent;
  MegaHttpIOStreamPrivate* priv;
};

struct _MegaHttpIOStreamClass
{
  GIOStreamClass parent_class;
};

G_BEGIN_DECLS

GType                   mega_http_io_stream_get_type    (void) G_GNUC_CONST;

MegaHttpIOStream*       mega_http_io_stream_new         (MegaHttpClient* client);

G_END_DECLS

#endif
