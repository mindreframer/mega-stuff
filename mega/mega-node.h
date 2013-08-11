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

#ifndef __MEGA_NODE_H__
#define __MEGA_NODE_H__

#include <mega/megatypes.h>

#define MEGA_TYPE_NODE            (mega_node_get_type())
#define MEGA_NODE(obj)            (G_TYPE_CHECK_INSTANCE_CAST((obj), MEGA_TYPE_NODE, MegaNode))
#define MEGA_NODE_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST((klass),  MEGA_TYPE_NODE, MegaNodeClass))
#define MEGA_IS_NODE(obj)         (G_TYPE_CHECK_INSTANCE_TYPE((obj), MEGA_TYPE_NODE))
#define MEGA_IS_NODE_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE((klass),  MEGA_TYPE_NODE))
#define MEGA_NODE_GET_CLASS(obj)  (G_TYPE_INSTANCE_GET_CLASS((obj),  MEGA_TYPE_NODE, MegaNodeClass))
#define MEGA_NODE_ERROR           mega_node_error_quark()

typedef struct _MegaNodeClass MegaNodeClass;
typedef struct _MegaNodePrivate MegaNodePrivate;

struct _MegaNode
{
  GObject parent;
  MegaNodePrivate* priv;
};

struct _MegaNodeClass
{
  GObjectClass parent_class;
};

typedef enum
{
  MEGA_NODE_ERROR_OTHER
} MegaNodeError;

typedef enum
{
  MEGA_NODE_TYPE_FILE = 0,
  MEGA_NODE_TYPE_FOLDER = 1,
  MEGA_NODE_TYPE_ROOT = 2,
  MEGA_NODE_TYPE_INBOX = 3,
  MEGA_NODE_TYPE_TRASH = 4,
  MEGA_NODE_TYPE_NETWORK = 9,
  MEGA_NODE_TYPE_CONTACT = 8
} MegaNodeType;

G_BEGIN_DECLS

GType                   mega_node_get_type              (void) G_GNUC_CONST;
gint                    mega_node_error_quark           (void) G_GNUC_CONST;

MegaNode*               mega_node_new                   (MegaFilesystem* filesystem);
MegaNode*               mega_node_new_contacts          (MegaFilesystem* filesystem);

gboolean                mega_node_load                  (MegaNode* node, const gchar* json, GError** error);
gboolean                mega_node_load_user             (MegaNode* node, const gchar* json, GError** error);

gboolean                mega_node_is                    (MegaNode* node, MegaNodeType type);
gboolean                mega_node_is_child              (MegaNode* node, MegaNode* parent);
const gchar*            mega_node_get_handle            (MegaNode* node);
const gchar*            mega_node_get_name              (MegaNode* node);
const gchar*            mega_node_get_path              (MegaNode* node);

gchar*                  mega_node_get_json              (MegaNode* node);
gboolean                mega_node_set_json              (MegaNode* node, const gchar* json);

void                    mega_node_clear                 (MegaNode* node);
MegaSession*            mega_node_get_session           (MegaNode* node);

void                    mega_node_add_child             (MegaNode* node, MegaNode* child);
void                    mega_node_remove_child          (MegaNode* node, MegaNode* child);
void                    mega_node_remove_children       (MegaNode* node);
GSList*                 mega_node_get_children          (MegaNode* node);
GSList*                 mega_node_collect_children      (MegaNode* node);
MegaNode*               mega_node_get_parent            (MegaNode* node);
gchar*                  mega_node_get_public_url        (MegaNode* node, gboolean include_key);

G_END_DECLS

#endif
