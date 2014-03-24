#ifndef DAGVIZ_HEADER_
#define DAGVIZ_HEADER_

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <assert.h>
#include <errno.h>
#include <gtk/gtk.h>
#include <cairo.h>

#include <fcntl.h>
#include <sys/mman.h>
#include <sys/types.h>
#include <sys/stat.h>

#include <dag_recorder_impl.h>


#define DV_ZOOM_INCREMENT 1.25
#define DV_HDIS 70
#define DV_VDIS 100
#define DV_RADIUS 30
#define NUM_COLORS 34


/*-----------------Data Structures-----------------*/

typedef struct dv_grid_line {
	double c;  /* coordinate */
	int lv;
	struct dv_grid_line * l;  /* left next grid line */
	struct dv_grid_line * r;  /* right next grid line */
} dv_grid_line_t;

typedef struct dv_graph_node {
	long idx;
	dr_dag_node_info * info;
	int ek; /* edge kind */
	union {
		/* create */
		struct {
			struct dv_graph_node * el;  /* edge left */
			struct dv_graph_node * er;  /* edge right */
		};
		/* wait_cont, end_task */
		struct dv_graph_node * ej;  /* edge join */
	};
	
	dv_grid_line_t * vl;  /* vertical line */
	dv_grid_line_t * hl;  /* horizontal line */
} dv_graph_node_t;

typedef struct dv_linked_list {
	void * item;
	struct dv_linked_list * next;
} dv_linked_list_t;

typedef struct dv_graph {
	long n;               /* num of nodes on the visual graph */
  long num_workers;		  /* num of workers */
	dv_graph_node_t * T;  /* all nodes in a contiguous array */
	dv_graph_node_t * root_node;  /* root node of the graph */
  dr_pi_string_table * S;	 /* string table */
	
	dv_grid_line_t * root_vl;  /* vertical line */
	dv_grid_line_t * root_hl;  /* horizontal line */
	double zoom_ratio;  /* zoom ratio of the graph to draw */
	double width, height;  /* viewport's size */
	double x, y;        /* current coordinates of the central point */
	dv_linked_list_t itl; /* list of nodes that have info tag */
} dv_graph_t;

typedef struct dv_status {
	char drag_on;
	double pressx, pressy;
	int nc; /* node color: 0->worker, 1->cpu, 2->kind, 3->last */
} dv_status_t;


/*-----------------Inlines-----------------*/

static int dv_check_(int condition, const char * condition_s, 
										 const char * __file__, int __line__, 
										 const char * func) {
	if (!condition) {
		fprintf(stderr, "%s:%d:%s: check failed : %s\n", 
						__file__, __line__, func, condition_s);
		exit(1);
	}
	return 1;
}

#define dv_check(x) (dv_check_(((x)?1:0), #x, __FILE__, __LINE__, __func__))


/*------Global variables-----*/

extern const char * const NODE_KIND_NAMES[];
extern const char * const EDGE_KIND_NAMES[];
extern const char * const DV_COLORS[];

extern dv_graph_t G[];
extern GtkWidget *window;
extern GtkWidget *darea;
extern dv_status_t S[];


/*-----------------Headers-----------------*/

/* print.c */
void print_pi_dag_node(dr_pi_dag_node *, int);
void print_pi_dag_edge(dr_pi_dag_edge *, int);
void print_pi_string_table(dr_pi_string_table *, int);
void print_dvgraph_to_stdout(dv_graph_t *);
void print_layout_to_stdout(dv_graph_t *);

/* check.c */
void read_dag_file_to_stdout(char *);
void check_layout(dv_graph_t *);

/* layout.c */
void read_dag_file_to_pidag(char *, dr_pi_dag *);
void convert_pidag_to_dvgraph(dr_pi_dag *, dv_graph_t *);
void layout_dvgraph(dv_graph_t *);

/* draw.c */
void draw_dvgraph(cairo_t *, dv_graph_t *);


#endif /* DAGVIZ_HEADER_ */
