/* GAIL - The GNOME Accessibility Enabling Library
 * Copyright 2001 Sun Microsystems Inc.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.	 See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public
 * License along with this library; if not, write to the
 * Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 */

#ifndef __GAIL_TEXT_CELL_H__
#define __GAIL_TEXT_CELL_H__

#include <atk/atk.h>
#include "gailrenderercell.h"
#include "gailtextutil.h"

G_BEGIN_DECLS

#define GAIL_TYPE_TEXT_CELL            (gail_text_cell_get_type ())
#define GAIL_TEXT_CELL(obj)            (G_TYPE_CHECK_INSTANCE_CAST ((obj), GAIL_TYPE_TEXT_CELL, GailTextCell))
#define GAIL_TEXT_CELL_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST ((klass), GAIL_TEXT_CELL, GailTextCellClass))
#define GAIL_IS_TEXT_CELL(obj)         (G_TYPE_CHECK_INSTANCE_TYPE ((obj), GAIL_TYPE_TEXT_CELL))
#define GAIL_IS_TEXT_CELL_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((klass), GAIL_TYPE_TEXT_CELL))
#define GAIL_TEXT_CELL_GET_CLASS(obj)  (G_TYPE_INSTANCE_GET_CLASS ((obj), GAIL_TYPE_TEXT_CELL, GailTextCellClass))

typedef struct _GailTextCell                  GailTextCell;
typedef struct _GailTextCellClass             GailTextCellClass;

struct _GailTextCell
{
  GailRendererCell parent;
  GailTextUtil *textutil;
  gchar *cell_text;
  gint caret_pos;
  gint cell_length;
};

GType gail_text_cell_get_type (void);

struct _GailTextCellClass
{
  GailRendererCellClass parent_class;
};

AtkObject *gail_text_cell_new (void);

G_END_DECLS

#endif /* __GAIL_TREE_VIEW_TEXT_CELL_H__ */
