#include "dagviz.c"

static const char * DEFAULT_BASE_DIR = "dagprof_graphs/fig/gpl/";

#define dp_open_file(X) dp_open_file_(X, __FILE__, __LINE__)

static FILE *
dp_open_file_(char * filename, char * file, int line) {
  if (!filename || strlen(filename) == 0) {
    fprintf(stderr, "Warning at %s:%d: no file name to output.\n", file, line);
    return NULL;
  }
  FILE * out = fopen(filename, "w");
  if (!out) {
    fprintf(stderr, "Warning at %s:%d: cannot open file.\n", file, line);
    return NULL;
  }
  return out;
}

static void
dp_close_file(FILE * fp) {
  fclose(fp);
}

static void
dp_global_state_init_nogtk(dv_global_state_t * CS) {
  CS->nP = 0;
  CS->nD = 0;
  CS->nV = 0;
  CS->nVP = 0;
  CS->FL = NULL;
  CS->activeV = NULL;
  CS->activeVP = NULL;
  CS->err = DV_OK;
  int i;
  for (i = 0; i < DV_NUM_COLOR_POOLS; i++)
    CS->CP_sizes[i] = 0;
  //dv_btsample_viewer_init(CS->btviewer);
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
  CS->SD->node_pool_label = NULL;
  CS->SD->entry_pool_label = NULL;
  CS->SD->fn = DV_STAT_DISTRIBUTION_OUTPUT_DEFAULT_NAME;
  CS->SD->bar_width = 20;
  for (i = 0; i < DV_MAX_DAG; i++) {
    CS->SBG->checked_D[i] = 1;
    CS->SBG->work[i] = 0.0;
    CS->SBG->delay[i] = 0.0;
    CS->SBG->nowork[i] = 0.0;
  }
  CS->SBG->fn = DV_STAT_BREAKDOWN_OUTPUT_DEFAULT_NAME;
  CS->SBG->fn_2 = DV_STAT_BREAKDOWN_OUTPUT_DEFAULT_NAME_2;
  int cp;
  for (cp = 0; cp < DV_NUM_CRITICAL_PATHS; cp++) {
    CS->SBG->checked_cp[cp] = 0;
    if (cp == DV_CRITICAL_PATH_1)
      CS->SBG->checked_cp[cp] = 1;
  }
  
  CS->context_view = NULL;
  CS->context_node = NULL;

  CS->verbose_level = DV_VERBOSE_LEVEL_DEFAULT;

  CS->oncp_flags[DV_CRITICAL_PATH_0] = DV_NODE_FLAG_CRITICAL_PATH_0;
  CS->oncp_flags[DV_CRITICAL_PATH_1] = DV_NODE_FLAG_CRITICAL_PATH_1;
  CS->oncp_flags[DV_CRITICAL_PATH_2] = DV_NODE_FLAG_CRITICAL_PATH_2;
  CS->cp_colors[DV_CRITICAL_PATH_0] = DV_CRITICAL_PATH_0_COLOR;
  CS->cp_colors[DV_CRITICAL_PATH_1] = DV_CRITICAL_PATH_1_COLOR;
  CS->cp_colors[DV_CRITICAL_PATH_2] = DV_CRITICAL_PATH_2_COLOR;
}

_static_unused_ void
dp_call_gnuplot(char * filename) {
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

void
dp_stat_graph_execution_time_breakdown(char * filename) {
  /* generate plots */
  FILE * out = dp_open_file(filename);
  if (!out) return;
  fprintf(out,
          "#set terminal png font arial 14 size 640,350\n"
          "#set output ~/Desktop/00dv_stat_breakdown.png\n"
          "set style data histograms\n"
          "set style histogram rowstacked\n"
          "set style fill solid 0.8 noborder\n"
          "set key off #outside center top horizontal\n"
          "set boxwidth 0.85 relative\n"
          "set yrange [0:]\n"
          //          "set xtics rotate by -30\n"
          //          "set xlabel \"clocks\"\n"
          "set ylabel \"cumul. clocks\"\n"
          "plot "
          "\"-\" u 2:xtic(1) w histogram t \"work\", "
          "\"-\" u 3 w histogram t \"delay\", "
          "\"-\" u 4 w histogram t \"nowork\"\n");
  int i, j;
  for (j = 0; j < 3; j++) {
    for (i = 0; i < CS->nD; i++) {
      dv_dag_t * D = &CS->D[i];
      fprintf(out,
              "\"%s\"  %lld %lld %lld\n",
              D->name_on_graph,
              CS->SBG->work[i],
              CS->SBG->delay[i],
              CS->SBG->nowork[i]);
    }
    fprintf(out,
            "e\n");
  }
  fprintf(out,
          "pause -1\n");
  dp_close_file(out);
  fprintf(stdout, "generated breakdown graphs to %s\n", filename);

  /* graph 2 */
  char filename_[1000];
  strcpy(filename_, filename);
  filename_[strlen(filename) - 4] = '\0';
  sprintf(filename_, "%s_2.gpl", filename_);
  out = dp_open_file(filename_);
  if (!out) return;
  fprintf(out,
          "#set terminal png font arial 14 size 640,350\n"
          "#set output ~/Desktop/00dv_stat_breakdown.png\n"
          "set style data histograms\n"
          "set style histogram rowstacked\n"
          "set style fill solid 0.8 noborder\n"
          "set key off #outside center top horizontal\n"
          "set boxwidth 0.85 relative\n"
          "set yrange [0:]\n"
          //          "set xtics rotate by -30\n"
          //          "set xlabel \"clocks\"\n"
          "set ylabel \"cumul. clocks\"\n"
          "plot "
          "\"-\" u 2:xtic(1) w histogram t \"work\", "
          "\"-\" u 3 w histogram t \"delay\", "
          "\"-\" u 4 w histogram t \"nowork\"\n");
  for (j = 0; j < 3; j++) {
    for (i = 1; i < CS->nD; i++) {
      dv_dag_t * D = &CS->D[i];
      fprintf(out,
              "\"%s\"  %lld %lld %lld\n",
              D->name_on_graph,
              CS->SBG->work[i] - CS->SBG->work[0],
              CS->SBG->delay[i] - CS->SBG->delay[0],
              CS->SBG->nowork[i] - CS->SBG->nowork[0]);
    }
    fprintf(out, "e\n");
  }
  fprintf(out, "pause -1\n");
  dp_close_file(out);
  fprintf(stdout, "generated breakdown graphs to %s\n", filename_);

  /* graph 3 */
  strcpy(filename_, filename);
  filename_[strlen(filename) - 4] = '\0';
  sprintf(filename_, "%s_3.gpl", filename_);
  out = dp_open_file(filename_);
  if (!out) return;
  fprintf(out,
          "#set terminal png font arial 14 size 640,350\n"
          "set terminal postscript eps enhanced color size 14cm,7cm\n"
          "#set output ~/Desktop/00dv_stat_breakdown.png\n"
          "set style data histograms\n"
          "set style histogram rowstacked\n"
          "set style fill solid 0.8 noborder\n"
          "set key off #outside center top horizontal\n"
          "set boxwidth 0.85 relative\n"
          "set yrange [0:]\n"
          "set xtics rotate by -20\n"
          "#set xlabel \"clocks\"\n"
          "set ylabel \"cumul. clocks\"\n"
          "plot "
          "\"-\" u 2:xtic(1) w histogram t \"work\", "
          "\"-\" u 3 w histogram t \"delay\", "
          "\"-\" u 4 w histogram t \"nowork\"\n");
  for (j = 0; j < 3; j++) {
    for (i = 1; i < CS->nD; i++) {
      dv_dag_t * D = &CS->D[i];
      fprintf(out,
              "\"%s\"  %lld %lld %lld\n",
              D->name_on_graph,
              CS->SBG->work[i] - CS->SBG->work[0],
              CS->SBG->delay[i] - CS->SBG->delay[0],
              CS->SBG->nowork[i] - CS->SBG->nowork[0]);
    }
    for (i = 1; i < CS->nD; i++) {
      dv_dag_t * D = &CS->D[i];
      int cp = DV_CRITICAL_PATH_1;
      fprintf(out,
              "\"%s nw\" 0.0 0.0 %lf\n",
              D->name_on_graph,
              D->rt->cpss[cp].sched_delay_nowork);
    }
    fprintf(out, "e\n");
  }
  fprintf(out, "pause -1\n");
  dp_close_file(out);
  fprintf(stdout, "generated breakdown graphs to %s\n", filename_);

  /* graph 4 */
  strcpy(filename_, filename);
  filename_[strlen(filename) - 4] = '\0';
  sprintf(filename_, "%s_4.gpl", filename_);
  out = dp_open_file(filename_);
  if (!out) return;
  fprintf(out,
          "#set terminal png font arial 14 size 640,350\n"
          "set terminal postscript eps enhanced color size 10cm,5cm\n"
          "#set output ~/Desktop/00dv_stat_breakdown.png\n"
          "set style fill solid 0.8 noborder\n"
          "set key outside center top horizontal\n"
          "set boxwidth 0.85 relative\n"
          "set yrange [*<0:]\n"
          "set y2range [*<0:100<*]\n"
          "#set xtics rotate by -20\n"
          "set ylabel \"cumul. clocks\"\n"
          "set y2tics\n"
          "set ytics nomirror\n"
          "set y2label \"percent\"\n"
          "plot "
          "\"-\" u 2:xtic(1) w boxes lc rgb \"orange\" t \"perf. loss\" axes x1y1, "
          "\"-\" u 3 w linespoints t \"percentage\" axes x1y2, "
          "\"-\" u 0:3:4 w labels center offset 0,1 axes x1y2 notitle \n");
  for (j = 0; j < 3; j++) {
    for (i = 1; i < CS->nD; i++) {
      dv_dag_t * D = &CS->D[i];
      double loss = (CS->SBG->work[i] - CS->SBG->work[0])
        + (CS->SBG->delay[i] - CS->SBG->delay[0])
        + (CS->SBG->nowork[i] - CS->SBG->nowork[0]);
      double percentage = loss * 100.0 / (CS->SBG->work[0] + CS->SBG->delay[0] + CS->SBG->nowork[0]);
      fprintf(out,
              "\"%s\"  %lf %lf \"%.1lf%%\"\n",
              D->name_on_graph,
              loss,
              percentage,
              percentage);
    }
    fprintf(out, "e\n");
  }
  fprintf(out, "pause -1\n");
  dp_close_file(out);
  fprintf(stdout, "generated breakdown graphs to %s\n", filename_);

}

void
dp_stat_graph_critical_path_breakdown(char * filename) {
  /* generate plots */
  FILE * out = dp_open_file(filename);
  if (!out) return;
  fprintf(out,
          "#set terminal postscript eps enhanced color size 12cm,5.5cm\n"
          "#set terminal png font arial 14 size 640,350\n"
          "#set output ~/Desktop/00dv_stat_breakdown.png\n"
          "set style data histograms\n"
          "set style histogram rowstacked\n"
          "set style fill solid 0.8 noborder\n"
          "set key off #outside center top horizontal\n"
          "set boxwidth 0.85 relative\n"
          "set yrange [0:]\n"
          "set ylabel \"cumul. clocks\"\n"
          "#set xtics rotate by -20\n"
          "plot "
          "\"-\" u 2:xtic(1) w histogram t \"work\", "
          "\"-\" u 3 w histogram t \"delay\"\n");
  int ptimes, cp;
  for (ptimes = 0; ptimes < 2; ptimes++) {
    
    int i;
    for (i = 0; i < CS->nD; i++) {
      dv_dag_t * D = &CS->D[i];
      /* exclude serial DAGs */
      if (strstr(D->name_on_graph, "serial")) continue;
      cp = DV_CRITICAL_PATH_1;
      fprintf(out, "\"%s", D->name_on_graph);
      fprintf(out,
              "\"  %lf %lf",
              D->rt->cpss[cp].work,
              D->rt->cpss[cp].delay);
      fprintf(out, "\n");
    }
    
    fprintf(out, "e\n");
  }

  fprintf(out,
          "pause -1\n");
  dp_close_file(out);
  fprintf(stdout, "generated critical path's work-delay breakdown graphs to %s\n", filename);
}

void
dp_stat_graph_critical_path_delay_breakdown(char * filename) {
  /* generate plots */
  FILE * out = dp_open_file(filename);
  if (!out) return;
  fprintf(out,
          "#set terminal postscript eps enhanced color size 12cm,5.5cm\n"
          "#set terminal png font arial 14 size 640,350\n"
          "#set output ~/Desktop/00dv_stat_breakdown.png\n"
          "set style data histograms\n"
          "set style histogram rowstacked\n"
          "set style fill solid 0.8 noborder\n"
          "set key off #outside center top horizontal\n"
          "set boxwidth 0.85 relative\n"
          "set yrange [0:]\n"
          "set ylabel \"cumul. clocks\"\n"
          "#set xtics rotate by -20\n"
          "plot "
          "\"-\" u 2:xtic(1) w histogram t \"work\", "
          "\"-\" u 3 w histogram t \"busy delay\", "
          "\"-\" u 4 w histogram t \"scheduler delay\"\n");

  int ptimes, cp;
  for (ptimes = 0; ptimes < 3; ptimes++) {

    int i;
    for (i = 0; i < CS->nD; i++) {
      dv_dag_t * D = &CS->D[i];
      /* exclude serial DAGs */
      if (strstr(D->name_on_graph, "serial")) continue;
      cp = DV_CRITICAL_PATH_1;
      fprintf(out, "\"%s", D->name_on_graph);
      fprintf(out,
              "\"  %lf %lf %lf",
              D->rt->cpss[cp].work,
              D->rt->cpss[cp].delay - D->rt->cpss[cp].sched_delay,
              D->rt->cpss[cp].sched_delay);
      fprintf(out, "\n");
    }
    
    fprintf(out, "e\n");
  }

  fprintf(out,
          "pause -1\n");
  dp_close_file(out);
  fprintf(stdout, "generated critical-path breakdown graphs to %s\n", filename);
}

void
dp_stat_graph_critical_path_edge_based_delay_breakdown(char * filename) {
  /* calculate critical paths */
  int to_print_other_cont = 0;
  int i;
  for (i = 0; i < CS->nD; i++) {
    dv_dag_t * D = &CS->D[i];
    int cp;
    for (cp = 0; cp < DV_NUM_CRITICAL_PATHS; cp++)
      if (D->rt->cpss[cp].sched_delays[dr_dag_edge_kind_other_cont] > 0.0)
        to_print_other_cont = 1;
  }

  /* generate plots */
  FILE * out = dp_open_file(filename);
  if (!out) return;
  fprintf(out,
          "#set terminal postscript eps enhanced color size 12cm,5.5cm\n"
          "#set terminal png font arial 14 size 640,350\n"
          "#set output ~/Desktop/00dv_stat_breakdown.png\n"
          "set style data histograms\n"
          "set style histogram rowstacked\n"
          "set style fill solid 0.8 noborder\n"
          "set key off #outside center top horizontal\n"
          "set boxwidth 0.85 relative\n"
          "set yrange [0:]\n"
          "set ylabel \"cumul. clocks\"\n"
          "#set xtics rotate by -20\n"
          "plot "
          "\"-\" u 2:xtic(1) w histogram t \"work\", "
          "\"-\" u 3 w histogram t \"busy delay\", "
          "\"-\" u 4 w histogram t \"end\", "
          "\"-\" u 5 w histogram t \"create\", "
          "\"-\" u 6 w histogram t \"create cont.\", "
          "\"-\" u 7 w histogram t \"wait cont.\"");
  if (to_print_other_cont)
    fprintf(out, ", \"-\" u 8 w histogram t \"other cont.\"\n");
  else
    fprintf(out, "\n");    

  /* print data */
  int nptimes = 7;
  if (!to_print_other_cont)
    nptimes = 6;
  int ptimes, cp;
  for (ptimes = 0; ptimes < nptimes; ptimes++) {
    
    for (i = 0; i < CS->nD; i++) {
      dv_dag_t * D = &CS->D[i];
      /* exclude serial DAGs */
      if (strstr(D->name_on_graph, "serial")) continue;
      cp = DV_CRITICAL_PATH_1;
      fprintf(out, "\"%s", D->name_on_graph);
      fprintf(out,
              "\"  %lf %lf",
              D->rt->cpss[cp].work,
              D->rt->cpss[cp].delay - D->rt->cpss[cp].sched_delay);
      int ek;
      for (ek = 0; ek < dr_dag_edge_kind_max; ek++)
        if (ek != dr_dag_edge_kind_other_cont || to_print_other_cont)
          fprintf(out, " %lf", D->rt->cpss[cp].sched_delays[ek]);
      fprintf(out, "\n");
    }
    
    fprintf(out, "e\n");
  }

  fprintf(out,
          "pause -1\n");
  dp_close_file(out);
  fprintf(stdout, "generated critical path's scheduler delay breakdown graphs to %s\n", filename);
}

void
dp_stat_graph_performance_loss_factors(char * filename_) {
  /* Get max y */
  int i;
  double max_y = 0.0;
  for (i = 0; i < CS->nD; i++) {
    double perf_loss = (CS->SBG->work[i] - CS->SBG->work[0])
      + (CS->SBG->delay[i] - CS->SBG->delay[0])
      + (CS->SBG->nowork[i] - CS->SBG->nowork[0]);
    if (perf_loss > max_y)
      max_y = perf_loss;
  }

  FILE * out;
  char filename[1000];

  /* work stretch */
  strcpy(filename, filename_);
  filename[strlen(filename) - 4] = '\0';
  sprintf(filename, "%s_work_stretch.gpl", filename);
  out = dp_open_file(filename);
  if (!out) return;
  fprintf(out,
          "#set terminal png font arial 14 size 640,350\n"
          "set terminal postscript eps enhanced color size 10cm,5cm\n"
          "#set output ~/Desktop/00dv_stat_breakdown.png\n"
          "set style fill solid 0.8 noborder\n"
          "set key outside center top horizontal\n"
          "set boxwidth 0.85 relative\n"
          "set yrange [*<0:%.0lf<*]\n"
          "set y2range [*<0:100<*]\n"
          "#set xtics rotate by -20\n"
          "set ylabel \"cumul. clocks\"\n"
          "set y2tics\n"
          "set ytics nomirror\n"
          "set y2label \"percent\"\n"
          "plot "
          "\"-\" u 2:xtic(1) w boxes lc rgb \"red\" t \"work stretch\" axes x1y1, "
          "\"-\" u 3 w linespoints t \"percentage\" axes x1y2, "
          "\"-\" u 0:3:4 w labels center offset 0,1 axes x1y2 notitle \n",
          max_y);
  int j;
  for (j = 0; j < 3; j++) {
    for (i = 1; i < CS->nD; i++) {
      dv_dag_t * D = &CS->D[i];
      double total_loss = (CS->SBG->work[i] - CS->SBG->work[0])
        + (CS->SBG->delay[i] - CS->SBG->delay[0])
        + (CS->SBG->nowork[i] - CS->SBG->nowork[0]);
      double factor = (CS->SBG->work[i] - CS->SBG->work[0]);
      double percentage = (factor / total_loss) * 100.0;
      if (total_loss < 0) percentage = 0.0;
      fprintf(out,
              "\"%s\"  %lf %lf \"%.1lf%%\"\n",
              D->name_on_graph,
              factor,
              percentage,
              percentage);
    }
    fprintf(out, "e\n");
  }
  fprintf(out, "pause -1\n");
  dp_close_file(out);
  fprintf(stdout, "generated graph of work stretch to %s\n", filename);

  /* delay */
  strcpy(filename, filename_);
  filename[strlen(filename) - 4] = '\0';
  sprintf(filename, "%s_delay.gpl", filename);
  out = dp_open_file(filename);
  if (!out) return;
  fprintf(out,
          "#set terminal png font arial 14 size 640,350\n"
          "set terminal postscript eps enhanced color size 10cm,5cm\n"
          "#set output ~/Desktop/00dv_stat_breakdown.png\n"
          "set style fill solid 0.8 noborder\n"
          "set key outside center top horizontal\n"
          "set boxwidth 0.85 relative\n"
          "set yrange [*<0:%.0lf<*]\n"
          "set y2range [*<0:100<*]\n"
          "#set xtics rotate by -20\n"
          "set ylabel \"cumul. clocks\"\n"
          "set y2tics\n"
          "set ytics nomirror\n"
          "set y2label \"percent\"\n"
          "plot "
          "\"-\" u 2:xtic(1) w boxes lc rgb \"green\" t \"delay\" axes x1y1, "
          "\"-\" u 3 w linespoints t \"percentage\" axes x1y2, "
          "\"-\" u 0:3:4 w labels center offset 0,1 axes x1y2 notitle \n",
          max_y);
  for (j = 0; j < 3; j++) {
    for (i = 1; i < CS->nD; i++) {
      dv_dag_t * D = &CS->D[i];
      double total_loss = (CS->SBG->work[i] - CS->SBG->work[0])
        + (CS->SBG->delay[i] - CS->SBG->delay[0])
        + (CS->SBG->nowork[i] - CS->SBG->nowork[0]);
      double factor = (CS->SBG->delay[i] - CS->SBG->delay[0]);
      double percentage = (factor / total_loss) * 100.0;
      if (total_loss < 0) percentage = 0.0;
      fprintf(out,
              "\"%s\"  %lf %lf \"%.1lf%%\"\n",
              D->name_on_graph,
              factor,
              percentage,
              percentage);
    }
    fprintf(out, "e\n");
  }
  fprintf(out, "pause -1\n");
  dp_close_file(out);
  fprintf(stdout, "generated graph of total delay to %s\n", filename);

  /* nowork */
  strcpy(filename, filename_);
  filename[strlen(filename) - 4] = '\0';
  sprintf(filename, "%s_nowork.gpl", filename);
  out = dp_open_file(filename);
  if (!out) return;
  fprintf(out,
          "#set terminal png font arial 14 size 640,350\n"
          "set terminal postscript eps enhanced color size 10cm,5cm\n"
          "#set output ~/Desktop/00dv_stat_breakdown.png\n"
          "set style fill solid 0.8 noborder\n"
          "set key outside center top horizontal\n"
          "set boxwidth 0.85 relative\n"
          "set yrange [*<0:%.0lf<*]\n"
          "set y2range [*<0:100<*]\n"
          "#set xtics rotate by -20\n"
          "set ylabel \"cumul. clocks\"\n"
          "set y2tics\n"
          "set ytics nomirror\n"
          "set y2label \"percent\"\n"
          "plot "
          "\"-\" u 2:xtic(1) w boxes lc rgb \"blue\" t \"nowork\" axes x1y1, "
          "\"-\" u 3 w linespoints t \"percentage\" axes x1y2, "
          "\"-\" u 0:3:4 w labels center offset 0,1 axes x1y2 notitle \n",
          max_y);
  for (j = 0; j < 3; j++) {
    for (i = 1; i < CS->nD; i++) {
      dv_dag_t * D = &CS->D[i];
      double total_loss = (CS->SBG->work[i] - CS->SBG->work[0])
        + (CS->SBG->delay[i] - CS->SBG->delay[0])
        + (CS->SBG->nowork[i] - CS->SBG->nowork[0]);
      double factor = (CS->SBG->nowork[i] - CS->SBG->nowork[0]);
      double percentage = (factor / total_loss) * 100.0;
      if (total_loss < 0) percentage = 0.0;
      fprintf(out,
              "\"%s\"  %lf %lf \"%.1lf%%\"\n",
              D->name_on_graph,
              factor,
              percentage,
              percentage);
    }
    fprintf(out, "e\n");
  }
  fprintf(out, "pause -1\n");
  dp_close_file(out);
  fprintf(stdout, "generated graph of nowork by scheduler delay to %s\n", filename);
}

void
dp_stat_graph_nowork_by_scheduler_delay(char * filename) {
  FILE * out = dp_open_file(filename);
  if (!out) return;
  /* get max y */
  int i;
  double max_y = 0.0;
  for (i = 0; i < CS->nD; i++) {
    double perf_loss = (CS->SBG->work[i] - CS->SBG->work[0])
      + (CS->SBG->delay[i] - CS->SBG->delay[0])
      + (CS->SBG->nowork[i] - CS->SBG->nowork[0]);
    if (perf_loss > max_y)
      max_y = perf_loss;
  }
  
  /* nowork by scheduler delay */
  fprintf(out,
          "#set terminal png font arial 14 size 640,350\n"
          "set terminal postscript eps enhanced color size 10cm,5cm\n"
          "#set output ~/Desktop/00dv_stat_breakdown.png\n"
          "set style fill solid 0.8 noborder\n"
          "set key outside center top horizontal\n"
          "set boxwidth 0.85 relative\n"
          "set yrange [*<0:%.0lf<*]\n"
          "set y2range [*<0:100<*]\n"
          "#set xtics rotate by -20\n"
          "set ylabel \"cumul. clocks\"\n"
          "set y2tics\n"
          "set ytics nomirror\n"
          "set y2label \"percent\"\n"
          "plot "
          "\"-\" u 2:xtic(1) w boxes lc rgb \"blue\" t \"nowork cp\" axes x1y1, "
          "\"-\" u 3 w linespoints t \"percentage\" axes x1y2, "
          "\"-\" u 0:3:4 w labels center offset 0,1 axes x1y2 notitle \n",
          max_y);
  int j;
  for (j = 0; j < 3; j++) {
    for (i = 1; i < CS->nD; i++) {
      dv_dag_t * D = &CS->D[i];
      double perf_loss = (CS->SBG->work[i] - CS->SBG->work[0])
        + (CS->SBG->delay[i] - CS->SBG->delay[0])
        + (CS->SBG->nowork[i] - CS->SBG->nowork[0]);
      int cp = DV_CRITICAL_PATH_1;
      double percentage = (D->rt->cpss[cp].sched_delay_nowork / perf_loss) * 100.0;
      if (perf_loss < 0) percentage = 0.0;
      fprintf(out,
              "\"%s\"  %lf %lf \"%.1lf%%\"\n",
              D->name_on_graph,
              D->rt->cpss[cp].sched_delay_nowork,
              percentage,
              percentage);
    }
    fprintf(out, "e\n");
  }
  fprintf(out, "pause -1\n");
  dp_close_file(out);
  fprintf(stdout, "generated graph of nowork by scheduler delay to %s\n", filename);

  /* remained nowork */
  char filename_[1000];
  strcpy(filename_, filename);
  filename_[strlen(filename_) - 4] = '\0';
  sprintf(filename_, "%s_remained_nowork.gpl", filename_);
  out = dp_open_file(filename_);
  if (!out) return;
  fprintf(out,
          "#set terminal png font arial 14 size 640,350\n"
          "set terminal postscript eps enhanced color size 10cm,5cm\n"
          "#set output ~/Desktop/00dv_stat_breakdown.png\n"
          "set style fill solid 0.8 noborder\n"
          "set key outside center top horizontal\n"
          "set boxwidth 0.85 relative\n"
          "set yrange [*<0:%.0lf<*]\n"
          "set y2range [*<0:100<*]\n"
          "#set xtics rotate by -20\n"
          "set ylabel \"cumul. clocks\"\n"
          "set y2tics\n"
          "set ytics nomirror\n"
          "set y2label \"percent\"\n"
          "plot "
          "\"-\" u 2:xtic(1) w boxes lc rgb \"blue\" t \"nowork w\" axes x1y1, "
          "\"-\" u 3 w linespoints t \"percentage\" axes x1y2, "
          "\"-\" u 0:3:4 w labels center offset 0,1 axes x1y2 notitle \n",
          max_y);
  for (j = 0; j < 3; j++) {
    for (i = 1; i < CS->nD; i++) {
      dv_dag_t * D = &CS->D[i];
      double perf_loss = (CS->SBG->work[i] - CS->SBG->work[0])
        + (CS->SBG->delay[i] - CS->SBG->delay[0])
        + (CS->SBG->nowork[i] - CS->SBG->nowork[0]);
      int cp = DV_CRITICAL_PATH_1;
      double amount = (CS->SBG->nowork[i] - CS->SBG->nowork[0]) - D->rt->cpss[cp].sched_delay_nowork;
      double percentage = (amount / perf_loss) * 100.0;
      if (perf_loss < 0) percentage = 0.0;
      fprintf(out,
              "\"%s\"  %lf %lf \"%.1lf%%\"\n",
              D->name_on_graph,
              amount,
              percentage,
              percentage);
    }
    fprintf(out, "e\n");
  }
  fprintf(out, "pause -1\n");
  dp_close_file(out);
  fprintf(stdout, "generated graph of nowork by scheduler delay to %s\n", filename);
}

int
main(int argc, char ** argv) {
  /* Initialization */
  const char * dp_prefix = getenv("DP_PREFIX");
  if (!dp_prefix)
    dp_prefix = DEFAULT_BASE_DIR;
  printf("dp_prefix = %s\n", dp_prefix);

  /* GTK */
  gtk_init(&argc, &argv);

  /* CS, GUI */
  dp_global_state_init_nogtk(CS);
  dv_gui_init(GUI);

  /* PIDAG */
  int i;
  for (i = 1; i < argc; i++) {
    glob_t globbuf;
    glob(argv[i], GLOB_TILDE | GLOB_PERIOD | GLOB_BRACE, NULL, &globbuf);
    int j;
    for (j = 0; j < (int) globbuf.gl_pathc; j++) {
      _unused_ dv_pidag_t * P = dv_create_new_pidag(globbuf.gl_pathv[j]);
    }
    if (globbuf.gl_pathc > 0)
      globfree(&globbuf);
  }
  for (i = 0; i < CS->nP; i++) {
    dv_pidag_t * P = &CS->P[i];
    _unused_ dv_dag_t * D = dv_create_new_dag(P);
    //_unused_ dv_view_t * V = dv_create_new_view(D);
  }

  /* Preparation */
  for (i = 0; i < CS->nD; i++) {
    dv_dag_t * D = &CS->D[i];

    dr_pi_dag * G = D->P->G;
    dr_basic_stat bs[1];
    dr_basic_stat_init(bs, G);
    dr_calc_inner_delay(bs, G);
    dr_calc_edges(bs, G);
    dr_pi_dag_chronological_traverse(G, (chronological_traverser *)bs);

    dr_clock_t work = bs->total_t_1;
    dr_clock_t delay = bs->cum_delay + (bs->total_elapsed - bs->total_t_1);
    dr_clock_t no_work = bs->cum_no_work;
    CS->SBG->work[i] = work;
    CS->SBG->delay[i] = delay;
    CS->SBG->nowork[i] = no_work;
  }
  for (i = 0; i < CS->nD; i++) {
    dv_dag_t * D = &CS->D[i];
    dv_dag_compute_critical_paths(D);
  }
  
  /* Graphs */
  char filename[1000];
  
  sprintf(filename, "%s/%s", dp_prefix, "execution_time_breakdown.gpl");
  dp_stat_graph_execution_time_breakdown(filename);
  //dp_call_gnuplot(filename);
  
  sprintf(filename, "%s/%s", dp_prefix, "critical_path_breakdown_0.gpl");
  dp_stat_graph_critical_path_breakdown(filename);
  //dp_call_gnuplot(filename);
  
  sprintf(filename, "%s/%s", dp_prefix, "critical_path_breakdown_1.gpl");
  dp_stat_graph_critical_path_delay_breakdown(filename);
  //dp_call_gnuplot(filename);
  
  sprintf(filename, "%s/%s", dp_prefix, "critical_path_breakdown_2.gpl");
  dp_stat_graph_critical_path_edge_based_delay_breakdown(filename);
  //dp_call_gnuplot(filename);
  
  sprintf(filename, "%s/%s", dp_prefix, "nowork_by_scheduler_delay.gpl");
  dp_stat_graph_nowork_by_scheduler_delay(filename);
  
  sprintf(filename, "%s/%s", dp_prefix, "performance_loss_factor.gpl");
  dp_stat_graph_performance_loss_factors(filename);

  /* Finalization */  
  return 1;
}