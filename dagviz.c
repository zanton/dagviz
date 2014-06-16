#include "dagviz.h"

GtkWidget *window;
GtkWidget *darea;
GtkWidget *dv_entry_radix;

/*--------Interactive processing functions------------*/

static void dv_do_zooming(double zoom_ratio, double posx, double posy)
{
  posx -= G->basex + G->x;
  posy -= G->basey + G->y;
  double deltax = posx / G->zoom_ratio * zoom_ratio - posx;
  double deltay = posy / G->zoom_ratio * zoom_ratio - posy;
  G->x -= deltax;
  G->y -= deltay;
  G->zoom_ratio = zoom_ratio;
  gtk_widget_queue_draw(darea);
}

static void dv_get_zoomfit_hoz_ratio(double w, double h, double *pz, double *px, double *py) {
  double zoom_ratio = 1.0;
  double x = 0.0;
  double y = 0.0;
  double d1, d2;
  double lw, rw;
  if (S->lt == 0) {
    // Grid-based
    lw = G->rt->vl->lc * DV_HDIS;
    rw = G->rt->vl->rc * DV_HDIS;
    d1 = lw + rw;
    d2 = w - 2 * (DV_ZOOM_TO_FIT_MARGIN + DV_RADIUS);
    if (d1 > d2)
      zoom_ratio = d2 / d1;
    x -= (rw - lw) * 0.5 * zoom_ratio;
  } else if (S->lt == 1) {
    // Bounding box
    d1 = G->rt->lw + G->rt->rw;
    d2 = w - 2 * DV_ZOOM_TO_FIT_MARGIN;
    if (d1 > d2)
      zoom_ratio = d2 / d1;
    x -= (G->rt->rw - G->rt->lw) * 0.5 * zoom_ratio;
  } else if (S->lt == 2) {
    // Timeline
    d1 = 2*DV_RADIUS + (G->nw - 1) * (2*DV_RADIUS + DV_HDIS);
    d2 = w - 2 * DV_ZOOM_TO_FIT_MARGIN;
    if (d1 > d2)
      zoom_ratio = d2 / d1;
  } else
    dv_check(0);
  *pz = zoom_ratio;
  *px = x;
  *py = y;
}

static void dv_get_zoomfit_ver_ratio(double w, double h, double *pz, double *px, double *py) {
  double zoom_ratio = 1.0;
  double x = 0.0;
  double y = 0.0;
  double d1, d2;
  double lw, rw;
  if (S->lt == 0) {
    // Grid-based
    d1 = G->rt->dc * DV_VDIS;
    d2 = h - 2 * (DV_ZOOM_TO_FIT_MARGIN + DV_RADIUS);
    if (d1 > d2)
      zoom_ratio = d2 / d1;
    lw = G->rt->vl->lc * DV_HDIS;
    rw = G->rt->vl->rc * DV_HDIS;
    x -= (rw - lw) * 0.5 * zoom_ratio;
  } else if (S->lt == 1) {
    // Bounding box
    d1 = G->rt->dw;
    d2 = h - 2 * DV_ZOOM_TO_FIT_MARGIN;
    if (d1 > d2)
      zoom_ratio = d2 / d1;    
    x -= (G->rt->rw - G->rt->lw) * 0.5 * zoom_ratio;
  } else if (S->lt == 2) {
    // Timeline
    d1 = 10 + G->rt->dw;
    d2 = h - 2 * DV_ZOOM_TO_FIT_MARGIN;
    if (d1 > d2)
      zoom_ratio = d2 / d1;
    double lrw = 2*DV_RADIUS + (G->nw - 1) * (2*DV_RADIUS + DV_HDIS);
    x += (w - lrw * zoom_ratio) * 0.5;
  } else
    dv_check(0);
  *pz = zoom_ratio;
  *px = x;
  *py = y;
}

static void dv_do_zoomfit_hoz() {
  G->x = G->y = 0.0;
  dv_get_zoomfit_hoz_ratio(S->vpw, S->vph, &G->zoom_ratio, &G->x, &G->y);
  gtk_widget_queue_draw(darea);
}

static void dv_do_zoomfit_ver() {
  dv_get_zoomfit_ver_ratio(S->vpw, S->vph, &G->zoom_ratio, &G->x, &G->y);
  gtk_widget_queue_draw(darea);
}

static void dv_do_drawing(cairo_t *cr)
{
  // First time only
  if (G->init) {
    dv_get_zoomfit_hoz_ratio(S->vpw, S->vph, &G->zoom_ratio, &G->x, &G->y);
    G->init = 0;
  }
  // Draw graph
  cairo_save(cr);
  cairo_translate(cr, G->basex + G->x, G->basey + G->y);
  cairo_scale(cr, G->zoom_ratio, G->zoom_ratio);
  dv_draw_dvdag(cr, G);
  cairo_restore(cr);

  // Draw status line
  dv_draw_status(cr);
}

static void dv_do_expanding(int e) {
  if (S->cur_d + e < 0 || S->cur_d + e > G->dmax)
    return;
  if (S->lt == 0) {
    if (!S->a->on) {
      S->a->new_d = S->cur_d + e;
      dv_animation_start(S->a);
    }
  } else if (S->lt == 2 || S->lt == 1) {
    int new_d = S->cur_d + e;
    dv_dag_node_t *node;
    int i;
    for (i=0; i<G->n; i++) {
      node = G->T + i;
      if (node->d >= new_d && !dv_is_shrinked(node)) {
        dv_node_flag_set(node->f, DV_NODE_FLAG_SHRINKED);
      } else if (node->d < new_d && dv_is_shrinked(node)) {
        dv_node_flag_remove(node->f, DV_NODE_FLAG_SHRINKED);
      }
    }
    S->cur_d = new_d;
    dv_relayout_dvdag(G);
    gtk_widget_queue_draw(darea);
  } else {
    dv_check(0);
  }
}

static dv_dag_node_t *dv_do_finding_clicked_node(double x, double y) {
  dv_dag_node_t * ret = NULL;
  int i;
  dv_dag_node_t *node;
  for (i=0; i<G->n; i++) {
    node = G->T + i;
    // Split process based on layout type
    if (S->lt == 0) {
      // grid-like layout
      if (dv_is_visible(node)) {
        double vc, hc;
        vc = node->vl->c;
        hc = node->c;
        if (vc - DV_RADIUS < x && x < vc + DV_RADIUS
            && hc - DV_RADIUS < y && y < hc + DV_RADIUS) {
          ret = node;
          break;
        }
      }      
    } else if (S->lt == 1 || S->lt == 2) {
      // bbox/timeline layouts
      if (dv_is_single(node)) {
        if (node->x - node->lw < x && x < node->x + node->rw
            && node->y < y && y < node->y + node->dw) {
          ret = node;
          break;
        }
      }
    }
  }
  return ret;
}

static void dv_set_entry_radix_text() {
  char str[DV_ENTRY_RADIX_MAX_LENGTH];
  double radix;
  if (S->sdt == 0)
    radix = S->log_radix;
  else if (S->sdt == 1)
    radix = S->power_radix;
  else if (S->sdt == 2)
    radix = S->linear_radix;
  else
    dv_check(0);
  sprintf(str, "%0.3lf", radix);
  gtk_entry_set_width_chars(GTK_ENTRY(dv_entry_radix), strlen(str));
  gtk_entry_set_text(GTK_ENTRY(dv_entry_radix), str);
}

static void dv_get_entry_radix_text() {
  double radix = atof(gtk_entry_get_text(GTK_ENTRY(dv_entry_radix)));
  if (S->sdt == 0)
    S->log_radix = radix;
  else if (S->sdt == 1)
    S->power_radix = radix;
  else if (S->sdt == 2)
    S->linear_radix = radix;
  else
    dv_check(0);
  dv_relayout_dvdag(G);
  gtk_widget_queue_draw(darea);
}

/*--------end of Interactive processing functions------------*/


/*-----------------DV Visualizer GUI-------------------*/

static gboolean on_draw_event(GtkWidget *widget, cairo_t *cr, gpointer user_data)
{
  if (S->lt == 0) {
    G->basex = 0.5 * S->vpw;
    G->basey = DV_ZOOM_TO_FIT_MARGIN + DV_RADIUS;
  } else if (S->lt == 1) {
    //G->basex = 0.5 * S->vpw - 0.5 * (G->rt->rw - G->rt->lw);
    G->basex = 0.5 * S->vpw;
    G->basey = DV_ZOOM_TO_FIT_MARGIN;
  } else if (S->lt == 2) {
    G->basex = DV_ZOOM_TO_FIT_MARGIN;
    G->basey = DV_ZOOM_TO_FIT_MARGIN;
  } else
    dv_check(0);
  // Draw
  dv_do_drawing(cr);
  return FALSE;
}

static gboolean on_btn_zoomfit_hoz_clicked(GtkWidget *widget, cairo_t *cr, gpointer user_data)
{
  dv_do_zoomfit_hoz();
  return TRUE;
}

static gboolean on_btn_zoomfit_ver_clicked(GtkWidget *widget, cairo_t *cr, gpointer user_data)
{
  dv_do_zoomfit_ver();
  return TRUE;
}

static gboolean on_btn_shrink_clicked(GtkWidget *widget, cairo_t *cr, gpointer user_data)
{
  dv_do_expanding(-1);
  return TRUE;
}

static gboolean on_btn_expand_clicked(GtkWidget *widget, cairo_t *cr, gpointer user_data)
{
  dv_do_expanding(1);
  return TRUE;
}

static gboolean on_scroll_event(GtkWidget *widget, GdkEventScroll *event, gpointer user_data) {
  if (event->direction == GDK_SCROLL_UP) {
    dv_do_zooming(G->zoom_ratio * DV_ZOOM_INCREMENT, event->x, event->y);
  } else if (event->direction == GDK_SCROLL_DOWN) {
    dv_do_zooming(G->zoom_ratio / DV_ZOOM_INCREMENT, event->x, event->y);
  }
  return TRUE;
}

static gboolean on_button_event(GtkWidget *widget, GdkEventButton *event, gpointer user_data)
{
  if (event->type == GDK_BUTTON_PRESS) {
    // Drag
    S->drag_on = 1;
    S->pressx = event->x;
    S->pressy = event->y;
    S->accdisx = 0.0;
    S->accdisy = 0.0;
  }  else if (event->type == GDK_BUTTON_RELEASE) {
    // Drag
    S->drag_on = 0;
    // Info tag
    if (S->accdisx < DV_SAFE_CLICK_RANGE
        && S->accdisy < DV_SAFE_CLICK_RANGE) {
      double ox = (event->x - G->basex - G->x) / G->zoom_ratio;
      double oy = (event->y - G->basey - G->y) / G->zoom_ratio;
      dv_dag_node_t *node_pressed = dv_do_finding_clicked_node(ox, oy);
      if (node_pressed) {
        if (!dv_llist_remove(G->itl, node_pressed)) {
          dv_llist_add(G->itl, node_pressed);
        }
        gtk_widget_queue_draw(darea);
      }
    }
  } else if (event->type == GDK_2BUTTON_PRESS) {
    // Shrink/Expand
  }
  return TRUE;
}

static gboolean on_motion_event(GtkWidget *widget, GdkEventMotion *event, gpointer user_data)
{
  if (S->drag_on) {
    // Drag
    double deltax = event->x - S->pressx;
    double deltay = event->y - S->pressy;
    G->x += deltax;
    G->y += deltay;
    S->accdisx += deltax;
    S->accdisy += deltay;
    S->pressx = event->x;
    S->pressy = event->y;
    gtk_widget_queue_draw(darea);
  }
  return TRUE;
}

static gboolean on_combobox_changed(GtkComboBox *widget, gpointer user_data) {
  S->nc = gtk_combo_box_get_active(widget);
  gtk_widget_queue_draw(darea);
  return TRUE;
}

static gboolean on_combobox2_changed(GtkComboBox *widget, gpointer user_data) {
  int old_lt = S->lt;
  S->lt = gtk_combo_box_get_active(widget);
  dv_relayout_dvdag(G);
  if (S->lt != old_lt)
    dv_do_zoomfit_hoz();
  return TRUE;
}

static gboolean on_combobox3_changed(GtkComboBox *widget, gpointer user_data) {
  S->sdt = gtk_combo_box_get_active(widget);
  dv_set_entry_radix_text();
  dv_relayout_dvdag(G);
  gtk_widget_queue_draw(darea);
  return TRUE;
}

static gboolean on_combobox4_changed(GtkComboBox *widget, gpointer user_data) {
  S->frombt = gtk_combo_box_get_active(widget);
  dv_relayout_dvdag(G);
  gtk_widget_queue_draw(darea);
  return TRUE;
}

static gboolean on_entry_radix_activate(GtkEntry *entry, gpointer user_data) {
  dv_get_entry_radix_text();
  return TRUE;
}

static gboolean on_darea_configure_event(GtkWidget *widget, GdkEventConfigure *event, gpointer user_data) {
  S->vpw = event->width;
  S->vph = event->height;
  return TRUE;
}

int open_gui(int argc, char *argv[])
{
  gtk_init(&argc, &argv);
  GdkRGBA white[1];
  gdk_rgba_parse(white, "white");

  // Main window
  window = gtk_window_new(GTK_WINDOW_TOPLEVEL);
  //gtk_window_set_position(GTK_WINDOW(window), GTK_WIN_POS_CENTER);
  gtk_window_set_default_size(GTK_WINDOW(window), 1000, 700);
  //gtk_window_fullscreen(GTK_WINDOW(window));
  gtk_window_maximize(GTK_WINDOW(window));
  gtk_window_set_title(GTK_WINDOW(window), "DAG Visualizer");
  g_signal_connect(window, "destroy", G_CALLBACK(gtk_main_quit), NULL);
  GtkWidget *vbox = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
  gtk_container_add(GTK_CONTAINER(window), vbox);  

  // Toolbar
  GtkWidget *toolbar = gtk_toolbar_new();
  //gtk_widget_override_background_color(GTK_WIDGET(toolbar), GTK_STATE_FLAG_NORMAL, white);

  // Layout type combobox
  GtkToolItem *btn_combo2 = gtk_tool_item_new();
  GtkWidget *combobox2 = gtk_combo_box_text_new();
  gtk_combo_box_text_append(GTK_COMBO_BOX_TEXT(combobox2), "grid", "Grid like");
  gtk_combo_box_text_append(GTK_COMBO_BOX_TEXT(combobox2), "bounding", "Bounding box");
  gtk_combo_box_text_append(GTK_COMBO_BOX_TEXT(combobox2), "timeline", "Timeline");
  gtk_combo_box_set_active(GTK_COMBO_BOX(combobox2), DV_LAYOUT_TYPE_INIT);
  g_signal_connect(G_OBJECT(combobox2), "changed", G_CALLBACK(on_combobox2_changed), NULL);
  gtk_container_add(GTK_CONTAINER(btn_combo2), combobox2);
  gtk_toolbar_insert(GTK_TOOLBAR(toolbar), btn_combo2, -1);

  // Node color combobox
  GtkToolItem *btn_combo = gtk_tool_item_new();
  GtkWidget *combobox = gtk_combo_box_text_new();
  gtk_combo_box_text_append(GTK_COMBO_BOX_TEXT(combobox), "worker", "Worker");
  gtk_combo_box_text_append(GTK_COMBO_BOX_TEXT(combobox), "cpu", "CPU");
  gtk_combo_box_text_append(GTK_COMBO_BOX_TEXT(combobox), "kind", "Node kind");
  gtk_combo_box_text_append(GTK_COMBO_BOX_TEXT(combobox), "code_start", "Code start");
  gtk_combo_box_text_append(GTK_COMBO_BOX_TEXT(combobox), "code_end", "Code end");
  gtk_combo_box_text_append(GTK_COMBO_BOX_TEXT(combobox), "code_start_end", "Code start-end");
  gtk_combo_box_set_active(GTK_COMBO_BOX(combobox), DV_NODE_COLOR_INIT);
  g_signal_connect(G_OBJECT(combobox), "changed", G_CALLBACK(on_combobox_changed), NULL);
  gtk_container_add(GTK_CONTAINER(btn_combo), combobox);
  gtk_toolbar_insert(GTK_TOOLBAR(toolbar), btn_combo, -1);

  // Scale-down type combobox
  GtkToolItem *btn_combo3 = gtk_tool_item_new();
  GtkWidget *combobox3 = gtk_combo_box_text_new();
  gtk_combo_box_text_append(GTK_COMBO_BOX_TEXT(combobox3), "log", "Log");
  gtk_combo_box_text_append(GTK_COMBO_BOX_TEXT(combobox3), "power", "Power");
  gtk_combo_box_text_append(GTK_COMBO_BOX_TEXT(combobox3), "linear", "Linear");
  gtk_combo_box_set_active(GTK_COMBO_BOX(combobox3), DV_SCALE_TYPE_INIT);
  g_signal_connect(G_OBJECT(combobox3), "changed", G_CALLBACK(on_combobox3_changed), NULL);
  gtk_container_add(GTK_CONTAINER(btn_combo3), combobox3);
  gtk_toolbar_insert(GTK_TOOLBAR(toolbar), btn_combo3, -1);

  // Radix value input
  GtkToolItem *btn_entry_radix = gtk_tool_item_new();
  dv_entry_radix = gtk_entry_new();
  //gtk_entry_set_max_length(GTK_ENTRY(dv_entry_radix), 10);
  dv_set_entry_radix_text();
  g_signal_connect(G_OBJECT(dv_entry_radix), "activate", G_CALLBACK(on_entry_radix_activate), NULL);
  gtk_container_add(GTK_CONTAINER(btn_entry_radix), dv_entry_radix);
  gtk_toolbar_insert(GTK_TOOLBAR(toolbar), btn_entry_radix, -1);

  // Frombt combobox
  GtkToolItem *btn_combo4 = gtk_tool_item_new();
  GtkWidget *combobox4 = gtk_combo_box_text_new();
  gtk_combo_box_text_append(GTK_COMBO_BOX_TEXT(combobox4), "not", "Not frombt");
  gtk_combo_box_text_append(GTK_COMBO_BOX_TEXT(combobox4), "frombt", "From BT");
  gtk_combo_box_set_active(GTK_COMBO_BOX(combobox4), DV_FROMBT_INIT);
  g_signal_connect(G_OBJECT(combobox4), "changed", G_CALLBACK(on_combobox4_changed), NULL);
  gtk_container_add(GTK_CONTAINER(btn_combo4), combobox4);
  gtk_toolbar_insert(GTK_TOOLBAR(toolbar), btn_combo4, -1);

  // Zoomfit-horizontally button
  GtkToolItem *btn_zoomfit_hoz = gtk_tool_button_new_from_stock(GTK_STOCK_ZOOM_FIT);
  gtk_toolbar_insert(GTK_TOOLBAR(toolbar), btn_zoomfit_hoz, -1);
  g_signal_connect(G_OBJECT(btn_zoomfit_hoz), "clicked", G_CALLBACK(on_btn_zoomfit_hoz_clicked), NULL);
  gtk_box_pack_start(GTK_BOX(vbox), toolbar, FALSE, FALSE, 0);

  // Zoomfit-vertically button
  GtkToolItem *btn_zoomfit_ver = gtk_tool_button_new_from_stock(GTK_STOCK_ZOOM_FIT);
  gtk_toolbar_insert(GTK_TOOLBAR(toolbar), btn_zoomfit_ver, -1);
  g_signal_connect(G_OBJECT(btn_zoomfit_ver), "clicked", G_CALLBACK(on_btn_zoomfit_ver_clicked), NULL);
  gtk_box_pack_start(GTK_BOX(vbox), toolbar, FALSE, FALSE, 0);

  // Shrink/Expand buttons
  GtkToolItem *btn_shrink = gtk_tool_button_new_from_stock(GTK_STOCK_ZOOM_OUT);
  GtkToolItem *btn_expand = gtk_tool_button_new_from_stock(GTK_STOCK_ZOOM_IN);
  gtk_toolbar_insert(GTK_TOOLBAR(toolbar), btn_shrink, -1);
  gtk_toolbar_insert(GTK_TOOLBAR(toolbar), btn_expand, -1);
  g_signal_connect(G_OBJECT(btn_shrink), "clicked", G_CALLBACK(on_btn_shrink_clicked), NULL);
  g_signal_connect(G_OBJECT(btn_expand), "clicked", G_CALLBACK(on_btn_expand_clicked), NULL);

  // Drawing Area
  darea = gtk_drawing_area_new();
  gtk_widget_override_background_color(GTK_WIDGET(darea), GTK_STATE_FLAG_NORMAL, white);
  g_signal_connect(G_OBJECT(darea), "draw", G_CALLBACK(on_draw_event), NULL);
  gtk_widget_add_events(GTK_WIDGET(darea), GDK_SCROLL_MASK | GDK_BUTTON_PRESS_MASK | GDK_BUTTON_RELEASE_MASK | GDK_POINTER_MOTION_MASK);
  g_signal_connect(G_OBJECT(darea), "scroll-event", G_CALLBACK(on_scroll_event), NULL);
  g_signal_connect(G_OBJECT(darea), "button-press-event", G_CALLBACK(on_button_event), NULL);
  g_signal_connect(G_OBJECT(darea), "button-release-event", G_CALLBACK(on_button_event), NULL);
  g_signal_connect(G_OBJECT(darea), "motion-notify-event", G_CALLBACK(on_motion_event), NULL);
  g_signal_connect(G_OBJECT(darea), "configure-event", G_CALLBACK(on_darea_configure_event), NULL);
  gtk_box_pack_start(GTK_BOX(vbox), darea, TRUE, TRUE, 0);

  // Run main loop
  gtk_widget_show_all(window);
  gtk_main();

  return 1;
}
/*-----------------end of DV Visualizer GUI-------------------*/


/*---------------Initialization Functions------*/

static void dv_status_init() {
  S->drag_on = 0;
  S->pressx = S->pressy = 0.0;
  S->accdisx = S->accdisy = 0.0;
  S->nc = DV_NODE_COLOR_INIT;
  S->vpw = S->vph = 0.0;
  S->cur_d = 2;
  dv_animation_init(S->a);
  S->nd = 0;
  S->lt = DV_LAYOUT_TYPE_INIT;
  S->sdt = DV_SCALE_TYPE_INIT;
  S->log_radix = DV_RADIX_LOG;
  S->power_radix = DV_RADIX_POWER;
  S->linear_radix = DV_RADIX_LINEAR;
  S->frombt = DV_FROMBT_INIT;
  int i;
  for (i=0; i<DV_NUM_COLOR_POOLS; i++)
    S->CP_sizes[i] = 0;
  S->fcc = 0;
}

/*---------------end of Initialization Functions------*/


/*---------------Environment Variables-----*/

static int dv_get_env_int(char * s, int * t) {
  char * v = getenv(s);
  if (v) {
    *t = atoi(v);
    return 1;
  }
  return 0;
}

static int dv_get_env_long(char * s, long * t) {
  char * v = getenv(s);
  if (v) {
    *t = atol(v);
    return 1;
  }
  return 0;
}

static int dv_get_env_string(char * s, char ** t) {
  char * v = getenv(s);
  if (v) {
    *t = strdup(v);
    return 1;
  }
  return 0;
}

static void dv_get_env() {
  dv_get_env_int("DV_DEPTH", &S->cur_d);
}

/*---------------end of Environment Variables-----*/


/*-----------------Main begins-----------------*/

int main(int argc, char *argv[])
{
  dv_status_init();
  dv_get_env();
  if (argc > 1) {
    dv_read_dag_file_to_pidag(argv[1], P);
    dv_convert_pidag_to_dvdag(P, G);
    //print_dvdag(G);
    dv_layout_dvdag(G);
    //check_layout(G);
  }
  //if (argc > 1)  print_dag_file(argv[1]);
  return open_gui(argc, argv);
  //return 1;
}

/*-----------------Main ends-------------------*/
