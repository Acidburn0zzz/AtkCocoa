/* GAIL - The GNOME Accessibility Implementation Library
 * Copyright 2001, 2002, 2003 Sun Microsystems Inc.
 * Copyright 2016 Microsoft Corporation
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the
 * Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 */

#include "config.h"

#include <string.h>
#include <gtk/gtk.h>
#ifdef GDK_WINDOWING_X11
#include <gdk/x11/gdkx.h>
#endif

#import <Cocoa/Cocoa.h>

#include "atk-cocoa/gailtreeview.h"
#include "atk-cocoa/gailrenderercell.h"
#include "atk-cocoa/gailbooleancell.h"
#include "atk-cocoa/gailcontainercell.h"
#include "atk-cocoa/gailtextcell.h"
#include "atk-cocoa/gailcellparent.h"
#include "atk-cocoa/gail-private-macros.h"

#include "atk-cocoa/acdebug.h"

#import "atk-cocoa/ACAccessibilityTreeCellElement.h"
#import "atk-cocoa/ACAccessibilityTreeColumnElement.h"
#import "atk-cocoa/ACAccessibilityTreeRowElement.h"
#import "atk-cocoa/ACAccessibilityCellElement.h"
#import "atk-cocoa/NSAccessibilityElement+AtkCocoa.h"
#import "atk-cocoa/NSArray+AtkCocoa.h"

typedef struct _GailTreeViewRowInfo    GailTreeViewRowInfo;

static void             gail_tree_view_class_init       (GailTreeViewClass      *klass);
static void             gail_tree_view_init             (GailTreeView           *view);
static void             gail_tree_view_real_initialize  (AtkObject              *obj,
                                                         gpointer               data);
static void             gail_tree_view_real_notify_gtk  (GObject		*obj,
                                                         GParamSpec		*pspec);
static void             gail_tree_view_finalize         (GObject                *object);

static void             gail_tree_view_connect_widget_destroyed 
                                                        (GtkAccessible          *accessible);
static void             gail_tree_view_destroyed        (GtkWidget              *widget,
                                                         GtkAccessible          *accessible); 
/* atkobject.h */

static gint             gail_tree_view_get_n_children   (AtkObject              *obj);
static AtkStateSet*     gail_tree_view_ref_state_set    (AtkObject              *obj);
           
/* signal handling */

static gboolean         gail_tree_view_expand_row_gtk   (GtkTreeView            *tree_view,
                                                         GtkTreeIter            *iter,
                                                         GtkTreePath            *path);
static gboolean         gail_tree_view_collapse_row_gtk (GtkTreeView            *tree_view,
                                                         GtkTreeIter            *iter,
                                                         GtkTreePath            *path);
static void             gail_tree_view_size_allocate_gtk (GtkWidget             *widget,
                                                         GtkAllocation          *allocation);
static void             gail_tree_view_set_scroll_adjustments
                                                        (GtkWidget              *widget,
                                                         GtkAdjustment          *hadj,
                                                         GtkAdjustment          *vadj);
static void             gail_tree_view_changed_gtk      (GtkTreeSelection       *selection,
                                                         gpointer               data);

static void             columns_changed                 (GtkTreeView            *tree_view);
static void             model_row_changed               (GtkTreeModel           *tree_model,
                                                         GtkTreePath            *path,
                                                         GtkTreeIter            *iter,
                                                         gpointer               user_data);
static void             column_visibility_changed       (GObject                *object,
                                                         GParamSpec             *param,
                                                         gpointer               user_data);
static void             column_destroy                  (GtkObject              *obj); 
static void             model_row_inserted              (GtkTreeModel           *tree_model,
                                                         GtkTreePath            *path,
                                                         GtkTreeIter            *iter,
                                                         gpointer               user_data);
static void             model_row_deleted               (GtkTreeModel           *tree_model,
                                                         GtkTreePath            *path,
                                                         gpointer               user_data);
static void             destroy_count_func              (GtkTreeView            *tree_view,
                                                         GtkTreePath            *path,
                                                         gint                   count,
                                                         gpointer               user_data);
static void             model_rows_reordered            (GtkTreeModel           *tree_model,
                                                         GtkTreePath            *path,
                                                         GtkTreeIter            *iter,
                                                         gint                   *new_order,
                                                         gpointer               user_data);
static void             adjustment_changed              (GtkAdjustment          *adjustment,
                                                         GtkTreeView            *tree_view);

/* Misc */
static void update_column_cells (GtkTreeView *tree_view,
                                 GtkTreePath *path,
                                 GtkTreeViewColumn *column);
static gboolean cocoa_update_cell_value (GailRendererCell *renderer_cell,
                                         GailTreeView     *gailview,
                                         ACAccessibilityTreeRowElement *rowElement,
                                         GtkTreeViewColumn *column,
                                         gboolean         emit_change_signal);
static void             connect_model_signals           (GtkTreeView            *view,
                                                         GailTreeView           *gailview); 
static void             disconnect_model_signals        (GailTreeView           *gailview); 

static void make_row_cache (GtkTreeModel *treeModel,
                            GtkTreeView *view,
                            GailTreeView *gailView,
                            GtkTreeIter *rootIter);
static ACAccessibilityTreeRowElement *find_row_element_for_path (GailTreeView *gailView,
                                                                 GtkTreePath *path);
static ACAccessibilityTreeColumnElement *find_column_element_for_column (GailTreeView *gailView,
                                                                         GtkTreeViewColumn *column);
static NSAccessibilityElement *make_accessibility_element_for_row (GtkTreeModel *treeModel,
                                                                   GtkTreeView *treeView,
                                                                   GailTreeView *gailView,
                                                                   GtkTreeIter  *rowIter);
static void sort_child_elements (GailTreeView *gailView);
static NSInteger row_column_index_sort (id objA,
                                        id objB,
                                        void *data);
static void update_accessibility_rows (GailTreeView *gailView);
static void update_column_headers (GtkTreeView *tree_view);
static void remove_all_children (GailTreeView *gailview,
                                 NSAccessibilityElement *treeElement,
                                 ACAccessibilityTreeRowElement *parentElement);

static GQuark quark_column_desc_object = 0;
static GQuark quark_column_header_object = 0;
static gboolean editing = FALSE;
static const gchar* hadjustment = "hadjustment";
static const gchar* vadjustment = "vadjustment";

G_DEFINE_TYPE (GailTreeView, gail_tree_view, GAIL_TYPE_CONTAINER)

static void
gail_tree_view_class_init (GailTreeViewClass *klass)
{
  AtkObjectClass *class = ATK_OBJECT_CLASS (klass);
  GObjectClass *gobject_class = G_OBJECT_CLASS (klass);
  GtkAccessibleClass *accessible_class;
  GailWidgetClass *widget_class;
  GailContainerClass *container_class;

  accessible_class = (GtkAccessibleClass*)klass;
  widget_class = (GailWidgetClass*)klass;
  container_class = (GailContainerClass*)klass;

  class->get_n_children = gail_tree_view_get_n_children;
  class->ref_state_set = gail_tree_view_ref_state_set;
  class->initialize = gail_tree_view_real_initialize;

  widget_class->notify_gtk = gail_tree_view_real_notify_gtk;

  accessible_class->connect_widget_destroyed = gail_tree_view_connect_widget_destroyed;

  /*
   * The children of a GtkTreeView are the buttons at the top of the columns
   * we do not represent these as children so we do not want to report
   * children added or deleted when these changed.
   */
  container_class->add_gtk = NULL;
  container_class->remove_gtk = NULL;

  gobject_class->finalize = gail_tree_view_finalize;

  quark_column_desc_object = g_quark_from_static_string ("gtk-column-object");
  quark_column_header_object = g_quark_from_static_string ("gtk-header-object");
}

static void
gail_tree_view_init (GailTreeView *view)
{
}

static void
print_refcount (id obj)
{
  NSLog (@"Count is %ld", CFGetRetainCount((__bridge CFTypeRef) obj));
}

#define ROOT_NODE(view) ((__bridge ACAccessibilityTreeRowElement *)(view)->rowRootNode)

static ACAccessibilityTreeRowElement *
add_row_to_row_map (GailTreeView *view,
                    ACAccessibilityTreeRowElement *row_element)
{
  ACAccessibilityTreeRowElement *parent;
  GtkTreePath *path = [row_element rowPath];

  if (path == NULL) {
    return nil;
  }

  char *pathString = gtk_tree_path_to_string (path);
  char *parentPathString;
  int idx;

  if (gtk_tree_path_get_depth (path) > 1) {
    gtk_tree_path_up (path);
    parentPathString = gtk_tree_path_to_string (path);

    parent = [ROOT_NODE (view) childAtPath:parentPathString];

    if (parent == nil) {
      g_warning ("No parent found at %s for %s", parentPathString, pathString);
      g_free (parentPathString);
      return NULL;
    }

    g_free (parentPathString);
  } else {
    parent = ROOT_NODE (view);
  }

  idx = last_path_index (pathString);
  g_free (pathString);

  [parent insertChild:row_element atIndex:idx];

  return parent;
}

static ACAccessibilityTreeRowElement *
get_row_from_row_map (GailTreeView *view,
                      GtkTreePath *path)
{
  ACAccessibilityTreeRowElement *row;
  char *pathString = gtk_tree_path_to_string (path);

  row = [ROOT_NODE (view) childAtPath:pathString];

  g_free (pathString);
  return row;
}

static void
remove_row_from_row_map (GailTreeView *view,
                         ACAccessibilityTreeRowElement *row)
{
  [row removeFromParent];
}

static void
gail_tree_view_real_initialize (AtkObject *obj,
                                gpointer  data)
{
  GailTreeView *view;
  GtkTreeView *tree_view;
  GtkTreeModel *tree_model; 
  GtkAdjustment *adj;
  GList *tv_cols, *tmp_list;
  GtkWidget *widget;

  ATK_OBJECT_CLASS (gail_tree_view_parent_class)->initialize (obj, data);

  view = GAIL_TREE_VIEW (obj);
  view->old_hadj = NULL;
  view->old_vadj = NULL;

  view->columnMap = g_hash_table_new (NULL, NULL);

  widget = GTK_WIDGET (data);
  tree_view = GTK_TREE_VIEW (widget);

  // We make a tree from the ACAccessibilityTreeRowElements that matches the GtkTreeModel
  // so we can quickly access the appropriate element given a row path. Using an array is too slow
  // with large tables, and a hashtable isn't feasible due to how GtkTreeModel works
  view->rowRootNode = (__bridge_retained void *)[[ACAccessibilityTreeRowElement alloc] initWithDelegate:NULL
                                                                                                treeRow:NULL
                                                                                               treeView:tree_view];

  g_signal_connect_after (widget,
                          "row-collapsed",
                          G_CALLBACK (gail_tree_view_collapse_row_gtk),
                          NULL);
  g_signal_connect (widget,
                    "row-expanded",
                    G_CALLBACK (gail_tree_view_expand_row_gtk),
                    NULL);
  g_signal_connect (widget,
                    "size-allocate",
                    G_CALLBACK (gail_tree_view_size_allocate_gtk),
                    NULL);

  tree_model = gtk_tree_view_get_model (tree_view);

  /* Set up signal handling */

  g_signal_connect_data (gtk_tree_view_get_selection (tree_view),
                         "changed",
                         (GCallback) gail_tree_view_changed_gtk,
                      	 obj, NULL, 0);

  g_signal_connect_data (tree_view, "columns-changed",
    (GCallback) columns_changed, NULL, NULL, 0);

  view->tree_model = tree_model;
  if (tree_model)
    {
      g_object_add_weak_pointer (G_OBJECT (view->tree_model), (gpointer *)&view->tree_model);
      connect_model_signals (tree_view, view);

      if (gtk_tree_model_get_flags (tree_model) & GTK_TREE_MODEL_LIST_ONLY)
        obj->role = ATK_ROLE_TABLE;
      else
        obj->role = ATK_ROLE_TREE_TABLE;
    }
  else
    {
      obj->role = ATK_ROLE_UNKNOWN;
    }

  /* adjustment callbacks */

  g_object_get (tree_view, hadjustment, &adj, NULL);
  view->old_hadj = adj;
  g_object_add_weak_pointer (G_OBJECT (view->old_hadj), (gpointer *)&view->old_hadj);
  g_signal_connect (adj, 
                    "value_changed",
                    G_CALLBACK (adjustment_changed),
                    tree_view);

  g_object_get (tree_view, vadjustment, &adj, NULL);
  view->old_vadj = adj;
  g_object_add_weak_pointer (G_OBJECT (view->old_vadj), (gpointer *)&view->old_vadj);
  g_signal_connect (adj, 
                    "value_changed",
                    G_CALLBACK (adjustment_changed),
                    tree_view);
  g_signal_connect_after (widget,
                          "set_scroll_adjustments",
                          G_CALLBACK (gail_tree_view_set_scroll_adjustments),
                          NULL);

  tv_cols = gtk_tree_view_get_columns (tree_view);

  for (tmp_list = tv_cols; tmp_list; tmp_list = tmp_list->next)
    {
      g_signal_connect_data (tmp_list->data, "notify::visible",
       (GCallback)column_visibility_changed, 
        tree_view, NULL, FALSE);
      g_signal_connect_data (tmp_list->data, "destroy",
       (GCallback)column_destroy, 
        NULL, NULL, FALSE);
    }

  g_list_free (tv_cols);
}

static void
clear_column_elements (gpointer key,
                       gpointer value,
                       gpointer data)
{
  id<NSAccessibility> c = (__bridge id<NSAccessibility>)value;
  [c setAccessibilityChildren:nil];
}

static void
destroy_root(GailTreeView *gailview)
{
  if (gailview->rowRootNode) {
    remove_all_children(gailview, ac_element_get_accessibility_element(AC_ELEMENT (gailview)), ROOT_NODE(gailview));
    CFBridgingRelease(gailview->rowRootNode);
    gailview->rowRootNode = nil;
  }
}

static void
gail_tree_view_real_notify_gtk (GObject             *obj,
                                GParamSpec          *pspec)
{
  GtkWidget *widget;
  AtkObject* atk_obj;
  GtkTreeView *tree_view;
  GailTreeView *gailview;
  GtkAdjustment *adj;

  widget = GTK_WIDGET (obj);
  atk_obj = gtk_widget_get_accessible (widget);
  tree_view = GTK_TREE_VIEW (widget);
  gailview = GAIL_TREE_VIEW (atk_obj);

  if (strcmp (pspec->name, "model") == 0)
    {
      GtkTreeModel *tree_model;
      AtkRole role;

      // Cancel any rows being updated
      if (gailview->rowUpdateId > 0) {
        g_source_remove (gailview->rowUpdateId);
        gailview->rowUpdateId = 0;
      }

      tree_model = gtk_tree_view_get_model (tree_view);
      if (gailview->tree_model)
        {
          g_object_remove_weak_pointer (G_OBJECT (gailview->tree_model), (gpointer *)&gailview->tree_model);
          disconnect_model_signals (gailview);
        }

      destroy_root(gailview);

      // The columns are part of the tree view, but the child elements are part of the model
      if (gailview->columnMap) {
        g_hash_table_foreach (gailview->columnMap, clear_column_elements, NULL);
      }

      gailview->tree_model = tree_model;
      /*
       * if there is no model the GtkTreeView is probably being destroyed
       */
      if (tree_model)
        {
          GtkTreeIter iter;

          g_object_add_weak_pointer (G_OBJECT (gailview->tree_model), (gpointer *)&gailview->tree_model);
          connect_model_signals (tree_view, gailview);

          if (gtk_tree_model_get_flags (tree_model) & GTK_TREE_MODEL_LIST_ONLY) {
            role = ATK_ROLE_TABLE;
          } else {
            role = ATK_ROLE_TREE_TABLE;
          }

          // We make a tree from the ACAccessibilityTreeRowElements that matches the GtkTreeModel
          // so we can quickly access the appropriate element given a row path. Using an array is too slow
          // with large tables, and a hashtable isn't feasible due to how GtkTreeModel works
          gailview->rowRootNode = (__bridge_retained void *)[[ACAccessibilityTreeRowElement alloc] initWithDelegate:NULL
                                                                                                            treeRow:NULL
                                                                                                           treeView:NULL];

          // When a model is set build the row cache
          if (gtk_tree_model_get_iter_first (tree_model, &iter)) {
            make_row_cache (tree_model, tree_view, gailview, &iter);
          }
        }
      else
        {
          role = ATK_ROLE_UNKNOWN;
          // FIXME: Tear down the row cache
        }

      atk_object_set_role (atk_obj, role);
      g_object_freeze_notify (G_OBJECT (atk_obj));
      g_signal_emit_by_name (atk_obj, "visible_data_changed");
      g_object_thaw_notify (G_OBJECT (atk_obj));
    }
  else if (strcmp (pspec->name, hadjustment) == 0)
    {
      g_object_get (tree_view, hadjustment, &adj, NULL);
      g_signal_handlers_disconnect_by_func (gailview->old_hadj, 
                                           (gpointer) adjustment_changed,
                                           widget);
      gailview->old_hadj = adj;
      g_object_add_weak_pointer (G_OBJECT (gailview->old_hadj), (gpointer *)&gailview->old_hadj);
      g_signal_connect (adj, 
                        "value_changed",
                        G_CALLBACK (adjustment_changed),
                        tree_view);
    }
  else if (strcmp (pspec->name, vadjustment) == 0)
    {
      g_object_get (tree_view, vadjustment, &adj, NULL);
      g_signal_handlers_disconnect_by_func (gailview->old_vadj, 
                                           (gpointer) adjustment_changed,
                                           widget);
      gailview->old_vadj = adj;
      g_object_add_weak_pointer (G_OBJECT (gailview->old_hadj), (gpointer *)&gailview->old_vadj);
      g_signal_connect (adj, 
                        "value_changed",
                        G_CALLBACK (adjustment_changed),
                        tree_view);
    }
  else if (strcmp (pspec->name, "headers-visible") == 0)
    {
      update_column_headers(tree_view);
    }
  else
    GAIL_WIDGET_CLASS (gail_tree_view_parent_class)->notify_gtk (obj, pspec);
}

static void
cleanup_caches (GailTreeView *gailview)
{
  destroy_root(gailview);

  if (gailview->columnMap) {
    g_hash_table_destroy (gailview->columnMap);
    gailview->columnMap = NULL;
  }
}

static void
gail_tree_view_finalize (GObject	    *object)
{
  GailTreeView *view = GAIL_TREE_VIEW (object);

  if (view->tree_model)
    {
      g_object_remove_weak_pointer (G_OBJECT (view->tree_model), (gpointer *)&view->tree_model);
      disconnect_model_signals (view);
    }

  cleanup_caches(view);
  G_OBJECT_CLASS (gail_tree_view_parent_class)->finalize (object);
}

static void
gail_tree_view_connect_widget_destroyed (GtkAccessible *accessible)
{
  if (accessible->widget)
    {
      g_signal_connect_after (accessible->widget,
                              "destroy",
                              G_CALLBACK (gail_tree_view_destroyed),
                              accessible);
    }
  GTK_ACCESSIBLE_CLASS (gail_tree_view_parent_class)->connect_widget_destroyed (accessible);
}

static void
gail_tree_view_destroyed (GtkWidget *widget,
                          GtkAccessible *accessible)
{
  GtkAdjustment *adj;
  GailTreeView *gailview;

  gail_return_if_fail (GTK_IS_TREE_VIEW (widget));

  gailview = GAIL_TREE_VIEW (accessible);

  if (gailview->rowUpdateId > 0) {
    g_source_remove (gailview->rowUpdateId);
    gailview->rowUpdateId = 0;
  }

  adj = gailview->old_hadj;
  if (adj)
    g_signal_handlers_disconnect_by_func (adj, 
                                          (gpointer) adjustment_changed,
                                          widget);
  adj = gailview->old_vadj;
  if (adj)
    g_signal_handlers_disconnect_by_func (adj, 
                                          (gpointer) adjustment_changed,
                                          widget);
  if (gailview->tree_model)
    {
      g_object_remove_weak_pointer (G_OBJECT (gailview->tree_model), (gpointer *)&gailview->tree_model);
      disconnect_model_signals (gailview);
      gailview->tree_model = NULL;
    }

  cleanup_caches (gailview);
}

/* atkobject.h */

static gint
gail_tree_view_get_n_children (AtkObject *obj)
{
  return 0;
}

static AtkStateSet*
gail_tree_view_ref_state_set (AtkObject *obj)
{
  AtkStateSet *state_set;
  GtkWidget *widget;

  state_set = ATK_OBJECT_CLASS (gail_tree_view_parent_class)->ref_state_set (obj);
  widget = GTK_ACCESSIBLE (obj)->widget;

  if (widget != NULL)
    atk_state_set_add_state (state_set, ATK_STATE_MANAGES_DESCENDANTS);

  return state_set;
}

/* signal handling */

static void
add_row_to_tree (GailTreeView *gailView,
                 ACAccessibilityTreeRowElement *child,
                 BOOL isDisclosed)
{
  ACAccessibilityElement *treeElement = (ACAccessibilityElement *)ac_element_get_accessibility_element(AC_ELEMENT(gailView));

  ACAccessibilityTreeRowElement *parentRowElement = add_row_to_row_map (gailView, child);

  [treeElement accessibilityAddChildElement:child];

  if (isDisclosed) {
    [parentRowElement addChildRowElement:child];
  }
  [child setAccessibilityWindow:[treeElement accessibilityWindow]];
  [child setAccessibilityTopLevelUIElement:[treeElement accessibilityWindow]];

  update_accessibility_rows(gailView);
}

static void
remove_row_from_tree (GailTreeView *gailView,
                      ACAccessibilityTreeRowElement *child)
{
  ACAccessibilityElement *treeElement = (ACAccessibilityElement *)ac_element_get_accessibility_element(AC_ELEMENT(gailView));

  [treeElement ac_accessibilityRemoveChildElement:child];

  NSArray *selectedRows = [treeElement accessibilitySelectedRows];
  if ([selectedRows containsObject:child]) {
    [treeElement setAccessibilitySelectedRows:[selectedRows ac_removeObject:child]];
  }

  [[child parent] removeChildRowElement:child];

  remove_row_from_row_map(gailView, child);

  // instead of walking the whole tree by calling update_accessibility_rows
  // we can just remove them by hand
  NSArray *rows = [treeElement accessibilityRows];
  [treeElement setAccessibilityRows:[rows ac_removeObject:child]];
  [treeElement setAccessibilityVisibleRows:[treeElement accessibilityRows]];
}

static gboolean
gail_tree_view_expand_row_gtk (GtkTreeView       *tree_view,
                               GtkTreeIter        *iter,
                               GtkTreePath        *path)
{
  AtkObject *atk_obj;
  GailTreeView *gailview;
  GtkTreeModel *tree_model;
  NSAccessibilityElement *treeElement;
  ACAccessibilityTreeRowElement *expandedElement;

  g_assert (GTK_IS_TREE_VIEW (tree_view));

  atk_obj = gtk_widget_get_accessible (GTK_WIDGET (tree_view));

  g_assert (GAIL_IS_TREE_VIEW (atk_obj));

  gailview = GAIL_TREE_VIEW (atk_obj);

  tree_model = gtk_tree_view_get_model (tree_view);

  // Get the NSA for path
  expandedElement = (ACAccessibilityTreeRowElement *) find_row_element_for_path (gailview, path);
  treeElement = ac_element_get_accessibility_element (AC_ELEMENT (gailview));

  if (expandedElement == nil) {
    g_warning ("Expanding row without a11y element %s", gtk_tree_path_to_string (path));
    return FALSE;
  } else {
    GtkTreeIter childIter;
    if (gtk_tree_model_iter_children (tree_model, &childIter, iter)) {
      // g_print ("Got children\n");
      NSMutableArray *disclosedRows = [[NSMutableArray alloc] init];

      // FIXME: I think the disclosedRows need to have the whole subtree flattened, not just the direct children
      do {
        NSAccessibilityElement *element = make_accessibility_element_for_row (tree_model, tree_view, gailview, &childIter);
        add_row_to_tree(gailview, (ACAccessibilityTreeRowElement *)element, YES);
      } while (gtk_tree_model_iter_next (tree_model, &childIter));

      // Now the indices have been updated, sort the disclosed rows
      [disclosedRows sortUsingFunction:row_column_index_sort context:NULL];
      [expandedElement setAccessibilityDisclosedRows:disclosedRows];
    }
  }

  NSAccessibilityPostNotification(expandedElement, NSAccessibilityRowExpandedNotification);

  return FALSE;
}

static void
remove_all_children (GailTreeView *gailview,
                     NSAccessibilityElement *treeElement,
                     ACAccessibilityTreeRowElement *parentElement)
{
  // Cancel any pending update until after we've removed these rows
  if (gailview->rowUpdateId > 0) {
    g_source_remove (gailview->rowUpdateId);
    gailview->rowUpdateId = 0;
  }

  NSMutableArray *childrenToRemove = [NSMutableArray array];
  [parentElement foreachChild:^void (ACAccessibilityTreeRowElement *parent, ACAccessibilityTreeRowElement *child, void *userdata) {
    remove_all_children ((GailTreeView *)userdata, treeElement, child);

    [childrenToRemove addObject:child];
  } userData:gailview];

  // Gather all the rows that need removed so we can remove them outside of the foreachChild loop
  for (ACAccessibilityTreeRowElement *c in childrenToRemove) {
    remove_row_from_tree(gailview, c);
  }

  [parentElement removeAllChildren];
}

static gboolean
gail_tree_view_collapse_row_gtk (GtkTreeView       *tree_view,
                                 GtkTreeIter        *iter,
                                 GtkTreePath        *path)
{
  GtkTreeModel *tree_model;
  AtkObject *atk_obj = gtk_widget_get_accessible (GTK_WIDGET (tree_view));
  GailTreeView *gailview = GAIL_TREE_VIEW (atk_obj);
  gint row;

  AC_NOTE (TREEWIDGET, g_print ("Collapsing row: %s\n", gtk_tree_path_to_string (path)));
  NSAccessibilityElement *treeElement;
  ACAccessibilityTreeRowElement *collapsedElement;

  tree_model = gtk_tree_view_get_model (tree_view);

  treeElement = ac_element_get_accessibility_element (AC_ELEMENT (gailview));
  collapsedElement = find_row_element_for_path (gailview, path);
  remove_all_children (gailview, treeElement, collapsedElement);

  NSAccessibilityPostNotification(collapsedElement, NSAccessibilityRowCollapsedNotification);

  return FALSE;
}

static void
gail_tree_view_size_allocate_gtk (GtkWidget     *widget,
                                  GtkAllocation *allocation)
{
}

static void
gail_tree_view_set_scroll_adjustments (GtkWidget     *widget,
                                       GtkAdjustment *hadj,
                                       GtkAdjustment *vadj)
{
  AtkObject *atk_obj = gtk_widget_get_accessible (widget);
  GailTreeView *gailview = GAIL_TREE_VIEW (atk_obj);
  GtkAdjustment *adj;

  g_object_get (widget, hadjustment, &adj, NULL);
  if (gailview->old_hadj != adj)
     {
        g_signal_handlers_disconnect_by_func (gailview->old_hadj, 
                                              (gpointer) adjustment_changed,
                                              widget);
        gailview->old_hadj = adj;
        g_object_add_weak_pointer (G_OBJECT (gailview->old_hadj), (gpointer *)&gailview->old_hadj);
        g_signal_connect (adj, 
                          "value_changed",
                          G_CALLBACK (adjustment_changed),
                          widget);
     } 
  g_object_get (widget, vadjustment, &adj, NULL);
  if (gailview->old_vadj != adj)
     {
        g_signal_handlers_disconnect_by_func (gailview->old_vadj, 
                                              (gpointer) adjustment_changed,
                                              widget);
        gailview->old_vadj = adj;
        g_object_add_weak_pointer (G_OBJECT (gailview->old_vadj), (gpointer *)&gailview->old_vadj);
        g_signal_connect (adj, 
                          "value_changed",
                          G_CALLBACK (adjustment_changed),
                          widget);
     } 
}

static void
gail_tree_view_changed_gtk (GtkTreeSelection *selection,
                            gpointer         data)
{
  GailTreeView *gailview;
  GtkTreeView *tree_view;
  GtkWidget *widget;
  GList *selected;
  GtkTreeModel *selectionModel;
  NSAccessibilityElement *element;

  gailview = GAIL_TREE_VIEW (data);
  widget = GTK_ACCESSIBLE (gailview)->widget;
  if (widget == NULL)
    /*
     * destroy signal emitted for widget
     */
    return;
  tree_view = GTK_TREE_VIEW (widget);

  selected = gtk_tree_selection_get_selected_rows(selection, &selectionModel);

  NSMutableArray *selRows = [NSMutableArray array];
  for (GList *l = selected; l; l = l->next) {
    GtkTreePath *p = l->data;
    ACAccessibilityTreeRowElement *row = get_row_from_row_map(gailview, p);

    if (row != nil) {
      [selRows addObject:row];
    }
  }

  element = ac_element_get_accessibility_element(AC_ELEMENT (gailview));
  [element setAccessibilitySelectedRows:selRows];

  NSAccessibilityPostNotification(element, NSAccessibilitySelectedRowsChangedNotification);

  g_list_foreach(selected, (GFunc)gtk_tree_path_free, NULL);
  g_list_free (selected);
}

static gboolean
remove_column_from_parent (gpointer key,
                           gpointer value,
                           gpointer data)
{
  NSAccessibilityElement *parent = (__bridge NSAccessibilityElement *)data;
  id<NSAccessibility> element = (__bridge id<NSAccessibility>)value;
  ACAccessibilityTreeColumnElement *columnElement = (ACAccessibilityTreeColumnElement *)element;

  [columnElement setAccessibilityWindow:nil];
  [columnElement setAccessibilityTopLevelUIElement:nil];

  [parent ac_accessibilityRemoveChildElement:element];
  [parent ac_accessibilityRemoveColumnElement:element];
  [parent setAccessibilityVisibleColumns:[parent accessibilityChildren]];

  id<NSAccessibility> headerElement = [columnElement columnHeaderElement];
  if (headerElement != nil) {
    NSArray *headers = [parent accessibilityColumnHeaderUIElements];

    if (headers != nil) {
      [parent setAccessibilityColumnHeaderUIElements:[headers ac_removeObject:headerElement]];
    }
  }

  // Remove this from the hashtable

  CFRelease(value);
  return TRUE;
}

static void
update_column_headers (GtkTreeView *tree_view)
{
  AtkObject *atk_obj = gtk_widget_get_accessible(GTK_WIDGET(tree_view));
  GailTreeView *gailview = GAIL_TREE_VIEW(atk_obj);
  ACAccessibilityElement *element = ac_element_get_accessibility_element(AC_ELEMENT(atk_obj));
  GList *tv_cols, *t;
  BOOL hasHeaders = gtk_tree_view_get_headers_visible(tree_view);
  NSMutableArray *headers;

  if (hasHeaders) {
    headers = [NSMutableArray array];

    tv_cols = gtk_tree_view_get_columns(tree_view);
    for (t = tv_cols; t; t = t->next) {
      ACAccessibilityTreeColumnElement *tc = find_column_element_for_column(gailview, t->data);
      if (tc == nil) {
        continue;
      }

      id<NSAccessibility> headerElement = [tc columnHeaderElement];
      if (headerElement) {
        [headers addObject:headerElement];
        [element accessibilityAddChildElement:headerElement];
        [headerElement setAccessibilityWindow:[element accessibilityWindow]];
        [headerElement setAccessibilityTopLevelUIElement:[element accessibilityWindow]];
      }
    }

    g_list_free (tv_cols);
  } else {
    for (ACAccessibilityElement *headerElement in [element accessibilityColumnHeaderUIElements]) {
      [element ac_accessibilityRemoveChildElement:headerElement];
    }

    headers = nil;
  }

  [element setAccessibilityColumnHeaderUIElements:headers];
}

static void
columns_changed (GtkTreeView *tree_view)
{
  AtkObject *atk_obj = gtk_widget_get_accessible (GTK_WIDGET(tree_view));
  GailTreeView *gailview = GAIL_TREE_VIEW (atk_obj);
  GList *tv_cols, *tmp_list;

  // If columnMap is NULL then we're probably being destroyed
  if (gailview->columnMap == NULL) {
    return;
  }

  tv_cols = gtk_tree_view_get_columns (tree_view);

  /* rebuild the array */
  int idx;
  NSAccessibilityElement *parentElement = ac_element_get_accessibility_element (AC_ELEMENT (atk_obj));

  g_hash_table_foreach_remove (gailview->columnMap, remove_column_from_parent, (__bridge void *)parentElement);

  idx = 0;
  for (tmp_list = tv_cols; tmp_list; tmp_list = tmp_list->next) {
    ACAccessibilityTreeColumnElement *tc;

    /* create a column for NSAccessibility */
    tc = [[ACAccessibilityTreeColumnElement alloc] initWithDelegate:AC_ELEMENT (atk_obj) treeColumn:tmp_list->data];

    [tc setAccessibilityIndex:idx];
    [tc setAccessibilityWindow:[parentElement accessibilityWindow]];
    [tc setAccessibilityTopLevelUIElement:[parentElement accessibilityWindow]];
    g_hash_table_insert (gailview->columnMap, tmp_list->data, (__bridge_retained void *)tc);

    /* Add the column as an accessibillty child */
    [parentElement accessibilityAddChildElement:tc];
    [parentElement ac_accessibilityAddColumnElement:tc];
    [parentElement setAccessibilityVisibleColumns:[parentElement accessibilityColumns]];

    idx++;
  }
  sort_child_elements (gailview);
  update_column_headers(tree_view);

  g_list_free (tv_cols);
}

static void
model_row_changed (GtkTreeModel *tree_model,
                   GtkTreePath  *path, 
                   GtkTreeIter  *iter,
                   gpointer     user_data)
{
  GtkTreeView *tree_view = GTK_TREE_VIEW(user_data);
  GailTreeView *gailview;
  ACAccessibilityTreeRowElement *row;
 
  gailview = GAIL_TREE_VIEW (gtk_widget_get_accessible (GTK_WIDGET (tree_view)));

  row = find_row_element_for_path (gailview, path);
  if (row == nil) {
    return;
  }

  [row setRowIsDirty:YES];
}

// FIXME: This should probably be done on a signal on GailCell
void
gail_tree_view_update_row_cells (GailTreeView *gailview,
                                 GtkTreeView *tree_view,
                                 ACAccessibilityTreeRowElement *rowElement)
{
  GtkTreePath *row_path;

  NSArray *children = [rowElement accessibilityChildren];
  if ([children count] == 0) {
    return;
  }

  row_path = [rowElement rowPath];
  if (row_path == NULL) {
    return;
  }

  for (ACAccessibilityTreeCellElement *tree_cell in children) {
    ACAccessibilityTreeColumnElement *columnElement = [tree_cell columnElement];
    GtkTreeViewColumn *column = [columnElement column];
    NSArray *cell_children = [tree_cell accessibilityChildren];

    // There should be one TreeCellElement per column, so only update the column cells once
    update_column_cells (tree_view, row_path, column);

    for (id<NSAccessibility> c in cell_children) {
      // c may be a disclosure triangle element.
      if ([c isKindOfClass:[ACAccessibilityCellElement class]]) {
        ACAccessibilityCellElement *cellElement = (ACAccessibilityCellElement *)c;

        GailRendererCell *renderer = (GailRendererCell *)[cellElement delegate];
        cocoa_update_cell_value (renderer, gailview, rowElement, column, TRUE);
      }
    }
  }

  gtk_tree_path_free (row_path);
  [rowElement setRowIsDirty:NO];
}

static void
column_visibility_changed (GObject    *object,
                           GParamSpec *pspec,
                           gpointer   user_data)
{
}

/*
 * This is the signal handler for the "destroy" signal for a GtkTreeViewColumn
 *
 * We check whether we have stored column description or column header
 * and if so we get rid of it.
 */
static void
column_destroy (GtkObject *obj)
{
  GtkTreeViewColumn *tv_col = GTK_TREE_VIEW_COLUMN (obj);
  AtkObject *header;
  gchar *desc;

  header = g_object_get_qdata (G_OBJECT (tv_col),
                          quark_column_header_object);
  if (header)
    g_object_unref (header);
  desc = g_object_get_qdata (G_OBJECT (tv_col),
                           quark_column_desc_object);
  g_free (desc); 
}

static void
iterate_row_children (GtkTreeModel *treeModel,
                      GtkTreeIter *iter,
                      GtkTreeView *treeView,
                      GailTreeView *gailView)
{
  do {
    GtkTreeIter childIter;
    GtkTreePath *path;

    path = gtk_tree_model_get_path (treeModel, iter);

    if (gtk_tree_model_iter_children (treeModel, &childIter, iter)) {
      if (gtk_tree_view_row_expanded (treeView, path)) {
        iterate_row_children (treeModel, &childIter, treeView, gailView);
      } else {
      }
    } else {
    }

    gtk_tree_path_free (path);
  } while (gtk_tree_model_iter_next (treeModel, iter));
}

static void
make_row_cache_recursive (GtkTreeModel *model,
                          GtkTreeView *view,
                          GailTreeView *gailView,
                          GtkTreeIter *iter,
                          ACAccessibilityTreeRowElement *parentElement,
                          BOOL isDisclosed)
{
  do {
    ACAccessibilityTreeRowElement *element;
    NSAccessibilityElement *treeElement;
    GtkTreeIter childIter;

    element = (ACAccessibilityTreeRowElement *)make_accessibility_element_for_row (model, view, gailView, iter);
    add_row_to_tree(gailView, element, isDisclosed);

    if (gtk_tree_model_iter_children (model, &childIter, iter)) {
      GtkTreePath *path = gtk_tree_model_get_path (model, iter);
      if (gtk_tree_view_row_expanded (view, path)) {
        make_row_cache_recursive (model, view, gailView, &childIter, element, YES);
      }
      gtk_tree_path_free (path);
    }
  } while (gtk_tree_model_iter_next (model, iter));
}

static void
make_row_cache (GtkTreeModel *model,
                GtkTreeView *treeView,
                GailTreeView *gailView,
                GtkTreeIter  *rootIter)
{
  ACAccessibilityTreeRowElement *element;

  element = ROOT_NODE (gailView);

  AC_NOTE (TREEWIDGET, g_print ("Making row cache\n"));
  make_row_cache_recursive (model, treeView, gailView, rootIter, element, NO);
}

// Cocoa appears to perform a sort on the children of an outline to keep them in
// row, column order, with rows and columns sorted by index
static NSInteger
row_column_index_sort (id objA,
                       id objB,
                       void *data)
{
  id<NSAccessibility> a = (id<NSAccessibility>)objA;
  id<NSAccessibility> b = (id<NSAccessibility>)objB;
  BOOL isARow, isBRow;

  isARow = [objA isKindOfClass:[ACAccessibilityTreeRowElement class]];
  isBRow = [objB isKindOfClass:[ACAccessibilityTreeRowElement class]];

  if ((isARow && isBRow) || (!isARow && !isBRow)) {
    NSInteger idxA = [a accessibilityIndex];
    NSInteger idxB = [b accessibilityIndex];

    if (idxA < idxB) {
      return NSOrderedAscending;
    } else if (idxA > idxB) {
      return NSOrderedDescending;
    } else {
      return NSOrderedSame;
    }
  } else {
    return isARow ? NSOrderedAscending : NSOrderedDescending;
  }
}

static void
sort_child_elements (GailTreeView *gailView)
{
  NSAccessibilityElement *treeElement = ac_element_get_accessibility_element (AC_ELEMENT (gailView));
  NSMutableArray *children = [[treeElement accessibilityChildren] mutableCopy];

  [children sortUsingFunction:row_column_index_sort context:NULL];
  [treeElement setAccessibilityChildren:children];
}

static void
gather_visible_rows_and_columns (GailTreeView *gailView)
{
  id<NSAccessibility> element = (id<NSAccessibility>)ac_element_get_accessibility_element (AC_ELEMENT (gailView));
  NSArray *children = [element accessibilityChildren];

  NSMutableArray *rows, *columns;
  rows = [[NSMutableArray alloc] init];
  columns = [[NSMutableArray alloc] init];

  NSInteger rowIdx = 0;
  for (id<NSAccessibility> e in children) {
    if ([e isKindOfClass:[ACAccessibilityTreeRowElement class]]) {
      [e setAccessibilityIndex:rowIdx];
      [rows addObject:e];
      rowIdx++;
    } else if ([e isKindOfClass:[ACAccessibilityTreeColumnElement class]]) {
      [columns addObject:e];
    } else {
      g_warning ("Unknown element in tree");
    }
  }

  [element setAccessibilityVisibleRows:rows];
  [element setAccessibilityVisibleColumns:columns];
}

static gboolean
check_row_has_data (GtkTreeModel *treeModel,
                    GtkTreeIter *rowIter)
{
  int idx, nCols;

  nCols = gtk_tree_model_get_n_columns (treeModel);
  for (idx = 0; idx < nCols; idx++) {
    GValue value = G_VALUE_INIT;

    // Check if the row has any data set yet. This function may be called when the row is inserted,
    // but before any data has been set. This checks if the value is empty before continuing.
    // Checking for peek_pointer being NULL helps to check for an empty value if the value is set from
    // managed code, in which case G_VALUE_TYPE will be a GtkSharpValue.
    gtk_tree_model_get_value (treeModel, rowIter, idx, &value);
    gpointer p = g_value_fits_pointer (&value) ? g_value_peek_pointer (&value) : NULL;
    if (G_VALUE_TYPE (&value) == 0 || p == NULL) {
      g_value_unset (&value);
      continue;
    }

    g_value_unset (&value);
    return TRUE;
  }

  return FALSE;
}

static gboolean
make_renderer_cells_for_column (GailTreeView *gailView,
                                ACAccessibilityTreeCellElement *treeCellElement,
                                ACAccessibilityTreeRowElement *rowElement,
                                GtkTreeViewColumn *column,
                                GtkTreeIter *rowIter,
                                GtkTreePath *path)
{
  ACAccessibilityElement *parentElement = ac_element_get_accessibility_element (AC_ELEMENT (gailView));
  GtkTreeView *treeView = GTK_TREE_VIEW (ac_element_get_owner (AC_ELEMENT (gailView)));
  GtkTreeModel *model = gailView->tree_model;
  GList *renderers, *r;
  AtkRegistry *default_registry = atk_get_default_registry ();
  int i;
  gboolean rowHasData = check_row_has_data (model, rowIter);
  gboolean isExpanderColumn = (gtk_tree_view_get_expander_column (treeView) == column);
  gboolean is_expanded = FALSE;

  renderers = gtk_cell_layout_get_cells (GTK_CELL_LAYOUT (column));

  if (isExpanderColumn) {
    is_expanded = gtk_tree_view_row_expanded (treeView, path);
  }

  for (r = renderers, i = 0; r; r = r->next, i++) {
    gboolean editable;
    GtkCellRenderer *renderer = GTK_CELL_RENDERER (r->data);
    AtkObjectFactory *factory;
    AtkObject *child;
    GailCell *gailCell;
    GailRendererCell *renderer_cell;
    NSAccessibilityElement *renderer_element = NULL;

    if (GTK_IS_CELL_RENDERER_TEXT (renderer)) {
      g_object_get (G_OBJECT (renderer), "editable", &editable, NULL);
    }

    factory = atk_registry_get_factory (default_registry,
                                        G_OBJECT_TYPE (renderer));

    child = atk_object_factory_create_accessible (factory,
                                                  G_OBJECT (renderer));
    if (child == NULL) {
      continue;
    }

    GType childType = G_OBJECT_TYPE (child);
    if (childType == GAIL_TYPE_RENDERER_CELL) {
      // If we have an unknown cell, then try to use it as a text cell
      g_object_unref (child);
      child = gail_text_cell_new ();
    }

    if (!GAIL_IS_RENDERER_CELL (child)) {
      continue;
    }

    gailCell = GAIL_CELL (child);

    // Set the parent as far as ATK understands to the GtkTreeView
    // because ATK doesn't understand the NSAccessibility parents for the AxCell
    // or AxRow
    gail_cell_initialise (gailCell,
                          GTK_WIDGET (treeView), ATK_OBJECT (gailView),
                          rowElement, column, i);

    id<NSAccessibility> realElement = renderer_element ?: gail_cell_get_real_cell (gailCell);
    [realElement setAccessibilityWindow:[parentElement accessibilityWindow]];
    [realElement setAccessibilityTopLevelUIElement:[parentElement accessibilityWindow]];
    [treeCellElement accessibilityAddChildElement:renderer_element ?: (NSAccessibilityElement *) gail_cell_get_real_cell (gailCell)];

    // Attach the NSAccessibility element for the cell to the cell renderer, so that a custom data function
    // is able to fill in the appropriate attributes
    g_object_set_data (G_OBJECT (renderer), "xamarin-private-atkcocoa-nsaccessibility", (__bridge gpointer) gail_cell_get_real_cell (gailCell));

    if (rowHasData && GAIL_IS_RENDERER_CELL (child) && GAIL_RENDERER_CELL (child)->renderer) {
      GailRendererCell *renderer_cell = GAIL_RENDERER_CELL (child);
      GtkCellRendererClass *gtk_cell_renderer_class = GTK_CELL_RENDERER_GET_CLASS (renderer_cell->renderer);
      GailRendererCellClass *gail_renderer_cell_class = GAIL_RENDERER_CELL_GET_CLASS (child);
      char **prop_list = gail_renderer_cell_class->property_list;

      gtk_tree_view_column_cell_set_cell_data (column, model, rowIter, isExpanderColumn, is_expanded);

      while (*prop_list) {
        GParamSpec *spec;

        spec = g_object_class_find_property (G_OBJECT_GET_CLASS (renderer), *prop_list);

        if (spec != NULL) {
          GValue value = { 0, };

          g_value_init (&value, spec->value_type);
          g_object_get_property (G_OBJECT (renderer), *prop_list, &value);
          g_object_set_property (G_OBJECT (renderer_cell->renderer),
                                  *prop_list, &value);
          g_value_unset(&value);
        } else {
          // Invalid properties are quite common with managed types because we don't know the parent type
          // so everything is defaulting to GtkCellRendererText at the moment.
          //g_warning ("Invalid property: %s\n", *prop_list);
        }
        prop_list++;
      }

      gail_renderer_cell_update_cache (renderer_cell, FALSE);
    }
  }

  g_list_free (renderers);

  return TRUE;
}

static NSAccessibilityElement *
make_accessibility_cell_for_column (GtkTreeModel *treeModel,
                                    GtkTreeView *treeView,
                                    GailTreeView *gailView,
                                    GtkTreeIter *rowIter,
                                    GtkTreeViewColumn *column,
                                    ACAccessibilityTreeRowElement *rowElement,
                                    ACAccessibilityTreeColumnElement *columnElement)
{
  id<NSAccessibility> parentElement;
  ACAccessibilityTreeCellElement *cell;
  GtkTreePath *path = gtk_tree_model_get_path (treeModel, rowIter);
  gboolean isExpanderColumn = (gtk_tree_view_get_expander_column (treeView) == column);
  gboolean is_expanded = FALSE;
  gboolean needs_disclosure = FALSE;
  GtkTreeSelection *selection;

  parentElement = ac_element_get_accessibility_element (AC_ELEMENT (gailView));

  if (isExpanderColumn && gtk_tree_model_iter_has_child (treeModel, rowIter)) {
    needs_disclosure = TRUE;
  }

  cell = [[ACAccessibilityTreeCellElement alloc] initWithDelegate:AC_ELEMENT (gailView) withDisclosureButton:needs_disclosure];
  [cell setAccessibilityParent:[parentElement accessibilityWindow]];
  [cell setAccessibilityTopLevelUIElement:[parentElement accessibilityWindow]];
  [cell addToRow:rowElement column:columnElement];

  if (isExpanderColumn) {
    is_expanded = gtk_tree_view_row_expanded (treeView, path);
  }

  // If the row is selected, the cell is selected, but the contents are not
  selection = gtk_tree_view_get_selection (treeView);
  if (gtk_tree_selection_path_is_selected (selection, path)) {
    [cell setAccessibilitySelected:YES];
  }

  make_renderer_cells_for_column (gailView, cell, rowElement, column, rowIter, path);

  gtk_tree_path_free (path);

  return cell;
}

static ACAccessibilityTreeRowElement *
find_row_element_for_path (GailTreeView *gailView,
                           GtkTreePath *path)
{
  ACAccessibilityTreeRowElement *row = get_row_from_row_map (gailView, path);
  return row;
}

static ACAccessibilityTreeColumnElement *
find_column_element_for_column (GailTreeView *gailView,
                                GtkTreeViewColumn *column)
{
  return (__bridge ACAccessibilityTreeColumnElement *)g_hash_table_lookup (gailView->columnMap, column);
}

static gboolean
update_accessibility_rows_idle (gpointer data)
{
  GailTreeView *gailView = (GailTreeView *)data;

  gailView->rowUpdateId = 0;

  NSArray *rows = [ROOT_NODE (gailView) flattenTree];
  id <NSAccessibility> parentElement = ac_element_get_accessibility_element (AC_ELEMENT (gailView));

  [parentElement setAccessibilityRows:rows];
  [parentElement setAccessibilityVisibleRows:rows];

  return FALSE;
}

static void
update_accessibility_rows (GailTreeView *gailView)
{
  if (gailView->rowUpdateId > 0) {
    return;
  }

  gailView->rowUpdateId = g_idle_add (update_accessibility_rows_idle, gailView);
}

static NSAccessibilityElement *
make_accessibility_element_for_row (GtkTreeModel *treeModel,
                                    GtkTreeView *treeView,
                                    GailTreeView *gailView,
                                    GtkTreeIter  *rowIter)
{
  id<NSAccessibility> parentElement;
  ACAccessibilityTreeRowElement *rowElement;
  GtkTreeRowReference *rowRef;
  GtkTreePath *rowPath;
  GList *columns, *c;

  parentElement = ac_element_get_accessibility_element (AC_ELEMENT (gailView));

  rowPath = gtk_tree_model_get_path (treeModel, rowIter);
  rowRef = gtk_tree_row_reference_new (treeModel, rowPath);
  rowElement = [[ACAccessibilityTreeRowElement alloc] initWithDelegate:AC_ELEMENT (gailView) treeRow:rowRef treeView:treeView];
  gtk_tree_path_free (rowPath);

  columns = gtk_tree_view_get_columns (treeView);
  for (c = columns; c; c = c->next) {
    ACAccessibilityTreeColumnElement *columnElement = find_column_element_for_column (gailView, c->data);

    if (columnElement == nil) {
      g_warning ("No column element found for column %s", gtk_tree_view_column_get_title (c->data));
      continue;
    }

    make_accessibility_cell_for_column (treeModel, treeView, gailView, rowIter, c->data, rowElement, columnElement);
  }
  g_list_free (columns);

  return rowElement;
}

static void
update_expandability (GtkTreeView *tree_view,
                      GtkTreeModel *tree_model,
                      GailTreeView *gailview,
                      GtkTreePath *path)
{
  GtkTreeViewColumn *expandColumn = gtk_tree_view_get_expander_column (tree_view);
  ACAccessibilityTreeColumnElement *columnElement = find_column_element_for_column (gailview, expandColumn);
  ACAccessibilityTreeRowElement *rowElement = find_row_element_for_path (gailview, path);
  NSArray *columnChildren = [columnElement accessibilityChildren];
  ACAccessibilityTreeCellElement *cell = NULL;
  GtkTreeIter iter;
  gboolean hasChildren;

  if (!gtk_tree_model_get_iter (tree_model, &iter, path)) {
    return;
  }

  hasChildren = gtk_tree_model_iter_has_child (tree_model, &iter);

  for (id<NSAccessibility> child in columnChildren) {
    if ([child accessibilityParent] == rowElement) {
      cell = child;
      break;
    }
  }

  if (cell == NULL) {
    return;
  }

  if (hasChildren) {
    [cell addDisclosureButton];
  } else {
    [cell removeDisclosureButton];
  }
}

static gboolean
check_visibility (GtkTreeModel *tree_model,
                  GtkTreeIter *iter,
                  GtkTreeView *view)
{
  GtkTreeIter parentIter;
  GtkTreePath *path;
  gboolean expanded;

  // If iter has no parent, then it is the root node and is visible
  if (!gtk_tree_model_iter_parent (tree_model, &parentIter, iter)) {
    return TRUE;
  }

  path = gtk_tree_model_get_path (tree_model, &parentIter);
  expanded = gtk_tree_view_row_expanded (view, path);
  gtk_tree_path_free (path);

  return expanded && check_visibility (tree_model, &parentIter, view);
}

static gboolean
model_row_is_visible (GtkTreeModel *tree_model,
                      GtkTreePath *path,
                      GtkTreeIter *iter,
                      GtkTreeView *view)
{
  return check_visibility (tree_model, iter, view);
}

static void
model_row_inserted (GtkTreeModel *tree_model,
                    GtkTreePath  *path, 
                    GtkTreeIter  *iter, 
                    gpointer     user_data)
{
  GtkTreeView *tree_view = (GtkTreeView *)user_data;
  GtkTreePath *path_copy;
  AtkObject *atk_obj = gtk_widget_get_accessible (GTK_WIDGET (tree_view));
  GailTreeView *gailview = GAIL_TREE_VIEW (atk_obj);
  gint n_inserted, child_row;

  AC_NOTE (TREEWIDGET, g_print ("Model row inserted: %s\n", gtk_tree_path_to_string (path)));

  /* Check to see if row is visible */
 /*
  * A row insert is not necessarily visible.  For example,
  * a row can be draged & dropped into another row, which
  * causes an insert on the model that isn't visible in the
  * view.  Only generate a signal if the inserted row is
  * visible.
  */
  if (model_row_is_visible (tree_model, path, iter, tree_view))
    {
      GtkTreeIter iter;
      NSAccessibilityElement *element, *parentElement;
      gint n_cols, col;
      int row = -1;

      AC_NOTE (TREEWIDGET, g_print ("Row is visible\n"));
      gtk_tree_model_get_iter (tree_model, &iter, path);

      element = make_accessibility_element_for_row (tree_model, tree_view, gailview, &iter);
      add_row_to_tree(gailview, (ACAccessibilityTreeRowElement *)element, NO);
    }
  else
    {
     /*
      * The row has been inserted inside another row.  This can
      * cause a row that previously couldn't be expanded to now
      * be expandable.
      */
      AC_NOTE(TREEWIDGET, g_print ("Not visible, maybe expand\n"));
      path_copy = gtk_tree_path_copy (path);
      gtk_tree_path_up (path_copy);

      update_expandability (tree_view, tree_model, gailview, path_copy);

      gtk_tree_path_free (path_copy);
    }

  NSAccessibilityPostNotification(ac_element_get_accessibility_element(AC_ELEMENT (atk_obj)), NSAccessibilityRowCountChangedNotification);
}

static void
model_row_deleted (GtkTreeModel *tree_model,
                   GtkTreePath  *path, 
                   gpointer     user_data)
{
  GtkTreeView *tree_view;
  GtkTreePath *path_copy;
  AtkObject *atk_obj;
  GailTreeView *gailview;
  ACAccessibilityElement *treeElement;
  ACAccessibilityTreeRowElement *rowElement;
  gint row, col, n_cols;

  tree_view = (GtkTreeView *)user_data;
  atk_obj = gtk_widget_get_accessible (GTK_WIDGET (tree_view));
  gailview = GAIL_TREE_VIEW (atk_obj);

  rowElement = find_row_element_for_path (gailview, path);
  if (rowElement != NULL) {
    remove_all_children (gailview, treeElement, rowElement);

    remove_row_from_tree(gailview, rowElement);
  }

  NSAccessibilityPostNotification(ac_element_get_accessibility_element(AC_ELEMENT (atk_obj)), NSAccessibilityRowCountChangedNotification);
}

static void 
model_rows_reordered (GtkTreeModel *tree_model,
                      GtkTreePath  *path, 
                      GtkTreeIter  *iter,
                      gint         *new_order, 
                      gpointer     user_data)
{
  GtkTreeView *view = GTK_TREE_VIEW (user_data);
  GailTreeView *gailview = GAIL_TREE_VIEW (gtk_widget_get_accessible (GTK_WIDGET (view)));
  ACAccessibilityTreeRowElement *parentElement = gtk_tree_path_get_depth (path) > 0 ? find_row_element_for_path (gailview, path) : ROOT_NODE (gailview);

  if (parentElement == nil) {
    // We might get reorders for rows that aren't visible yet, so we don't care about them
    AC_NOTE (TREEWIDGET, g_print ("No reorder for %p %s - %d\n", path, path ? gtk_tree_path_to_string (path) : "<null>", gtk_tree_path_get_depth (path)));
    return;
  }

  [parentElement reorderChildrenToNewIndicies:new_order];
}

static void
adjustment_changed (GtkAdjustment *adjustment, 
                    GtkTreeView   *tree_view)
{
}

/* Misc Public */

static void
update_column_cells (GtkTreeView *tree_view,
                     GtkTreePath *path,
                     GtkTreeViewColumn *column)
{
  GtkTreeModel *tree_model;
  GtkTreeIter iter;
  gboolean is_expander, is_expanded;

  tree_model = gtk_tree_view_get_model (tree_view);

  if (!gtk_tree_model_get_iter (tree_model, &iter, path)) {
    g_warning ("Invalid iter: %s", gtk_tree_path_to_string (path));
    return;
  }

  is_expander = FALSE;
  is_expanded = FALSE;
  if (gtk_tree_model_iter_has_child (tree_model, &iter))
    {
      GtkTreeViewColumn *expander_tv;

      expander_tv = gtk_tree_view_get_expander_column (tree_view);
      if (expander_tv == column)
        {
          is_expander = TRUE;
          is_expanded = gtk_tree_view_row_expanded (tree_view, path);
        }
    } 

  gtk_tree_view_column_cell_set_cell_data (column, tree_model, &iter, is_expander, is_expanded);
}

static gboolean
cocoa_update_cell_value (GailRendererCell *renderer_cell,
                         GailTreeView     *gailview,
                         ACAccessibilityTreeRowElement *rowElement,
                         GtkTreeViewColumn *column,
                         gboolean         emit_change_signal)
{
  GtkTreeView *tree_view;
  GtkTreeModel *tree_model;
  GtkTreePath *path;
  GtkTreeIter iter;
  GList *renderers, *cur_renderer;
  GParamSpec *spec;
  GailRendererCellClass *gail_renderer_cell_class;
  GtkCellRendererClass *gtk_cell_renderer_class;
  GailCell *cell;
  gchar **prop_list;
  AtkObject *parent;
  
  gail_renderer_cell_class = GAIL_RENDERER_CELL_GET_CLASS (renderer_cell);
  if (renderer_cell->renderer)
    gtk_cell_renderer_class = GTK_CELL_RENDERER_GET_CLASS (renderer_cell->renderer);
  else
    gtk_cell_renderer_class = NULL;

  prop_list = gail_renderer_cell_class->property_list;

  cell = GAIL_CELL (renderer_cell);

  renderers = gtk_cell_layout_get_cells (GTK_CELL_LAYOUT (column));
  gail_return_val_if_fail (renderers, FALSE);

  parent = atk_object_get_parent (ATK_OBJECT (cell));
  if (!ATK_IS_OBJECT (cell)) {
    g_on_error_query (NULL);
  }

  cur_renderer = g_list_nth (renderers, cell->index);
  
  gail_return_val_if_fail (cur_renderer != NULL, FALSE);

  if (gtk_cell_renderer_class)
    {
      while (*prop_list)
        {
          spec = g_object_class_find_property
                           (G_OBJECT_GET_CLASS (cur_renderer->data), *prop_list);

          if (spec != NULL)
            {
              GValue value = { 0, };

              g_value_init (&value, spec->value_type);
              g_object_get_property (cur_renderer->data, *prop_list, &value);
              g_object_set_property (G_OBJECT (renderer_cell->renderer),
                                     *prop_list, &value);

              g_value_unset(&value);
            }
          else {
            // Silence this warning as invalid properties are quite command as we don't know the appropriate type for
            // managed types.
            // g_warning ("Invalid property: %s\n", *prop_list);
          }
          prop_list++;
        }
    }
  g_list_free (renderers);

  //return gail_renderer_cell_update_cache (renderer_cell, emit_change_signal);
  return TRUE;
}

/* Misc Private */

static void
connect_model_signals (GtkTreeView  *view,
                       GailTreeView *gailview)
{
  GObject *obj;

  obj = G_OBJECT (gailview->tree_model);
  g_signal_connect_data (obj, "row-changed",
                         (GCallback) model_row_changed, view, NULL, 0);
  g_signal_connect_data (obj, "row-inserted",
                         (GCallback) model_row_inserted, view, NULL, 
                         G_CONNECT_AFTER);
  g_signal_connect_data (obj, "row-deleted",
                         (GCallback) model_row_deleted, view, NULL, 
                         G_CONNECT_AFTER);
  g_signal_connect_data (obj, "rows-reordered",
                         (GCallback) model_rows_reordered, view, NULL, 
                         G_CONNECT_AFTER);
}

static void
disconnect_model_signals (GailTreeView *view) 
{
  GObject *obj;
  GtkWidget *widget;

  obj = G_OBJECT (view->tree_model);
  widget = GTK_ACCESSIBLE (view)->widget;
  g_signal_handlers_disconnect_by_func (obj, (gpointer) model_row_changed, widget);
  g_signal_handlers_disconnect_by_func (obj, (gpointer) model_row_inserted, widget);
  g_signal_handlers_disconnect_by_func (obj, (gpointer) model_row_deleted, widget);
  g_signal_handlers_disconnect_by_func (obj, (gpointer) model_rows_reordered, widget);
}
