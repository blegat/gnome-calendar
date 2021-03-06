/* -*- mode: c; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/*
 * gcal-week-view.c
 *
 * Copyright (C) 2012 - Erick Pérez Castellanos
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program. If not, see <http://www.gnu.org/licenses/>.
 */

#include "gcal-week-view.h"
#include "gcal-utils.h"
#include "gcal-view.h"
#include "gcal-event-widget.h"

#include <glib/gi18n.h>

#include <libecal/libecal.h>

#include <math.h>

static const double dashed [] =
{
  1.0,
  1.0
};

enum
{
  PROP_0,
  PROP_DATE
};

struct _GcalWeekViewChild
{
  GtkWidget *widget;
  gboolean   hidden_by_me;

  /* vertical index */
  gint       index;
};

typedef struct _GcalWeekViewChild GcalWeekViewChild;

struct _GcalWeekViewPrivate
{
  /**
   * This is where we keep the refs of the child widgets.
   * Every child added to the list placed in the position
   * of it corresponding cell number.
   * The cell number is calculated in _add method.
   */
  GList          *days [7];

  GdkWindow      *view_window;
  GdkWindow      *grid_window;

  GtkWidget      *vscrollbar;

  /* property */
  icaltimetype   *date;

  GdkWindow      *event_window;

  gint            clicked_cell;

  /* helpers */
  GdkRectangle   *prev_rectangle;
  GdkRectangle   *next_rectangle;
  gboolean        clicked_prev;
  gboolean        clicked_next;
};

static void           gcal_view_interface_init             (GcalViewIface  *iface);

static void           gcal_week_view_constructed           (GObject        *object);

static void           gcal_week_view_finalize              (GObject        *object);

static void           gcal_week_view_set_property          (GObject        *object,
                                                            guint           property_id,
                                                            const GValue   *value,
                                                            GParamSpec     *pspec);

static void           gcal_week_view_get_property          (GObject        *object,
                                                            guint           property_id,
                                                            GValue         *value,
                                                            GParamSpec     *pspec);

static void           gcal_week_view_realize               (GtkWidget      *widget);

static void           gcal_week_view_unrealize             (GtkWidget      *widget);

static void           gcal_week_view_size_allocate         (GtkWidget      *widget,
                                                            GtkAllocation  *allocation);

static gboolean       gcal_week_view_draw                  (GtkWidget      *widget,
                                                            cairo_t        *cr);

static gboolean       gcal_week_view_scroll_event          (GtkWidget      *widget,
                                                            GdkEventScroll *event);

static gboolean       gcal_week_view_button_press_event    (GtkWidget      *widget,
                                                            GdkEventButton *event);

static gboolean       gcal_week_view_button_release_event  (GtkWidget      *widget,
                                                            GdkEventButton *event);

static void           gcal_week_view_add                   (GtkContainer   *constainer,
                                                            GtkWidget      *widget);

static void           gcal_week_view_remove                (GtkContainer   *constainer,
                                                            GtkWidget      *widget);

static void           gcal_week_view_forall                (GtkContainer   *container,
                                                            gboolean        include_internals,
                                                            GtkCallback     callback,
                                                            gpointer        callback_data);

static void           gcal_week_view_set_date              (GcalWeekView   *view,
                                                            icaltimetype   *date);

static void           gcal_week_view_draw_header           (GcalWeekView   *view,
                                                            cairo_t        *cr,
                                                            GtkAllocation  *alloc,
                                                            GtkBorder      *padding);

static void           gcal_week_view_draw_grid_window      (GcalWeekView   *view,
                                                            cairo_t        *cr);

static gint           gcal_week_view_get_sidebar_width     (GtkWidget      *widget);

static gint           gcal_week_view_get_start_grid_y      (GtkWidget      *widget);

static void           gcal_week_view_scroll_value_changed  (GtkAdjustment  *adjusment,
                                                            gpointer        user_data);

static icaltimetype*  gcal_week_view_get_initial_date      (GcalView       *view);

static icaltimetype*  gcal_week_view_get_final_date        (GcalView       *view);

static gboolean       gcal_week_view_contains              (GcalView       *view,
                                                            icaltimetype   *date);

static void           gcal_week_view_remove_by_uuid        (GcalView       *view,
                                                            const gchar    *uuid);

static GtkWidget*     gcal_week_view_get_by_uuid           (GcalView       *view,
                                                            const gchar    *uuid);

G_DEFINE_TYPE_WITH_CODE (GcalWeekView,
                         gcal_week_view,
                         GTK_TYPE_CONTAINER,
                         G_IMPLEMENT_INTERFACE (GCAL_TYPE_VIEW,
                                                gcal_view_interface_init));

static void
gcal_week_view_class_init (GcalWeekViewClass *klass)
{
  GtkContainerClass *container_class;
  GtkWidgetClass *widget_class;
  GObjectClass *object_class;

  container_class = GTK_CONTAINER_CLASS (klass);
  container_class->add   = gcal_week_view_add;
  container_class->remove = gcal_week_view_remove;
  container_class->forall = gcal_week_view_forall;
  gtk_container_class_handle_border_width (container_class);

  widget_class = GTK_WIDGET_CLASS (klass);
  widget_class->realize = gcal_week_view_realize;
  widget_class->unrealize = gcal_week_view_unrealize;
  widget_class->size_allocate = gcal_week_view_size_allocate;
  widget_class->draw = gcal_week_view_draw;
  widget_class->scroll_event = gcal_week_view_scroll_event;
  widget_class->button_press_event = gcal_week_view_button_press_event;
  widget_class->button_release_event = gcal_week_view_button_release_event;

  object_class = G_OBJECT_CLASS (klass);
  object_class->constructed = gcal_week_view_constructed;
  object_class->finalize = gcal_week_view_finalize;
  object_class->set_property = gcal_week_view_set_property;
  object_class->get_property = gcal_week_view_get_property;

  g_object_class_override_property (object_class, PROP_DATE, "active-date");

  g_type_class_add_private ((gpointer)klass, sizeof (GcalWeekViewPrivate));
}



static void
gcal_week_view_init (GcalWeekView *self)
{
  GcalWeekViewPrivate *priv;
  gint i;

  gtk_widget_set_has_window (GTK_WIDGET (self), FALSE);

  self->priv = G_TYPE_INSTANCE_GET_PRIVATE (self,
                                            GCAL_TYPE_WEEK_VIEW,
                                            GcalWeekViewPrivate);
  priv = self->priv;

  priv->prev_rectangle = NULL;
  priv->next_rectangle = NULL;

  priv->clicked_prev = FALSE;
  priv->clicked_next = FALSE;

  for (i = 0; i < 7; i++)
    {
      priv->days[i] = NULL;
    }

  priv->view_window = NULL;
  priv->grid_window = NULL;

  gtk_style_context_add_class (
      gtk_widget_get_style_context (GTK_WIDGET (self)),
      "calendar-view");
}

static void
gcal_view_interface_init (GcalViewIface *iface)
{
  iface->get_initial_date = gcal_week_view_get_initial_date;
  iface->get_final_date = gcal_week_view_get_final_date;

  iface->contains = gcal_week_view_contains;
  iface->remove_by_uuid = gcal_week_view_remove_by_uuid;
  iface->get_by_uuid = gcal_week_view_get_by_uuid;
}

static void
gcal_week_view_constructed (GObject *object)
{
  GcalWeekViewPrivate *priv;

  g_return_if_fail (GCAL_IS_WEEK_VIEW (object));
  priv = GCAL_WEEK_VIEW (object)->priv;

  if (G_OBJECT_CLASS (gcal_week_view_parent_class)->constructed != NULL)
      G_OBJECT_CLASS (gcal_week_view_parent_class)->constructed (object);

  gtk_widget_push_composite_child ();
  priv->vscrollbar = gtk_scrollbar_new (GTK_ORIENTATION_VERTICAL, NULL);
  gtk_widget_pop_composite_child ();

  gtk_widget_set_parent (priv->vscrollbar, GTK_WIDGET (object));
  gtk_widget_show (priv->vscrollbar);

  g_signal_connect (gtk_range_get_adjustment (GTK_RANGE (priv->vscrollbar)),
                    "value-changed",
                    G_CALLBACK (gcal_week_view_scroll_value_changed),
                    object);

  g_object_ref (priv->vscrollbar);
}

static void
gcal_week_view_finalize (GObject       *object)
{
  GcalWeekViewPrivate *priv = GCAL_WEEK_VIEW (object)->priv;

  if (priv->date != NULL)
    g_free (priv->date);

  if (priv->vscrollbar != NULL)
    {
      gtk_widget_unparent (priv->vscrollbar);
      g_clear_object (&(priv->vscrollbar));
    }

  if (priv->prev_rectangle != NULL)
    g_free (priv->prev_rectangle);
  if (priv->next_rectangle != NULL)
    g_free (priv->next_rectangle);

  /* Chain up to parent's finalize() method. */
  G_OBJECT_CLASS (gcal_week_view_parent_class)->finalize (object);
}

static void
gcal_week_view_set_property (GObject       *object,
                             guint          property_id,
                             const GValue  *value,
                             GParamSpec    *pspec)
{
  g_return_if_fail (GCAL_IS_WEEK_VIEW (object));

  switch (property_id)
    {
    case PROP_DATE:
      gcal_week_view_set_date (
          GCAL_WEEK_VIEW (object),
          g_value_dup_boxed (value));
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
      break;
    }
}

static void
gcal_week_view_get_property (GObject       *object,
                              guint          property_id,
                              GValue        *value,
                              GParamSpec    *pspec)
{
  GcalWeekViewPrivate *priv;

  g_return_if_fail (GCAL_IS_WEEK_VIEW (object));
  priv = GCAL_WEEK_VIEW (object)->priv;

  switch (property_id)
    {
    case PROP_DATE:
      g_value_set_boxed (value, priv->date);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
      break;
    }
}

static void
gcal_week_view_realize (GtkWidget *widget)
{
  GcalWeekViewPrivate *priv;
  GdkWindow *parent_window;
  GdkWindowAttr attributes;
  gint attributes_mask;
  GtkAllocation allocation;

  gint i;
  GList *l;

  priv = GCAL_WEEK_VIEW (widget)->priv;
  gtk_widget_set_realized (widget, TRUE);

  gtk_widget_get_allocation (widget, &allocation);

  parent_window = gtk_widget_get_parent_window (widget);
  gtk_widget_set_window (widget, parent_window);

  attributes.window_type = GDK_WINDOW_CHILD;
  attributes.wclass = GDK_INPUT_ONLY;
  attributes.x = allocation.x;
  attributes.y = allocation.y;
  attributes.event_mask = gtk_widget_get_events (widget);
  attributes.event_mask |= (GDK_BUTTON_PRESS_MASK |
                            GDK_BUTTON_RELEASE_MASK);
  attributes_mask = GDK_WA_X | GDK_WA_Y;

  priv->event_window = gdk_window_new (parent_window,
                                       &attributes,
                                       attributes_mask);
  gdk_window_set_user_data (priv->event_window, widget);

  attributes.window_type = GDK_WINDOW_CHILD;
  attributes.wclass = GDK_INPUT_OUTPUT;
  attributes.x = allocation.x;
  attributes.y = allocation.y;
  attributes.event_mask = gtk_widget_get_events (widget);
  attributes.event_mask |= (GDK_EXPOSURE_MASK |
                            GDK_BUTTON_PRESS_MASK |
                            GDK_BUTTON_RELEASE_MASK |
                            GDK_BUTTON_MOTION_MASK |
                            GDK_POINTER_MOTION_MASK |
                            GDK_SCROLL_MASK |
                            GDK_TOUCH_MASK |
                            GDK_SMOOTH_SCROLL_MASK);
  attributes.visual = gtk_widget_get_visual (widget);
  attributes_mask = GDK_WA_X | GDK_WA_Y | GDK_WA_VISUAL;

  priv->view_window = gdk_window_new (parent_window,
                                      &attributes,
                                      attributes_mask);
  gdk_window_set_user_data (priv->view_window, widget);

  attributes.event_mask = gtk_widget_get_events (widget);
  attributes.event_mask |= GDK_EXPOSURE_MASK;
  priv->grid_window = gdk_window_new (priv->view_window,
                                      &attributes,
                                      attributes_mask);
  gdk_window_set_user_data (priv->grid_window, widget);

  gdk_window_show (priv->event_window);
  gdk_window_show (priv->grid_window);
  gdk_window_show (priv->view_window);

  /* setting parent_window for every event child */
  for (i = 0; i < 7; i++)
    {
      for (l = priv->days[i]; l != NULL; l = l->next)
        {
          GcalWeekViewChild *child;

          child = (GcalWeekViewChild*) l->data;
          gtk_widget_set_parent_window (child->widget, priv->grid_window);
        }
    }
}

static void
gcal_week_view_unrealize (GtkWidget *widget)
{
  GcalWeekViewPrivate *priv;

  priv = GCAL_WEEK_VIEW (widget)->priv;
  if (priv->view_window != NULL)
    {
      gdk_window_set_user_data (priv->view_window, NULL);
      gdk_window_destroy (priv->view_window);
      priv->view_window = NULL;
    }

  if (priv->grid_window != NULL)
    {
      gdk_window_set_user_data (priv->grid_window, NULL);
      gdk_window_destroy (priv->grid_window);
      priv->grid_window = NULL;
    }
  if (priv->event_window != NULL)
    {
      gdk_window_set_user_data (priv->event_window, NULL);
      gdk_window_destroy (priv->event_window);
      priv->event_window = NULL;
    }

  GTK_WIDGET_CLASS (gcal_week_view_parent_class)->unrealize (widget);
}

static void
gcal_week_view_size_allocate (GtkWidget     *widget,
                              GtkAllocation *allocation)
{
  GcalWeekViewPrivate *priv;

  GtkBorder padding;
  PangoLayout *layout;
  PangoFontDescription *font_desc;
  gint font_height;

  gint start_grid_y;
  gint sidebar_width;

  GtkAllocation scroll_allocation;
  gint min;
  gint natural;

  gdouble horizontal_block;
  gdouble vertical_block;
  gdouble adj_value;

  gint i;
  GList *l;

  gboolean scroll_needed;
  gint grid_height;
  gint view_height;

  priv = GCAL_WEEK_VIEW (widget)->priv;

  gtk_widget_set_allocation (widget, allocation);

  if (! gtk_widget_get_realized (widget))
    return;

  gtk_style_context_get_padding (
      gtk_widget_get_style_context (widget),
      gtk_widget_get_state_flags (widget),
      &padding);

  start_grid_y = gcal_week_view_get_start_grid_y (widget);

  gdk_window_move_resize (priv->event_window,
                          allocation->x,
                          allocation->y,
                          allocation->width,
                          start_grid_y);

  /* Estimating cell height */
  layout = pango_layout_new (gtk_widget_get_pango_context (widget));
  gtk_style_context_get (gtk_widget_get_style_context (widget),
                         gtk_widget_get_state_flags (widget),
                         "font", &font_desc,
                         NULL);
  pango_layout_set_font_description (layout, font_desc);
  pango_layout_get_pixel_size (layout, NULL, &font_height);
  pango_font_description_free (font_desc);

  gtk_widget_get_preferred_width (priv->vscrollbar, &min, &natural);

  view_height = allocation->height - start_grid_y;
  grid_height = 24 * (padding.top + padding.bottom + 3 * font_height);
  scroll_needed = grid_height > allocation->height - start_grid_y ? TRUE : FALSE;
  grid_height = scroll_needed ? grid_height : allocation->height - start_grid_y;

  if (scroll_needed)
    {
      adj_value = gtk_adjustment_get_value (
          gtk_range_get_adjustment (GTK_RANGE (priv->vscrollbar)));
    }
  else
    {
      adj_value = 0;
    }

  gdk_window_move_resize (priv->view_window,
                          allocation->x,
                          allocation->y + start_grid_y,
                          allocation->width,
                          allocation->height - start_grid_y);
  gdk_window_move_resize (priv->grid_window,
                          allocation->x,
                          - adj_value,
                          allocation->width,
                          grid_height);

  if (scroll_needed)
    {
      /* FIXME: change those values for something not hardcoded */
      scroll_allocation.x = allocation->width - 10;
      scroll_allocation.y = 7 + start_grid_y + 2;
      scroll_allocation.width = 8;
      scroll_allocation.height = view_height - 7 - 2;
      gtk_widget_size_allocate (priv->vscrollbar, &scroll_allocation);

      gtk_adjustment_set_page_size (
          gtk_range_get_adjustment (GTK_RANGE (priv->vscrollbar)),
          view_height * view_height / grid_height);
      gtk_adjustment_set_upper (
          gtk_range_get_adjustment (GTK_RANGE (priv->vscrollbar)),
          grid_height - view_height + (view_height * view_height / grid_height));
    }
  else
    {
      gtk_widget_hide (priv->vscrollbar);
    }

  sidebar_width = gcal_week_view_get_sidebar_width (widget);
  horizontal_block = (allocation->width - sidebar_width) / 7;
  vertical_block = gdk_window_get_height (priv->grid_window) / 24;

  for (i = 0; i < 7; i++)
    {
      gdouble *added_height;
      added_height = g_malloc0 (sizeof (gdouble) * 25);

      for (l = priv->days[i]; l != NULL; l = l->next)
        {
          GcalWeekViewChild *child;
          gint min_height;
          gint natural_height;
          gdouble vertical_span;
          GtkAllocation child_allocation;

          child = (GcalWeekViewChild*) l->data;

          if ((! gtk_widget_get_visible (child->widget)) && (! child->hidden_by_me))
            continue;

          gtk_widget_get_preferred_height (child->widget,
                                           &min_height,
                                           &natural_height);

          child_allocation.x = i * horizontal_block + sidebar_width;
          if (child->index == -1)
            {
              vertical_span = 2 * font_height;
              child_allocation.y = start_grid_y - vertical_span;
            }
          else
            {
              vertical_span = vertical_block;
              child_allocation.y = child->index * vertical_block;
            }

          child_allocation.width = horizontal_block;
          child_allocation.height = MIN (natural_height, vertical_span);
          if (added_height[child->index + 1] + child_allocation.height > vertical_span)
            {
              gtk_widget_hide (child->widget);
              child->hidden_by_me = TRUE;
            }
          else
            {
              gtk_widget_show (child->widget);
              child->hidden_by_me = FALSE;
              child_allocation.y = child_allocation.y + added_height[child->index + 1];
              gtk_widget_size_allocate (child->widget, &child_allocation);
              added_height[child->index + 1] += child_allocation.height;
            }
        }
      g_free (added_height);
    }
}

static gboolean
gcal_week_view_draw (GtkWidget *widget,
                     cairo_t   *cr)
{
  GcalWeekViewPrivate *priv;
  GtkStyleContext *context;
  GtkStateFlags state;
  GtkBorder padding;
  GtkAllocation alloc;

  g_return_val_if_fail (GCAL_IS_WEEK_VIEW (widget), FALSE);
  priv = GCAL_WEEK_VIEW (widget)->priv;

  context = gtk_widget_get_style_context (widget);
  state = gtk_widget_get_state_flags (widget);

  /* getting padding and allocation */
  gtk_style_context_get_padding (context, state, &padding);
  gtk_widget_get_allocation (widget, &alloc);

  if (gtk_cairo_should_draw_window (cr, gtk_widget_get_window (widget)))
    {
      cairo_save (cr);
      cairo_set_line_width (cr, 0.3);
      gcal_week_view_draw_header (GCAL_WEEK_VIEW (widget),
                                  cr,
                                  &alloc,
                                  &padding);
      cairo_restore (cr);
    }

  if (priv->view_window != NULL &&
      gtk_cairo_should_draw_window (cr, priv->view_window))
    {
      gint y;

      gdk_window_get_position (priv->view_window, NULL, &y);
      cairo_rectangle (cr,
                       alloc.x,
                       y,
                       alloc.width,
                       gdk_window_get_height (priv->view_window));
      cairo_clip (cr);
    }

  if (priv->grid_window != NULL &&
      gtk_cairo_should_draw_window (cr, priv->grid_window))
    {
      cairo_save (cr);
      cairo_set_line_width (cr, 0.3);
      gcal_week_view_draw_grid_window (GCAL_WEEK_VIEW (widget), cr);
      cairo_restore (cr);
    }

  GTK_WIDGET_CLASS (gcal_week_view_parent_class)->draw (widget, cr);

  return FALSE;
}

static gboolean
gcal_week_view_scroll_event (GtkWidget      *widget,
                             GdkEventScroll *event)
{
  GcalWeekViewPrivate *priv;

  gdouble delta_y;
  gdouble delta;

  g_return_val_if_fail (GCAL_IS_WEEK_VIEW (widget), FALSE);
  g_return_val_if_fail (event != NULL, FALSE);

  priv = GCAL_WEEK_VIEW (widget)->priv;

  if (gdk_event_get_scroll_deltas ((GdkEvent *) event, NULL, &delta_y))
    {
      if (delta_y != 0.0 &&
          priv->vscrollbar  != NULL &&
          gtk_widget_get_visible (priv->vscrollbar))
        {
          GtkAdjustment *adj;
          gdouble new_value;
          gdouble page_size;
          gdouble scroll_unit;

          adj = gtk_range_get_adjustment (GTK_RANGE (priv->vscrollbar));
          page_size = gtk_adjustment_get_page_size (adj);
          scroll_unit = pow (page_size, 2.0 / 3.0);

          new_value = CLAMP (gtk_adjustment_get_value (adj) + delta_y * scroll_unit,
                             gtk_adjustment_get_lower (adj),
                             gtk_adjustment_get_upper (adj) -
                             gtk_adjustment_get_page_size (adj));

          gtk_adjustment_set_value (adj, new_value);

          return TRUE;
        }
    }
  else
    {
      GtkWidget *range = NULL;

      if (event->direction == GDK_SCROLL_UP || event->direction == GDK_SCROLL_DOWN)
        range = priv->vscrollbar;

      if (range != NULL && gtk_widget_get_visible (range))
        {
          GtkAdjustment *adj;
          gdouble new_value;
          gdouble page_size;

          adj = gtk_range_get_adjustment (GTK_RANGE (range));
          page_size = gtk_adjustment_get_page_size (adj);
          delta = pow (page_size, 2.0 / 3.0);

          new_value = CLAMP (gtk_adjustment_get_value (adj) + delta,
                             gtk_adjustment_get_lower (adj),
                             gtk_adjustment_get_upper (adj) -
                             gtk_adjustment_get_page_size (adj));

          gtk_adjustment_set_value (adj, new_value);

          return TRUE;
        }
    }

  return FALSE;
}

static gboolean
gcal_week_view_button_press_event (GtkWidget      *widget,
                                   GdkEventButton *event)
{
  GcalWeekViewPrivate *priv;
  gdouble x, y;
  gdouble start_grid_y;

  priv = GCAL_WEEK_VIEW (widget)->priv;

  x = event->x;
  y = event->y;

  start_grid_y = gcal_week_view_get_start_grid_y (widget);

  if (y - start_grid_y < 0)
    {
      if (priv->prev_rectangle->x < x &&
          x < priv->prev_rectangle->x + priv->prev_rectangle->width &&
          priv->prev_rectangle->y < y &&
          y < priv->prev_rectangle->y + priv->prev_rectangle->height)
        {
          priv->clicked_prev = TRUE;
        }
      else if (priv->next_rectangle->x < x &&
          x < priv->next_rectangle->x + priv->next_rectangle->width &&
          priv->next_rectangle->y < y &&
          y < priv->next_rectangle->y + priv->next_rectangle->height)
        {
          priv->clicked_next = TRUE;
        }

      return TRUE;
    }

  return FALSE;
}

static gboolean
gcal_week_view_button_release_event (GtkWidget      *widget,
                                     GdkEventButton *event)
{
  GcalWeekViewPrivate *priv;
  gdouble x, y;
  gdouble start_grid_y;

  priv = GCAL_WEEK_VIEW (widget)->priv;

  x = event->x;
  y = event->y;

  start_grid_y = gcal_week_view_get_start_grid_y (widget);

  if (y - start_grid_y < 0)
    {
      if (priv->prev_rectangle->x < x &&
          x < priv->prev_rectangle->x + priv->prev_rectangle->width &&
          priv->prev_rectangle->y < y &&
          y < priv->prev_rectangle->y + priv->prev_rectangle->height &&
          priv->clicked_prev)
        {
          icaltimetype *prev_week;
          prev_week = gcal_view_get_date (GCAL_VIEW (widget));
          icaltime_adjust (prev_week, - 7, 0, 0, 0);

          g_signal_emit_by_name (GCAL_VIEW (widget), "updated", prev_week);

          g_free (prev_week);
        }
      else if (priv->next_rectangle->x < x &&
          x < priv->next_rectangle->x + priv->next_rectangle->width &&
          priv->next_rectangle->y < y &&
          y < priv->next_rectangle->y + priv->next_rectangle->height &&
          priv->clicked_next)
        {
          icaltimetype *next_week;
          next_week = gcal_view_get_date (GCAL_VIEW (widget));

          icaltime_adjust (next_week, 7, 0, 0, 0);
          g_signal_emit_by_name (GCAL_VIEW (widget), "updated", next_week);

          g_free (next_week);
        }

      priv->clicked_cell = -1;
      priv->clicked_prev = FALSE;
      priv->clicked_next = FALSE;
      return TRUE;
    }

  priv->clicked_cell = -1;
  priv->clicked_prev = FALSE;
  priv->clicked_next = FALSE;
  return FALSE;
}

static void
gcal_week_view_add (GtkContainer *container,
                     GtkWidget    *widget)
{
  GcalWeekViewPrivate *priv;
  GList *l;

  gint day;
  icaltimetype *date;

  GcalWeekViewChild *new_child;

  g_return_if_fail (GCAL_IS_WEEK_VIEW (container));
  g_return_if_fail (GCAL_IS_EVENT_WIDGET (widget));
  g_return_if_fail (gtk_widget_get_parent (widget) == NULL);
  priv = GCAL_WEEK_VIEW (container)->priv;

  /* Check if it's already added for date */
  date = gcal_event_widget_get_date (GCAL_EVENT_WIDGET (widget));
  day = icaltime_day_of_week (*date);

  for (l = priv->days[day - 1]; l != NULL; l = l->next)
    {
      GcalWeekViewChild *child;

      child = (GcalWeekViewChild*) l->data;

      if (g_strcmp0 (
            gcal_event_widget_peek_uuid (GCAL_EVENT_WIDGET (widget)),
            gcal_event_widget_peek_uuid (GCAL_EVENT_WIDGET (child->widget))) == 0)
        {
          //TODO: remove once the main-dev phase its over
          g_warning ("Trying to add an event with the same uuid to the view");
          return;
        }
    }

  new_child = g_new0 (GcalWeekViewChild, 1);
  new_child->widget = widget;
  new_child->hidden_by_me = FALSE;

  if (gcal_event_widget_get_all_day (GCAL_EVENT_WIDGET (widget)))
    {
      new_child->index = -1;
      if (gtk_widget_get_window (widget) != NULL)
        gtk_widget_set_parent_window (widget, gtk_widget_get_window (widget));
    }
  else
    {
      new_child->index = date->hour;
      if (priv->grid_window != NULL)
        gtk_widget_set_parent_window (widget, priv->grid_window);
    }

  priv->days[day - 1] = g_list_append (priv->days[day - 1], new_child);
  gtk_widget_set_parent (widget, GTK_WIDGET (container));

  g_free (date);
}

static void
gcal_week_view_remove (GtkContainer *container,
                        GtkWidget    *widget)
{
  GcalWeekViewPrivate *priv;
  GList *l;
  icaltimetype *date;
  gint day;
  gboolean was_visible;

  g_return_if_fail (GCAL_IS_WEEK_VIEW (container));
  g_return_if_fail (gtk_widget_get_parent (widget) == GTK_WIDGET (container));
  priv = GCAL_WEEK_VIEW (container)->priv;

  date = gcal_event_widget_get_date (GCAL_EVENT_WIDGET (widget));
  day = icaltime_day_of_week (*date);

  for (l = priv->days[day - 1]; l != NULL; l = l->next)
    {
      GcalWeekViewChild *child;

      child = (GcalWeekViewChild*) l->data;
      if (child->widget == widget)
        {
          priv->days[day - 1] = g_list_remove (priv->days[day - 1], child);
          g_free (child);
          break;
        }
    }


  was_visible = gtk_widget_get_visible (widget);
  gtk_widget_unparent (widget);

  if (was_visible)
    gtk_widget_queue_resize (GTK_WIDGET (container));

  g_free (date);
}

static void
gcal_week_view_forall (GtkContainer *container,
                        gboolean      include_internals,
                        GtkCallback   callback,
                        gpointer      callback_data)
{
  GcalWeekViewPrivate *priv;
  gint i;
  GList *l;

  priv = GCAL_WEEK_VIEW (container)->priv;

  for (i = 0; i < 7; i++)
    {
      l = priv->days[i];

      while (l)
        {
          GcalWeekViewChild *child;

          child = (GcalWeekViewChild*) l->data;
          l  = l->next;

          (* callback) (child->widget, callback_data);
        }
    }

  if (include_internals && priv->vscrollbar != NULL)
    (* callback) (priv->vscrollbar, callback_data);
}

static void
gcal_week_view_set_date (GcalWeekView *view,
                         icaltimetype *date)
{
  GcalWeekViewPrivate *priv;
  gboolean will_resize;

  gint i;
  GList *l;
  GList *to_remove;

  priv = view->priv;
  will_resize = FALSE;

  /* if span_updated: queue_resize */
  will_resize = ! gcal_view_contains (GCAL_VIEW (view), date);

  if (priv->date != NULL)
    g_free (priv->date);

  priv->date = date;

  if (will_resize)
    {
      to_remove = NULL;

      for (i = 0; i < 7; i++)
        {
          for (l = priv->days[i]; l != NULL; l = l->next)
            {
              GcalWeekViewChild *child;
              icaltimetype *child_date;

              child = (GcalWeekViewChild*) l->data;
              child_date = gcal_event_widget_get_date (GCAL_EVENT_WIDGET (child->widget));
              if (! gcal_view_contains (GCAL_VIEW (view), child_date))
                to_remove = g_list_append (to_remove, child->widget);
            }
        }
      g_list_foreach (to_remove, (GFunc) gtk_widget_destroy, NULL);

      gtk_widget_queue_resize (GTK_WIDGET (view));
    }
}

static void
gcal_week_view_draw_header (GcalWeekView  *view,
                            cairo_t       *cr,
                            GtkAllocation *alloc,
                            GtkBorder     *padding)
{
  GcalWeekViewPrivate *priv;
  GtkWidget *widget;
  GtkStyleContext *context;
  GtkStateFlags state;
  GdkRGBA color;
  GtkBorder header_padding;

  PangoLayout *layout;
  PangoFontDescription *bold_font;
  PangoFontDescription *font_desc;
  gint header_width;
  gint layout_width;
  gint layout_height;

  gchar *left_header;
  gchar *right_header;
  gchar start_date[64];
  gchar end_date[64];

  gint i;
  gint start_grid_y;
  gint font_height;
  gint sidebar_width;
  icaltimetype *start_of_week;
  icaltimetype *end_of_week;
  struct tm tm_date;

  cairo_pattern_t *pattern;

  GtkIconTheme *icon_theme;
  GdkPixbuf *pixbuf;

  priv = view->priv;
  widget = GTK_WIDGET (view);

  cairo_save (cr);
  start_grid_y = gcal_week_view_get_start_grid_y (widget);
  context = gtk_widget_get_style_context (widget);
  state = gtk_widget_get_state_flags (widget);

  /* adding shadow */
  pattern = cairo_pattern_create_linear(0, start_grid_y - 18, 0, start_grid_y + 6);

  cairo_pattern_add_color_stop_rgba(pattern, 0.0, 0, 0, 0, 0.6);
  cairo_pattern_add_color_stop_rgba(pattern, 1.0, 0, 0, 0, 0.0);

  cairo_set_source(cr, pattern);
  cairo_pattern_destroy(pattern);

  cairo_rectangle(cr, 0, start_grid_y, alloc->width, 6);
  cairo_fill(cr);

  gtk_style_context_save (context);
  gtk_style_context_add_region (context, "header", 0);

  gtk_style_context_get_padding (context, state, &header_padding);
  gtk_style_context_get_color (context, state, &color);
  cairo_set_source_rgb (cr, color.red, color.green, color.blue);

  layout = pango_cairo_create_layout (cr);
  gtk_style_context_get (context, state, "font", &font_desc, NULL);
  pango_layout_set_font_description (layout, font_desc);

  /* Here translators should put the widgest letter in their alphabet, this
   * taken to make it align with week-view header, which is the larger for now */
  pango_layout_set_text (layout, _("WWW 99 - WWW 99"), -1);
  pango_cairo_update_layout (cr, layout);
  pango_layout_get_pixel_size (layout, &layout_width, &layout_height);

  start_of_week = gcal_week_view_get_initial_date (GCAL_VIEW (view));
  tm_date = icaltimetype_to_tm (start_of_week);
  e_utf8_strftime_fix_am_pm (start_date, 64, "%b %d", &tm_date);

  end_of_week = gcal_week_view_get_final_date (GCAL_VIEW (view));
  tm_date = icaltimetype_to_tm (end_of_week);
  e_utf8_strftime_fix_am_pm (end_date, 64, "%b %d", &tm_date);

  left_header = g_strdup_printf ("%s - %s", start_date, end_date);
  right_header = g_strdup_printf ("%s %d, %d",
                                  _("Week"),
                                  icaltime_week_number (*start_of_week) + 1,
                                  priv->date->year);

  pango_layout_set_text (layout, left_header, -1);
  pango_cairo_update_layout (cr, layout);
  pango_layout_get_pixel_size (layout, &header_width, NULL);

  cairo_move_to (cr,
                 alloc->x + header_padding.left + ((layout_width - header_width) / 2),
                 alloc->y + header_padding.top);
  pango_cairo_show_layout (cr, layout);

  /* Drawing arrows */
  icon_theme = gtk_icon_theme_get_default ();
  pixbuf = gtk_icon_theme_load_icon (icon_theme,
                                     "go-previous-symbolic",
                                     layout_height,
                                     0,
                                     NULL);

  gdk_cairo_set_source_pixbuf (cr,
                               pixbuf,
                               alloc->x + layout_width + 2 * header_padding.left,
                               alloc->y + header_padding.top);
  g_object_unref (pixbuf);
  cairo_paint (cr);

  pixbuf = gtk_icon_theme_load_icon (icon_theme,
                                     "go-next-symbolic",
                                     layout_height,
                                     0,
                                     NULL);

  gdk_cairo_set_source_pixbuf (cr,
                               pixbuf,
                               alloc->x + layout_width + 2 * header_padding.left + layout_height,
                               alloc->y + header_padding.top);
  g_object_unref (pixbuf);
  cairo_paint (cr);

  /* allocating rects */
  if (priv->prev_rectangle == NULL)
    priv->prev_rectangle = g_new0 (GdkRectangle, 1);
  priv->prev_rectangle->x = alloc->x + layout_width + 2 * header_padding.left;
  priv->prev_rectangle->y = alloc->y + header_padding.top;
  priv->prev_rectangle->width = layout_height;
  priv->prev_rectangle->height = layout_height;

  if (priv->next_rectangle == NULL)
    priv->next_rectangle = g_new0 (GdkRectangle, 1);
  priv->next_rectangle->x = alloc->x + layout_width + 2 * header_padding.left + layout_height;
  priv->next_rectangle->y = alloc->y + header_padding.top;
  priv->next_rectangle->width = layout_height;
  priv->next_rectangle->height = layout_height;

  gtk_style_context_get_color (context,
                               state | GTK_STATE_FLAG_INSENSITIVE,
                               &color);
  cairo_set_source_rgb (cr, color.red, color.green, color.blue);

  pango_layout_set_text (layout, right_header, -1);
  pango_cairo_update_layout (cr, layout);
  pango_layout_get_pixel_size (layout, &layout_width, NULL);

  cairo_move_to (cr,
                 alloc->width - header_padding.right - layout_width,
                 alloc->y + header_padding.top);
  pango_cairo_show_layout (cr, layout);

  gtk_style_context_restore (context);

  /* grid header */
  gtk_style_context_get_color (context, state, &color);
  cairo_set_source_rgb (cr, color.red, color.green, color.blue);

  gtk_style_context_get (context, state, "font", &bold_font, NULL);
  pango_font_description_set_weight (bold_font, PANGO_WEIGHT_SEMIBOLD);
  pango_layout_set_font_description (layout, bold_font);

  sidebar_width = gcal_week_view_get_sidebar_width (widget);
  for (i = 0; i < 7; i++)
    {
      gchar *weekday_header;
      gchar *weekday_abv;
      gint n_day;

      n_day = start_of_week->day + i;
      if (n_day > icaltime_days_in_month (start_of_week->month, start_of_week->year))
        n_day = n_day - icaltime_days_in_month (start_of_week->month, start_of_week->year);

      weekday_abv = gcal_get_weekday (i);
      weekday_header = g_strdup_printf ("%s %d",weekday_abv, n_day);

      pango_layout_set_text (layout, weekday_header, -1);
      pango_cairo_update_layout (cr, layout);
      pango_layout_get_pixel_size (layout, NULL, &font_height);
      cairo_move_to (cr,
                     padding->left + ((alloc->width  - sidebar_width)/ 7) * i + sidebar_width,
                     start_grid_y - padding->bottom - font_height - (2 * font_height));
      pango_cairo_show_layout (cr, layout);

      cairo_save (cr);
      gtk_style_context_get_color (context,
                                   state | GTK_STATE_FLAG_INSENSITIVE,
                                   &color);
      cairo_set_source_rgb (cr, color.red, color.green, color.blue);

      cairo_move_to (cr,
                     ((alloc->width  - sidebar_width)/ 7) * i + sidebar_width + 0.4,
                     start_grid_y - (2 * font_height));
      cairo_rel_line_to (cr, 0, 2 * font_height);
      cairo_stroke (cr);
      cairo_restore (cr);

      g_free (weekday_header);
    }

  /* horizontal line */
  gtk_style_context_get_color (context,
                               state | GTK_STATE_FLAG_INSENSITIVE,
                               &color);
  cairo_set_source_rgb (cr, color.red, color.green, color.blue);
  cairo_move_to (cr, sidebar_width, start_grid_y - (2 * font_height) + 0.4);
  cairo_rel_line_to (cr, alloc->width - sidebar_width, 0);

  cairo_stroke (cr);

  cairo_restore (cr);

  g_free (start_of_week);
  g_free (end_of_week);
  g_free (left_header);
  g_free (right_header);
  pango_font_description_free (bold_font);
  pango_font_description_free (font_desc);
  g_object_unref (layout);
}

static void
gcal_week_view_draw_grid_window (GcalWeekView  *view,
                                 cairo_t       *cr)
{
  GcalWeekViewPrivate *priv;
  GtkWidget *widget;

  GtkStyleContext *context;
  GtkStateFlags state;
  GtkBorder padding;
  GdkRGBA color;

  gint i;
  gint width;
  gint height;
  gint sidebar_width;

  PangoLayout *layout;
  PangoFontDescription *font_desc;

  priv = view->priv;
  widget = GTK_WIDGET (view);

  /* INSENSITIVE to make the lines ligther */
  context = gtk_widget_get_style_context (widget);
  state = gtk_widget_get_state_flags (widget);
  state |= GTK_STATE_FLAG_INSENSITIVE;
  gtk_style_context_get_color (context, state, &color);

  state = gtk_widget_get_state_flags (widget);
  gtk_style_context_get_padding (context, state, &padding);

  layout = pango_cairo_create_layout (cr);
  gtk_style_context_get (context, state, "font", &font_desc, NULL);
  pango_layout_set_font_description (layout, font_desc);
  cairo_set_source_rgb (cr, color.red, color.green, color.blue);

  sidebar_width = gcal_week_view_get_sidebar_width (widget);
  width = gdk_window_get_width (priv->grid_window);
  height = gdk_window_get_height (priv->grid_window);

  gtk_cairo_transform_to_window (cr, widget, priv->grid_window);

  /* grid, sidebar hours */
  for (i = 0; i < 24; i++)
    {
      gchar *hours;
      hours = g_strdup_printf ("%d %s", i % 12, i < 12 ? _("AM") : _("PM"));

      if (i == 0)
        pango_layout_set_text (layout, _("Midnight"), -1);
      else if (i == 12)
        pango_layout_set_text (layout, _("Noon"), -1);
      else
        pango_layout_set_text (layout, hours, -1);

      pango_cairo_update_layout (cr, layout);
      cairo_move_to (cr, padding.left, padding.top + (height / 24) * i);
      pango_cairo_show_layout (cr, layout);

      g_free (hours);
    }

  /* grid, vertical lines first */
  for (i = 0; i < 7; i++)
    {
      cairo_move_to (cr,
                     sidebar_width + ((width - sidebar_width) / 7) * i + 0.4,
                     0);
      cairo_rel_line_to (cr, 0, height);
    }

  /* rest of the lines */
  for (i = 0; i < 24; i++)
    {
      /* hours lines */
      cairo_move_to (cr, 0, (height / 24) * i + 0.4);
      cairo_rel_line_to (cr, width, 0);
    }

  cairo_stroke (cr);

  cairo_set_dash (cr, dashed, 2, 0);
  for (i = 0; i < 24; i++)
    {
      /* 30 minutes lines */
      cairo_move_to (cr, sidebar_width, (height / 24) * i + (height / 48) + 0.4);
      cairo_rel_line_to (cr, width - sidebar_width, 0);
    }

  cairo_stroke (cr);

  pango_font_description_free (font_desc);
  g_object_unref (layout);
}

static gint
gcal_week_view_get_sidebar_width (GtkWidget *widget)
{
  GtkStyleContext *context;
  GtkBorder padding;

  PangoLayout *layout;
  PangoFontDescription *font_desc;
  gint mid_width;
  gint noon_width;
  gint sidebar_width;

  context = gtk_widget_get_style_context (widget);

  gtk_style_context_get_padding (
      gtk_widget_get_style_context (widget),
      gtk_widget_get_state_flags (widget),
      &padding);

  layout = pango_layout_new (gtk_widget_get_pango_context (widget));
  gtk_style_context_get (context,
                         gtk_widget_get_state_flags(widget),
                         "font", &font_desc,
                         NULL);
  pango_layout_set_font_description (layout, font_desc);

  pango_layout_set_text (layout, _("Midnight"), -1);
  pango_layout_get_pixel_size (layout, &mid_width, NULL);

  pango_layout_set_text (layout, _("Noon"), -1);
  pango_layout_get_pixel_size (layout, &noon_width, NULL);
  sidebar_width = noon_width > mid_width ? noon_width : mid_width;
  sidebar_width += padding.left + padding.right;

  pango_font_description_free (font_desc);
  g_object_unref (layout);

  return sidebar_width;
}

/**
 * gcal_week_view_get_start_grid_y:
 *
 * In GcalMonthView this method returns the height of the headers of the view
 * and the grid. Here this points just the place where the grid_window hides
 * behind the header
 * Here this height includes:
 *  - The big header of the view
 *  - The grid header dislaying weekdays
 *  - The cell containing all-day events.
 */
static gint
gcal_week_view_get_start_grid_y (GtkWidget *widget)
{
  GtkStyleContext *context;
  GtkBorder padding;
  GtkBorder header_padding;

  PangoLayout *layout;
  PangoFontDescription *font_desc;
  gint font_height;
  gdouble start_grid_y;

  context = gtk_widget_get_style_context (widget);
  layout = pango_layout_new (gtk_widget_get_pango_context (widget));

  /* init header values */
  gtk_style_context_save (context);
  gtk_style_context_add_region (context, "header", 0);

  gtk_style_context_get_padding (gtk_widget_get_style_context (widget),
                                 gtk_widget_get_state_flags (widget),
                                 &header_padding);

  gtk_style_context_get (context,
                         gtk_widget_get_state_flags(widget),
                         "font", &font_desc,
                         NULL);
  pango_layout_set_font_description (layout, font_desc);
  pango_layout_get_pixel_size (layout, NULL, &font_height);
  pango_font_description_free (font_desc);

  /* 6: is padding around the header */
  start_grid_y = header_padding.top + font_height + header_padding.bottom;
  gtk_style_context_restore (context);

  /* init grid values */
  gtk_style_context_get (context,
                         gtk_widget_get_state_flags(widget),
                         "font", &font_desc,
                         NULL);
  pango_layout_set_font_description (layout, font_desc);
  pango_layout_get_pixel_size (layout, NULL, &font_height);
  pango_font_description_free (font_desc);

  gtk_style_context_get_padding (gtk_widget_get_style_context (widget),
                                 gtk_widget_get_state_flags (widget),
                                 &padding);

  start_grid_y += padding.top + font_height;

  /* for including the all-day cells */
  start_grid_y += 2 * font_height;

  g_object_unref (layout);
  return start_grid_y;
}

static void
gcal_week_view_scroll_value_changed (GtkAdjustment *adjusment,
                                     gpointer       user_data)
{
  GcalWeekViewPrivate *priv;

  g_return_if_fail (GCAL_IS_WEEK_VIEW (user_data));
  priv = GCAL_WEEK_VIEW (user_data)->priv;

  if (priv->grid_window != NULL)
    {
      gdk_window_move (priv->grid_window,
                       0,
                       - gtk_adjustment_get_value (adjusment));
    }
}

/* GcalView Interface API */
/**
 * gcal_week_view_get_initial_date:
 *
 * Since: 0.1
 * Return value: the first day of the month
 * Returns: (transfer full): Release with g_free
 **/
static icaltimetype*
gcal_week_view_get_initial_date (GcalView *view)
{
  GcalWeekViewPrivate *priv;
  icaltimetype *new_date;

  g_return_val_if_fail (GCAL_IS_WEEK_VIEW (view), NULL);
  priv = GCAL_WEEK_VIEW (view)->priv;
  new_date = g_new0 (icaltimetype, 1);
  *new_date = icaltime_from_day_of_year (
      icaltime_day_of_year (*(priv->date)) - icaltime_day_of_week (*(priv->date)) + 1,
      priv->date->year);

  return new_date;
}

/**
 * gcal_week_view_get_final_date:
 *
 * Since: 0.1
 * Return value: the last day of the month
 * Returns: (transfer full): Release with g_free
 **/
static icaltimetype*
gcal_week_view_get_final_date (GcalView *view)
{
  GcalWeekViewPrivate *priv;
  icaltimetype *new_date;

  g_return_val_if_fail (GCAL_IS_WEEK_VIEW (view), NULL);
  priv = GCAL_WEEK_VIEW (view)->priv;
  new_date = g_new0 (icaltimetype, 1);
  *new_date = icaltime_from_day_of_year (
      icaltime_day_of_year (*(priv->date)) + 7 - icaltime_day_of_week (*(priv->date)),
      priv->date->year);

  return new_date;
}

static gboolean
gcal_week_view_contains (GcalView     *view,
                         icaltimetype *date)
{
  GcalWeekViewPrivate *priv;

  g_return_val_if_fail (GCAL_IS_WEEK_VIEW (view), FALSE);
  priv = GCAL_WEEK_VIEW (view)->priv;

  if (priv->date == NULL)
    return FALSE;
  if (icaltime_week_number (*(priv->date)) == icaltime_week_number (*date)
      && priv->date->year == date->year)
    {
      return TRUE;
    }
  else
    {
      return FALSE;
    }
}

static void
gcal_week_view_remove_by_uuid (GcalView    *view,
                                const gchar *uuid)
{
  GcalWeekViewPrivate *priv;
  gint i;
  GList *l;

  g_return_if_fail (GCAL_IS_WEEK_VIEW (view));
  priv = GCAL_WEEK_VIEW (view)->priv;

  for (i = 0; i < 7; i++)
    {
      for (l = priv->days[i]; l != NULL; l = l->next)
        {
          GcalWeekViewChild *child;
          const gchar* widget_uuid;

          child = (GcalWeekViewChild*) l->data;
          widget_uuid = gcal_event_widget_peek_uuid (GCAL_EVENT_WIDGET (child->widget));
          if (g_strcmp0 (uuid, widget_uuid) == 0)
            {
              gtk_widget_destroy (child->widget);
              i = 8;
              break;
            }
        }
    }
}

static GtkWidget*
gcal_week_view_get_by_uuid (GcalView    *view,
                            const gchar *uuid)
{
  GcalWeekViewPrivate *priv;
  gint i;
  GList *l;

  g_return_val_if_fail (GCAL_IS_WEEK_VIEW (view), NULL);
  priv = GCAL_WEEK_VIEW (view)->priv;

  for (i = 0; i < 7; i++)
    {
      for (l = priv->days[i]; l != NULL; l = l->next)
        {
          GcalWeekViewChild *child;
          const gchar* widget_uuid;

          child = (GcalWeekViewChild*) l->data;
          widget_uuid = gcal_event_widget_peek_uuid (GCAL_EVENT_WIDGET (child->widget));
          if (g_strcmp0 (uuid, widget_uuid) == 0)
            return child->widget;
        }
    }
  return NULL;
}

/* Public API */
/**
 * gcal_week_view_new:
 * @date:
 *
 * Since: 0.1
 * Return value: the new month view widget
 * Returns: (transfer full):
 **/
GtkWidget*
gcal_week_view_new (void)
{
  return g_object_new (GCAL_TYPE_WEEK_VIEW, NULL);
}
