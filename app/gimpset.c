#include "gimpsignal.h"
#include "gimpsetP.h"

/* Yep, this can be optimized quite a lot */


typedef struct _GimpSetHandler {
	const gchar* signame;
	GtkSignalFunc func;
	gpointer user_data;
} GimpSetHandler;

typedef struct {
	gpointer object;
	GArray* handlers;
	guint destroy_handler;
} Node;

enum
{
  ADD,
  REMOVE,
  MEMBER_MODIFIED,
  LAST_SIGNAL
};

static Node* gimp_set_find_node (GimpSet* set, gpointer ob);
static Node* gimp_set_node_new (GimpSet* set, gpointer ob);
static void gimp_set_node_free (GimpSet* set, Node* n);





static guint gimp_set_signals [LAST_SIGNAL];

static GimpObjectClass* parent_class;

static void
gimp_set_destroy (GtkObject* ob)
{
	GimpSet* set=GIMP_SET(ob);
	GSList* l;
	for(l=set->list;l;l=l->next)
		gimp_set_node_free(set, l->data);
	g_slist_free(set->list);
	g_array_free(set->handlers, TRUE);
	GTK_OBJECT_CLASS(parent_class)->destroy (ob);
}

static void
gimp_set_init (GimpSet* set)
{
	set->list=NULL;
	set->type=GTK_TYPE_OBJECT;
	set->handlers=g_array_new(FALSE, FALSE, sizeof(GimpSetHandler));
}

static void
gimp_set_class_init (GimpSetClass* klass)
{
	GtkObjectClass *object_class = GTK_OBJECT_CLASS (klass);
	GtkType type = object_class->type;

	parent_class=gtk_type_parent_class(type);
	
	object_class->destroy = gimp_set_destroy;
	
	gimp_set_signals[ADD]=
		gimp_signal_new ("add", 0, type, 0, gimp_sigtype_pointer);
	gimp_set_signals[REMOVE]=
		gimp_signal_new ("remove", 0, type, 0, gimp_sigtype_pointer);
	gimp_set_signals[MEMBER_MODIFIED]=
		gimp_signal_new ("member_modified", 0, type, 0, gimp_sigtype_pointer);
	gtk_object_class_add_signals (object_class,
				      gimp_set_signals,
				      LAST_SIGNAL);
}

GtkType gimp_set_get_type (void)
{
	static GtkType type;
	GIMP_TYPE_INIT(type,
		       GimpSet,
		       GimpSetClass,
		       gimp_set_init,
		       gimp_set_class_init,
		       GIMP_TYPE_OBJECT);
	return type;
}


GimpSet*
gimp_set_new (GtkType type, gboolean weak){
	GimpSet* set;

	/* untyped sets must not be weak, since we can't attach a
         * destroy handler */
	g_assert(!(type == GTK_TYPE_NONE && weak == TRUE));

	set=gtk_type_new (gimp_set_get_type ());
	set->type=type;
	set->weak=weak;
	return set;
}

static void
gimp_set_destroy_cb (GtkObject* ob, gpointer data){
	GimpSet* set=GIMP_SET(data);
	gimp_set_remove(set, ob);
}

static Node*
gimp_set_find_node (GimpSet* set, gpointer ob)
{
	GSList* l = set->list;
	for(l = set->list; l; l = l->next){
		Node* n = l->data;
		if (n->object == ob)
			return n;
	}
	return NULL;
}

static Node*
gimp_set_node_new (GimpSet* set, gpointer ob)
{
	gint i;
	Node* n = g_new(Node, 1);
	n->object = ob;
	n->handlers = g_array_new(FALSE, FALSE, sizeof(guint));
	g_array_set_size(n->handlers, set->handlers->len);
	for(i = 0;i < n->handlers->len; i++){
		GimpSetHandler* h
			= &g_array_index(set->handlers, GimpSetHandler, i);
		if(h->signame)
			g_array_index(n->handlers, guint, i)
				= gtk_signal_connect(GTK_OBJECT(ob),
						     h->signame,
						     h->func,
						     h->user_data);
	}
	if(set->weak)
		n->destroy_handler
			= gtk_signal_connect(GTK_OBJECT(ob),
					     "destroy",
					     GTK_SIGNAL_FUNC(gimp_set_destroy_cb),
					     set);
	return n;
}

static void
gimp_set_node_free (GimpSet* set, Node* n)
{
	gint i;
	GimpObject* ob = n->object;
	for(i=0;i < set->handlers->len; i++){
		GimpSetHandler* h
			= &g_array_index(set->handlers, GimpSetHandler, i);
		if(h->signame)
			gtk_signal_disconnect(GTK_OBJECT(ob),
					      g_array_index(n->handlers,
							    guint,
							    i));
	}
	if(set->weak)
		gtk_signal_disconnect(GTK_OBJECT(ob),
				      n->destroy_handler);
	g_array_free(n->handlers, TRUE);
	g_free(n);
}

		
gboolean
gimp_set_add (GimpSet* set, gpointer val)
{
	g_return_val_if_fail(set, FALSE);

	if (set->type != GTK_TYPE_NONE)
		g_return_val_if_fail(GTK_CHECK_TYPE(val, set->type), FALSE);
	
	if(gimp_set_find_node(set, val))
		return FALSE;
	
	set->list=g_slist_prepend(set->list, gimp_set_node_new(set, val));
	
	gtk_signal_emit (GTK_OBJECT(set), gimp_set_signals[ADD], val);
	return TRUE;
}

gboolean
gimp_set_remove (GimpSet* set, gpointer val) {
	Node* n;
	
	g_return_val_if_fail(set, FALSE);

	n = gimp_set_find_node (set, val);
	g_return_val_if_fail(n, FALSE);

	gtk_signal_emit (GTK_OBJECT(set), gimp_set_signals[REMOVE], val);

	gimp_set_node_free(set, n);
	
	set->list=g_slist_remove(set->list, n);
	return TRUE;
}

gboolean
gimp_set_have (GimpSet* set, gpointer val) {
	return !!gimp_set_find_node(set, val);
}

void
gimp_set_foreach(GimpSet* set, GFunc func, gpointer user_data)
{
	GSList* l;
	for(l=set->list; l;l=l->next){
		Node* n = l->data;
		func(n->object, user_data);
	}
}

GtkType
gimp_set_type (GimpSet* set){
	return set->type;
}

GimpSetHandlerId
gimp_set_add_handler(GimpSet* set, const gchar* signame,
		     GtkSignalFunc handler, gpointer user_data){
	GimpSetHandler h;
	GSList* l;
	guint a;

	g_assert(signame);

	/* you can't set a handler on something that's not a GTK
         * object */
	g_assert(set->type != GTK_TYPE_NONE);
	
	h.signame = signame;
	h.func = handler;
	h.user_data = user_data;
	
	for(a=0;a<set->handlers->len;a++)
		if(!g_array_index(set->handlers, GimpSetHandler, a).signame)
			break;
	if(a<set->handlers->len){
		g_array_index(set->handlers, GimpSetHandler, a) = h;
		for(l=set->list;l;l=l->next){
			Node* n = l->data;
			guint i = gtk_signal_connect(GTK_OBJECT(n->object),
						     signame,
						     handler,
						     user_data);
			g_array_index(n->handlers, guint, a) = i;
		}
	} else{
		g_array_append_val(set->handlers, h);
		for(l=set->list;l;l=l->next){
			Node* n = l->data;
			guint i = gtk_signal_connect(GTK_OBJECT(n->object),
						     signame,
						     handler,
						     user_data);
			g_array_append_val(n->handlers, i);
		}
	}
	return a;
}

void
gimp_set_remove_handler(GimpSet* set, GimpSetHandlerId id)
{
	GSList* l;

	/* you can't remove a signal handler on something that's not a
         * GTK object */
	g_return_if_fail(set->type != GTK_TYPE_NONE);

	for(l=set->list;l;l=l->next){
		Node* n = l->data;
		gtk_signal_disconnect(GTK_OBJECT(n->object),
				      g_array_index(n->handlers, guint, id));
	}
	g_array_index(set->handlers, GimpSetHandler, id).signame = NULL;
}

	
void
gimp_set_member_modified(GimpSet* set, gpointer ob)
{
    g_return_if_fail (gimp_set_find_node (set, ob) != NULL);

    gtk_signal_emit (GTK_OBJECT(set), gimp_set_signals[MEMBER_MODIFIED], ob);
}
