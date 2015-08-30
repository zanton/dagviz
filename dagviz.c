#include "dagviz.h"

dv_global_state_t  CS[1];

const char * const DV_COLORS[] =
  {"orange", "gold", "cyan", "azure", "green",
   "magenta", "brown1", "burlywood1", "peachpuff", "aquamarine",
   "chartreuse", "skyblue", "burlywood", "cadetblue", "chocolate",
   "coral", "cornflowerblue", "cornsilk4", "darkolivegreen1", "darkorange1",
   "khaki3", "lavenderblush2", "lemonchiffon1", "lightblue1", "lightcyan",
   "lightgoldenrod", "lightgoldenrodyellow", "lightpink2", "lightsalmon2", "lightskyblue1",
   "lightsteelblue3", "lightyellow3", "maroon1", "yellowgreen"};

const char * const DV_HISTOGRAM_COLORS[] =
  {"red", "green1", "blue", "magenta1", "cyan1", "yellow"};

const int DV_LINEAR_PATTERN_STOPS_NUM = 3;
const char * const DV_LINEAR_PATTERN_STOPS[] =
  //{"white", "black", "white"};
  //{"black", "white", "black"};
  ///{"green", "yellowgreen", "yellowgreen", "cyan"};
  //{"orange", "yellow", "coral"};
  //{"white", "white", "white"};
  {"orange", "yellow", "cyan"};

const char * const DV_RADIAL_PATTERN_STOPS[] =
  {"black", "white"};

const int DV_RADIAL_PATTERN_STOPS_NUM = 2;

/*---------------Environment Variables-----*/

_static_unused_ int
dv_get_env_int(char * s, int * t) {
  char * v = getenv(s);
  if (v) {
    *t = atoi(v);
    return 1;
  }
  return 0;
}

_static_unused_ int
dv_get_env_long(char * s, long * t) {
  char * v = getenv(s);
  if (v) {
    *t = atol(v);
    return 1;
  }
  return 0;
}

_static_unused_ int
dv_get_env_string(char * s, char ** t) {
  char * v = getenv(s);
  if (v) {
    *t = strdup(v);
    return 1;
  }
  return 0;
}

_static_unused_ void
dv_get_env() {
  //dv_get_env_int("DV_DEPTH", &S->cur_d);
}

/*---------------end of Environment Variables-----*/



/*-----------------Global State-----------------*/

void
dv_global_state_init(dv_global_state_t * CS) {
  CS->nP = 0;
  CS->nD = 0;
  CS->nV = 0;
  CS->nVP = 0;
  CS->FL = NULL;
  CS->activeV = NULL;
  CS->err = DV_OK;
  int i;
  for (i = 0; i < DV_NUM_COLOR_POOLS; i++)
    CS->CP_sizes[i] = 0;
  dv_btsample_viewer_init(CS->btviewer);
  CS->box_viewport_configure = NULL;
  dv_dag_node_pool_init(CS->pool);
  dv_histogram_entry_pool_init(CS->epool);
  CS->SD->ne = 0;
  for (i = 0; i < DV_MAX_DISTRIBUTION; i++) {
    CS->SD->e[i].dag_id = -1; /* none */
    CS->SD->e[i].type = 0;
    CS->SD->e[i].stolen = 0;
    CS->SD->e[i].title = NULL;
    CS->SD->e[i].title_entry = NULL;
  }
  CS->SD->xrange_from = 0;
  CS->SD->xrange_to = 10000;
  for (i = 0; i < DV_MAX_DAG; i++) {
    CS->SD->dag_status_labels[i] = NULL;
  }
  CS->SD->node_pool_label = NULL;
  CS->SD->entry_pool_label = NULL;
  CS->SD->fn = DV_STAT_DISTRIBUTION_OUTPUT_DEFAULT_NAME;
  CS->SD->bar_width = 20;
  for (i = 0; i < DV_MAX_DAG; i++) {
    CS->SBG->D[i] = 0;
  }
  CS->SBG->fn = DV_STAT_BREAKDOWN_OUTPUT_DEFAULT_NAME;
}

void dv_global_state_set_active_view(dv_view_t *V) {
  CS->activeV = V;
  dv_statusbar_update_selection_status();
}

dv_view_t * dv_global_state_get_active_view() {
  return CS->activeV;
}

/*-----------------end of Global State-----------------*/



/*--------Interactive processing functions------------*/

void
dv_queue_draw(dv_view_t * V) {
  int i;
  for (i=0; i<CS->nVP; i++)
    if (V->mVP[i])
      gtk_widget_queue_draw(CS->VP[i].darea);
}

void
dv_queue_draw_d(dv_view_t * V) {
  dv_dag_t * D = V->D;
  int i;
  for (i=0; i<CS->nV; i++)
    if (CS->V[i].D == D)
      dv_queue_draw(&CS->V[i]);
}

void
dv_queue_draw_d_p(dv_view_t * V) {
  dv_pidag_t * P = V->D->P;
  int i;
  for (i=0; i<CS->nV; i++)
    if (CS->V[i].D->P == P)
      dv_queue_draw(&CS->V[i]);
}

void
dv_view_clip(dv_view_t * V, cairo_t * cr) {
  cairo_rectangle(cr, DV_CLIPPING_FRAME_MARGIN, DV_CLIPPING_FRAME_MARGIN, V->S->vpw - DV_CLIPPING_FRAME_MARGIN * 2, V->S->vph - DV_CLIPPING_FRAME_MARGIN * 2);
  cairo_clip(cr);
}

double
dv_view_clip_get_bound_left(dv_view_t * V) {
  return (DV_CLIPPING_FRAME_MARGIN - V->S->basex - V->S->x) / V->S->zoom_ratio_x;
}

double
dv_view_clip_get_bound_right(dv_view_t * V) {
  return ((V->S->vpw - DV_CLIPPING_FRAME_MARGIN) - V->S->basex - V->S->x) / V->S->zoom_ratio_x;
}

double
dv_view_clip_get_bound_up(dv_view_t * V) {
  return (DV_CLIPPING_FRAME_MARGIN - V->S->basey - V->S->y) / V->S->zoom_ratio_y;
}

double
dv_view_clip_get_bound_down(dv_view_t * V) {
  return ((V->S->vph - DV_CLIPPING_FRAME_MARGIN) - V->S->basey - V->S->y) / V->S->zoom_ratio_y;
}

double
dv_view_cairo_coordinate_bound_left(dv_view_t * V) {
  return (DV_CAIRO_BOUND_MIN - V->S->basex - V->S->x) / V->S->zoom_ratio_x;
}

double
dv_view_cairo_coordinate_bound_right(dv_view_t * V) {
  return (DV_CAIRO_BOUND_MAX - V->S->basex - V->S->x) / V->S->zoom_ratio_x;
}

double
dv_view_cairo_coordinate_bound_up(dv_view_t * V) {
  return (DV_CAIRO_BOUND_MIN - V->S->basey - V->S->y) / V->S->zoom_ratio_y;
}

double
dv_view_cairo_coordinate_bound_down(dv_view_t * V) {
  return (DV_CAIRO_BOUND_MAX - V->S->basey - V->S->y) / V->S->zoom_ratio_y;
}

static void
dv_view_prepare_drawing(dv_view_t * V, cairo_t * cr) {
  dv_view_status_t * S = V->S;
  if (S->do_zoomfit) {
    dv_view_do_zoomfit_based_on_lt(V);
    S->do_zoomfit = 0;
  }
  
  cairo_save(cr);
  
  /* Draw graph */
  // Clipping
  dv_view_clip(V, cr);
  // Transforming
  cairo_matrix_t mt[1];
  cairo_matrix_init_translate(mt, S->basex + S->x, S->basey + S->y);
  cairo_matrix_scale(mt, S->zoom_ratio_x, S->zoom_ratio_y);
  cairo_transform(cr, mt);
  //fprintf(stderr, "matrix:\n%lf %lf %lf\n%lf %lf %lf\n", mt->xx, mt->xy, mt->x0, mt->yx, mt->yy, mt->y0);
  //fprintf(stderr, "to compare with:\n%lf %lf %lf\n%lf %lf %lf\n", S->zoom_ratio_x, 0.0, S->basex + S->x, 0.0, S->zoom_ratio_y, S->basey + S->y);
  // Transforming
  /*
  cairo_translate(cr, S->basex + S->x, S->basey + S->y);
  cairo_scale(cr, S->zoom_ratio_x, S->zoom_ratio_y);
  cairo_matrix_t mt_[1];
  cairo_get_matrix(cr, mt_);
  fprintf(stderr, "matrix:\n%lf %lf %lf\n%lf %lf %lf\n", mt_->xx, mt_->xy, mt_->x0, mt_->yx, mt_->yy, mt_->y0);
  fprintf(stderr, "to compare with:\n%lf %lf %lf\n%lf %lf %lf\n", mt->xx, mt->xy, mt->x0, mt->yx, mt->yy, mt->y0);
  */
  
  dv_view_draw(V, cr);
  
  /* Draw infotags */
  /* TODO: to make it not scale unequally infotags */
  //dv_view_draw_infotags(V, cr, NULL);
  
  cairo_restore(cr);
}

static void
dv_viewport_draw(dv_viewport_t * VP, cairo_t * cr) {
  dv_view_t * V;
  dv_view_status_t * S;
  int count = 0;
  int i;
  for (i=0; i<CS->nV; i++)
    if (VP->mV[i]) {
      V = CS->V + i;
      S = V->S;
      switch (S->lt) {
      case 0:
        S->basex = 0.5 * S->vpw;
        S->basey = DV_ZOOM_TO_FIT_MARGIN;
        break;
      case 1:
        //G->basex = 0.5 * S->vpw - 0.5 * (G->rt->rw - G->rt->lw);
        S->basex = 0.5 * S->vpw;
        S->basey = DV_ZOOM_TO_FIT_MARGIN;
        break;
      case 2:
      case 3:
        S->basex = DV_ZOOM_TO_FIT_MARGIN;
        S->basey = DV_ZOOM_TO_FIT_MARGIN;
        break;
      case 4:
        S->basex = DV_HISTOGRAM_MARGIN;
        S->basey = S->vph - DV_HISTOGRAM_MARGIN_DOWN;
        break;
      default:
        dv_check(0);
      }
      // Draw
      dv_view_prepare_drawing(V, cr);
      if (V->S->show_status)
        dv_view_draw_status(V, cr, count);
      if (V->S->show_legend)
        dv_view_draw_legend(V, cr);
      count++;
    }
  //dv_viewport_draw_label(VP, cr);
  dv_statusbar_update_selection_status();
}

/*--------end of Interactive processing functions------------*/


/*-----------------VIEW's functions-----------------*/

#include "control.c"

void
dv_view_toolbox_init(dv_view_toolbox_t * T, dv_view_t * V) {
  T->V = V;
  T->window = NULL;
  T->combobox_lt = NULL;
  T->combobox_nc = NULL;
  T->combobox_sdt = NULL;
  T->entry_radix = NULL;
  T->combobox_frombt = NULL;
  T->combobox_et = NULL;
  T->togg_eaffix = NULL;
  T->checkbox_legend = NULL;
  T->checkbox_status = NULL;
  T->entry_remark = NULL;
  T->checkbox_remain_inner = NULL;
  T->checkbox_color_remarked_only = NULL;

  /* entry_search */
  T->entry_search = gtk_entry_new();
  gtk_entry_set_placeholder_text(GTK_ENTRY(T->entry_search), "Search e.g. 0");
  gtk_widget_set_tooltip_text(GTK_WIDGET(T->entry_search), "Search by node's index in DAG file (first number in node's info tag)");
  gtk_entry_set_max_length(GTK_ENTRY(T->entry_search), 7);
  g_signal_connect(G_OBJECT(T->entry_search), "activate", G_CALLBACK(on_entry_search_activate), (void *) V);

  /* checkbox_xzoom */
  T->checkbox_xzoom = gtk_check_button_new_with_label("X Zoom");
  gtk_widget_set_tooltip_text(GTK_WIDGET(T->checkbox_xzoom), "Cairo's horizontal zoom when scrolling");
  gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(T->checkbox_xzoom), V->S->do_zoom_x);
  g_signal_connect(G_OBJECT(T->checkbox_xzoom), "toggled", G_CALLBACK(on_checkbox_xzoom_toggled), (void *) V);

  /* checkbox_yzoom */
  T->checkbox_yzoom = gtk_check_button_new_with_label("Y Zoom");
  gtk_widget_set_tooltip_text(GTK_WIDGET(T->checkbox_yzoom), "Cairo's vertical zoom when scrolling");
  gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(T->checkbox_yzoom), V->S->do_zoom_y);
  g_signal_connect(G_OBJECT(T->checkbox_yzoom), "toggled", G_CALLBACK(on_checkbox_yzoom_toggled), (void *) V);

  /* checkbox_scale_radix */
  T->checkbox_scale_radix = gtk_check_button_new_with_label("Scale Node Length");
  gtk_widget_set_tooltip_text(GTK_WIDGET(T->checkbox_scale_radix), "Scale node's lenth when scrolling");
  gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(T->checkbox_scale_radix), V->S->do_scale_radix);
  g_signal_connect(G_OBJECT(T->checkbox_scale_radix), "toggled", G_CALLBACK(on_checkbox_scale_radix_toggled), (void *) V);

  /* checkbox_scale_radius */
  T->checkbox_scale_radius = gtk_check_button_new_with_label("Scale Node Width");
  gtk_widget_set_tooltip_text(GTK_WIDGET(T->checkbox_scale_radius), "Scale node's width when scrolling");
  gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(T->checkbox_scale_radius), V->S->do_scale_radius);
  g_signal_connect(G_OBJECT(T->checkbox_scale_radius), "toggled", G_CALLBACK(on_checkbox_scale_radius_toggled), (void *) V);

  /* combobox_cm */
  T->combobox_cm = gtk_combo_box_text_new();
  gtk_widget_set_tooltip_text(GTK_WIDGET(T->combobox_cm), "When clicking a node");
  gtk_combo_box_text_append(GTK_COMBO_BOX_TEXT(T->combobox_cm), "none", "None");
  gtk_combo_box_text_append(GTK_COMBO_BOX_TEXT(T->combobox_cm), "info", "Info box");
  gtk_combo_box_text_append(GTK_COMBO_BOX_TEXT(T->combobox_cm), "expand", "Expand/Collapse");
  gtk_combo_box_set_active(GTK_COMBO_BOX(T->combobox_cm), V->S->cm);
  g_signal_connect(G_OBJECT(T->combobox_cm), "changed", G_CALLBACK(on_combobox_cm_changed), (void *) V);
  
  /* combobox_hm */
  T->combobox_hm = gtk_combo_box_text_new();
  gtk_widget_set_tooltip_text(GTK_WIDGET(T->combobox_hm), "When hovering a node");
  gtk_combo_box_text_append(GTK_COMBO_BOX_TEXT(T->combobox_hm), "none", "None");
  gtk_combo_box_text_append(GTK_COMBO_BOX_TEXT(T->combobox_hm), "info", "Info box");
  gtk_combo_box_text_append(GTK_COMBO_BOX_TEXT(T->combobox_hm), "expand", "Expand");
  gtk_combo_box_text_append(GTK_COMBO_BOX_TEXT(T->combobox_hm), "collapse", "Colapse");
  gtk_combo_box_text_append(GTK_COMBO_BOX_TEXT(T->combobox_hm), "expcoll", "Expand/Collapse");
  gtk_combo_box_set_active(GTK_COMBO_BOX(T->combobox_hm), V->S->hm);
  g_signal_connect(G_OBJECT(T->combobox_hm), "changed", G_CALLBACK(on_combobox_hm_changed), (void *) V);

  /* combobox_azf */
  T->combobox_azf = gtk_combo_box_text_new();
  gtk_widget_set_tooltip_text(GTK_WIDGET(T->combobox_azf), "Auto zoom DAG fitly");
  gtk_combo_box_text_append(GTK_COMBO_BOX_TEXT(T->combobox_azf), "none", "None");
  gtk_combo_box_text_append(GTK_COMBO_BOX_TEXT(T->combobox_azf), "hor", "Horizontal");
  gtk_combo_box_text_append(GTK_COMBO_BOX_TEXT(T->combobox_azf), "ver", "Vertical");
  gtk_combo_box_text_append(GTK_COMBO_BOX_TEXT(T->combobox_azf), "based", "based on view type");
  gtk_combo_box_text_append(GTK_COMBO_BOX_TEXT(T->combobox_azf), "full", "Full");  
  gtk_combo_box_set_active(GTK_COMBO_BOX(T->combobox_azf), V->S->auto_zoomfit);
  g_signal_connect(G_OBJECT(T->combobox_azf), "changed", G_CALLBACK(on_combobox_azf_changed), (void *) V);

  /* checkbox_azf */
  T->checkbox_azf = gtk_check_button_new();
  gtk_widget_set_tooltip_text(GTK_WIDGET(T->checkbox_scale_radius), "Auto adjust auto zoom fit based on context");
  gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(T->checkbox_azf), V->S->adjust_auto_zoomfit);
  g_signal_connect(G_OBJECT(T->checkbox_azf), "toggled", G_CALLBACK(on_checkbox_azf_toggled), (void *) V);
}

GtkWidget *
dv_view_toolbox_get_combobox_lt(dv_view_toolbox_t * T) {
  if (!GTK_IS_WIDGET(T->combobox_lt)) {
    GtkWidget * combobox_lt = T->combobox_lt = gtk_combo_box_text_new();
    gtk_widget_set_tooltip_text(GTK_WIDGET(combobox_lt), "How to layout nodes");
    gtk_combo_box_text_append(GTK_COMBO_BOX_TEXT(combobox_lt), "dag", "DAG");
    gtk_combo_box_text_append(GTK_COMBO_BOX_TEXT(combobox_lt), "dagbox", "DAG with boxes");
    gtk_combo_box_text_append(GTK_COMBO_BOX_TEXT(combobox_lt), "timelinev", "Vertical timeline");
    gtk_combo_box_text_append(GTK_COMBO_BOX_TEXT(combobox_lt), "timeline", "Horizontal timeline");
    gtk_combo_box_text_append(GTK_COMBO_BOX_TEXT(combobox_lt), "paraprof", "Parallelism profile");
    gtk_combo_box_set_active(GTK_COMBO_BOX(combobox_lt), T->V->S->lt);
    g_signal_connect(G_OBJECT(combobox_lt), "changed", G_CALLBACK(on_combobox_lt_changed), (void *) T->V);
  }
  return T->combobox_lt;
}

GtkWidget *
dv_view_toolbox_get_combobox_nc(dv_view_toolbox_t * T) {
  if (!GTK_IS_WIDGET(T->combobox_nc)) {
    GtkWidget * combobox_nc = T->combobox_nc = gtk_combo_box_text_new();
    gtk_widget_set_tooltip_text(GTK_WIDGET(combobox_nc), "How to color nodes");
    gtk_combo_box_text_append(GTK_COMBO_BOX_TEXT(combobox_nc), "worker", "Worker");
    gtk_combo_box_text_append(GTK_COMBO_BOX_TEXT(combobox_nc), "cpu", "CPU");
    gtk_combo_box_text_append(GTK_COMBO_BOX_TEXT(combobox_nc), "kind", "Node kind");
    gtk_combo_box_text_append(GTK_COMBO_BOX_TEXT(combobox_nc), "code_start", "Code start");
    gtk_combo_box_text_append(GTK_COMBO_BOX_TEXT(combobox_nc), "code_end", "Code end");
    gtk_combo_box_text_append(GTK_COMBO_BOX_TEXT(combobox_nc), "code_start_end", "Code start-end pair");
    gtk_combo_box_set_active(GTK_COMBO_BOX(combobox_nc), T->V->S->nc);
    g_signal_connect(G_OBJECT(combobox_nc), "changed", G_CALLBACK(on_combobox_nc_changed), (void *) T->V);
  }
  return T->combobox_nc;
}

GtkWidget *
dv_view_toolbox_get_combobox_sdt(dv_view_toolbox_t * T) {
  if (!GTK_IS_WIDGET(T->combobox_sdt)) {
    GtkWidget * combobox_sdt = T->combobox_sdt = gtk_combo_box_text_new();
    gtk_widget_set_tooltip_text(GTK_WIDGET(combobox_sdt), "How to scale down nodes' length");
    gtk_combo_box_text_append(GTK_COMBO_BOX_TEXT(combobox_sdt), "log", "Log");
    gtk_combo_box_text_append(GTK_COMBO_BOX_TEXT(combobox_sdt), "power", "Power");
    gtk_combo_box_text_append(GTK_COMBO_BOX_TEXT(combobox_sdt), "linear", "Linear");
    gtk_combo_box_set_active(GTK_COMBO_BOX(combobox_sdt), T->V->D->sdt);
    g_signal_connect(G_OBJECT(combobox_sdt), "changed", G_CALLBACK(on_combobox_sdt_changed), (void *) T->V);
  }
  return T->combobox_sdt;
}

GtkWidget *
dv_view_toolbox_get_entry_radix(dv_view_toolbox_t * T) {
  if (!GTK_IS_WIDGET(T->entry_radix)) {
    GtkWidget * entry_radix = T->entry_radix = gtk_entry_new();
    gtk_widget_set_tooltip_text(GTK_WIDGET(entry_radix), "Radix of the scale-down function");
    g_signal_connect(G_OBJECT(entry_radix), "activate", G_CALLBACK(on_entry_radix_activate), (void *) T->V);
  }
  dv_view_set_entry_radix_text(T->V);
  return T->entry_radix;
}

GtkWidget *
dv_view_toolbox_get_combobox_frombt(dv_view_toolbox_t * T) {
  if (!GTK_IS_WIDGET(T->combobox_frombt)) {
    GtkWidget * combobox_frombt = T->combobox_frombt = gtk_combo_box_text_new();
    gtk_widget_set_tooltip_text(GTK_WIDGET(combobox_frombt), "Scale down overall from start time (must for timeline)");
    gtk_combo_box_text_append(GTK_COMBO_BOX_TEXT(combobox_frombt), "not", "No");
    gtk_combo_box_text_append(GTK_COMBO_BOX_TEXT(combobox_frombt), "frombt", "Yes");
    gtk_combo_box_set_active(GTK_COMBO_BOX(combobox_frombt), T->V->D->frombt);
    g_signal_connect(G_OBJECT(combobox_frombt), "changed", G_CALLBACK(on_combobox_frombt_changed), (void *) T->V);
  }
  return T->combobox_frombt;
}

GtkWidget *
dv_view_toolbox_get_combobox_et(dv_view_toolbox_t * T) {
  if (!GTK_IS_WIDGET(T->combobox_et)) {
    GtkWidget * combobox_et = T->combobox_et = gtk_combo_box_text_new();
    gtk_widget_set_tooltip_text(GTK_WIDGET(combobox_et), "How to draw edges");
    gtk_combo_box_text_append(GTK_COMBO_BOX_TEXT(combobox_et), "none", "None");
    gtk_combo_box_text_append(GTK_COMBO_BOX_TEXT(combobox_et), "straight", "Straight");
    gtk_combo_box_text_append(GTK_COMBO_BOX_TEXT(combobox_et), "down", "Down");
    gtk_combo_box_text_append(GTK_COMBO_BOX_TEXT(combobox_et), "winding", "Winding");
    gtk_combo_box_set_active(GTK_COMBO_BOX(combobox_et), T->V->S->et);
    g_signal_connect(G_OBJECT(combobox_et), "changed", G_CALLBACK(on_combobox_et_changed), (void *) T->V);
  }
  return T->combobox_et;
}

GtkWidget *
dv_view_toolbox_get_togg_eaffix(dv_view_toolbox_t * T) {
  if (!GTK_IS_WIDGET(T->togg_eaffix) || gtk_widget_get_parent(T->togg_eaffix)) {
    GtkWidget * togg_eaffix = T->togg_eaffix = gtk_toggle_button_new_with_label("Edge Affix");
    gtk_widget_set_tooltip_text(GTK_WIDGET(togg_eaffix), "Add a short line segment btwn edges & nodes");
    if (T->V->S->edge_affix == 0)
      gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(togg_eaffix), FALSE);
    else
      gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(togg_eaffix), TRUE);
    g_signal_connect(G_OBJECT(togg_eaffix), "toggled", G_CALLBACK(on_togg_eaffix_toggled), (void *) T->V);
  }
  return T->togg_eaffix;
}

GtkWidget *
dv_view_toolbox_get_checkbox_legend(dv_view_toolbox_t * T) {
  if (!GTK_IS_WIDGET(T->checkbox_legend)) {
    T->checkbox_legend = gtk_check_button_new();
    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(T->checkbox_legend), T->V->S->show_legend);
    g_signal_connect(G_OBJECT(T->checkbox_legend), "toggled", G_CALLBACK(on_checkbox_legend_toggled), (void *) T->V);
  }
  return T->checkbox_legend;
}

GtkWidget *
dv_view_toolbox_get_checkbox_status(dv_view_toolbox_t * T) {
  if (!GTK_IS_WIDGET(T->checkbox_status)) {
    T->checkbox_status = gtk_check_button_new();
    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(T->checkbox_status), T->V->S->show_status);
    g_signal_connect(G_OBJECT(T->checkbox_status), "toggled", G_CALLBACK(on_checkbox_status_toggled), (void *) T->V);
  }
  return T->checkbox_status;
}

GtkWidget *
dv_view_toolbox_get_entry_remark(dv_view_toolbox_t * T) {
  if (!GTK_IS_WIDGET(T->entry_remark)) {
    T->entry_remark = gtk_entry_new();
    gtk_widget_set_tooltip_text(GTK_WIDGET(T->entry_remark), "All ID(s) (e.g. worker numbers) to remark");
    gtk_entry_set_placeholder_text(GTK_ENTRY(T->entry_remark), "e.g. 0, 1, 2");
    g_signal_connect(G_OBJECT(T->entry_remark), "activate", G_CALLBACK(on_entry_remark_activate), (void *) T->V);
  }
  return T->entry_remark;
}

GtkWidget *
dv_view_toolbox_get_checkbox_remain_inner(dv_view_toolbox_t * T) {
  if (!GTK_IS_WIDGET(T->checkbox_remain_inner)) {
    T->checkbox_remain_inner = gtk_check_button_new();
    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(T->checkbox_remain_inner), T->V->S->remain_inner);
    g_signal_connect(G_OBJECT(T->checkbox_remain_inner), "toggled", G_CALLBACK(on_checkbox_remain_inner_toggled), (void *) T->V);
  }
  return T->checkbox_remain_inner;
}

GtkWidget *
dv_view_toolbox_get_checkbox_color_remarked_only(dv_view_toolbox_t * T) {
  if (!GTK_IS_WIDGET(T->checkbox_color_remarked_only)) {
    T->checkbox_color_remarked_only = gtk_check_button_new();
    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(T->checkbox_color_remarked_only), T->V->S->color_remarked_only);
    g_signal_connect(G_OBJECT(T->checkbox_color_remarked_only), "toggled", G_CALLBACK(on_checkbox_color_remarked_only_toggled), (void *) T->V);
  }
  return T->checkbox_color_remarked_only;
}

//gboolean
//on_view_toolbox_window_closed(_unused_ GtkWidget * widget, GdkEvent * event, gpointer user_data) {
//  dv_view_t * V = (dv_view_t *) user_data;
//  gtk_widget_hide(V->T->window);
//}

GtkWidget *
dv_view_toolbox_get_window(dv_view_toolbox_t * T) {
  if (T->window)
    return T->window;
  
  /* Build toolbox window */
  GtkWidget * window = gtk_window_new(GTK_WINDOW_TOPLEVEL);
  char s[DV_STRING_LENGTH];
  sprintf(s, "Toolbox for DAG %ld", T->V - CS->V);
  gtk_window_set_title(GTK_WINDOW(window), s);
  gtk_window_set_position(GTK_WINDOW(window), GTK_WIN_POS_MOUSE);
  gtk_window_set_modal(GTK_WINDOW(window), 0);
  gtk_window_set_transient_for(GTK_WINDOW(window), GTK_WINDOW(CS->gui->window));
  g_signal_connect(G_OBJECT(window), "delete-event", G_CALLBACK(gtk_widget_hide_on_delete), NULL);
  GtkWidget * window_box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 5);
  gtk_container_add(GTK_CONTAINER(window), window_box);
  GtkWidget * notebook = gtk_notebook_new();
  gtk_box_pack_start(GTK_BOX(window_box), notebook, TRUE, TRUE, 0);

  GtkWidget * tab_label;
  GtkWidget * tab_box;

  /* Build tab "Common" */
  {
    tab_label = gtk_label_new("Common");
    tab_box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
    gtk_notebook_append_page(GTK_NOTEBOOK(notebook), tab_box, tab_label);

    GtkWidget * grid = gtk_grid_new();
    gtk_box_pack_start(GTK_BOX(tab_box), grid, TRUE, TRUE, 0);
    int num;
    GtkWidget * label;
    GtkWidget * widget;

    num = 0;
    label = gtk_label_new("              View type: ");
    widget = dv_view_toolbox_get_combobox_lt(T);
    gtk_widget_set_hexpand(GTK_WIDGET(label), TRUE);
    gtk_label_set_justify(GTK_LABEL(label), GTK_JUSTIFY_RIGHT);
    gtk_grid_attach(GTK_GRID(grid), label, 0, num, 1, 1);
    gtk_grid_attach(GTK_GRID(grid), widget, 1, num, 1, 1);
    
    num = 1;
    label = gtk_label_new("             Node color: ");
    widget = dv_view_toolbox_get_combobox_nc(T);
    gtk_grid_attach(GTK_GRID(grid), label, 0, num, 1, 1);
    gtk_grid_attach(GTK_GRID(grid), widget, 1, num, 1, 1);

    num = 2;
    label = gtk_label_new("        Scale-down type: ");
    widget = dv_view_toolbox_get_combobox_sdt(T);
    gtk_grid_attach(GTK_GRID(grid), label, 0, num, 1, 1);
    gtk_grid_attach(GTK_GRID(grid), widget, 1, num, 1, 1);

    num = 3;
    label = gtk_label_new("       Scale-down radix: ");
    widget = dv_view_toolbox_get_entry_radix(T);
    gtk_grid_attach(GTK_GRID(grid), label, 0, num, 1, 1);
    gtk_grid_attach(GTK_GRID(grid), widget, 1, num, 1, 1);

    num = 4;
    label = gtk_label_new("    Scale radix by scrolling: ");
    widget = T->checkbox_scale_radix;
    gtk_grid_attach(GTK_GRID(grid), label, 0, num, 1, 1);
    gtk_grid_attach(GTK_GRID(grid), widget, 1, num, 1, 1);

    num = 5;
    label = gtk_label_new("          Clicking mode: ");
    widget = T->combobox_cm;
    gtk_grid_attach(GTK_GRID(grid), label, 0, num, 1, 1);
    gtk_grid_attach(GTK_GRID(grid), widget, 1, num, 1, 1);

    num = 6;
    label = gtk_label_new("          Hovering mode: ");
    widget = T->combobox_hm;
    gtk_grid_attach(GTK_GRID(grid), label, 0, num, 1, 1);
    gtk_grid_attach(GTK_GRID(grid), widget, 1, num, 1, 1);

    num = 7;
    label = gtk_label_new("Show legend");
    widget = dv_view_toolbox_get_checkbox_legend(T);
    gtk_grid_attach(GTK_GRID(grid), label, 0, num, 1, 1);
    gtk_grid_attach(GTK_GRID(grid), widget, 1, num, 1, 1);

    num = 8;
    label = gtk_label_new("Show status");
    widget = dv_view_toolbox_get_checkbox_status(T);
    gtk_grid_attach(GTK_GRID(grid), label, 0, num, 1, 1);
    gtk_grid_attach(GTK_GRID(grid), widget, 1, num, 1, 1);

    num = 9;
    label = gtk_label_new("Automatically zoom fit DAG");
    widget = T->combobox_azf;
    gtk_grid_attach(GTK_GRID(grid), label, 0, num, 1, 1);
    gtk_grid_attach(GTK_GRID(grid), widget, 1, num, 1, 1);

    num = 10;
    label = gtk_label_new("Auto adjust auto zoom fit");
    widget = T->checkbox_azf;
    gtk_grid_attach(GTK_GRID(grid), label, 0, num, 1, 1);
    gtk_grid_attach(GTK_GRID(grid), widget, 1, num, 1, 1);

    num = 11;
    label = gtk_label_new("Zoom fit DAG horizontally");
    widget = gtk_button_new_with_label("Zoom fit horizontal");
    gtk_widget_set_tooltip_text(GTK_WIDGET(widget), "Zoom fit horizontally (H)");
    g_signal_connect(G_OBJECT(widget), "clicked", G_CALLBACK(on_btn_zoomfit_hor_clicked), (void *) T->V);
    gtk_grid_attach(GTK_GRID(grid), label, 0, num, 1, 1);
    gtk_grid_attach(GTK_GRID(grid), widget, 1, num, 1, 1);

    num = 12;
    label = gtk_label_new("Zoom fit DAG vertically");
    widget = gtk_button_new_with_label("Zoom fit vertical");
    gtk_widget_set_tooltip_text(GTK_WIDGET(widget), "Zoom fit vertically (V)");
    g_signal_connect(G_OBJECT(widget), "clicked", G_CALLBACK(on_btn_zoomfit_ver_clicked), (void *) T->V);
    gtk_grid_attach(GTK_GRID(grid), label, 0, num, 1, 1);
    gtk_grid_attach(GTK_GRID(grid), widget, 1, num, 1, 1);
}
  
  /* Build tab "Advance" */
  {
    tab_label = gtk_label_new("Advance");
    tab_box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
    gtk_notebook_append_page(GTK_NOTEBOOK(notebook), tab_box, tab_label);

    GtkWidget * grid = gtk_grid_new();
    gtk_box_pack_start(GTK_BOX(tab_box), grid, TRUE, TRUE, 0);
    int num;
    GtkWidget * label;
    GtkWidget * widget;

    num = 0;
    label = gtk_label_new("Remark");
    widget = dv_view_toolbox_get_entry_remark(T);
    gtk_grid_attach(GTK_GRID(grid), label, 0, num, 1, 1);
    gtk_grid_attach(GTK_GRID(grid), widget, 1, num, 1, 1);

    num = 1;
    label = gtk_label_new("Remain inner after scanning");
    widget = dv_view_toolbox_get_checkbox_remain_inner(T);
    gtk_grid_attach(GTK_GRID(grid), label, 0, num, 1, 1);
    gtk_grid_attach(GTK_GRID(grid), widget, 1, num, 1, 1);

    num = 2;
    label = gtk_label_new("Scan DAG");
    widget = gtk_button_new_with_label("Scan");
    g_signal_connect(G_OBJECT(widget), "clicked", G_CALLBACK(on_btn_run_dag_scan_clicked), (void *) T->V);
    gtk_grid_attach(GTK_GRID(grid), label, 0, num, 1, 1);
    gtk_grid_attach(GTK_GRID(grid), widget, 1, num, 1, 1);

    num = 3;
    label = gtk_label_new("Color only remarked nodes");
    widget = dv_view_toolbox_get_checkbox_color_remarked_only(T);
    gtk_grid_attach(GTK_GRID(grid), label, 0, num, 1, 1);
    gtk_grid_attach(GTK_GRID(grid), widget, 1, num, 1, 1);    
  }
  
  /* Build tab "Developer" */
  {
    tab_label = gtk_label_new("Developer");
    tab_box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
    gtk_notebook_append_page(GTK_NOTEBOOK(notebook), tab_box, tab_label);

    GtkWidget * grid = gtk_grid_new();
    gtk_box_pack_start(GTK_BOX(tab_box), grid, TRUE, TRUE, 0);
    int num;
    GtkWidget * label;
    GtkWidget * widget;

    num = 0;
    label = gtk_label_new("    Cairo's X Zoom: ");
    widget = T->checkbox_xzoom;
    gtk_grid_attach(GTK_GRID(grid), label, 0, num, 1, 1);
    gtk_grid_attach(GTK_GRID(grid), widget, 1, num, 1, 1);

    num = 1;
    label = gtk_label_new("    Cairo's Y Zoom: ");
    widget = T->checkbox_yzoom;
    gtk_grid_attach(GTK_GRID(grid), label, 0, num, 1, 1);
    gtk_grid_attach(GTK_GRID(grid), widget, 1, num, 1, 1);

    num = 2;
    label = gtk_label_new("Scale Node Width by Scrolling: ");
    widget = T->checkbox_scale_radius;
    gtk_grid_attach(GTK_GRID(grid), label, 0, num, 1, 1);
    gtk_grid_attach(GTK_GRID(grid), widget, 1, num, 1, 1);    

    num = 3;
    label = gtk_label_new(" Search: ");
    widget = T->entry_search;
    gtk_grid_attach(GTK_GRID(grid), label, 0, num, 1, 1);
    gtk_grid_attach(GTK_GRID(grid), widget, 1, num, 1, 1);

    num = 4;
    label = gtk_label_new("    From start time: ");
    widget = dv_view_toolbox_get_combobox_frombt(T);
    gtk_grid_attach(GTK_GRID(grid), label, 0, num, 1, 1);
    gtk_grid_attach(GTK_GRID(grid), widget, 1, num, 1, 1);

    num = 5;
    label = gtk_label_new("      Edge drawing type: ");
    widget = dv_view_toolbox_get_combobox_et(T);
    gtk_grid_attach(GTK_GRID(grid), label, 0, num, 1, 1);
    gtk_grid_attach(GTK_GRID(grid), widget, 1, num, 1, 1);

    num = 6;
    label = gtk_label_new("Affix btwn edges & nodes: ");
    widget = dv_view_toolbox_get_togg_eaffix(T);
    gtk_grid_attach(GTK_GRID(grid), label, 0, num, 1, 1);
    gtk_grid_attach(GTK_GRID(grid), widget, 1, num, 1, 1);
  }

  T->window = window;
  return T->window;
}


void
dv_view_init(dv_view_t * V) {
  V->D = NULL;
  dv_view_status_init(V, V->S);
  int i;
  for (i = 0; i < DV_MAX_VIEWPORT; i++)
    V->mVP[i] = 0;
  V->mainVP = NULL;
  dv_view_toolbox_init(V->T, V);
  
}

dv_view_t * dv_view_create_new_with_dag(dv_dag_t *D) {
  /* Get new VIEW */
  dv_check(CS->nV < DV_MAX_VIEW);
  dv_view_t * V = &CS->V[CS->nV++];
  dv_view_init(V);

  // Set values
  V->D = D;
  D->tolayout[V->S->lt]++;

  return V;
}

void
dv_view_open_toolbox_window(dv_view_t * V) {
  GtkWidget * toolbox_window = dv_view_toolbox_get_window(V->T);
  gtk_widget_show_all(toolbox_window);
}

void
dv_view_change_mainvp(dv_view_t * V, dv_viewport_t * VP) {
  if (V->mainVP == VP)
    return;
  V->mainVP = VP;
  if (!VP)
    return;
  V->S->do_zoomfit = 1;
} 

void
dv_view_add_viewport(dv_view_t * V, dv_viewport_t * VP) {
  int idx = VP - CS->VP;
  if (V->mVP[idx])
    return;
  V->mVP[idx] = 1;
  dv_viewport_add_view(VP, V);
  if (!V->mainVP)
    dv_view_change_mainvp(V, VP);
}

void
dv_view_remove_viewport(dv_view_t * V, dv_viewport_t * VP) {
  int idx = VP - CS->VP;
  if (!V->mVP[idx])
    return;
  V->mVP[idx] = 0;
  dv_viewport_remove_view(VP, V);
  if (V->mainVP == VP) {
    dv_viewport_t * new_vp = NULL;
    int i;
    for (i = 0; i < CS->nVP; i++)
      if (V->mVP[i]) {
        new_vp = &CS->VP[i];
        break;
      }
    dv_view_change_mainvp(V, new_vp);
  }
}

void
dv_view_switch_viewport(dv_view_t * V, dv_viewport_t * VP1, dv_viewport_t * VP2) {
  int sw = 0;
  //fprintf(stderr, "mainvp:%ld, vp1:%ld\n", V->mainVP - CS->VP, VP1 - CS->VP);
  if (V->mainVP == VP1)
    sw = 1;
  dv_view_remove_viewport(V, VP1);
  dv_view_add_viewport(V, VP2);
  if (sw)
    dv_view_change_mainvp(V, VP2);
}

/*-----------------end of VIEW's functions-----------------*/



/*-----------------VIEWPORT's functions-----------------*/

dv_viewport_t *
dv_viewport_create_new() {
  if (CS->nVP >= DV_MAX_VIEWPORT)
    return NULL;
  dv_viewport_t * ret = &CS->VP[CS->nVP];
  CS->nVP++;
  return ret;
}

void
dv_viewport_toolbox_init(dv_viewport_t * VP) {
  char s[DV_STRING_LENGTH];
  sprintf(s, "DAG %ld", VP->mainV - CS->V);

  // White color
  GdkRGBA white[1];
  gdk_rgba_parse(white, "white");

  // Toolbar
  GtkWidget * toolbar = VP->T->toolbar = gtk_toolbar_new();
  g_object_ref(toolbar);
  //gtk_widget_override_background_color(GTK_WIDGET(toolbar), GTK_STATE_FLAG_NORMAL, white);

  /*
  GtkListStore * list_store = gtk_list_store_new(2, G_TYPE_STRING, G_TYPE_STRING);
  GtkTreeIter iter;
  gtk_list_store_append(list_store, &iter);
  gtk_list_store_set(list_store, &iter,
                     0, "abc",
                     -1);
  gtk_list_store_append(list_store, &iter);
  gtk_list_store_set(list_store, &iter,
                     1, "xyz",
                     -1);
  GtkWidget * tree_view = gtk_tree_view_new_with_model(GTK_TREE_MODEL(list_store));

  GtkToolItem * btn_views = gtk_tool_item_new();
  gtk_toolbar_insert(GTK_TOOLBAR(toolbar), btn_views, -1);
  GtkWidget * combobox_views = gtk_combo_box_new_with_model(GTK_TREE_MODEL(list_store));
  gtk_container_add(GTK_CONTAINER(btn_views), combobox_views);
  //gtk_combo_box_set_model(GTK_COMBO_BOX(combobox_views), GTK_TREE_MODEL(list_store));
  */
  
  // Focused toggle
  /*
  GtkToolItem * btn_togg_focused = gtk_tool_item_new();
  gtk_toolbar_insert(GTK_TOOLBAR(toolbar), btn_togg_focused, -1);
  gtk_widget_set_tooltip_text(GTK_WIDGET(btn_togg_focused), "Indicates if this VIEW is focused for hotkeys (use tab key to change btwn VIEWs)");
  GtkWidget * togg_focused = VP->T->togg_focused = gtk_toggle_button_new_with_label(s);
  gtk_container_add(GTK_CONTAINER(btn_togg_focused), togg_focused);
  g_signal_connect(G_OBJECT(togg_focused), "toggled", G_CALLBACK(on_togg_focused_toggled), (void *) VP);
  */

  // Label
  GtkWidget * label = VP->T->label = gtk_label_new(s);
  //GtkWidget * btn_label = gtk_tool_item_new();
  //gtk_toolbar_insert(GTK_TOOLBAR(toolbar), btn_label, -1);
  //gtk_container_add(GTK_CONTAINER(btn_label), label);
  gtk_widget_set_tooltip_text(GTK_WIDGET(label), "The DAG that is focused for e.g. hot keys (change by Tab)");

  // Separator
  //gtk_toolbar_insert(GTK_TOOLBAR(toolbar), gtk_separator_tool_item_new(), -1);

  // View-attribute dialog button
  GtkToolItem *btn_attrs = gtk_tool_button_new(NULL, NULL);
  gtk_toolbar_insert(GTK_TOOLBAR(toolbar), btn_attrs, -1);
  gtk_tool_button_set_icon_name(GTK_TOOL_BUTTON(btn_attrs), "preferences-system");
  gtk_widget_set_tooltip_text(GTK_WIDGET(btn_attrs), "Open dialog to adjust VIEW's attributes (A)");
  g_signal_connect(G_OBJECT(btn_attrs), "clicked", G_CALLBACK(on_viewport_tool_icon_clicked), (void *) VP);

  // Separator
  gtk_toolbar_insert(GTK_TOOLBAR(toolbar), gtk_separator_tool_item_new(), -1);

  // Zoomfit-full button
  GtkToolItem *btn_zoomfit_full = gtk_tool_button_new(NULL, NULL);
  gtk_toolbar_insert(GTK_TOOLBAR(toolbar), btn_zoomfit_full, -1);
  gtk_tool_button_set_icon_name(GTK_TOOL_BUTTON(btn_zoomfit_full), "zoom-fit-best");
  gtk_widget_set_tooltip_text(GTK_WIDGET(btn_zoomfit_full), "Fit full (F)");
  g_signal_connect(G_OBJECT(btn_zoomfit_full), "clicked", G_CALLBACK(on_btn_zoomfit_full_clicked), (void *) VP);

  // Shrink/Expand buttons
  GtkToolItem *btn_shrink = gtk_tool_button_new(NULL, NULL);
  GtkToolItem *btn_expand = gtk_tool_button_new(NULL, NULL);
  gtk_toolbar_insert(GTK_TOOLBAR(toolbar), btn_shrink, -1);
  gtk_toolbar_insert(GTK_TOOLBAR(toolbar), btn_expand, -1);
  gtk_tool_button_set_icon_name(GTK_TOOL_BUTTON(btn_shrink), "zoom-out");
  gtk_tool_button_set_icon_name(GTK_TOOL_BUTTON(btn_expand), "zoom-in");
  gtk_widget_set_tooltip_text(GTK_WIDGET(btn_shrink), "Collapse one depth (C)"); 
  gtk_widget_set_tooltip_text(GTK_WIDGET(btn_expand), "Expand one depth (X)");
  g_signal_connect(G_OBJECT(btn_shrink), "clicked", G_CALLBACK(on_btn_shrink_clicked), (void *) VP);  
  g_signal_connect(G_OBJECT(btn_expand), "clicked", G_CALLBACK(on_btn_expand_clicked), (void *) VP);
}

void
dv_viewport_init(dv_viewport_t * VP) {
  VP->split = 0;
  // Frame
  //char s[DV_STRING_LENGTH];
  //sprintf(s, "Viewport %ld", VP - CS->VP);
  VP->frame = gtk_frame_new(NULL);
  g_object_ref(G_OBJECT(VP->frame));
  // Paned
  VP->paned = gtk_paned_new(GTK_ORIENTATION_HORIZONTAL);
  g_object_ref(G_OBJECT(VP->paned));
  VP->orientation = GTK_ORIENTATION_HORIZONTAL;
  VP->vp1 = VP->vp2 = NULL;
  // White color
  GdkRGBA white[1];
  gdk_rgba_parse(white, "white");
  // Box
  VP->box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
  g_object_ref(G_OBJECT(VP->box));
  gtk_container_add(GTK_CONTAINER(VP->frame), GTK_WIDGET(VP->box));
  // Drawing Area
  VP->darea = gtk_drawing_area_new();
  g_object_ref(G_OBJECT(VP->darea));
  gtk_box_pack_end(GTK_BOX(VP->box), VP->darea, TRUE, TRUE, 0);
  GtkWidget * darea = VP->darea;
  gtk_widget_override_background_color(GTK_WIDGET(darea), GTK_STATE_FLAG_NORMAL, white);
  g_signal_connect(G_OBJECT(darea), "draw", G_CALLBACK(on_draw_event), (void *) VP);
  gtk_widget_add_events(GTK_WIDGET(darea), GDK_SCROLL_MASK | GDK_BUTTON_PRESS_MASK | GDK_BUTTON_RELEASE_MASK | GDK_POINTER_MOTION_MASK);
  g_signal_connect(G_OBJECT(darea), "scroll-event", G_CALLBACK(on_scroll_event), (void *) VP);
  g_signal_connect(G_OBJECT(darea), "button-press-event", G_CALLBACK(on_button_event), (void *) VP);
  g_signal_connect(G_OBJECT(darea), "button-release-event", G_CALLBACK(on_button_event), (void *) VP);
  g_signal_connect(G_OBJECT(darea), "motion-notify-event", G_CALLBACK(on_motion_event), (void *) VP);
  g_signal_connect(G_OBJECT(darea), "configure-event", G_CALLBACK(on_darea_configure_event), (void *) VP);
  // VP <-> V
  int i;
  for (i = 0; i < DV_MAX_VIEW; i++)
    VP->mV[i] = 0;
  // vpw, vph
  VP->vpw = VP->vph = 0.0;
  dv_viewport_toolbox_init(VP);
  gtk_box_pack_start(GTK_BOX(VP->box), VP->T->toolbar, FALSE, FALSE, 0);
}

void dv_viewport_show(dv_viewport_t * vp);

void
dv_viewport_show_children(dv_viewport_t * vp) {
  dv_check(vp->split);
  dv_check(vp->paned);
  // Child 1
  if (vp->vp1) {
    dv_viewport_t * vp1 = vp->vp1;
    gtk_paned_pack1(GTK_PANED(vp->paned), vp1->frame, TRUE, TRUE);
    dv_viewport_show(vp1);
  }
  // Child 2
  if (vp->vp2) {
    dv_viewport_t * vp2 = vp->vp2;
    gtk_paned_pack2(GTK_PANED(vp->paned), vp2->frame, TRUE, TRUE);
    dv_viewport_show(vp2);
  }
}

void
dv_viewport_show(dv_viewport_t * VP) {
  GtkWidget * child = gtk_bin_get_child(GTK_BIN(VP->frame));
  if (VP->split) {
    if (child != VP->paned) {
      if (child) gtk_container_remove(GTK_CONTAINER(VP->frame), child);
      dv_check(VP->paned);
      gtk_container_add(GTK_CONTAINER(VP->frame), VP->paned);
      dv_viewport_show_children(VP);
    }
  } else {
    if (child != VP->box) {
      if (child) gtk_container_remove(GTK_CONTAINER(VP->frame), child);
      gtk_container_add(GTK_CONTAINER(VP->frame), VP->box);
    }
  }
}

void
dv_viewport_change_split(dv_viewport_t * VP, int new_split) {
  int old_split = VP->split;
  if (new_split == old_split) return;
  if (new_split) {
    // Check children's existence
    if (!VP->vp1) {
      VP->vp1 = dv_viewport_create_new();
      if (VP->vp1)
        dv_viewport_init(VP->vp1);
    }
    int i;
    for (i=0; i<CS->nV; i++)
      if (VP->mV[i]) {
        dv_view_switch_viewport(&CS->V[i], VP, VP->vp1);
      }
    if (!VP->vp2) {
      VP->vp2 = dv_viewport_create_new();
      if (VP->vp2)
        dv_viewport_init(VP->vp2);
    }
  } else {
    int i;
    for (i=0; i<CS->nV; i++)
      if (VP->vp1->mV[i]) {
        dv_view_switch_viewport(&CS->V[i], VP->vp1, VP);
      }
  }
  VP->split = new_split;
  dv_viewport_show(VP);
}

void
dv_viewport_change_orientation(dv_viewport_t * vp, GtkOrientation o) {
  GtkWidget * child1 = NULL;
  GtkWidget * child2 = NULL;
  if (vp->paned) {
    child1 = gtk_paned_get_child1(GTK_PANED(vp->paned));
    if (child1)
      gtk_container_remove(GTK_CONTAINER(vp->paned), child1);
    child2 = gtk_paned_get_child2(GTK_PANED(vp->paned));
    if (child2)
      gtk_container_remove(GTK_CONTAINER(vp->paned), child2);
    gtk_widget_destroy(GTK_WIDGET(vp->paned));
  }
  vp->paned = gtk_paned_new(o);
  g_object_ref(vp->paned);
  vp->orientation = o;
  dv_viewport_show(vp);
}

void
dv_viewport_change_mainv(dv_viewport_t * VP, dv_view_t * V) {
  if (VP->mainV == V)
    return;
  VP->mainV = V;
  char s[DV_STRING_LENGTH];
  if (!V) {
    sprintf(s, "None");
  } else {
    sprintf(s, "DAG %ld", V - CS->V);
    //V->S->do_zoomfit = 1;
  }
  gtk_label_set_text(GTK_LABEL(VP->T->label), s);
} 

void
dv_viewport_add_view(dv_viewport_t * VP, dv_view_t * V) {
  int idx = V - CS->V;
  dv_check(!VP->mV[idx]);
  VP->mV[idx] = 1;
  if (!VP->mainV)
    dv_viewport_change_mainv(VP, V);
}

void
dv_viewport_remove_view(dv_viewport_t * VP, dv_view_t * V) {
  int idx = V - CS->V;
  dv_check(VP->mV[idx]);
  VP->mV[idx] = 0;
  if (VP->mainV == V) {
    dv_view_t * new_main_v = NULL;
    int i;
    for (i = 0; i < CS->nV; i++)
      if (VP->mV[i]) {
        new_main_v = &CS->V[i];
        break;
      }
    dv_viewport_change_mainv(VP, new_main_v);
  }
}

static GtkWidget *
dv_viewport_create_frame(dv_viewport_t * VP) {
  char s[DV_STRING_LENGTH];
  sprintf(s, "Viewport %ld", VP - CS->VP);
  GtkWidget * frame = gtk_frame_new(s);
  if (VP->split) {
    GtkWidget * paned = gtk_paned_new(VP->orientation);
    gtk_container_add(GTK_CONTAINER(frame), paned);
    if (VP->vp1)
      gtk_paned_pack1(GTK_PANED(paned), dv_viewport_create_frame(VP->vp1), TRUE, TRUE);
    if (VP->vp2)
      gtk_paned_pack2(GTK_PANED(paned), dv_viewport_create_frame(VP->vp2), TRUE, TRUE);
  }
  return frame;
}

static void dv_viewport_redraw_configure_box();
static void dv_alternate_menubar();

static gboolean
on_viewport_options_split_changed(_unused_ GtkWidget * widget, gpointer user_data) {
  dv_viewport_t * VP = (dv_viewport_t *) user_data;
  int active = gtk_combo_box_get_active(GTK_COMBO_BOX(widget));
  if (active == 0) {
    // No-split
    if (VP->split != 0)
      dv_viewport_change_split(VP, 0);
  } else {
    // Split
    if (VP->split != 1)
      dv_viewport_change_split(VP, 1);
  }

  // Redraw viewports
  gtk_widget_show_all(GTK_WIDGET(VP->frame));
  gtk_widget_queue_draw(GTK_WIDGET(VP->frame));
  // Redraw viewport configure dialog
  dv_viewport_redraw_configure_box();
  // Redraw menubar
  dv_alternate_menubar();
  
  return TRUE;
}

static gboolean
on_viewport_options_orientation_changed(_unused_ GtkWidget * widget, gpointer user_data) {
  dv_viewport_t * vp = (dv_viewport_t *) user_data;
  int active = gtk_combo_box_get_active(GTK_COMBO_BOX(widget));
  if (active == 0) {
    // Horizontal
    if (vp->orientation != GTK_ORIENTATION_HORIZONTAL)
      dv_viewport_change_orientation(vp, GTK_ORIENTATION_HORIZONTAL);
  } else {
    // Vertical
    if (vp->orientation != GTK_ORIENTATION_VERTICAL)
      dv_viewport_change_orientation(vp, GTK_ORIENTATION_VERTICAL);
  }

  // Redraw viewports
  gtk_widget_show_all(GTK_WIDGET(vp->frame));
  gtk_widget_queue_draw(GTK_WIDGET(vp->frame));
  // Redraw viewport configure dialog
  dv_viewport_redraw_configure_box();
  
  return TRUE;
}

static void
dv_viewport_update_configure_box() {
  // Box
  GtkWidget * box = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 5);
  GList * list = gtk_container_get_children(GTK_CONTAINER(CS->box_viewport_configure));
  GtkWidget * child = (GtkWidget *) g_list_nth_data(list, 0);
  if (child)
    gtk_container_remove(GTK_CONTAINER(CS->box_viewport_configure), child);
  gtk_box_pack_start(GTK_BOX(CS->box_viewport_configure), box, TRUE, TRUE, 3);

  // Left
  GtkWidget * left = dv_viewport_create_frame(CS->VP);
  gtk_box_pack_start(GTK_BOX(box), left, TRUE, TRUE, 3);

  // Separator
  gtk_box_pack_start(GTK_BOX(box), gtk_separator_new(GTK_ORIENTATION_VERTICAL), FALSE, FALSE, 4);

  // Right
  GtkWidget * right = gtk_scrolled_window_new(NULL, NULL);
  gtk_box_pack_start(GTK_BOX(box), right, FALSE, FALSE, 3);
  gtk_widget_set_size_request(GTK_WIDGET(right), 315, 0);
  GtkWidget * right_box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 5);
  gtk_container_add(GTK_CONTAINER(right), right_box);
  int i;
  for (i=0; i<CS->nVP; i++) {
    char s[DV_STRING_LENGTH];
    sprintf(s, "Viewport %d: ", i);
    GtkWidget * vp_frame = gtk_frame_new(NULL);
    gtk_box_pack_start(GTK_BOX(right_box), vp_frame, FALSE, FALSE, 3);
    GtkWidget * vp_hbox = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 5);
    gtk_container_add(GTK_CONTAINER(vp_frame), vp_hbox);

    // Label
    GtkWidget * label = gtk_label_new(s);
    gtk_box_pack_start(GTK_BOX(vp_hbox), label, FALSE, FALSE, 3);

    // Split
    GtkWidget * split_combobox = gtk_combo_box_text_new();
    gtk_box_pack_start(GTK_BOX(vp_hbox), split_combobox, TRUE, FALSE, 0);
    gtk_combo_box_text_append(GTK_COMBO_BOX_TEXT(split_combobox), "nosplit", "No split");
    gtk_combo_box_text_append(GTK_COMBO_BOX_TEXT(split_combobox), "split", "Split");
    gtk_combo_box_set_active(GTK_COMBO_BOX(split_combobox), CS->VP[i].split);
    g_signal_connect(G_OBJECT(split_combobox), "changed", G_CALLBACK(on_viewport_options_split_changed), (void *) &CS->VP[i]);

    // Orientation
    GtkWidget * orient_combobox = gtk_combo_box_text_new();
    gtk_box_pack_start(GTK_BOX(vp_hbox), orient_combobox, TRUE, FALSE, 0);
    gtk_combo_box_text_append(GTK_COMBO_BOX_TEXT(orient_combobox), "horizontal", "Horizontally");
    gtk_combo_box_text_append(GTK_COMBO_BOX_TEXT(orient_combobox), "vertical", "Vertically");
    gtk_combo_box_set_active(GTK_COMBO_BOX(orient_combobox), CS->VP[i].orientation);
    g_signal_connect(G_OBJECT(orient_combobox), "changed", G_CALLBACK(on_viewport_options_orientation_changed), (void *) &CS->VP[i]);

  }
}

static void
dv_viewport_redraw_configure_box() {
  dv_viewport_update_configure_box();
  gtk_widget_show_all(CS->box_viewport_configure);
  gtk_widget_queue_draw(GTK_WIDGET(CS->box_viewport_configure));
}

/*-----------------end of VIEWPORT's functions-----------------*/



/*-----------------Menubar functions-----------------*/

static GtkWidget * dv_create_menubar();

static void
dv_alternate_menubar() {
  gtk_container_remove(GTK_CONTAINER(CS->gui->vbox0), CS->gui->menubar);
  CS->gui->menubar = dv_create_menubar();
  gtk_box_pack_start(GTK_BOX(CS->gui->vbox0), CS->gui->menubar, FALSE, FALSE, 0);
  gtk_widget_show_all(CS->gui->menubar);
  gtk_widget_queue_draw(GTK_WIDGET(CS->gui->menubar));
}

static void
on_view_add_new(_unused_ GtkMenuItem * menuitem, gpointer user_data) {
  // Create new view
  dv_dag_t * D = (dv_dag_t *) user_data;
  dv_view_t * V = dv_view_create_new_with_dag(D);
  if (V) {
    dv_view_layout(V);
    // Alternate menubar
    dv_alternate_menubar();
  }
}

static void
on_dag_add_new(_unused_ GtkMenuItem * menuitem, gpointer user_data) {
  // Create new view
  dv_pidag_t * P = (dv_pidag_t *) user_data;
  dv_dag_t * D = dv_dag_create_new_with_pidag(P);
  if (D) {
    // Alternate menubar
    dv_alternate_menubar();
  }
}

static void
on_menu_item_view_samples_clicked(_unused_ GtkToolButton * toolbtn, _unused_ gpointer user_data) {
  dv_pidag_t * P = CS->activeV->D->P;
  CS->btviewer->P = P;

  // Build dialog
  GtkWidget * dialog = gtk_dialog_new();
  gtk_window_set_title(GTK_WINDOW(dialog), "View Backtrace Samples");
  gtk_window_set_default_size(GTK_WINDOW(dialog), 800, 700);
  GtkWidget * dialog_vbox = gtk_dialog_get_content_area(GTK_DIALOG(dialog));
  gtk_box_set_spacing(GTK_BOX(dialog_vbox), 5);

  // HBox 1: dag file
  GtkWidget * hbox1 = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 5);
  gtk_box_pack_start(GTK_BOX(dialog_vbox), hbox1, FALSE, FALSE, 0);
  GtkWidget * hbox1_label = gtk_label_new("DAG File:");
  gtk_box_pack_start(GTK_BOX(hbox1), hbox1_label, FALSE, FALSE, 0);
  GtkWidget * hbox1_filename = CS->btviewer->label_dag_file_name;
  gtk_box_pack_start(GTK_BOX(hbox1), hbox1_filename, TRUE, TRUE, 0);
  gtk_label_set_text(GTK_LABEL(hbox1_filename), P->fn);
  
  // HBox 2: bt file
  GtkWidget * hbox2 = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 5);
  gtk_box_pack_start(GTK_BOX(dialog_vbox), hbox2, FALSE, FALSE, 0);
  GtkWidget * hbox2_label = gtk_label_new("        BT File:");
  gtk_box_pack_start(GTK_BOX(hbox2), hbox2_label, FALSE, FALSE, 0);
  GtkWidget * hbox2_filename = CS->btviewer->entry_bt_file_name;
  gtk_box_pack_start(GTK_BOX(hbox2), hbox2_filename, TRUE, TRUE, 0);
  GtkWidget * hbox2_btn = gtk_button_new_with_label("Choose file");
  gtk_box_pack_start(GTK_BOX(hbox2), hbox2_btn, FALSE, FALSE, 0);
  g_signal_connect(G_OBJECT(hbox2_btn), "clicked", G_CALLBACK(on_btn_choose_bt_file_clicked), (void *) CS->btviewer);
  
  // HBox 3: binary file
  GtkWidget * hbox3 = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 5);
  gtk_box_pack_start(GTK_BOX(dialog_vbox), hbox3, FALSE, FALSE, 0);
  GtkWidget * hbox3_label = gtk_label_new("Binary File:");
  gtk_box_pack_start(GTK_BOX(hbox3), hbox3_label, FALSE, FALSE, 0);
  GtkWidget * hbox3_filename = CS->btviewer->entry_binary_file_name;
  gtk_box_pack_start(GTK_BOX(hbox3), hbox3_filename, TRUE, TRUE, 0);
  GtkWidget * hbox3_btn = gtk_button_new_with_label("Choose file");
  gtk_box_pack_start(GTK_BOX(hbox3), hbox3_btn, FALSE, FALSE, 0);
  g_signal_connect(G_OBJECT(hbox3_btn), "clicked", G_CALLBACK(on_btn_choose_binary_file_clicked), (void *) CS->btviewer);

  // HBox 4: node ID
  GtkWidget * hbox4 = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 5);
  gtk_box_pack_start(GTK_BOX(dialog_vbox), hbox4, FALSE, FALSE, 0);
  GtkWidget * hbox4_label = gtk_label_new("Node ID:");
  gtk_box_pack_start(GTK_BOX(hbox4), hbox4_label, FALSE, FALSE, 0);
  GtkWidget * hbox4_nodeid = CS->btviewer->entry_node_id;
  gtk_box_pack_start(GTK_BOX(hbox4), hbox4_nodeid, FALSE, FALSE, 0);
  gtk_entry_set_max_length(GTK_ENTRY(hbox4_nodeid), 10);
  GtkWidget * hbox4_btn = gtk_button_new_with_label("Find node");
  gtk_box_pack_start(GTK_BOX(hbox4), hbox4_btn, FALSE, FALSE, 0);
  g_signal_connect(G_OBJECT(hbox4_btn), "clicked", G_CALLBACK(on_btn_find_node_clicked), (void *) CS->btviewer);

  // HBox 5: worker & time interval
  GtkWidget * hbox5 = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 5);
  gtk_box_pack_start(GTK_BOX(dialog_vbox), hbox5, FALSE, FALSE, 0);
  GtkWidget * hbox5_label = gtk_label_new("On worker");
  gtk_box_pack_start(GTK_BOX(hbox5), hbox5_label, FALSE, FALSE, 0);
  GtkWidget * hbox5_combo = CS->btviewer->combobox_worker;
  gtk_box_pack_start(GTK_BOX(hbox5), hbox5_combo, FALSE, FALSE, 0);
  gtk_combo_box_text_remove_all(GTK_COMBO_BOX_TEXT(hbox5_combo));
  int i;
  char str[10];
  for (i=0; i<P->num_workers; i++) {
    sprintf(str, "%d", i);
    gtk_combo_box_text_append(GTK_COMBO_BOX_TEXT(hbox5_combo), str, str);
  }
  GtkWidget * hbox5_label2 = gtk_label_new("from clock");
  gtk_box_pack_start(GTK_BOX(hbox5), hbox5_label2, FALSE, FALSE, 0);
  GtkWidget * hbox5_from = CS->btviewer->entry_time_from;
  gtk_box_pack_start(GTK_BOX(hbox5), hbox5_from, TRUE, TRUE, 0);
  GtkWidget * hbox5_label3 = gtk_label_new("to clock");
  gtk_box_pack_start(GTK_BOX(hbox5), hbox5_label3, FALSE, FALSE, 0);
  GtkWidget * hbox5_to = CS->btviewer->entry_time_to;
  gtk_box_pack_start(GTK_BOX(hbox5), hbox5_to, TRUE, TRUE, 0);

  // HBox 6: run button
  GtkWidget * hbox6 = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 5);
  gtk_box_pack_start(GTK_BOX(dialog_vbox), hbox6, FALSE, FALSE, 0);
  GtkWidget * hbox6_btn = gtk_button_new_with_label("Run");
  gtk_box_pack_end(GTK_BOX(hbox6), hbox6_btn, FALSE, FALSE, 0);
  g_signal_connect(G_OBJECT(hbox6_btn), "clicked", G_CALLBACK(on_btn_run_view_bt_samples_clicked), (void *) CS->btviewer);

  // Text view
  GtkWidget *scrolledwindow = gtk_scrolled_window_new(NULL, NULL);
  gtk_box_pack_end(GTK_BOX(dialog_vbox), scrolledwindow, TRUE, TRUE, 0);
  gtk_container_add(GTK_CONTAINER(scrolledwindow), CS->btviewer->text_view);

  // Run
  gtk_widget_show_all(dialog_vbox);
  gtk_dialog_run(GTK_DIALOG(dialog));

  // Destroy
  gtk_container_remove(GTK_CONTAINER(hbox1), CS->btviewer->label_dag_file_name);
  gtk_container_remove(GTK_CONTAINER(hbox2), CS->btviewer->entry_bt_file_name);
  gtk_container_remove(GTK_CONTAINER(hbox3), CS->btviewer->entry_binary_file_name);
  gtk_container_remove(GTK_CONTAINER(hbox4), CS->btviewer->entry_node_id);
  gtk_container_remove(GTK_CONTAINER(hbox5), CS->btviewer->combobox_worker);
  gtk_container_remove(GTK_CONTAINER(hbox5), CS->btviewer->entry_time_from);
  gtk_container_remove(GTK_CONTAINER(hbox5), CS->btviewer->entry_time_to);
  gtk_container_remove(GTK_CONTAINER(scrolledwindow), CS->btviewer->text_view);
  gtk_widget_destroy(dialog);
}

static void
on_help_hotkeys_clicked(_unused_ GtkToolButton * toolbtn, _unused_ gpointer user_data) {
  // Build dialog
  GtkWidget * dialog = gtk_dialog_new();
  gtk_window_set_title(GTK_WINDOW(dialog), "Hotkeys");
  gtk_window_set_default_size(GTK_WINDOW(dialog), 300, 170);
  GtkWidget * dialog_vbox = gtk_dialog_get_content_area(GTK_DIALOG(dialog));

  // Label
  GtkWidget * label = gtk_label_new("\n"
                                    "Tab : switch control between VIEWs\n"
                                    "Ctrl + 1 : dag view\n"
                                    "Ctrl + 2 : dagbox view\n"
                                    "Ctrl + 3 : vertical timeline view\n"
                                    "Ctrl + 4 : horizontal timeline view\n"
                                    "X : expand\n"
                                    "C : collapse\n"
                                    "H : horizontal fit\n"
                                    "V : vertical fit\n"
                                    "A : open view configuration dialog\n"
                                    "Alt + [some key] : access menu\n"
                                    "Arrow Keys : move around\n");
  gtk_box_pack_start(GTK_BOX(dialog_vbox), label, TRUE, FALSE, 0);

  // Run
  gtk_widget_show_all(dialog_vbox);
  gtk_dialog_run(GTK_DIALOG(dialog));

  // Destroy
  gtk_widget_destroy(dialog);  
}

static void
dv_viewport_export_to_surface(dv_viewport_t * VP, cairo_surface_t * surface) {
  cairo_t * cr = cairo_create(surface);
  // Whiten background
  GdkRGBA white[1];
  gdk_rgba_parse(white, "white");
  cairo_set_source_rgba(cr, white->red, white->green, white->blue, white->alpha);
  cairo_paint(cr);
  // Draw viewport
  dv_viewport_draw(VP, cr);
  // Finish
  cairo_destroy(cr);
}

static void
on_file_export_clicked(_unused_ GtkToolButton * toolbtn, _unused_ gpointer user_data) {
  dv_view_t * V = CS->activeV;
  if (!V) {
    fprintf(stderr, "Warning: there is no active V to export.\n");
    return;
  }
  dv_viewport_t * VP = V->mainVP;
  if (!VP) {
    fprintf(stderr, "Warning: there is no main VP for the active V.\n");
    return;
  }
  cairo_surface_t * surface;

  /* PNG */
  surface = cairo_image_surface_create(CAIRO_FORMAT_ARGB32, VP->vpw, VP->vph);
  dv_viewport_export_to_surface(VP, surface);
  cairo_surface_write_to_png(surface, "00dv.png");
  fprintf(stdout, "Exported viewport %ld to 00dv.png\n", VP - CS->VP);
  cairo_surface_destroy(surface);

  /* EPS */
  surface = cairo_ps_surface_create("00dv.eps", VP->vpw, VP->vph);
  cairo_ps_surface_set_eps(surface, TRUE);
  dv_viewport_export_to_surface(VP, surface);
  fprintf(stdout, "Exported viewport %ld to 00dv.eps\n", VP - CS->VP);
  cairo_surface_destroy(surface);

  /* SVG */
  surface = cairo_svg_surface_create("00dv.svg", VP->vpw, VP->vph);
  dv_viewport_export_to_surface(VP, surface);
  fprintf(stdout, "Exported viewport %ld to 00dv.svg\n", VP - CS->VP);
  cairo_surface_destroy(surface);

  return;  
}


static void
dv_export_viewports_get_size_r(dv_viewport_t * VP, double * w, double * h) {
  if (!VP) {
    *w = 0.0;
    *h = 0.0;
  } else if (!VP->split) {
    *w = VP->vpw;
    *h = VP->vph;
  } else {
    double w1, h1, w2, h2;
    dv_export_viewports_get_size_r(VP->vp1, &w1, &h1);
    dv_export_viewports_get_size_r(VP->vp2, &w2, &h2);
    if (VP->orientation == GTK_ORIENTATION_HORIZONTAL) {
      *w = w1 + w2;
      *h = dv_max(h1, h2);
    } else {
      *w = dv_max(w1, w2);
      *h = h1 + h2;
    }
  }
}

static void
dv_export_viewports_to_img_r(dv_viewport_t * VP, cairo_surface_t * surface, double x, double y) {
  if (!VP) {
    return;
  } else if (!VP->split) {
    cairo_surface_t * surface_child = cairo_surface_create_for_rectangle(surface, x, y, VP->vpw, VP->vph);
    dv_viewport_export_to_surface(VP, surface_child);
  } else {
    double w1, h1;
    dv_export_viewports_get_size_r(VP->vp1, &w1, &h1);
    if (VP->orientation == GTK_ORIENTATION_HORIZONTAL) {
      dv_export_viewports_to_img_r(VP->vp1, surface, x, y);
      dv_export_viewports_to_img_r(VP->vp2, surface, x + w1, y);
    } else {
      dv_export_viewports_to_img_r(VP->vp1, surface, x, y);
      dv_export_viewports_to_img_r(VP->vp2, surface, x, y + h1);
    }
  }
}

static void
dv_export_viewports_to_eps_r(dv_viewport_t * VP, cairo_t * cr, double x, double y) {
  if (!VP) {
    return;
  } else if (!VP->split) {
    cairo_save(cr);
    cairo_translate(cr, x, y);
    dv_viewport_draw(VP, cr);
    cairo_restore(cr);
  } else {
    double w1, h1;
    dv_export_viewports_get_size_r(VP->vp1, &w1, &h1);
    if (VP->orientation == GTK_ORIENTATION_HORIZONTAL) {
      dv_export_viewports_to_eps_r(VP->vp1, cr, x, y);
      dv_export_viewports_to_eps_r(VP->vp2, cr, x + w1, y);
    } else {
      dv_export_viewports_to_eps_r(VP->vp1, cr, x, y);
      dv_export_viewports_to_eps_r(VP->vp2, cr, x, y + h1);
    }
  }
}

static void
dv_export_viewports_to_svg_r(dv_viewport_t * VP, cairo_t * cr, double x, double y) {
  if (!VP) {
    return;
  } else if (!VP->split) {
    cairo_save(cr);
    cairo_translate(cr, x, y);
    dv_viewport_draw(VP, cr);
    cairo_restore(cr);
  } else {
    double w1, h1;
    dv_export_viewports_get_size_r(VP->vp1, &w1, &h1);
    if (VP->orientation == GTK_ORIENTATION_HORIZONTAL) {
      dv_export_viewports_to_svg_r(VP->vp1, cr, x, y);
      dv_export_viewports_to_svg_r(VP->vp2, cr, x + w1, y);
    } else {
      dv_export_viewports_to_svg_r(VP->vp1, cr, x, y);
      dv_export_viewports_to_svg_r(VP->vp2, cr, x, y + h1);
    }
  }
}

static void
on_file_export_all_clicked(_unused_ GtkToolButton * toolbtn, _unused_ gpointer user_data) {
  double w, h;
  dv_export_viewports_get_size_r(CS->VP, &w, &h);
  cairo_surface_t * surface;
  cairo_t * cr;
  
  /* PNG */
  surface = cairo_image_surface_create(CAIRO_FORMAT_ARGB32, w, h);
  dv_export_viewports_to_img_r(CS->VP, surface, 0.0, 0.0);
  cairo_surface_write_to_png(surface, "00dv.png");
  fprintf(stdout, "Exported all viewports to 00dv.png\n");
  cairo_surface_destroy(surface);
  
  GdkRGBA white[1];
  gdk_rgba_parse(white, "white");

  /* EPS */
  surface = cairo_ps_surface_create("00dv.eps", w, h);
  cairo_ps_surface_set_eps(surface, TRUE);
  cr = cairo_create(surface);
  // Whiten background
  cairo_set_source_rgba(cr, white->red, white->green, white->blue, white->alpha);
  cairo_paint(cr);
  dv_export_viewports_to_eps_r(CS->VP, cr, 0.0, 0.0);
  fprintf(stdout, "Exported all viewports to 00dv.eps\n");
  cairo_destroy(cr);
  cairo_surface_destroy(surface);
  
  /* EPS */
  surface = cairo_svg_surface_create("00dv.svg", w, h);
  cr = cairo_create(surface);
  // Whiten background
  cairo_set_source_rgba(cr, white->red, white->green, white->blue, white->alpha);
  cairo_paint(cr);
  dv_export_viewports_to_svg_r(CS->VP, cr, 0.0, 0.0);
  fprintf(stdout, "Exported all viewports to 00dv.svg\n");
  cairo_destroy(cr);
  cairo_surface_destroy(surface);
  
  return;
}


static void
on_viewport_select_view(GtkCheckMenuItem * checkmenuitem, gpointer user_data) {
  dv_viewport_t * vp = (dv_viewport_t *) user_data;
  const gchar * label = gtk_menu_item_get_label(GTK_MENU_ITEM(checkmenuitem));
  gboolean active = gtk_check_menu_item_get_active(GTK_CHECK_MENU_ITEM(checkmenuitem));
  // Find V
  char s[DV_STRING_LENGTH];
  int i;
  for (i=0; i<CS->nV; i++) {
    sprintf(s, "VIEW _%d", i);
    if (strcmp(s, label) == 0)
      break;
  }
  if (i >= CS->nV) {
    fprintf(stderr, "on_viewport_select_view: could not find view (%s)\n", label);
    return;
  }
  dv_view_t * v = &CS->V[i];
  // Actions
  if (active) {
    dv_view_add_viewport(v, vp);
  } else {
    dv_view_remove_viewport(v, vp);
  }
  gtk_widget_show_all(GTK_WIDGET(vp->frame));
  gtk_widget_queue_draw(GTK_WIDGET(vp->frame));
}

static void
on_viewport_configure_clicked(_unused_ GtkMenuItem * menuitem, _unused_ gpointer user_data) {
  GtkWidget * dialog = gtk_dialog_new();
  gtk_window_set_title(GTK_WINDOW(dialog), "Configure Viewports");
  gtk_window_set_default_size(GTK_WINDOW(dialog), 800, 400);
  
  GtkWidget * box = CS->box_viewport_configure;
  if (!box) {
    box = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 5);
    g_object_ref(box);
    CS->box_viewport_configure = box;
  }

  // Update dialog's content
  dv_viewport_update_configure_box();

  // GUI
  GtkWidget * vbox = gtk_dialog_get_content_area(GTK_DIALOG(dialog));
  gtk_box_pack_start(GTK_BOX(vbox), box, TRUE, TRUE, 0);

  // Run
  gtk_widget_show_all(dialog);
  gtk_dialog_run(GTK_DIALOG(dialog));

  // Destroy
  gtk_container_remove(GTK_CONTAINER(vbox), box);
  gtk_widget_destroy(dialog);
}

static gboolean
on_stat_distribution_dag_changed(GtkWidget * widget, gpointer user_data) {
  long i = (long) user_data;
  int new_id = gtk_combo_box_get_active(GTK_COMBO_BOX(widget)) - 1;
  char * new_title = "";
  if (new_id >= 0)
    new_title = dv_filename_get_short_name(CS->D[new_id].P->fn);
  dv_stat_distribution_entry_t * e = &CS->SD->e[i];
  int old_id = e->dag_id;
  char * old_title = NULL;
  if (old_id >= 0)
    old_title = dv_filename_get_short_name(CS->D[old_id].P->fn);
  if ( !e->title || strlen(e->title) == 0 ||
       (old_title && strcmp(e->title, old_title) == 0) ) {
    if (e->title && strlen(e->title))
      dv_free(e->title, strlen(e->title) + 1);
    e->title = new_title;
    if (e->title_entry) 
      gtk_entry_set_text(GTK_ENTRY(e->title_entry), e->title);
  } else {
    if (strlen(new_title))
      dv_free(new_title, strlen(new_title) + 1);
  }
  e->dag_id = new_id;
  return TRUE;
}

static gboolean
on_stat_distribution_type_changed(GtkWidget * widget, gpointer user_data) {
  long i = (long) user_data;
  int new_type = gtk_combo_box_get_active(GTK_COMBO_BOX(widget));
  CS->SD->e[i].type = new_type;
  return TRUE;
}

static gboolean
on_stat_distribution_stolen_changed(GtkWidget * widget, gpointer user_data) {
  long i = (long) user_data;
  int new_stolen = gtk_combo_box_get_active(GTK_COMBO_BOX(widget));
  CS->SD->e[i].stolen = new_stolen;
  return TRUE;
}

static gboolean
on_stat_distribution_title_activate(GtkWidget * widget, gpointer user_data) {
  long i = (long) user_data;
  const char * new_title = gtk_entry_get_text(GTK_ENTRY(widget));
  dv_stat_distribution_entry_t * e = &CS->SD->e[i];
  if (strlen(e->title))
    dv_free(e->title, strlen(e->title) + 1);
  e->title = dv_malloc( sizeof(char) * (strlen(new_title) + 1) );
  strcpy(e->title, new_title);
  return TRUE;
}

static gboolean
on_stat_distribution_xrange_from_activate(_unused_ GtkWidget * widget, _unused_ gpointer user_data) {
  long new_xrange_from = atol(gtk_entry_get_text(GTK_ENTRY(widget)));
  CS->SD->xrange_from = new_xrange_from;
  return TRUE;
}

static gboolean
on_stat_distribution_xrange_to_activate(GtkWidget * widget, _unused_ gpointer user_data) {
  long new_xrange_to = atol(gtk_entry_get_text(GTK_ENTRY(widget)));
  CS->SD->xrange_to = new_xrange_to;
  return TRUE;
}

static gboolean
on_stat_distribution_granularity_activate(GtkWidget * widget, _unused_ gpointer user_data) {
  long new_width = atoi(gtk_entry_get_text(GTK_ENTRY(widget)));
  CS->SD->bar_width = new_width;
  return TRUE;
}

static gboolean
on_stat_distribution_add_button_clicked(_unused_ GtkWidget * widget, _unused_ gpointer user_data) {
  CS->SD->ne++;
  return TRUE;
}

static gboolean
on_stat_distribution_output_filename_activate(GtkWidget * widget, _unused_ gpointer user_data) {
  const char * new_output = gtk_entry_get_text(GTK_ENTRY(widget));
  if (strcmp(CS->SD->fn, DV_STAT_DISTRIBUTION_OUTPUT_DEFAULT_NAME) != 0) {
    dv_free(CS->SD->fn, strlen(CS->SD->fn) + 1);
  }
  CS->SD->fn = (char *) dv_malloc( sizeof(char) * ( strlen(new_output) + 1) );
  strcpy(CS->SD->fn, new_output);
  return TRUE;
}

static gboolean
on_stat_distribution_show_button_clicked(_unused_ GtkWidget * widget, _unused_ gpointer user_data) {
  char * filename;
  FILE * out;
  
  /* generate plots */
  filename = CS->SD->fn;
  if (!filename || strlen(filename) == 0) {
    fprintf(stderr, "Error: no file name to output.");
    return FALSE;
  }
  out = fopen(filename, "w");
  dv_check(out);
  fprintf(out,
          "width=%d\n"
          "#set terminal png font arial 14 size 640,350\n"
          "#set output ~/Desktop/00dv_stat_dis.png\n"
          "hist(x,width)=width*floor(x/width)+width/2.0\n"
          "set xrange [%ld:%ld]\n"
          "set yrange [0:]\n"
          "set boxwidth width\n"
          "set style fill solid 1.0 noborder\n"
          "set xlabel \"clocks\"\n"
          "set ylabel \"count\"\n"
          //          "plot \"-\" u (hist($1,width)):(1.0) smooth frequency w boxes lc rgb\"red\" t \"spawn\"\n");
          "plot ",
          CS->SD->bar_width,
          CS->SD->xrange_from,
          CS->SD->xrange_to);
  dv_stat_distribution_entry_t * e;
  int i;
  int count_e = 0;
  for (i = 0; i < CS->SD->ne; i++) {
    e = &CS->SD->e[i];
    if (e->dag_id >= 0)
      count_e++;
  }    
  for (i = 0; i < CS->SD->ne; i++) {
    e = &CS->SD->e[i];
    if (e->dag_id >= 0) {
      fprintf(out, "\"-\" u (hist($1,width)):(1.0) smooth frequency w boxes t \"%s\"", CS->SD->e[i].title);
      if (count_e > 1)
        fprintf(out, ", ");
      else
        fprintf(out, "\n");
      count_e--;
    }
  }
  for (i = 0; i < CS->SD->ne; i++) {
    e = &CS->SD->e[i];
    if (e->dag_id < 0)
      continue;
    dv_dag_t * D = CS->D + e->dag_id;
    if (e->type == 0 || e->type == 1)
      dv_dag_collect_delays_r(D, D->rt, out, e);
    else if (e->type == 2)
      dv_dag_collect_sync_delays_r(D, D->rt, out, e);
    else if (e->type == 3)
      dv_dag_collect_intervals_r(D, D->rt, out, e);
    fprintf(out,
            "e\n");
  }
  fprintf(out,
          "pause -1\n");
  fclose(out);
  fprintf(stdout, "generated distribution of delays to %s\n", filename);
  
  /* call gnuplot */
  GPid pid;
  char * argv[4];
  argv[0] = "gnuplot";
  argv[1] = "-persist";
  argv[2] = filename;
  argv[3] = NULL;
  int ret = g_spawn_async_with_pipes(NULL, argv, NULL,
                                     G_SPAWN_SEARCH_PATH, //G_SPAWN_DEFAULT | G_SPAWN_SEARCH_PATH,
                                     NULL, NULL, &pid,
                                     NULL, NULL, NULL, NULL);
  if (!ret) {
    fprintf(stderr, "g_spawn_async_with_pipes() failed.\n");
  }
  return TRUE;
}

static gboolean
on_stat_distribution_open_stat_button_clicked(_unused_ GtkWidget * widget, gpointer user_data) {
  dv_dag_t * D = (dv_dag_t *) user_data;
  int n = strlen(D->P->fn);
  char * filename = (char *) dv_malloc( sizeof(char) * (n + 2) );
  strcpy(filename, D->P->fn);
  if (strcmp(filename + n - 3, "dag") != 0)
    return FALSE;
  filename[n-3] = 's';
  filename[n-2] = 't';
  filename[n-1] = 'a';
  filename[n]   = 't';
  filename[n+1] = '\0';
  /* call gnuplot */
  GPid pid;
  char * argv[3];
  argv[0] = "gedit";
  argv[1] = filename;
  argv[2] = NULL;
  int ret = g_spawn_async_with_pipes(NULL, argv, NULL,
                                     G_SPAWN_SEARCH_PATH, //G_SPAWN_DEFAULT | G_SPAWN_SEARCH_PATH,
                                     NULL, NULL, &pid,
                                     NULL, NULL, NULL, NULL);
  if (!ret) {
    fprintf(stderr, "g_spawn_async_with_pipes() failed.\n");
  }
  dv_free(filename, strlen(filename) + 1);
  return TRUE;
}

static gboolean
on_stat_distribution_open_pp_button_clicked(_unused_ GtkWidget * widget, gpointer user_data) {
  dv_dag_t * D = (dv_dag_t *) user_data;
  int n = strlen(D->P->fn);
  char * filename = (char *) dv_malloc( sizeof(char) * (n + 1) );
  strcpy(filename, D->P->fn);
  if (strcmp(filename + n - 3, "dag") != 0)
    return FALSE;
  filename[n-3] = 'g';
  filename[n-2] = 'p';
  filename[n-1] = 'l';
  /* call gnuplot */
  GPid pid;
  char * argv[4];
  argv[0] = "gnuplot";
  argv[1] = "-persist";
  argv[2] = filename;
  argv[3] = NULL;
  int ret = g_spawn_async_with_pipes(NULL, argv, NULL,
                                     G_SPAWN_SEARCH_PATH, //G_SPAWN_DEFAULT | G_SPAWN_SEARCH_PATH,
                                     NULL, NULL, &pid,
                                     NULL, NULL, NULL, NULL);
  if (!ret) {
    fprintf(stderr, "g_spawn_async_with_pipes() failed.\n");
  }
  dv_free(filename, strlen(filename) + 1);
  return TRUE;
}

static gboolean
on_stat_distribution_expand_dag_button_clicked(_unused_ GtkWidget * widget, gpointer user_data) {
  dv_dag_t * D = (dv_dag_t *) user_data;
  dv_dag_expand_implicitly(D);
  dv_dag_set_status_label(D, CS->SD->dag_status_labels[D - CS->D]);
  dv_dag_node_pool_set_status_label(CS->pool, CS->SD->node_pool_label);
  dv_histogram_entry_pool_set_status_label(CS->epool, CS->SD->entry_pool_label);
  return TRUE;
}

static gboolean
on_stat_breakdown_dag_checkbox_toggled(_unused_ GtkWidget * widget, gpointer user_data) {
  long i = (long) user_data;
  CS->SBG->D[i] = 1 - CS->SBG->D[i];
  return TRUE;
}

static gboolean
on_stat_breakdown_output_filename_activate(GtkWidget * widget, _unused_ gpointer user_data) {
  const char * new_output = gtk_entry_get_text(GTK_ENTRY(widget));
  if (strcmp(CS->SBG->fn, DV_STAT_BREAKDOWN_OUTPUT_DEFAULT_NAME) != 0) {
    dv_free(CS->SBG->fn, strlen(CS->SBG->fn) + 1);
  }
  CS->SBG->fn = (char *) dv_malloc( sizeof(char) * ( strlen(new_output) + 1) );
  strcpy(CS->SBG->fn, new_output);
  return TRUE;
}

typedef struct {
  void (*process_event)(chronological_traverser * ct, dr_event evt);
  dr_pi_dag * G;
  dr_clock_t cum_running;
  dr_clock_t cum_delay;
  dr_clock_t cum_no_work;
  dr_clock_t t;
  long n_running;
  long n_ready;
  long n_workers;
  dr_clock_t total_elapsed;
  dr_clock_t total_t_1;
  long * edge_counts;		/* kind,u,v */
} dr_basic_stat;

static void 
dr_basic_stat_process_event(chronological_traverser * ct, 
			    dr_event evt);

static void
dr_calc_inner_delay(dr_basic_stat * bs, dr_pi_dag * G) {
  long n = G->n;
  long i;
  dr_clock_t total_elapsed = 0;
  dr_clock_t total_t_1 = 0;
  dr_pi_dag_node * T = G->T;
  long n_negative_inner_delays = 0;
  for (i = 0; i < n; i++) {
    dr_pi_dag_node * t = &T[i];
    dr_clock_t t_1 = t->info.t_1;
    dr_clock_t elapsed = t->info.end.t - t->info.start.t;
    if (t->info.kind < dr_dag_node_kind_section
	|| t->subgraphs_begin_offset == t->subgraphs_end_offset) {
      total_elapsed += elapsed;
      total_t_1 += t_1;
      if (elapsed < t_1 && t->info.worker != -1) {
	if (1 || (n_negative_inner_delays == 0)) {
	  fprintf(stderr,
		  "warning: node %ld has negative"
		  " inner delay (worker=%d, start=%llu, end=%llu,"
		  " t_1=%llu, end - start - t_1 = -%llu\n",
		  i, t->info.worker,
		  t->info.start.t, t->info.end.t, t->info.t_1,
		  t_1 - elapsed);
	}
	n_negative_inner_delays++;
      }
    }
  }
  if (n_negative_inner_delays > 0) {
    fprintf(stderr,
	    "warning: there are %ld nodes that have negative delays",
	    n_negative_inner_delays);
  }
  bs->total_elapsed = total_elapsed;
  bs->total_t_1 = total_t_1;
}

static void
dr_calc_edges(dr_basic_stat * bs, dr_pi_dag * G) {
  long n = G->n;
  long m = G->m;
  long nw = G->num_workers;
  /* C : a three dimensional array
     C(kind,i,j) is the number of type k edges from 
     worker i to worker j.
     we may counter nodes with worker id = -1
     (executed by more than one workers);
     we use worker id = nw for such entries
  */
  long * C_ = (long *)dr_malloc(sizeof(long) * dr_dag_edge_kind_max * (nw + 1) * (nw + 1));
#define EDGE_COUNTS(k,i,j) C_[k*(nw+1)*(nw+1)+i*(nw+1)+j]
  dr_dag_edge_kind_t k;
  long i, j;
  for (k = 0; k < dr_dag_edge_kind_max; k++) {
    for (i = 0; i < nw + 1; i++) {
      for (j = 0; j < nw + 1; j++) {
	EDGE_COUNTS(k,i,j) = 0;
      }
    }
  }
  for (i = 0; i < n; i++) {
    dr_pi_dag_node * t = &G->T[i];
    if (t->info.kind >= dr_dag_node_kind_section
	&& t->subgraphs_begin_offset == t->subgraphs_end_offset) {
      for (k = 0; k < dr_dag_edge_kind_max; k++) {
	int w = t->info.worker;
	if (w == -1) {
#if 0
	  fprintf(stderr, 
		  "warning: node %ld (kind=%s) has worker = %d)\n",
		  i, dr_dag_node_kind_to_str(t->info.kind), w);
#endif
	  EDGE_COUNTS(k, nw, nw) += t->info.logical_edge_counts[k];
	} else {
	  (void)dr_check(w >= 0);
	  (void)dr_check(w < nw);
	  EDGE_COUNTS(k, w, w) += t->info.logical_edge_counts[k];
	}
      }
    }    
  }
  for (i = 0; i < m; i++) {
    dr_pi_dag_edge * e = &G->E[i];
    int uw = G->T[e->u].info.worker;
    int vw = G->T[e->v].info.worker;
    if (uw == -1) {
#if 0
      fprintf(stderr, "warning: source node (%ld) of edge %ld %ld (kind=%s) -> %ld (kind=%s) has worker = %d\n",
	      e->u,
	      i, 
	      e->u, dr_dag_node_kind_to_str(G->T[e->u].info.kind), 
	      e->v, dr_dag_node_kind_to_str(G->T[e->v].info.kind), uw);
#endif
      uw = nw;
    }
    if (vw == -1) {
#if 0
      fprintf(stderr, "warning: dest node (%ld) of edge %ld %ld (kind=%s) -> %ld (kind=%s) has worker = %d\n",
	      e->v,
	      i, 
	      e->u, dr_dag_node_kind_to_str(G->T[e->u].info.kind), 
	      e->v, dr_dag_node_kind_to_str(G->T[e->v].info.kind), vw);
#endif
      vw = nw;
    }
    (void)dr_check(uw >= 0);
    (void)dr_check(uw <= nw);
    (void)dr_check(vw >= 0);
    (void)dr_check(vw <= nw);
    EDGE_COUNTS(e->kind, uw, vw)++;
  }
#undef EDGE_COUNTS
  bs->edge_counts = C_;
}

static void 
dr_basic_stat_init(dr_basic_stat * bs, dr_pi_dag * G) {
  bs->process_event = dr_basic_stat_process_event;
  bs->G = G;
  bs->n_running = 0;
  bs->n_ready = 0;
  bs->n_workers = G->num_workers;
  bs->cum_running = 0;		/* cumulative running cpu time */
  bs->cum_delay = 0;		/* cumulative delay cpu time */
  bs->cum_no_work = 0;		/* cumulative no_work cpu time */
  bs->t = 0;			/* time of the last event */
}

/*
static void
dr_basic_stat_destroy(dr_basic_stat * bs, dr_pi_dag * G) {
  long nw = G->num_workers;
  dr_free(bs->edge_counts, 
	  sizeof(long) * dr_dag_edge_kind_max * (nw + 1) * (nw + 1));
}
*/

static void 
dr_basic_stat_process_event(chronological_traverser * ct, 
			    dr_event evt) {
  dr_basic_stat * bs = (dr_basic_stat *)ct;
  dr_clock_t dt = evt.t - bs->t;

  int n_running = bs->n_running;
  int n_delay, n_no_work;
  if (bs->n_running >= bs->n_workers) {
    /* great, all workers are running */
    n_delay = 0;
    n_no_work = 0;
    if (bs->n_running > bs->n_workers) {
      fprintf(stderr, 
	      "warning: n_running = %ld"
	      " > n_workers = %ld (clock skew?)\n",
	      bs->n_running, bs->n_workers);
    }
    n_delay = 0;
    n_no_work = 0;
  } else if (bs->n_running + bs->n_ready <= bs->n_workers) {
    /* there were enough workers to run ALL ready tasks */
    n_delay = bs->n_ready;
    n_no_work = bs->n_workers - (bs->n_running + bs->n_ready);
  } else {
    n_delay = bs->n_workers - bs->n_running;
    n_no_work = 0;
  }
  bs->cum_running += n_running * dt;
  bs->cum_delay   += n_delay * dt;
  bs->cum_no_work += n_no_work * dt;

  switch (evt.kind) {
  case dr_event_kind_ready: {
    bs->n_ready++;
    break;
  }
  case dr_event_kind_start: {
    bs->n_running++;
    break;
  }
  case dr_event_kind_last_start: {
    bs->n_ready--;
    break;
  }
  case dr_event_kind_end: {
    bs->n_running--;
    break;
  }
  default:
    assert(0);
    break;
  }

  bs->t = evt.t;
}


static gboolean
on_stat_breakdown_show_button_clicked(_unused_ GtkWidget * widget, _unused_ gpointer user_data) {
  char * filename;
  FILE * out;
  
  /* generate plots */
  filename = CS->SBG->fn;
  if (!filename || strlen(filename) == 0) {
    fprintf(stderr, "Error: no file name to output.");
    return FALSE;
  }
  out = fopen(filename, "w");
  dv_check(out);
  fprintf(out,
          "#set terminal png font arial 14 size 640,350\n"
          "#set output ~/Desktop/00dv_stat_breakdown.png\n"
          "set style data histograms\n"
          "set style histogram rowstacked\n"
          "set style fill solid 0.8 noborder\n"
          "set key outside center top horizontal\n"
          "set boxwidth 0.75 relative\n"
          "set yrange [0:]\n"
          //          "set xtics rotate by -30\n"
          //          "set xlabel \"clocks\"\n"
          "set ylabel \"cumul. clocks\"\n"
          "plot "
          "\"-\" u 2:xtic(1) w histogram t \"t1\", "
          "\"-\" u 3 w histogram t \"delay\", "
          "\"-\" u 4 w histogram t \"nowork\"\n");
  dr_clock_t works[DV_MAX_DAG];
  dr_clock_t delays[DV_MAX_DAG];
  dr_clock_t noworks[DV_MAX_DAG];
  int DAGs[DV_MAX_DAG];
  int n = 0;
  int i;
  for (i = 0; i < CS->nD; i++) {
    if (CS->SBG->D[i] == 0)
      continue;
    dv_dag_t * D = &CS->D[i];
    DAGs[n] = i;

    dr_pi_dag * G = D->P->G;
    dr_basic_stat bs[1];
    dr_basic_stat_init(bs, G);
    dr_calc_inner_delay(bs, G);
    dr_calc_edges(bs, G);
    dr_pi_dag_chronological_traverse(G, (chronological_traverser *)bs);

    dr_clock_t work = bs->total_t_1;
    dr_clock_t delay = bs->cum_delay + (bs->total_elapsed - bs->total_t_1);
    dr_clock_t no_work = bs->cum_no_work;
    works[n] = work;
    delays[n] = delay;
    noworks[n] = no_work;
    n++;
  }
  int j;
  for (j = 0; j < 3; j++) {
    for (i = 0; i < n; i++) {
      fprintf(out,
              "DAG_%d  %lld %lld %lld\n",
              DAGs[i],
              works[i],
              delays[i],
              noworks[i]);
    }
    fprintf(out,
            "e\n");
  }
  fprintf(out,
          "pause -1\n");
  fclose(out);
  fprintf(stdout, "generated breakdown graphs to %s\n", filename);
  
  /* call gnuplot */
  GPid pid;
  char * argv[4];
  argv[0] = "gnuplot";
  argv[1] = "-persist";
  argv[2] = filename;
  argv[3] = NULL;
  int ret = g_spawn_async_with_pipes(NULL, argv, NULL,
                                     G_SPAWN_SEARCH_PATH, //G_SPAWN_DEFAULT | G_SPAWN_SEARCH_PATH,
                                     NULL, NULL, &pid,
                                     NULL, NULL, NULL, NULL);
  if (!ret) {
    fprintf(stderr, "g_spawn_async_with_pipes() failed.\n");
  }
  return TRUE;
}

static void
dv_open_statistics_dialog() {
  /* Get default DAG */
  dv_view_t * V = CS->activeV;
  if (!V) {
    V = &CS->V[0];
  }
  
  /* Build dialog */
  GtkWidget * dialog = gtk_dialog_new();
  gtk_window_set_title(GTK_WINDOW(dialog), "Statistics");
  //gtk_window_set_default_size(GTK_WINDOW(dialog), 600, -1);
  gtk_window_set_position(GTK_WINDOW(dialog), GTK_WIN_POS_MOUSE);
  GtkWidget * dialog_box = gtk_dialog_get_content_area(GTK_DIALOG(dialog));
  GtkWidget * notebook = gtk_notebook_new();
  gtk_box_pack_start(GTK_BOX(dialog_box), notebook, TRUE, TRUE, 0);

  GtkWidget * tab_label;
  GtkWidget * tab_box;

  /* Build delay distribution tab */
  {
    tab_label = gtk_label_new("Delay Distributions");
    tab_box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
    gtk_notebook_append_page(GTK_NOTEBOOK(notebook), tab_box, tab_label);
    if (CS->SD->ne == 0) {
      CS->SD->ne = CS->nP;
      if (CS->SD->ne > DV_MAX_DISTRIBUTION)
        CS->SD->ne = DV_MAX_DISTRIBUTION;
      if (CS->SD->ne < 4)
        CS->SD->ne = 4;
    }
    long i;
    for (i = 0; i < CS->SD->ne; i++) {
      dv_stat_distribution_entry_t * e = &CS->SD->e[i];
      GtkWidget * e_box = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 0);
      gtk_container_add(GTK_CONTAINER(tab_box), e_box);

      GtkWidget * combobox;
      /* dag */
      combobox = gtk_combo_box_text_new();
      gtk_box_pack_start(GTK_BOX(e_box), combobox, FALSE, FALSE, 0);
      gtk_widget_set_tooltip_text(GTK_WIDGET(combobox), "Choose DAG");
      gtk_combo_box_text_append(GTK_COMBO_BOX_TEXT(combobox), "none", "None");
      int j;
      for (j = 0; j < CS->nD; j++) {
        char str[30];
        sprintf(str, "DAG %d", j);
        gtk_combo_box_text_append(GTK_COMBO_BOX_TEXT(combobox), "dag", str);
      }
      gtk_combo_box_set_active(GTK_COMBO_BOX(combobox), e->dag_id + 1);
      g_signal_connect(G_OBJECT(combobox), "changed", G_CALLBACK(on_stat_distribution_dag_changed), (void *) i);
      /* type */
      combobox = gtk_combo_box_text_new();
      gtk_box_pack_start(GTK_BOX(e_box), combobox, FALSE, FALSE, 0);
      gtk_widget_set_tooltip_text(GTK_WIDGET(combobox), "Choose delay type");
      gtk_combo_box_text_append(GTK_COMBO_BOX_TEXT(combobox), "spawn", "spawn");
      gtk_combo_box_text_append(GTK_COMBO_BOX_TEXT(combobox), "cont", "cont");
      gtk_combo_box_text_append(GTK_COMBO_BOX_TEXT(combobox), "sync", "sync");
      gtk_combo_box_text_append(GTK_COMBO_BOX_TEXT(combobox), "intervals", "intervals");
      gtk_combo_box_set_active(GTK_COMBO_BOX(combobox), e->type);
      g_signal_connect(G_OBJECT(combobox), "changed", G_CALLBACK(on_stat_distribution_type_changed), (void *) i);
      /* stolen */
      combobox = gtk_combo_box_text_new();
      gtk_box_pack_start(GTK_BOX(e_box), combobox, FALSE, FALSE, 0);
      gtk_widget_set_tooltip_text(GTK_WIDGET(combobox), "Choose all or stolen only");
      gtk_combo_box_text_append(GTK_COMBO_BOX_TEXT(combobox), "all", "All");
      gtk_combo_box_text_append(GTK_COMBO_BOX_TEXT(combobox), "stolen", "Stolen");
      gtk_combo_box_set_active(GTK_COMBO_BOX(combobox), e->stolen);
      g_signal_connect(G_OBJECT(combobox), "changed", G_CALLBACK(on_stat_distribution_stolen_changed), (void *) i);
      /* title */
      GtkWidget * entry = gtk_entry_new();
      e->title_entry = entry;
      gtk_box_pack_start(GTK_BOX(e_box), e->title_entry, FALSE, FALSE, 0);
      if (e->title)
        gtk_entry_set_text(GTK_ENTRY(entry), e->title);
      gtk_widget_set_tooltip_text(GTK_WIDGET(entry), "Title of the plot");
      gtk_entry_set_width_chars(GTK_ENTRY(entry), 20);
      g_signal_connect(G_OBJECT(entry), "activate", G_CALLBACK(on_stat_distribution_title_activate), (void *) i);
    }

    GtkWidget * hbox, * label, * entry;
    GtkWidget * button;

    hbox = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 0);
    gtk_box_pack_start(GTK_BOX(tab_box), hbox, FALSE, FALSE, 0);
    button = gtk_button_new_with_mnemonic("_Add +");
    gtk_box_pack_start(GTK_BOX(hbox), button, FALSE, FALSE, 0);
    g_signal_connect(G_OBJECT(button), "clicked", G_CALLBACK(on_stat_distribution_add_button_clicked), (void *) NULL);
    
    gtk_box_pack_start(GTK_BOX(tab_box), gtk_separator_new(GTK_ORIENTATION_HORIZONTAL), FALSE, FALSE, 4);

    hbox = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 0);
    gtk_box_pack_start(GTK_BOX(tab_box), hbox, FALSE, FALSE, 0);
    label = gtk_label_new("xrange: from ");
    gtk_box_pack_start(GTK_BOX(hbox), label, FALSE, FALSE, 0);
    entry = gtk_entry_new();
    gtk_box_pack_start(GTK_BOX(hbox), entry, FALSE, FALSE, 0);
    gtk_entry_set_width_chars(GTK_ENTRY(entry), 10);
    char str[10];
    sprintf(str, "%ld", CS->SD->xrange_from);
    gtk_entry_set_text(GTK_ENTRY(entry), str);
    g_signal_connect(G_OBJECT(entry), "activate", G_CALLBACK(on_stat_distribution_xrange_from_activate), (void *) NULL);
    label = gtk_label_new(" to ");
    gtk_box_pack_start(GTK_BOX(hbox), label, FALSE, FALSE, 0);
    entry = gtk_entry_new();
    gtk_box_pack_start(GTK_BOX(hbox), entry, FALSE, FALSE, 0);
    gtk_entry_set_width_chars(GTK_ENTRY(entry), 10);
    sprintf(str, "%ld", CS->SD->xrange_to);
    gtk_entry_set_text(GTK_ENTRY(entry), str);
    g_signal_connect(G_OBJECT(entry), "activate", G_CALLBACK(on_stat_distribution_xrange_to_activate), (void *) NULL);

    hbox = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 0);
    gtk_box_pack_start(GTK_BOX(tab_box), hbox, FALSE, FALSE, 0);
    label = gtk_label_new("granularity: ");
    gtk_box_pack_start(GTK_BOX(hbox), label, FALSE, FALSE, 0);
    entry = gtk_entry_new();
    gtk_box_pack_start(GTK_BOX(hbox), entry, FALSE, FALSE, 0);
    gtk_entry_set_width_chars(GTK_ENTRY(entry), 10);
    sprintf(str, "%d", CS->SD->bar_width);
    gtk_entry_set_text(GTK_ENTRY(entry), str);
    g_signal_connect(G_OBJECT(entry), "activate", G_CALLBACK(on_stat_distribution_granularity_activate), (void *) NULL);
    
    gtk_box_pack_start(GTK_BOX(tab_box), gtk_separator_new(GTK_ORIENTATION_HORIZONTAL), FALSE, FALSE, 4);
  
    hbox = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 0);
    gtk_box_pack_start(GTK_BOX(tab_box), hbox, FALSE, FALSE, 0);
    label = gtk_label_new("Output: ");
    gtk_box_pack_start(GTK_BOX(hbox), label, FALSE, FALSE, 0);
    entry = gtk_entry_new();
    gtk_box_pack_start(GTK_BOX(hbox), entry, FALSE, FALSE, 0);
    gtk_entry_set_width_chars(GTK_ENTRY(entry), 15);
    gtk_entry_set_text(GTK_ENTRY(entry), CS->SD->fn);
    g_signal_connect(G_OBJECT(entry), "activate", G_CALLBACK(on_stat_distribution_output_filename_activate), (void *) NULL);
    button = gtk_button_new_with_mnemonic("_Show");
    gtk_box_pack_end(GTK_BOX(hbox), button, FALSE, FALSE, 0);
    g_signal_connect(G_OBJECT(button), "clicked", G_CALLBACK(on_stat_distribution_show_button_clicked), (void *) NULL);
  }

  /* Build DAG(s) tab */
  {
    tab_label = gtk_label_new("DAG(s)");
    tab_box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
    gtk_notebook_append_page(GTK_NOTEBOOK(notebook), tab_box, tab_label);
    GtkWidget * hbox, * label;
    int i;
    for (i = 0; i < CS->nD; i++) {
      dv_dag_t * D = &CS->D[i];
      /* Line 1 */
      GtkWidget * dag_box = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 0);
      gtk_box_pack_start(GTK_BOX(tab_box), dag_box, FALSE, FALSE, 0);
      char str[20];
      sprintf(str, "DAG %2d :  ", i);
      label = gtk_label_new(str);
      gtk_box_pack_start(GTK_BOX(dag_box), label, FALSE, FALSE, 0);
      GtkWidget * button;
      button = gtk_button_new_with_label("Open DR's Stat");
      gtk_box_pack_start(GTK_BOX(dag_box), button, FALSE, FALSE, 0);
      g_signal_connect(G_OBJECT(button), "clicked", G_CALLBACK(on_stat_distribution_open_stat_button_clicked), (void *) D);
      button = gtk_button_new_with_label("Open DR's PP");
      gtk_box_pack_start(GTK_BOX(dag_box), button, FALSE, FALSE, 0);
      g_signal_connect(G_OBJECT(button), "clicked", G_CALLBACK(on_stat_distribution_open_pp_button_clicked), (void *) D);
      gtk_box_pack_start(GTK_BOX(dag_box), gtk_separator_new(GTK_ORIENTATION_VERTICAL), FALSE, FALSE, 4);
      button = gtk_button_new_with_label("Load Full");
      gtk_box_pack_start(GTK_BOX(dag_box), button, FALSE, FALSE, 0);
      g_signal_connect(G_OBJECT(button), "clicked", G_CALLBACK(on_stat_distribution_expand_dag_button_clicked), (void *) D);
      /* Line 2 */
      dag_box = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 0);
      gtk_box_pack_start(GTK_BOX(tab_box), dag_box, FALSE, FALSE, 0);
      //GtkWidget * entry = gtk_entry_new();
      //gtk_box_pack_start(GTK_BOX(dag_box), entry, FALSE, FALSE, 0);
      //gtk_entry_set_width_chars(GTK_ENTRY(entry), 15);
      //gtk_entry_set_text(GTK_ENTRY(entry), D->P->fn);
      label = gtk_label_new(D->P->fn);
      gtk_box_pack_start(GTK_BOX(dag_box), label, FALSE, FALSE, 0);
      /* Line 3 */
      dag_box = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 0);
      gtk_box_pack_start(GTK_BOX(tab_box), dag_box, FALSE, FALSE, 0);
      label = gtk_label_new("");
      gtk_box_pack_start(GTK_BOX(dag_box), label, FALSE, FALSE, 0);
      CS->SD->dag_status_labels[i] = label;
      dv_dag_set_status_label(D, label);
      gtk_box_pack_start(GTK_BOX(tab_box), gtk_separator_new(GTK_ORIENTATION_HORIZONTAL), FALSE, FALSE, 4);
    }
    /* Node Pool's status */
    hbox = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 0);
    gtk_box_pack_start(GTK_BOX(tab_box), hbox, FALSE, FALSE, 0);
    label = gtk_label_new("");
    gtk_box_pack_start(GTK_BOX(hbox), label, FALSE, FALSE, 0);
    CS->SD->node_pool_label = label;
    dv_dag_node_pool_set_status_label(CS->pool, label);
    /* Entry Pool's status */
    hbox = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 0);
    gtk_box_pack_start(GTK_BOX(tab_box), hbox, FALSE, FALSE, 0);
    label = gtk_label_new("");
    gtk_box_pack_start(GTK_BOX(hbox), label, FALSE, FALSE, 0);
    CS->SD->entry_pool_label = label;
    dv_histogram_entry_pool_set_status_label(CS->epool, label);
  }

  /* Build breakdown graphs tab */
  {
    tab_label = gtk_label_new("Breakdown Graphs");
    tab_box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
    gtk_notebook_append_page(GTK_NOTEBOOK(notebook), tab_box, tab_label);
    long i;
    for (i = 0; i < CS->nD; i++) {
      GtkWidget * dag_box = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 0);
      gtk_box_pack_start(GTK_BOX(tab_box), dag_box, FALSE, FALSE, 0);
      char str[20];
      sprintf(str, "DAG %2ld ", i);
      GtkWidget * checkbox = gtk_check_button_new_with_label(str);
      gtk_box_pack_start(GTK_BOX(dag_box), checkbox, FALSE, FALSE, 0);
      gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(checkbox), CS->SBG->D[i]);
      g_signal_connect(G_OBJECT(checkbox), "toggled", G_CALLBACK(on_stat_breakdown_dag_checkbox_toggled), (void *) i);
    }

    gtk_box_pack_start(GTK_BOX(tab_box), gtk_separator_new(GTK_ORIENTATION_HORIZONTAL), FALSE, FALSE, 4);

    GtkWidget * hbox, * label, * entry, * button;
    hbox = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 0);
    gtk_box_pack_start(GTK_BOX(tab_box), hbox, FALSE, FALSE, 0);
    label = gtk_label_new("Output: ");
    gtk_box_pack_start(GTK_BOX(hbox), label, FALSE, FALSE, 0);
    entry = gtk_entry_new();
    gtk_box_pack_start(GTK_BOX(hbox), entry, FALSE, FALSE, 0);
    gtk_entry_set_width_chars(GTK_ENTRY(entry), 15);
    gtk_entry_set_text(GTK_ENTRY(entry), CS->SBG->fn);
    g_signal_connect(G_OBJECT(entry), "activate", G_CALLBACK(on_stat_breakdown_output_filename_activate), (void *) NULL);
    button = gtk_button_new_with_mnemonic("_Show");
    gtk_box_pack_end(GTK_BOX(hbox), button, FALSE, FALSE, 0);
    g_signal_connect(G_OBJECT(button), "clicked", G_CALLBACK(on_stat_breakdown_show_button_clicked), (void *) NULL);
  }


  /* Run */
  gtk_widget_show_all(dialog_box);
  gtk_dialog_run(GTK_DIALOG(dialog));
  
  /* Destroy */
  gtk_widget_destroy(dialog);
}

static void
on_file_statistics_clicked(_unused_ GtkMenuItem * menuitem, _unused_ gpointer user_data) {
  dv_open_statistics_dialog();
}

/*-----------------end of Menubar functions-----------------*/



/*-----------------Main begins-----------------*/

static GtkWidget *
dv_create_menubar() {
  // Menu Bar
  GtkWidget * menubar = gtk_menu_bar_new();
  
  // submenu File
  GtkWidget * file = gtk_menu_item_new_with_mnemonic("_File");
  gtk_menu_shell_append(GTK_MENU_SHELL(menubar), file);
  GtkWidget * file_menu = gtk_menu_new();
  gtk_menu_item_set_submenu(GTK_MENU_ITEM(file), file_menu);
  GtkWidget * export = gtk_menu_item_new_with_mnemonic("E_xport focused viewport");
  gtk_menu_shell_append(GTK_MENU_SHELL(file_menu), export);
  g_signal_connect(G_OBJECT(export), "activate", G_CALLBACK(on_file_export_clicked), NULL);
  GtkWidget * exportall = gtk_menu_item_new_with_mnemonic("Export _all viewports");
  gtk_menu_shell_append(GTK_MENU_SHELL(file_menu), exportall);
  g_signal_connect(G_OBJECT(exportall), "activate", G_CALLBACK(on_file_export_all_clicked), NULL);

  
  // submenu Viewports
  GtkWidget * viewports = gtk_menu_item_new_with_mnemonic("Viewp_orts");
  gtk_menu_shell_append(GTK_MENU_SHELL(menubar), viewports);
  GtkWidget * viewports_menu = gtk_menu_new();
  gtk_menu_item_set_submenu(GTK_MENU_ITEM(viewports), viewports_menu);
  GtkWidget * viewport, * viewport_menu;
  viewport = gtk_menu_item_new_with_mnemonic("_Configure");
  gtk_menu_shell_append(GTK_MENU_SHELL(viewports_menu), viewport);
  g_signal_connect(G_OBJECT(viewport), "activate", G_CALLBACK(on_viewport_configure_clicked), NULL);
  GSList * group;
  GtkWidget * item;
  char s[DV_STRING_LENGTH];
  int i, j;
  for (i=0; i<CS->nVP; i++) {
    if (!CS->VP[i].split) {
      sprintf(s, "Viewport _%d", i);
      viewport = gtk_menu_item_new_with_mnemonic(s);
      gtk_menu_shell_append(GTK_MENU_SHELL(viewports_menu), viewport);
      viewport_menu = gtk_menu_new();
      gtk_menu_item_set_submenu(GTK_MENU_ITEM(viewport), viewport_menu);
      for (j=0; j<CS->nV; j++) {
        sprintf(s, "VIEW _%d", j);
        item = gtk_check_menu_item_new_with_mnemonic(s);
        gtk_menu_shell_append(GTK_MENU_SHELL(viewport_menu), item);
        g_signal_connect(G_OBJECT(item), "toggled", G_CALLBACK(on_viewport_select_view), (void *) &CS->VP[i]);
        if (CS->VP[i].mV[j])
          gtk_check_menu_item_set_active(GTK_CHECK_MENU_ITEM(item), TRUE);
      }
    }
  }
  
  // submenu Views
  GtkWidget * views = gtk_menu_item_new_with_mnemonic("_VIEWs");
  gtk_menu_shell_append(GTK_MENU_SHELL(menubar), views);
  GtkWidget * views_menu = gtk_menu_new();
  gtk_menu_item_set_submenu(GTK_MENU_ITEM(views), views_menu);
  GtkWidget * view, * view_menu;
  view = gtk_menu_item_new_with_label("Add new VIEW");
  gtk_menu_shell_append(GTK_MENU_SHELL(views_menu), view);
  view_menu = gtk_menu_new();
  gtk_menu_item_set_submenu(GTK_MENU_ITEM(view), view_menu);
  for (j=0; j<CS->nD; j++) {
    sprintf(s, "for DAG %d", j);
    item = gtk_menu_item_new_with_label(s);
    gtk_menu_shell_append(GTK_MENU_SHELL(view_menu), item);
    g_signal_connect(G_OBJECT(item), "activate", G_CALLBACK(on_view_add_new), (void *) &CS->D[j]);
  }    
  for (i=0; i<CS->nV; i++) {
    group = NULL;
    sprintf(s, "VIEW %d", i);
    view = gtk_menu_item_new_with_label(s);
    gtk_menu_shell_append(GTK_MENU_SHELL(views_menu), view);
    view_menu = gtk_menu_new();
    gtk_menu_item_set_submenu(GTK_MENU_ITEM(view), view_menu);
    for (j=0; j<CS->nD; j++) {
      sprintf(s, "DAG %d", j);
      item = gtk_radio_menu_item_new_with_label(group, s);
      gtk_menu_shell_append(GTK_MENU_SHELL(view_menu), item);
      group = gtk_radio_menu_item_get_group(GTK_RADIO_MENU_ITEM(item));
      if (j == (CS->V[i].D - CS->D))
        gtk_check_menu_item_set_active(GTK_CHECK_MENU_ITEM(item), TRUE);
    }    
  }
  // submenu DAGs
  GtkWidget * dags = gtk_menu_item_new_with_mnemonic("_DAGs");
  gtk_menu_shell_append(GTK_MENU_SHELL(menubar), dags);
  GtkWidget * dags_menu = gtk_menu_new();
  gtk_menu_item_set_submenu(GTK_MENU_ITEM(dags), dags_menu);
  GtkWidget * dag, * dag_menu;
  dag = gtk_menu_item_new_with_label("Add new DAG");
  gtk_menu_shell_append(GTK_MENU_SHELL(dags_menu), dag);
  dag_menu = gtk_menu_new();
  gtk_menu_item_set_submenu(GTK_MENU_ITEM(dag), dag_menu);
  for (j=0; j<CS->nP; j++) {
    sprintf(s, "for PIDAG %d", j);
    item = gtk_menu_item_new_with_label(s);
    gtk_menu_shell_append(GTK_MENU_SHELL(dag_menu), item);
    g_signal_connect(G_OBJECT(item), "activate", G_CALLBACK(on_dag_add_new), (void *) &CS->P[j]);
  }
  for (i=0; i<CS->nD; i++) {
    group = NULL;
    sprintf(s, "DAG %d", i);
    dag = gtk_menu_item_new_with_label(s);
    gtk_menu_shell_append(GTK_MENU_SHELL(dags_menu), dag);
    dag_menu = gtk_menu_new();
    gtk_menu_item_set_submenu(GTK_MENU_ITEM(dag), dag_menu);
    for (j=0; j<CS->nP; j++) {
      sprintf(s, "PIDAG %d: [%ld nodes] %s", j, CS->P[j].n, CS->P[j].fn);
      item = gtk_radio_menu_item_new_with_label(group, s);
      gtk_menu_shell_append(GTK_MENU_SHELL(dag_menu), item);
      group = gtk_radio_menu_item_get_group(GTK_RADIO_MENU_ITEM(item));
      if (j == (CS->D[i].P - CS->P))
        gtk_check_menu_item_set_active(GTK_CHECK_MENU_ITEM(item), TRUE);
    }    
  }
  // submenu PIDAGs
  GtkWidget * pidags = gtk_menu_item_new_with_mnemonic("_PIDAGs");
  gtk_menu_shell_append(GTK_MENU_SHELL(menubar), pidags);
  GtkWidget * pidags_menu = gtk_menu_new();
  gtk_menu_item_set_submenu(GTK_MENU_ITEM(pidags), pidags_menu);
  GtkWidget * pidag;
  //pidag = gtk_menu_item_new_with_label("Add new dag file");
  //gtk_menu_shell_append(GTK_MENU_SHELL(pidags_menu), pidag);
  for (i=0; i<CS->nP; i++) {
    sprintf(s, "PIDAG %d: [%ld nodes] %s", i, CS->P[i].n, CS->P[i].fn);
    pidag = gtk_menu_item_new_with_label(s);
    gtk_menu_shell_append(GTK_MENU_SHELL(pidags_menu), pidag);
  }
  
  // submenu Tools
  GtkWidget * tools = gtk_menu_item_new_with_mnemonic("_Tools");
  gtk_menu_shell_append(GTK_MENU_SHELL(menubar), tools);
  GtkWidget * tools_menu = gtk_menu_new();
  gtk_menu_item_set_submenu(GTK_MENU_ITEM(tools), tools_menu);
  GtkWidget * statistics = gtk_menu_item_new_with_mnemonic("_Statistics");
  gtk_menu_shell_append(GTK_MENU_SHELL(tools_menu), statistics);
  g_signal_connect(G_OBJECT(statistics), "activate", G_CALLBACK(on_file_statistics_clicked), NULL);
  GtkWidget * samplebt = gtk_menu_item_new_with_mnemonic("S_amples");
  gtk_menu_shell_append(GTK_MENU_SHELL(tools_menu), samplebt);
  GtkWidget * samplebt_menu = gtk_menu_new();
  gtk_menu_item_set_submenu(GTK_MENU_ITEM(samplebt), samplebt_menu);
  GtkWidget * samplebt_open = gtk_menu_item_new_with_mnemonic("_View Backtrace Samples");
  gtk_menu_shell_append(GTK_MENU_SHELL(samplebt_menu), samplebt_open);
  g_signal_connect(G_OBJECT(samplebt_open), "activate", G_CALLBACK(on_menu_item_view_samples_clicked), (void *) 0);

  // submenu Help
  GtkWidget * help = gtk_menu_item_new_with_mnemonic("_Help");
  gtk_menu_shell_append(GTK_MENU_SHELL(menubar), help);
  GtkWidget * help_menu = gtk_menu_new();
  gtk_menu_item_set_submenu(GTK_MENU_ITEM(help), help_menu);  
  GtkWidget * hotkeys = gtk_menu_item_new_with_mnemonic("List of available hot_keys");
  gtk_menu_shell_append(GTK_MENU_SHELL(help_menu), hotkeys);
  g_signal_connect(G_OBJECT(hotkeys), "activate", G_CALLBACK(on_help_hotkeys_clicked), NULL);
  
  return menubar;
}

static void
dv_gui_init(dv_gui_t * gui) {
  gui->window = gtk_window_new(GTK_WINDOW_TOPLEVEL);
  gui->vbox0 = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
  gui->menubar = dv_create_menubar();
  gui->toolbar = gtk_toolbar_new();
  gui->statusbar1 = gtk_statusbar_new();
  gui->statusbar2 = gtk_statusbar_new();
  gui->statusbar3 = gtk_statusbar_new();  

  gtk_container_add(GTK_CONTAINER(gui->window), gui->vbox0);
  gtk_box_pack_start(GTK_BOX(gui->vbox0), gui->menubar, FALSE, FALSE, 0);
  gtk_box_pack_start(GTK_BOX(gui->vbox0), gui->toolbar, FALSE, FALSE, 0);
  
  GtkWidget * statusbar_box = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 0);
  gtk_box_pack_end(GTK_BOX(gui->vbox0), statusbar_box, FALSE, FALSE, 0);
  GtkWidget * statusbar_box_2 = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 0);
  gtk_box_pack_start(GTK_BOX(statusbar_box), statusbar_box_2, TRUE, TRUE, 0);
  gtk_box_set_homogeneous(GTK_BOX(statusbar_box_2), TRUE);
  gtk_box_pack_start(GTK_BOX(statusbar_box_2), gui->statusbar1, TRUE, TRUE, 0);
  GtkWidget * statusbar_box_3 = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 0);
  gtk_box_pack_start(GTK_BOX(statusbar_box_2), statusbar_box_3, TRUE, TRUE, 0);
  gtk_box_pack_start(GTK_BOX(statusbar_box_3), gtk_separator_new(GTK_ORIENTATION_VERTICAL), FALSE, FALSE, 0);
  gtk_box_pack_start(GTK_BOX(statusbar_box_3), gui->statusbar2, TRUE, TRUE, 0);
  gtk_box_pack_end(GTK_BOX(statusbar_box), gui->statusbar3, FALSE, FALSE, 0);
  gtk_box_pack_end(GTK_BOX(statusbar_box), gtk_separator_new(GTK_ORIENTATION_VERTICAL), FALSE, FALSE, 0);
  
  gtk_widget_set_tooltip_text(GTK_WIDGET(gui->statusbar1), "Interaction status");
  gtk_widget_set_tooltip_text(GTK_WIDGET(gui->statusbar2), "Selection status");
  gtk_widget_set_tooltip_text(GTK_WIDGET(gui->statusbar3), "Memory pool status");
}

static int
open_gui(_unused_ int argc, _unused_ char * argv[]) {
  /* Initialize GUI widgets */
  dv_gui_init(CS->gui);

  /* Window */
  GtkWidget * window = CS->gui->window;
  //gtk_window_fullscreen(GTK_WINDOW(window));
  //gtk_window_set_position(GTK_WINDOW(window), GTK_WIN_POS_CENTER);
  gtk_window_set_default_size(GTK_WINDOW(window), 1000, 700);
  gtk_window_maximize(GTK_WINDOW(window));
  //gtk_window_set_position(GTK_WINDOW(window), GTK_WIN_POS_CENTER);
  gtk_window_set_title(GTK_WINDOW(window), "DAG Visualizer");
  g_signal_connect(G_OBJECT(window), "destroy", G_CALLBACK(gtk_main_quit), NULL);
  g_signal_connect(G_OBJECT(CS->gui->window), "key-press-event", G_CALLBACK(on_window_key_event), NULL);

  // Set icon
  /*
  char * icon_filename = "smile_icon.png";
  GError * error = 0; 
  GdkPixbuf *pixbuf = gdk_pixbuf_new_from_file(icon_filename, &error);
  if (!pixbuf) {
    fprintf(stderr, "Cannot set icon %s: %s\n", icon_filename, error->message);
  } else {
    gtk_window_set_icon(GTK_WINDOW(window), pixbuf);
  }
  */

  /* Toolbar */
  GtkWidget * toolbar = CS->gui->toolbar;
  //GtkToolItem * btn_attrs = gtk_tool_button_new();

  /* Viewports */
  dv_viewport_t * vp0 = CS->VP;
  gtk_box_pack_start(GTK_BOX(CS->gui->vbox0), vp0->frame, TRUE, TRUE, 0);
  dv_viewport_show(vp0);

  /* Status bars */
  dv_statusbar_update_selection_status();
  dv_statusbar_update_pool_status();

  /* Run main loop */
  gtk_widget_show_all(window);
  gtk_main();
  return 1;
}


_static_unused_ void
dv_alarm_set() {
  alarm(3);
}

_unused_ void
dv_signal_handler(int signo) {
  if (signo == SIGALRM) {
    fprintf(stderr, "received SIGALRM\n");
    dv_get_callpath_by_libunwind();
    dv_get_callpath_by_backtrace();
  } else
    fprintf(stderr, "received unknown signal\n");
  dv_alarm_set();
}

_static_unused_ void
dv_alarm_init() {
  if (signal(SIGALRM, dv_signal_handler) == SIG_ERR)
    fprintf(stderr, "cannot catch SIGALRM\n");
  else
    dv_alarm_set();
}


int
main(int argc, char * argv[]) {
  /* General initialization */
  gtk_init(&argc, &argv);
  dv_global_state_init(CS);
  //dv_get_env();
  //if (argc > 1)  print_dag_file(argv[1]);
  //dv_alarm_init();
  
  /* PIDAG initialization */
  int i;
  if (argc > 1) {
    for (i=1; i<argc; i++)
      dv_pidag_read_new_file(argv[i]);
  }
  
  /* Viewport initialization */
  dv_viewport_t * VP = dv_viewport_create_new();
  dv_viewport_init(VP);

  /* DAG -> VIEW <- Viewport initialization */
  dv_dag_t * D;
  dv_view_t * V;
  for (i=0; i<CS->nP; i++) {
    D = dv_dag_create_new_with_pidag(&CS->P[i]);
    //print_dvdag(D);
    V = dv_view_create_new_with_dag(D);
    // Expand
    int count = 0;
    while (V->D->n < 10 && count < 2) {
      printf("V %ld: %ld\n", V-CS->V, V->D->n);
      dv_do_expanding_one(V);
      count++;
    }
  }
  if (CS->nV == 1) {
    V = CS->V;
    dv_view_add_viewport(V, VP);
    //dv_view_change_lt(V, 4);
  } else if (CS->nV >= 2) {
    dv_viewport_change_split(VP, 1);
    dv_view_add_viewport(&CS->V[0], VP->vp1);
    dv_view_add_viewport(&CS->V[1], VP->vp2);
  }
  
  dv_do_set_focused_view(CS->V, 1);
  
  /* Open GUI */
  return open_gui(argc, argv);
}

/*-----------------Main ends-------------------*/
