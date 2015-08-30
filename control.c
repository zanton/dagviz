
/****************** GUI Callbacks **************************************/

static gboolean
on_draw_event(_unused_ GtkWidget * widget, cairo_t * cr, gpointer user_data) {
  dv_viewport_t * VP = (dv_viewport_t *) user_data;
  dv_viewport_draw(VP, cr);
  return FALSE;
}

static void
on_btn_zoomfit_full_clicked(_unused_ GtkToolButton * toolbtn, gpointer user_data) {
  dv_viewport_t * VP = (dv_viewport_t *) user_data;
  dv_view_do_zoomfit_full(VP->mainV);
}

static void
on_btn_shrink_clicked(_unused_ GtkToolButton * toolbtn, gpointer user_data) {
  dv_viewport_t * VP = (dv_viewport_t *) user_data;
  dv_do_collapsing_one(VP->mainV);
}

static void
on_btn_expand_clicked(_unused_ GtkToolButton * toolbtn, gpointer user_data) {
  dv_viewport_t * VP = (dv_viewport_t *) user_data;
  dv_do_expanding_one(VP->mainV);
}

static gboolean
on_scroll_event(_unused_ GtkWidget * widget, GdkEventScroll * event, gpointer user_data) {
  dv_viewport_t * VP = (dv_viewport_t *) user_data;
  if ( VP->mV[ CS->activeV - CS->V ] ) {
    dv_do_scrolling(CS->activeV, event);
  } else {
    int i;
    for (i = 0; i < CS->nV; i++)
      if (VP->mV[i])
        dv_do_scrolling(&CS->V[i], event);
  }
  return TRUE;
}

static gboolean
on_button_event(_unused_ GtkWidget * widget, GdkEventButton * event, gpointer user_data) {
  dv_viewport_t * VP = (dv_viewport_t *) user_data;
  if ( VP->mV[ CS->activeV - CS->V ] ) {
    dv_do_button_event(CS->activeV, event);
  } else {
    int i;
    for (i = 0; i < CS->nV; i++)
      if ( VP->mV[i] )
        dv_do_button_event(&CS->V[i], event);
  }
  return TRUE;
}

static gboolean
on_motion_event(_unused_ GtkWidget * widget, GdkEventMotion * event, gpointer user_data) {
  dv_viewport_t * VP = (dv_viewport_t *) user_data;
  if ( VP->mV[ CS->activeV - CS->V ] ) {
    dv_do_motion_event(CS->activeV, event);
  } else {
    int i;
    for (i = 0; i < CS->nV; i++)
      if ( VP->mV[i] )
        dv_do_motion_event(&CS->V[i], event);
  }
  return TRUE;
}

static gboolean
on_combobox_lt_changed(GtkComboBox * widget, gpointer user_data) {
  dv_view_t * V = (dv_view_t *) user_data;
  int new_lt = gtk_combo_box_get_active(GTK_COMBO_BOX(widget));
  dv_check(0 <= new_lt && new_lt < DV_NUM_LAYOUT_TYPES);
  dv_view_change_lt(V, new_lt);
  return TRUE;  
}

static gboolean
on_combobox_nc_changed(GtkComboBox * widget, gpointer user_data) {
  dv_view_t * V = (dv_view_t *) user_data;
  dv_view_change_nc(V, gtk_combo_box_get_active(GTK_COMBO_BOX(widget)));
  dv_queue_draw(V);
  return TRUE;
}

static gboolean
on_combobox_sdt_changed(GtkComboBox * widget, gpointer user_data) {
  dv_view_t * V = (dv_view_t *) user_data;
  dv_view_change_sdt(V, gtk_combo_box_get_active(GTK_COMBO_BOX(widget)));
  dv_view_layout(V);
  if (V->S->auto_zoomfit)
    dv_view_do_zoomfit_full(V);
  dv_queue_draw_d(V);
  return TRUE;
}

static gboolean
on_entry_radix_activate(GtkEntry * entry, gpointer user_data) {
  dv_view_t * V = (dv_view_t *) user_data;
  double radix = atof(gtk_entry_get_text(GTK_ENTRY(entry)));
  dv_view_change_radix(V, radix);
  dv_view_layout(V);
  dv_queue_draw_d(V);
  return TRUE;
}

static gboolean
on_entry_remark_activate(GtkEntry * entry, gpointer user_data) {
  dv_view_t * V = (dv_view_t *) user_data;
  /* Read string */
  const char * str = gtk_entry_get_text(GTK_ENTRY(entry));
  char str2[strlen(str) + 1];
  strcpy(str2, str);
  /* Read values in string to array */
  V->D->nr = 0;
  long v;
  char * p = str2;
  while (*p) {
    if (isdigit(*p)) {
      v = strtol(p, &p, 10);
      if (V->D->nr < DV_MAX_NUM_REMARKS)
        V->D->ar[V->D->nr++] = v;
    } else {
      p++;
    }
  }
  /* Convert read values back to string */
  char str3[V->D->nr * 3 + 1];
  str3[0] = 0;
  int i;
  for (i = 0; i < V->D->nr; i++) {
    v = V->D->ar[i];
    if (i == 0)
      sprintf(str2, "%ld", v);
    else
      sprintf(str2, ", %ld", v);
    strcat(str3, str2);
  }
  dv_view_set_entry_remark_text(V, str3);
  dv_queue_draw_d(V);
  return TRUE;
}

static gboolean on_entry_search_activate(GtkEntry *entry, gpointer user_data) {
  dv_view_t *V = (dv_view_t *) user_data;
  dv_dag_t *D = V->D;
  dv_view_status_t *S = V->S;
  // Get node
  long pii = atol(gtk_entry_get_text(GTK_ENTRY(entry)));
  dv_dag_node_t *node = dv_find_node_with_pii_r(V, pii, D->rt);
  if (!node)
    return TRUE;
  // Get target zoom ratio, x, y
  dv_node_coordinate_t *co = &node->c[S->coord];
  double zoom_ratio = 1.0;
  double x = 0.0;
  double y = 0.0;
  double d1, d2;
  switch (S->lt) {
  case 0:
    // DAG
    d1 = co->lw + co->rw;
    d2 = S->vpw - 2 * DV_ZOOM_TO_FIT_MARGIN;
    if (d1 > d2)
      zoom_ratio = d2 / d1;
    d1 = co->dw;
    d2 = S->vph - DV_ZOOM_TO_FIT_MARGIN - DV_ZOOM_TO_FIT_MARGIN_DOWN;
    if (d1 > d2)
      zoom_ratio = dv_min(zoom_ratio, d2 / d1);
    zoom_ratio = dv_min(zoom_ratio, dv_min(S->zoom_ratio_x, S->zoom_ratio_y));
    x -= (co->x + (co->rw - co->lw) * 0.5) * zoom_ratio;
    y -= co->y * zoom_ratio - (S->vph - DV_ZOOM_TO_FIT_MARGIN - DV_ZOOM_TO_FIT_MARGIN_DOWN - co->dw * zoom_ratio) * 0.5;
    break;
  case 1:
    // DAG box
    d1 = co->lw + co->rw;
    d2 = S->vpw - 2 * DV_ZOOM_TO_FIT_MARGIN;
    if (d1 > d2)
      zoom_ratio = d2 / d1;
    d1 = co->dw;
    d2 = S->vph - DV_ZOOM_TO_FIT_MARGIN - DV_ZOOM_TO_FIT_MARGIN_DOWN;
    if (d1 > d2)
      zoom_ratio = dv_min(zoom_ratio, d2 / d1);
    zoom_ratio = dv_min(zoom_ratio, dv_min(S->zoom_ratio_x, S->zoom_ratio_y));
    x -= (co->x + (co->rw - co->lw) * 0.5) * zoom_ratio;
    y -= co->y * zoom_ratio - (S->vph - DV_ZOOM_TO_FIT_MARGIN - DV_ZOOM_TO_FIT_MARGIN_DOWN - co->dw * zoom_ratio) * 0.5;
    break;
  case 2:
    // Vertical Timeline
    d1 = 2*D->radius + (D->P->num_workers - 1) * DV_HDIS;
    d2 = S->vpw - 2 * DV_ZOOM_TO_FIT_MARGIN;
    if (d1 > d2)
      zoom_ratio = d2 / d1;
    d1 = co->dw;
    d2 = S->vph - DV_ZOOM_TO_FIT_MARGIN - DV_ZOOM_TO_FIT_MARGIN_DOWN;
    if (d1 > d2)
      zoom_ratio = dv_min(zoom_ratio, d2 / d1);
    zoom_ratio = dv_min(zoom_ratio, dv_min(S->zoom_ratio_x, S->zoom_ratio_y));
    x -= (co->x + (co->rw - co->lw) * 0.5) * zoom_ratio - S->vpw * 0.5;
    y -= co->y * zoom_ratio - (S->vph - DV_ZOOM_TO_FIT_MARGIN  - DV_ZOOM_TO_FIT_MARGIN_DOWN - co->dw * zoom_ratio) * 0.5;
    break;
  case 3:
    // Horizontal Timeline
    d1 = D->P->num_workers * (D->radius * 2);
    d2 = S->vph - DV_ZOOM_TO_FIT_MARGIN - DV_ZOOM_TO_FIT_MARGIN_DOWN;
    if (d1 > d2)
      zoom_ratio = d2 / d1;
    d1 = co->rw;
    d2 = S->vpw - 2 * DV_ZOOM_TO_FIT_MARGIN;
    if (d1 > d2)
      zoom_ratio = dv_min(zoom_ratio, d2 / d1);
    x -= (co->x + co->rw * 0.5) * zoom_ratio - S->vpw * 0.5;
    y -= co->y * zoom_ratio - (S->vph - DV_ZOOM_TO_FIT_MARGIN - DV_ZOOM_TO_FIT_MARGIN_DOWN - co->dw * zoom_ratio) * 0.5;
    break;
  case 4:
    // Parallelism profile
    d1 = D->P->num_workers * (D->radius * 2);
    d2 = S->vph - DV_ZOOM_TO_FIT_MARGIN - DV_ZOOM_TO_FIT_MARGIN_DOWN;
    if (d1 > d2)
      zoom_ratio = d2 / d1;
    d1 = co->rw;
    d2 = S->vpw - 2 * DV_ZOOM_TO_FIT_MARGIN;
    if (d1 > d2)
      zoom_ratio = dv_min(zoom_ratio, d2 / d1);
    x -= (co->x + co->rw * 0.5) * zoom_ratio - S->vpw * 0.5;
    y -= co->y * zoom_ratio - (S->vph - DV_ZOOM_TO_FIT_MARGIN - DV_ZOOM_TO_FIT_MARGIN_DOWN - co->dw * zoom_ratio) * 0.5;
    break;
  default:
    dv_check(0);
  }
  // Set info tag
  if (!dv_llist_has(D->P->itl, (void *) pii)) {
    dv_llist_add(D->P->itl, (void *) pii);
  }  
  // Set motion
  if (!S->m->on)
    dv_motion_start(S->m, pii, x, y, zoom_ratio, zoom_ratio);
  else
    dv_motion_reset_target(S->m, pii, x, y, zoom_ratio, zoom_ratio);
  return TRUE;
}

static gboolean on_combobox_frombt_changed(GtkComboBox *widget, gpointer user_data) {
  dv_view_t *V = (dv_view_t *) user_data;
  V->D->frombt = gtk_combo_box_get_active(GTK_COMBO_BOX(widget));
  dv_view_layout(V);
  dv_queue_draw_d(V);
  return TRUE;
}

static gboolean on_combobox_et_changed(GtkComboBox *widget, gpointer user_data) {
  dv_view_t *V = (dv_view_t *) user_data;
  V->S->et = gtk_combo_box_get_active(GTK_COMBO_BOX(widget));
  dv_queue_draw(V);
  return TRUE;
}

static void
on_togg_eaffix_toggled(_unused_ GtkWidget * widget, gpointer user_data) {
  dv_view_t * V = (dv_view_t *) user_data;
  if (gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(widget))) {
    dv_view_change_eaffix(V, 1);
  } else {
    dv_view_change_eaffix(V, 0);
  }
  dv_queue_draw(V);
}

/*
static void
on_togg_focused_toggled(_unused_ GtkWidget * widget, gpointer user_data) {
  dv_viewport_t * VP = (dv_viewport_t *) user_data;
  int focused = gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(widget));
  dv_do_set_focused_view(VP->mainV, focused);
}
*/

static gboolean
on_combobox_cm_changed(GtkComboBox * widget, gpointer user_data) {
  dv_view_t * V = (dv_view_t *) user_data;
  V->S->cm = gtk_combo_box_get_active(GTK_COMBO_BOX(widget));
  return TRUE;
}

static gboolean
on_combobox_hm_changed(GtkComboBox * widget, gpointer user_data) {
  dv_view_t * V = (dv_view_t *) user_data;
  V->S->hm = gtk_combo_box_get_active(GTK_COMBO_BOX(widget));
  return TRUE;
}

static gboolean
on_combobox_azf_changed(GtkComboBox * widget, gpointer user_data) {
  dv_view_t * V = (dv_view_t *) user_data;
  int new_val = gtk_combo_box_get_active(GTK_COMBO_BOX(widget));
  dv_view_change_azf(V, new_val);
  return TRUE;
}

static gboolean
on_darea_configure_event(_unused_ GtkWidget * widget, GdkEventConfigure * event, gpointer user_data) {
  dv_viewport_t * VP = (dv_viewport_t *) user_data;
  VP->vpw = event->width;
  VP->vph = event->height;
  dv_view_t * V = NULL;
  int i;
  for (i=0; i<CS->nV; i++)
    if (VP->mV[i]) {
      V = CS->V + i;
      if (V->mainVP == VP) {
        V->S->vpw = event->width;
        V->S->vph = event->height;
      }
    }
  return TRUE;
}

static gboolean
on_window_key_event(_unused_ GtkWidget * widget, GdkEvent * event, _unused_ gpointer user_data) {
  dv_view_t * aV = dv_global_state_get_active_view();
  if (!aV)
    return FALSE;
  
  GdkModifierType modifiers = gtk_accelerator_get_default_mod_mask();
  
  GdkEventKey * e = (GdkEventKey *) event;
  int i;
  //printf("key: %d\n", e->keyval);
  switch (e->keyval) {
  case 120: /* X */
  case 43: /* + */
    dv_do_expanding_one(aV);    
    return TRUE;
  case 99: /* C */
  case 45: /* - */
    dv_do_collapsing_one(aV);
    return TRUE;
  case 104: /* H */
    if (aV->S->adjust_auto_zoomfit)
      dv_view_change_azf(aV, 1);
    dv_do_zoomfit_hor(aV);
    return TRUE;
  case 118: /* V */
    if (aV->S->adjust_auto_zoomfit)
      dv_view_change_azf(aV, 2);
    dv_do_zoomfit_ver(aV);
    return TRUE;
  case 102: /* F */
    if (aV->S->adjust_auto_zoomfit)
      dv_view_change_azf(aV, 4);
    dv_view_do_zoomfit_full(aV);
    return TRUE;
  case 49: /* Ctrl + 1 */
    if ((e->state & modifiers) == GDK_CONTROL_MASK) {
      dv_view_change_lt(aV, 0);
      return TRUE;
    }
    break;
  case 50: /* Ctrl + 2 */
    if ((e->state & modifiers) == GDK_CONTROL_MASK) {
      dv_view_change_lt(aV, 1);
      return TRUE;
    }
    break;
  case 51: /* Ctrl + 3 */
    if ((e->state & modifiers) == GDK_CONTROL_MASK) {
      dv_view_change_lt(aV, 2);
      return TRUE;
    }
    break;
  case 52: /* Ctrl + 4 */
    if ((e->state & modifiers) == GDK_CONTROL_MASK) {
      dv_view_change_lt(aV, 3);
      return TRUE;
    }
    break;
  case 53: /* Ctrl + 5 */
    if ((e->state & modifiers) == GDK_CONTROL_MASK) {
      dv_view_change_lt(aV, 4);
      return TRUE;
    }
    break;
  case 65289: /* Tab */
    if (!aV)
      i = 0;
    else {
      i = aV - CS->V;
      int boo = 0;
      int count = 0;
      while (boo == 0 && count < CS->nV) {
        i = (i + 1) % CS->nV;
        int j;
        for (j = 0; j < CS->nVP; j++)
          if (CS->V[i].mVP[j])
            boo = 1;
        count++;
      }
    }
    dv_do_set_focused_view(CS->V + i, 1);
    break;
  case 65361: /* Left */
    aV->S->x += 15;
    dv_queue_draw(aV);
    return TRUE;
  case 65362: /* Up */
    aV->S->y += 15;
    dv_queue_draw(aV);
    return TRUE;
  case 65363: /* Right */
    aV->S->x -= 15;
    dv_queue_draw(aV);
    return TRUE;
  case 65364: /* Down */
    aV->S->y -= 15;
    dv_queue_draw(aV);
    return TRUE;
  case 116: /* T */
    fprintf(stderr, "open toolbox window of V %ld\n", aV - CS->V);
    dv_view_open_toolbox_window(aV);
    return TRUE;
  default:
    return FALSE;
  }
  return FALSE;
}

static void
on_viewport_tool_icon_clicked(_unused_ GtkToolButton * toolbtn, gpointer user_data) {
  dv_viewport_t * VP = (dv_viewport_t *) user_data;
  dv_view_open_toolbox_window(VP->mainV);
}

static void
on_btn_choose_bt_file_clicked(_unused_ GtkToolButton * toolbtn, gpointer user_data) {
  dv_btsample_viewer_t * btviewer = (dv_btsample_viewer_t *) user_data;

  // Build dialog
  GtkFileChooserAction action = GTK_FILE_CHOOSER_ACTION_OPEN;
  GtkWidget * dialog = gtk_file_chooser_dialog_new("Choose BT File",
                                                   0,
                                                   action,
                                                   "_Cancel",
                                                   GTK_RESPONSE_CANCEL,
                                                   "Ch_oose",
                                                   GTK_RESPONSE_ACCEPT,
                                                   NULL);
  gtk_file_chooser_set_filename(GTK_FILE_CHOOSER(dialog), btviewer->P->fn);
  gint res = gtk_dialog_run(GTK_DIALOG(dialog));
  if (res == GTK_RESPONSE_ACCEPT) {
    GtkFileChooser * chooser = GTK_FILE_CHOOSER(dialog);
    char * filename = gtk_file_chooser_get_filename(chooser);
    gtk_entry_set_text(GTK_ENTRY(btviewer->entry_bt_file_name), filename);
    g_free(filename);
  }
  
  gtk_widget_destroy(dialog);  
}

static void
on_btn_choose_binary_file_clicked(_unused_ GtkToolButton * toolbtn, gpointer user_data) {
  dv_btsample_viewer_t * btviewer = (dv_btsample_viewer_t *) user_data;

  // Build dialog
  GtkFileChooserAction action = GTK_FILE_CHOOSER_ACTION_OPEN;
  GtkWidget * dialog = gtk_file_chooser_dialog_new("Choose Binary File",
                                                   0,
                                                   action,
                                                   "_Cancel",
                                                   GTK_RESPONSE_CANCEL,
                                                   "Ch_oose",
                                                   GTK_RESPONSE_ACCEPT,
                                                   NULL);
  gtk_file_chooser_set_filename(GTK_FILE_CHOOSER(dialog), btviewer->P->fn);
  gint res = gtk_dialog_run(GTK_DIALOG(dialog));
  if (res == GTK_RESPONSE_ACCEPT) {
    GtkFileChooser * chooser = GTK_FILE_CHOOSER(dialog);
    char * filename = gtk_file_chooser_get_filename(chooser);
    gtk_entry_set_text(GTK_ENTRY(btviewer->entry_binary_file_name), filename);
    g_free(filename);
  }
  
  gtk_widget_destroy(dialog);  
}

static void
on_btn_find_node_clicked(_unused_ GtkToolButton * toolbtn, gpointer user_data) {
  dv_btsample_viewer_t * btviewer = (dv_btsample_viewer_t *) user_data;
  long pii = atol(gtk_entry_get_text(GTK_ENTRY(btviewer->entry_node_id)));
  //dv_dag_node_t *node = dv_find_node_with_pii_r(CS->activeV, pii, CS->activeV->D->rt);
  dr_pi_dag_node *pi = dv_pidag_get_node_by_id(btviewer->P, pii);
  if (pi) {
    gtk_combo_box_set_active(GTK_COMBO_BOX(btviewer->combobox_worker), pi->info.worker);
    char str[30];
    sprintf(str, "%llu", (unsigned long long) pi->info.start.t);
    gtk_entry_set_text(GTK_ENTRY(btviewer->entry_time_from), str);
    sprintf(str, "%llu", (unsigned long long) pi->info.end.t);
    gtk_entry_set_text(GTK_ENTRY(btviewer->entry_time_to), str);
  } else {
    gtk_combo_box_set_active(GTK_COMBO_BOX(btviewer->combobox_worker), -1);
    gtk_entry_set_text(GTK_ENTRY(btviewer->entry_time_from), "");
    gtk_entry_set_text(GTK_ENTRY(btviewer->entry_time_to), "");
  }
}

static void
on_btn_run_view_bt_samples_clicked(_unused_ GtkToolButton * toolbtn, gpointer user_data) {
  dv_btsample_viewer_t * btviewer = (dv_btsample_viewer_t *) user_data;

  int worker = gtk_combo_box_get_active(GTK_COMBO_BOX(btviewer->combobox_worker));
  unsigned long long from = atoll(gtk_entry_get_text(GTK_ENTRY(btviewer->entry_time_from)));
  unsigned long long to = atoll(gtk_entry_get_text(GTK_ENTRY(btviewer->entry_time_to)));
  if (from < btviewer->P->start_clock)
    from += btviewer->P->start_clock;
  if (to < btviewer->P->start_clock)
    to += btviewer->P->start_clock;

  dv_btsample_viewer_extract_interval(btviewer, worker, from, to);
}

static void
on_checkbox_xzoom_toggled(_unused_ GtkWidget * widget, gpointer user_data) {
  dv_view_t * V = (dv_view_t *) user_data;
  V->S->do_zoom_x = 1 - V->S->do_zoom_x;
}

static void
on_checkbox_yzoom_toggled(_unused_ GtkWidget * widget, gpointer user_data) {
  dv_view_t * V = (dv_view_t *) user_data;
  V->S->do_zoom_y = 1 - V->S->do_zoom_y;
}

static void
on_checkbox_scale_radix_toggled(_unused_ GtkWidget * widget, gpointer user_data) {
  dv_view_t * V = (dv_view_t *) user_data;
  V->S->do_scale_radix = 1 - V->S->do_scale_radix;
}

static void
on_checkbox_legend_toggled(_unused_ GtkWidget * widget, gpointer user_data) {
  dv_view_t * V = (dv_view_t *) user_data;
  V->S->show_legend = 1 - V->S->show_legend;
  dv_queue_draw(V);
}

static void
on_checkbox_status_toggled(_unused_ GtkWidget * widget, gpointer user_data) {
  dv_view_t * V = (dv_view_t *) user_data;
  V->S->show_status = 1 - V->S->show_status;
  dv_queue_draw(V);
}

static void
on_checkbox_remain_inner_toggled(_unused_ GtkWidget * widget, gpointer user_data) {
  dv_view_t * V = (dv_view_t *) user_data;
  V->S->remain_inner = 1 - V->S->remain_inner;
}

static void
on_checkbox_color_remarked_only_toggled(_unused_ GtkWidget * widget, gpointer user_data) {
  dv_view_t * V = (dv_view_t *) user_data;
  V->S->color_remarked_only = 1 - V->S->color_remarked_only;
  dv_queue_draw(V);
}

static void
on_checkbox_scale_radius_toggled(_unused_ GtkWidget * widget, gpointer user_data) {
  dv_view_t * V = (dv_view_t *) user_data;
  V->S->show_legend = 1 - V->S->show_legend;
}

static void
on_checkbox_azf_toggled(_unused_ GtkWidget * widget, gpointer user_data) {
  dv_view_t * V = (dv_view_t *) user_data;
  V->S->adjust_auto_zoomfit = 1 - V->S->adjust_auto_zoomfit;
}

static void
on_btn_run_dag_scan_clicked(_unused_ GtkButton * button, gpointer user_data) {
  dv_view_t * V = (dv_view_t *) user_data;
  dv_view_scan(V);
  dv_queue_draw_d(V);
}

static void
on_btn_zoomfit_hor_clicked(_unused_ GtkToolButton * toolbtn, gpointer user_data) {
  dv_view_t * V = (dv_view_t *) user_data;
  dv_do_zoomfit_hor(V);
}

static void
on_btn_zoomfit_ver_clicked(_unused_ GtkToolButton * toolbtn, gpointer user_data) {
  dv_view_t * V = (dv_view_t *) user_data;
  dv_do_zoomfit_ver(V);
}

static void
on_toolbar_settings_button_clicked(_unused_ GtkToolButton * toolbtn, _unused_ gpointer user_data) {
  
}

static void
on_toolbar_zoomfit_button_clicked(_unused_ GtkToolButton * toolbtn, _unused_ gpointer user_data) {
  
}

static void
on_toolbar_shrink_button_clicked(_unused_ GtkToolButton * toolbtn, _unused_ gpointer user_data) {
  
}

static void
on_toolbar_expand_button_clicked(_unused_ GtkToolButton * toolbtn, _unused_ gpointer user_data) {
  
}

/****************** end of GUI Callbacks **************************************/
