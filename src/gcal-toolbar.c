/* -*- mode: c; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/*
 * gcal-toolbar.c
 * Copyright (C) 2012 Erick Pérez Castellanos <erickpc@gnome.org>
 *
 * gnome-calendar is free software: you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the
 * Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * gnome-calendar is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include "gcal-toolbar.h"
#include "gcal-utils.h"

#include <glib/gi18n.h>

struct _GcalToolbarPrivate
{
  GtkWidget           *widget;

  GtkToolItem         *left_item;
  GtkToolItem         *central_item;
  GtkToolItem         *right_item;

  /* overview widgets */
  GtkWidget           *add_button;

  GtkWidget           *views_box;

  GtkWidget           *right_box;
  GtkWidget           *sources_button;
  GtkWidget           *search_button;

  /* events widgets */
  GtkWidget           *back_button;
  GtkWidget           *edit_button;

  GtkWidget           *active_view_button;
};

enum
{
  /* From overview mode */
  VIEW_CHANGED = 1,
  SOURCES_SHOWN,
  ADD_EVENT,
  SEARCH_EVENTS,

  BACK,
  EDIT_EVENT,
  DONE_EDIT,

  NUM_SIGNALS
};

static guint signals[NUM_SIGNALS] = { 0, };

static void gcal_toolbar_constructed            (GObject      *object);

static void gcal_toolbar_finalize               (GObject      *object);

static void gcal_toolbar_clear                  (GcalToolbar  *toolbar);

static void gcal_toolbar_set_overview_mode      (GcalToolbar  *toolbar);

static void gcal_toolbar_set_event_mode         (GcalToolbar  *toolbar);

static void gcal_toolbar_view_changed           (GtkWidget    *button,
                                                 gpointer      user_data);

static void gcal_toolbar_sources_shown          (GtkWidget    *button,
                                                 gpointer      user_data);

static void gcal_toolbar_add_event              (GtkWidget    *button,
                                                 gpointer      user_data);

static void gcal_toolbar_search_events          (GtkWidget    *button,
                                                 gpointer      user_data);

static void gcal_toolbar_back_clicked           (GtkWidget    *button,
                                                 gpointer      user_data);

static void gcal_toolbar_event_edited           (GtkWidget    *button,
                                                 gpointer      user_data);

G_DEFINE_TYPE (GcalToolbar, gcal_toolbar, GTK_CLUTTER_TYPE_ACTOR);

static void
gcal_toolbar_class_init (GcalToolbarClass *klass)
{
  GObjectClass *object_class;

  object_class = G_OBJECT_CLASS (klass);

  object_class->constructed = gcal_toolbar_constructed;
  object_class->finalize = gcal_toolbar_finalize;

  signals[VIEW_CHANGED] = g_signal_new ("view-changed",
                                        GCAL_TYPE_TOOLBAR,
                                        G_SIGNAL_RUN_LAST,
                                        G_STRUCT_OFFSET (GcalToolbarClass,
                                                         view_changed),
                                        NULL, NULL,
                                        g_cclosure_marshal_VOID__UINT,
                                        G_TYPE_NONE,
                                        1,
                                        G_TYPE_UINT);

  signals[SOURCES_SHOWN] = g_signal_new ("sources-shown",
                                         GCAL_TYPE_TOOLBAR,
                                         G_SIGNAL_RUN_LAST,
                                         G_STRUCT_OFFSET (GcalToolbarClass,
                                                          sources_shown),
                                         NULL, NULL,
                                         g_cclosure_marshal_VOID__BOOLEAN,
                                         G_TYPE_NONE,
                                         1,
                                         G_TYPE_BOOLEAN);

  signals[ADD_EVENT] = g_signal_new ("add-event",
                                     GCAL_TYPE_TOOLBAR,
                                     G_SIGNAL_RUN_LAST,
                                     G_STRUCT_OFFSET (GcalToolbarClass,
                                                      add_event),
                                     NULL, NULL,
                                     g_cclosure_marshal_VOID__VOID,
                                     G_TYPE_NONE,
                                     0);

  signals[SEARCH_EVENTS] = g_signal_new ("search-events",
                                         GCAL_TYPE_TOOLBAR,
                                         G_SIGNAL_RUN_LAST,
                                         G_STRUCT_OFFSET (GcalToolbarClass,
                                                          search_events),
                                         NULL, NULL,
                                         g_cclosure_marshal_VOID__VOID,
                                         G_TYPE_NONE,
                                         0);

  signals[BACK] = g_signal_new ("back",
                                GCAL_TYPE_TOOLBAR,
                                G_SIGNAL_RUN_LAST,
                                G_STRUCT_OFFSET (GcalToolbarClass, back),
                                NULL, NULL,
                                g_cclosure_marshal_VOID__VOID,
                                G_TYPE_NONE,
                                0);

  signals[EDIT_EVENT] = g_signal_new ("edit-event",
                                      GCAL_TYPE_TOOLBAR,
                                      G_SIGNAL_RUN_LAST,
                                      G_STRUCT_OFFSET (GcalToolbarClass,
                                                       edit_event),
                                      NULL, NULL,
                                      g_cclosure_marshal_VOID__VOID,
                                      G_TYPE_NONE,
                                      0);

  signals[DONE_EDIT] = g_signal_new ("done-edit",
                                     GCAL_TYPE_TOOLBAR,
                                     G_SIGNAL_RUN_LAST,
                                     G_STRUCT_OFFSET (GcalToolbarClass,
                                                      done_edit),
                                     NULL, NULL,
                                     g_cclosure_marshal_VOID__VOID,
                                     G_TYPE_NONE,
                                     0);

  g_type_class_add_private ((gpointer) klass, sizeof(GcalToolbarPrivate));
}

static void
gcal_toolbar_init (GcalToolbar *self)
{
  self->priv = G_TYPE_INSTANCE_GET_PRIVATE (self,
                                            GCAL_TYPE_TOOLBAR,
                                            GcalToolbarPrivate);
}

static void
gcal_toolbar_constructed (GObject *object)
{
  GcalToolbarPrivate *priv;

  GtkStyleContext *context;
  GtkToolItem *spacer;
  GtkWidget *bin;

  priv = GCAL_TOOLBAR (object)->priv;
  if (G_OBJECT_CLASS (gcal_toolbar_parent_class)->constructed != NULL)
    G_OBJECT_CLASS (gcal_toolbar_parent_class)->constructed (object);

  priv->widget = gtk_toolbar_new ();
  gtk_widget_set_hexpand (priv->widget, TRUE);
  gtk_widget_set_vexpand (priv->widget, TRUE);
  gtk_toolbar_set_icon_size (GTK_TOOLBAR (priv->widget),
                             GTK_ICON_SIZE_BUTTON);

  context = gtk_widget_get_style_context (priv->widget);
  gtk_style_context_add_class (context, "main-toolbar");

  /* adding toolbar */
  bin = gtk_clutter_actor_get_widget (GTK_CLUTTER_ACTOR (object));
  gtk_container_add (GTK_CONTAINER (bin), priv->widget);

  /* adding sections */
  /* left */
  priv->left_item = gtk_tool_item_new ();
  gtk_toolbar_insert (GTK_TOOLBAR (priv->widget), priv->left_item, 0);

  /* spacer */
  spacer = gtk_tool_item_new ();
  gtk_tool_item_set_expand (spacer, TRUE);
  gtk_toolbar_insert (GTK_TOOLBAR (priv->widget), spacer, -1);

  /* central */
  priv->central_item = gtk_tool_item_new ();
  gtk_toolbar_insert (GTK_TOOLBAR (priv->widget), priv->central_item, -1);

  /* spacer */
  spacer = gtk_tool_item_new ();
  gtk_tool_item_set_expand (spacer, TRUE);
  gtk_toolbar_insert (GTK_TOOLBAR (priv->widget), spacer, -1);

  /* right */
  priv->right_item = gtk_tool_item_new ();
  gtk_toolbar_insert (GTK_TOOLBAR (priv->widget), priv->right_item, -1);

  gcal_toolbar_set_overview_mode (GCAL_TOOLBAR (object));
  gtk_widget_show_all (bin);
}

static void
gcal_toolbar_finalize (GObject *object)
{
  GcalToolbarPrivate *priv;

  g_return_if_fail (GCAL_IS_TOOLBAR (object));
  priv = GCAL_TOOLBAR (object)->priv;

  g_clear_object (&(priv->add_button));
  g_clear_object (&(priv->views_box));
  g_clear_object (&(priv->right_box));

  g_clear_object (&(priv->back_button));
  g_clear_object (&(priv->edit_button));

  G_OBJECT_CLASS (gcal_toolbar_parent_class)->finalize (object);
}

static void
gcal_toolbar_clear (GcalToolbar *toolbar)
{
  GcalToolbarPrivate *priv;
  GtkWidget *child;

  g_return_if_fail (GCAL_IS_TOOLBAR (toolbar));
  priv = toolbar->priv;

  if ((child = gtk_bin_get_child (GTK_BIN (priv->left_item))) != NULL)
    gtk_container_remove (GTK_CONTAINER (priv->left_item), child);

  if ((child = gtk_bin_get_child (GTK_BIN (priv->central_item))) != NULL)
    gtk_container_remove (GTK_CONTAINER (priv->central_item), child);

  if ((child = gtk_bin_get_child (GTK_BIN (priv->right_item))) != NULL)
  gtk_container_remove (GTK_CONTAINER (priv->right_item), child);
}

static void
gcal_toolbar_set_overview_mode (GcalToolbar *toolbar)
{
  GcalToolbarPrivate *priv;
  GtkStyleContext *context;
  GtkWidget *button;

  g_return_if_fail (GCAL_IS_TOOLBAR (toolbar));
  priv = toolbar->priv;

  /* add */
  if (priv->add_button == NULL)
    {
      priv->add_button = gtk_button_new_with_label (_("New Event"));
      g_object_ref_sink (priv->add_button);

      gtk_widget_set_size_request (priv->add_button, 100, -1);
      context = gtk_widget_get_style_context (priv->add_button);
      gtk_style_context_add_class (context, "suggested-action");

      g_signal_connect (priv->add_button,
                        "clicked",
                        G_CALLBACK (gcal_toolbar_add_event),
                        toolbar);
    }
  gtk_container_add (GTK_CONTAINER (priv->left_item), priv->add_button);
  gtk_widget_show_all (priv->add_button);

  /* views_box */
  if (priv->views_box == NULL)
    {
      priv->views_box = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 0);
      g_object_ref_sink (priv->views_box);
      gtk_widget_set_hexpand (priv->views_box, TRUE);

      context = gtk_widget_get_style_context (priv->views_box);
      gtk_style_context_add_class (context, "linked");

      /* day */
      button = gtk_toggle_button_new_with_label (_("Day"));
      gtk_widget_set_size_request (button, 80, -1);

      context = gtk_widget_get_style_context (button);
      gtk_style_context_add_class (context, "raised");

      gtk_container_add (GTK_CONTAINER (priv->views_box), button);

      g_object_set_data (G_OBJECT (button),
                        "view-type",
                        GUINT_TO_POINTER (GCAL_WINDOW_VIEW_DAY));
      g_signal_connect (button,
                        "toggled",
                        G_CALLBACK (gcal_toolbar_view_changed),
                        toolbar);

      /* week */
      button = gtk_toggle_button_new_with_label (_("Week"));
      gtk_widget_set_size_request (button, 80, -1);

      context = gtk_widget_get_style_context (button);
      gtk_style_context_add_class (context, "raised");

      gtk_container_add (GTK_CONTAINER (priv->views_box), button);

      g_object_set_data (G_OBJECT (button),
                        "view-type",
                        GUINT_TO_POINTER (GCAL_WINDOW_VIEW_WEEK));
      g_signal_connect (button,
                        "toggled",
                        G_CALLBACK (gcal_toolbar_view_changed),
                        toolbar);

      /* month */
      button = gtk_toggle_button_new_with_label (_("Month"));
      gtk_widget_set_size_request (button, 80, -1);

      context = gtk_widget_get_style_context (button);
      gtk_style_context_add_class (context, "raised");

      gtk_container_add (GTK_CONTAINER (priv->views_box), button);

      g_object_set_data (G_OBJECT (button),
                        "view-type",
                        GUINT_TO_POINTER (GCAL_WINDOW_VIEW_MONTH));
      g_signal_connect (button,
                        "toggled",
                        G_CALLBACK (gcal_toolbar_view_changed),
                        toolbar);

      /* year */
      button = gtk_toggle_button_new_with_label (_("Year"));
      gtk_widget_set_size_request (button, 80, -1);

      context = gtk_widget_get_style_context (button);
      gtk_style_context_add_class (context, "raised");

      gtk_container_add (GTK_CONTAINER (priv->views_box), button);

      g_object_set_data (G_OBJECT (button),
                        "view-type",
                        GUINT_TO_POINTER (GCAL_WINDOW_VIEW_YEAR));
      g_signal_connect (button,
                        "toggled",
                        G_CALLBACK (gcal_toolbar_view_changed),
                        toolbar);

      /* list */
      button = gtk_toggle_button_new_with_label (_("List"));
      gtk_widget_set_size_request (button, 80, -1);

      context = gtk_widget_get_style_context (button);
      gtk_style_context_add_class (context, "raised");

      gtk_container_add (GTK_CONTAINER (priv->views_box), button);

      g_object_set_data (G_OBJECT (button),
                        "view-type",
                        GUINT_TO_POINTER (GCAL_WINDOW_VIEW_LIST));
      g_signal_connect (button,
                        "toggled",
                        G_CALLBACK (gcal_toolbar_view_changed),
                        toolbar);
    }
  gtk_container_add (GTK_CONTAINER (priv->central_item), priv->views_box);
  gtk_widget_show_all (priv->views_box);

  /* right_box */
  if (priv->right_box == NULL)
    {
      GtkWidget *child;
      priv->right_box = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 0);
      g_object_ref_sink (priv->right_box);
      gtk_widget_set_hexpand (priv->right_box, TRUE);
      gtk_box_set_spacing (GTK_BOX (priv->right_box), 6);

      /* sources_button */
      priv->sources_button = gtk_toggle_button_new ();
      child = gtk_image_new_from_icon_name ("x-office-calendar-symbolic",
                                            GTK_ICON_SIZE_MENU);
      g_object_set (child, "margin", get_icon_margin (), NULL);
      gtk_container_add (GTK_CONTAINER (priv->sources_button), child);

      context = gtk_widget_get_style_context (priv->sources_button);
      gtk_style_context_add_class (context, "raised");

      gtk_container_add (GTK_CONTAINER (priv->right_box),
                         priv->sources_button);

      g_signal_connect (priv->sources_button,
                        "clicked",
                        G_CALLBACK (gcal_toolbar_sources_shown),
                        toolbar);

      /* search_button */
      priv->search_button = gtk_button_new ();
      child = gtk_image_new_from_icon_name ("folder-saved-search-symbolic",
                                            GTK_ICON_SIZE_MENU);
      g_object_set (child, "margin", get_icon_margin (), NULL);
      gtk_container_add (GTK_CONTAINER (priv->search_button), child);

      context = gtk_widget_get_style_context (priv->search_button);
      gtk_style_context_add_class (context, "raised");

      gtk_container_add (GTK_CONTAINER (priv->right_box),
                         priv->search_button);

      g_signal_connect (priv->search_button,
                        "clicked",
                        G_CALLBACK (gcal_toolbar_search_events),
                        toolbar);
    }
  gtk_container_add (GTK_CONTAINER (priv->right_item), priv->right_box);
  gtk_widget_show_all (priv->sources_button);
}

static void
gcal_toolbar_set_event_mode (GcalToolbar *toolbar)
{
  GcalToolbarPrivate *priv;
  GtkStyleContext *context;

  g_return_if_fail (GCAL_IS_TOOLBAR (toolbar));
  priv = toolbar->priv;

  /* back */
  if (priv->back_button == NULL)
    {
      priv->back_button = gtk_button_new_with_label (_("Back"));
      g_object_ref_sink (priv->back_button);
      gtk_button_set_image (
          GTK_BUTTON (priv->back_button),
          gtk_image_new_from_icon_name ("go-previous-symbolic",
                                        GTK_ICON_SIZE_MENU));

      context = gtk_widget_get_style_context (priv->back_button);
      gtk_style_context_add_class (context, "raised");

      g_signal_connect (priv->back_button,
                        "clicked",
                        G_CALLBACK (gcal_toolbar_back_clicked),
                        toolbar);
    }
  gtk_container_add (GTK_CONTAINER (priv->left_item), priv->back_button);
  gtk_widget_show_all (priv->back_button);

  /* edit */
  if (priv->edit_button == NULL)
    {
      priv->edit_button = gtk_toggle_button_new_with_label (_("Edit"));
      g_object_ref_sink (priv->edit_button);

      context = gtk_widget_get_style_context (priv->edit_button);
      gtk_style_context_add_class (context, "raised");

      g_signal_connect (priv->edit_button,
                        "toggled",
                        G_CALLBACK (gcal_toolbar_event_edited),
                        toolbar);
    }
  /* reset morphing edit_button */
  gtk_button_set_label (GTK_BUTTON (priv->edit_button), _("Edit"));
  g_signal_handlers_block_by_func (priv->edit_button, gcal_toolbar_event_edited, toolbar);
  gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (priv->edit_button), FALSE);
  g_signal_handlers_unblock_by_func (priv->edit_button, gcal_toolbar_event_edited, toolbar);

  gtk_container_add (GTK_CONTAINER (priv->right_item), priv->edit_button);
  gtk_widget_show_all (priv->edit_button);
}

static void
gcal_toolbar_view_changed (GtkWidget *button,
                           gpointer   user_data)
{
  GcalToolbarPrivate *priv;
  guint view_type;

  priv = GCAL_TOOLBAR (user_data)->priv;

  if (priv->active_view_button == button &&
      ! gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (button)))
    {
      g_signal_handlers_block_by_func (priv->active_view_button,
                                       gcal_toolbar_view_changed,
                                       user_data);
      gtk_toggle_button_set_active (
          GTK_TOGGLE_BUTTON (priv->active_view_button),
          TRUE);
      g_signal_handlers_unblock_by_func (priv->active_view_button,
                                         gcal_toolbar_view_changed,
                                         user_data);
      return;
    }

  if (priv->active_view_button != NULL)
    {
      g_signal_handlers_block_by_func (priv->active_view_button,
                                       gcal_toolbar_view_changed,
                                       user_data);
      gtk_toggle_button_set_active (
          GTK_TOGGLE_BUTTON (priv->active_view_button),
          FALSE);
      g_signal_handlers_unblock_by_func (priv->active_view_button,
                                         gcal_toolbar_view_changed,
                                         user_data);
    }

  priv->active_view_button = button;
  view_type = GPOINTER_TO_UINT (g_object_get_data (G_OBJECT (button),
                                                   "view-type"));

  g_signal_emit (GCAL_TOOLBAR (user_data),
                 signals[VIEW_CHANGED],
                 0,
                 view_type);
}

static void
gcal_toolbar_sources_shown (GtkWidget *button,
                            gpointer   user_data)
{
  GcalToolbar *toolbar;

  toolbar = GCAL_TOOLBAR (user_data);
  g_signal_emit (toolbar,
                 signals[SOURCES_SHOWN],
                 0,
                 gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (button)));
}

static void
gcal_toolbar_add_event (GtkWidget *button,
                        gpointer   user_data)
{
  GcalToolbar *toolbar;

  toolbar = GCAL_TOOLBAR (user_data);
  g_signal_emit (toolbar, signals[ADD_EVENT], 0);
}

static void
gcal_toolbar_search_events (GtkWidget *button,
                            gpointer   user_data)
{
  GcalToolbar *toolbar;

  toolbar = GCAL_TOOLBAR (user_data);
  g_signal_emit (toolbar, signals[SEARCH_EVENTS], 0);
}

static void
gcal_toolbar_back_clicked (GtkWidget *button,
                           gpointer   user_data)
{
  GcalToolbar *toolbar;

  toolbar = GCAL_TOOLBAR (user_data);
  g_signal_emit (toolbar, signals[BACK], 0);
}

static void
gcal_toolbar_event_edited (GtkWidget *button,
                           gpointer   user_data)
{
  GcalToolbarPrivate *priv;
  GcalToolbar *toolbar;

  toolbar = GCAL_TOOLBAR (user_data);
  priv = toolbar->priv;

  /* morphing edit_button */
  if (gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (button)))
    {
      g_signal_emit (toolbar, signals[EDIT_EVENT], 0);
      gtk_button_set_label (GTK_BUTTON (priv->edit_button), _("Done"));
    }
  else
    {
      g_signal_emit (toolbar, signals[DONE_EDIT], 0);
      gtk_button_set_label (GTK_BUTTON (priv->edit_button), _("Edit"));
    }
}

/* Public API */
ClutterActor*
gcal_toolbar_new (void)
{
  return g_object_new (gcal_toolbar_get_type (), NULL);
}

void
gcal_toolbar_set_mode (GcalToolbar     *toolbar,
                       GcalToolbarMode  mode)
{
  g_return_if_fail (GCAL_IS_TOOLBAR (toolbar));
  gcal_toolbar_clear (toolbar);
  switch (mode)
    {
      case GCAL_TOOLBAR_OVERVIEW:
        gcal_toolbar_set_overview_mode (toolbar);
        return;
      case GCAL_TOOLBAR_VIEW_EVENT:
        gcal_toolbar_set_event_mode (toolbar);
    }
}

void
gcal_toolbar_set_active_view (GcalToolbar        *toolbar,
                              GcalWindowViewType  view_type)
{
  GcalToolbarPrivate *priv;
  GList *children;
  GList *l;
  guint button_view_type;

  g_return_if_fail (GCAL_IS_TOOLBAR (toolbar));
  priv = toolbar->priv;

  children = gtk_container_get_children (GTK_CONTAINER (priv->views_box));
  for (l = children; l != NULL; l = l->next)
    {
      button_view_type =
        GPOINTER_TO_UINT (g_object_get_data (G_OBJECT (l->data),
                                             "view-type"));
      if (view_type == button_view_type)
        {
          gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (l->data), TRUE);
          break;
        }
    }

  g_list_free (children);
}
