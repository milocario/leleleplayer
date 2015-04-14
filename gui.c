#include <stdio.h>
#include "gui.h"

static void destroy (GtkWidget *window, gpointer data) {
	ShutdownOpenAL();
	gtk_main_quit ();
}

bool InitOpenAL(void) {
	ALCdevice *Device = alcOpenDevice(NULL);
	if(!Device) {
		printf("Error while opening the device\n");
		exit(1);
	}
	
	ALCcontext *Context = alcCreateContext(Device, NULL);
	if(!Context) {
		printf("Error while creating context\n");
		exit(1);
	}

	if(!alcMakeContextCurrent(Context)) {
		printf("Error while activating context\n");
		exit(1);
	}
	return 0;
}

static void ShutdownOpenAL(void) {
	ALCcontext *Context = alcGetCurrentContext();
	ALCdevice *Device = alcGetContextsDevice(Context);

	alcMakeContextCurrent(NULL);
	alcDestroyContext(Context);
	alcCloseDevice(Device);
}

static void row_activated(GtkTreeView *treeview, GtkTreePath *path, GtkTreeViewColumn *column, struct arguments *argument) {
	GtkTreeModel *model;
	GtkTreeIter iter;
	char *tempfile;

	alDeleteSources(1, &argument->source);
	alGenSources(1, &argument->source);

	model = gtk_tree_view_get_model(treeview);
	if(gtk_tree_model_get_iter(model, &iter, path)) {
		gtk_tree_model_get(model, &iter, AFILE, &tempfile, -1);
	}
	next_sample_array = audio_decode(&next_song, tempfile);
	current_sample_array = next_sample_array;
	current_song = next_song;

	gtk_adjustment_configure(argument->adjust, 0, 0, current_song.duration, 1, 1, 1);
	gtk_adjustment_changed(argument->adjust);
	bufferize(next_sample_array, argument);
	argument->buffer_old = argument->buffer;

	play(NULL, argument);
	free(current_sample_array);
	if(gtk_tree_model_iter_next(model, &iter)) {
		gtk_tree_model_get(model, &iter, AFILE, &tempfile, -1);
		next_sample_array = audio_decode(&next_song, tempfile);
		bufferize(next_sample_array, argument);
	}
	argument->iter = iter;
}


static gboolean continue_track(gpointer argument) {
	GtkTreeModel *model;
	char *tempfile;
	int buffers;

	((struct arguments*)argument)->offset = 0;

	alDeleteBuffers(1, &(((struct arguments*)argument)->buffer_old)); // ?
	current_sample_array = next_sample_array;
	current_song = next_song;

	free(current_sample_array);
	((struct arguments*)argument)->buffer_old = ((struct arguments*)argument)->buffer;
	model = gtk_tree_view_get_model((GtkTreeView *)(((struct arguments*)argument)->treeview));
	do {
	alGetSourcei(((struct arguments*)argument)->source, AL_BUFFERS_PROCESSED, &buffers);
	}
	while(buffers == 0);
	alSourceUnqueueBuffers(((struct arguments*)argument)->source, 1, &(((struct arguments*)argument)->buffer_old));
	gtk_adjustment_configure(((struct arguments*)argument)->adjust, 0, 0, current_song.duration, 1, 1, 1);
	gtk_adjustment_changed(((struct arguments*)argument)->adjust);

	if(gtk_tree_model_iter_next(model, &(((struct arguments*)argument)->iter))) {
			gtk_tree_model_get(model, &(((struct arguments *)argument)->iter), AFILE, &tempfile, -1);
			next_sample_array = audio_decode(&next_song, tempfile);
			bufferize(next_sample_array, argument);
	}	
}

int bufferize(int8_t *sample_array, struct arguments *argument) {
	ALenum format;
	int i;
	if(argument->first == 1) {
		InitOpenAL();
		alSourcei(argument->source, AL_BUFFER, 0);
		alDeleteSources(1, &argument->source);
		alGenSources(1, &argument->source);
	}
	argument->first = 0;
	alGenBuffers(1, &(argument->buffer));

	if(nb_bytes_per_sample == 1 && channels == 1)
		format = AL_FORMAT_MONO8;
	else if(nb_bytes_per_sample == 1 && channels == 2)
		format = AL_FORMAT_STEREO8;
	else if(nb_bytes_per_sample == 2 && channels == 1)
		format = AL_FORMAT_MONO16;
	else if(nb_bytes_per_sample == 2 && channels == 2)
		format = AL_FORMAT_STEREO16;
	else if(nb_bytes_per_sample == 4 && channels == 1)
		format = AL_FORMAT_MONO_FLOAT32;
	else if(nb_bytes_per_sample == 4 && channels == 2)
		format = AL_FORMAT_STEREO_FLOAT32;

	for(; ((int16_t*)sample_array)[nSamples] == 0; --nSamples)
		;
	if(nSamples % 2)
		nSamples--;

	alBufferData(argument->buffer, format, sample_array, nSamples * nb_bytes_per_sample, sample_rate);
	alSourceQueueBuffers(argument->source, 1, &(argument->buffer));
}

int play(GtkWidget *button, struct arguments *argument) {
	alSourcePlay(argument->source);

	g_source_remove(argument->tag);
	argument->bartag = g_timeout_add_seconds(1, timer_progressbar, argument);
	argument->tag = g_timeout_add_seconds(current_song.duration, continue_track, argument);
	return 0;
}

static timer_progressbar(gpointer argument) {
	int time;
	float timef;
	int bytes;
	int buffers;

	alGetSourcei(((struct arguments*)argument)->source, AL_BYTE_OFFSET, &bytes);
	
	timef = bytes / (float)(sample_rate * channels * nb_bytes_per_sample);

	printf("%d, %f\n", buffers, timef);
	alGetSourcei(((struct arguments*)argument)->source, AL_BUFFERS_PROCESSED, &buffers);

	alGetSourcei(((struct arguments*)argument)->source, AL_SOURCE_STATE, &((struct arguments*)argument)->status);
	alGetSourcei(((struct arguments*)argument)->source, AL_SEC_OFFSET, &time);
	if(((struct arguments*)argument)->status == AL_PLAYING) {
		gtk_adjustment_set_value (((struct arguments*)argument)->adjust, timef);
		gtk_adjustment_changed(((struct arguments*)argument)->adjust);
		g_source_remove(((struct arguments*)argument)->bartag);

		((struct arguments*)argument)->bartag = g_timeout_add_seconds(1, timer_progressbar, argument);
	}
}

static void slider_changed(GtkRange *progressbar, struct arguments *argument) {
	int time;
	float timef;
	int bytes;
	alGetSourcei(((struct arguments*)argument)->source, AL_SEC_OFFSET, &time);
	alGetSourcei(((struct arguments*)argument)->source, AL_BYTE_OFFSET, &bytes);
	
	timef = bytes / (float)(sample_rate * channels * nb_bytes_per_sample);

	if(fabs(gtk_adjustment_get_value(argument->adjust) - timef) > 0.005f) {
		printf("%f, %f\n", gtk_adjustment_get_value(argument->adjust), timef);
		alSourcei(argument->source, AL_BYTE_OFFSET, channels * (int) ((gdouble) gtk_adjustment_get_value(argument->adjust) * ((gdouble) (sample_rate * nb_bytes_per_sample))));
		g_source_remove(argument->tag);
		argument->tag = g_timeout_add_seconds((int) (((gdouble)current_song.duration) - ((gdouble)gtk_adjustment_get_value(argument->adjust))), continue_track, argument);
		printf("Timeout dans %d secondes sur %d sec.\n", (int) (((gdouble)current_song.duration) - ((gdouble)gtk_adjustment_get_value(argument->adjust))), (int)current_song.duration);
		argument->offset = gtk_adjustment_get_value(argument->adjust);

	//	printf("%f\n", argument->offset);
	} 
} 

static void setup_tree_view(GtkWidget *treeview) {
	GtkCellRenderer *renderer;
	GtkTreeViewColumn *column;

	renderer = gtk_cell_renderer_text_new();
	column = gtk_tree_view_column_new_with_attributes("Track number", renderer, "text", TRACKNUMBER, NULL);
	gtk_tree_view_column_set_resizable (column, TRUE);
	gtk_tree_view_column_set_sort_column_id(column, TRACKNUMBER);
	gtk_tree_view_column_set_sizing(column, GTK_TREE_VIEW_COLUMN_FIXED);
	gtk_tree_view_column_set_fixed_width(column, 150);
	gtk_tree_view_append_column(GTK_TREE_VIEW(treeview), column);

	renderer = gtk_cell_renderer_text_new();
	column = gtk_tree_view_column_new_with_attributes("Track", renderer, "text", TRACK, NULL);
	gtk_tree_view_column_set_resizable (column, TRUE);
	gtk_tree_view_column_set_sort_column_id(column, TRACK);
	gtk_tree_view_column_set_sizing(column, GTK_TREE_VIEW_COLUMN_FIXED);
	gtk_tree_view_column_set_fixed_width(column, 300);
	gtk_tree_view_append_column(GTK_TREE_VIEW(treeview), column);


	renderer = gtk_cell_renderer_text_new();
	column = gtk_tree_view_column_new_with_attributes("Album", renderer, "text", ALBUM, NULL);
	gtk_tree_view_column_set_resizable (column, TRUE);
	gtk_tree_view_column_set_sort_column_id(column, ALBUM);
	gtk_tree_view_column_set_sizing(column, GTK_TREE_VIEW_COLUMN_FIXED);
	gtk_tree_view_column_set_fixed_width(column, 150);
	gtk_tree_view_append_column(GTK_TREE_VIEW(treeview), column);

	renderer = gtk_cell_renderer_text_new();
	column = gtk_tree_view_column_new_with_attributes("Artist", renderer, "text", ARTIST, NULL);
	gtk_tree_view_column_set_resizable (column, TRUE);
	gtk_tree_view_column_set_sort_column_id(column, ARTIST);
	gtk_tree_view_column_set_sizing(column, GTK_TREE_VIEW_COLUMN_FIXED);
	gtk_tree_view_column_set_fixed_width(column, 150);
	gtk_tree_view_append_column(GTK_TREE_VIEW(treeview), column);

	renderer = gtk_cell_renderer_text_new();
	column = gtk_tree_view_column_new_with_attributes("Force", renderer, "text", FORCE, NULL);
	gtk_tree_view_column_set_resizable (column, TRUE);
	gtk_tree_view_column_set_sort_column_id(column, FORCE);
	gtk_tree_view_column_set_sizing(column, GTK_TREE_VIEW_COLUMN_FIXED);
	gtk_tree_view_column_set_fixed_width(column, 70);
	gtk_tree_view_append_column(GTK_TREE_VIEW(treeview), column);

	gtk_tree_view_columns_autosize(GTK_TREE_VIEW(treeview));
	/*renderer = gtk_cell_renderer_text_new();
	column = gtk_tree_view_column_new_with_attributes("File", renderer, "text", AFILE, NULL);
	gtk_tree_view_append_column(GTK_TREE_VIEW(treeview), column);*/
}

static void display_library(GtkTreeView *treeview, GtkTreeIter iter, GtkListStore *store) {
	FILE *library;
	size_t i = 0;
	char tempfile[1000];
	char temptrack[1000];
	char tempalbum[1000];
	char tempartist[1000];
	char temptracknumber[1000];
	char tempforce[1000];

	library = fopen("library.txt", "r");

	while(fgets(tempfile, 1000, library) != NULL) {
		tempfile[strcspn(tempfile, "\n")] = '\0';
		fgets(temptracknumber, 1000, library);
		temptracknumber[strcspn(temptracknumber, "\n")] = '\0';
		fgets(temptrack, 1000, library);
		temptrack[strcspn(temptrack, "\n")] = '\0';
		fgets(tempalbum, 1000, library);
		tempalbum[strcspn(tempalbum, "\n")] = '\0';
		fgets(tempartist, 1000, library);
		tempartist[strcspn(tempartist, "\n")] = '\0';
		fgets(tempforce, 1000, library);
		tempforce[strcspn(tempforce, "\n")] = '\0';
		if(atoi(tempforce) == 0)
			strcpy(tempforce, "Loud");
		else if(atoi(tempforce) == 1)
			strcpy(tempforce, "Calm");
		else
			strcpy(tempforce, "Can't conclude");

		gtk_list_store_append(store, &iter);
		gtk_list_store_set(store, &iter, TRACKNUMBER, temptracknumber, TRACK, temptrack, ALBUM, tempalbum, ARTIST, tempartist, FORCE, tempforce, AFILE, tempfile, -1);
	}
	g_object_unref(store);
}

int main(int argc, char **argv) {
	struct arguments argument;
	struct arguments *pargument = &argument;

	GtkWidget *window, *treeview, *scrolled_win, *vboxv, *p_button, *progressbar;
	GtkListStore *store;
	GtkTreeSortable *sortable;
	GtkTreeIter iter;

	pargument->first = 1;	
	pargument->status = 0;
	pargument->offset = 0;
	pargument->elapsed = g_timer_new();

	gtk_init(&argc, &argv);
	
	window = gtk_window_new(GTK_WINDOW_TOPLEVEL);
	gtk_window_set_title(GTK_WINDOW(window), "lelele player");
	gtk_widget_set_size_request(window, 900, 500);

	//treeview = gtk_tree_view_new();

	scrolled_win = gtk_scrolled_window_new(NULL, NULL);
	store = gtk_list_store_new(COLUMNS, G_TYPE_STRING, G_TYPE_STRING, G_TYPE_STRING, G_TYPE_STRING, G_TYPE_STRING, G_TYPE_STRING);
	sortable = GTK_TREE_SORTABLE(store);
	gtk_tree_sortable_set_sort_column_id(sortable, 0, GTK_SORT_ASCENDING);
	treeview = gtk_tree_view_new_with_model(GTK_TREE_MODEL(store));
	setup_tree_view(treeview);
	pargument->treeview = treeview;

	//gtk_tree_view_set_model(GTK_TREE_VIEW(treeview), GTK_TREE_MODEL(store));


	//sorted_model.set_sort_column_id(1, Gtk.SortType.ASCENDING);
	gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(scrolled_win), GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC);
	pargument->elapsed = g_timer_new();
	p_button = gtk_button_new_with_mnemonic("_Play/Pause");
	pargument->adjust = (GtkAdjustment*)gtk_adjustment_new(0, 0, 100, 1, 1, 1);
	progressbar = gtk_hscale_new(pargument->adjust);
	gtk_scale_set_draw_value((GtkScale*)progressbar, FALSE);
	vboxv = gtk_vbox_new(TRUE, 5);

	/* Signal management */
	g_signal_connect(G_OBJECT(p_button), "clicked", G_CALLBACK(play), pargument);
	g_signal_connect(G_OBJECT(treeview), "row-activated", G_CALLBACK(row_activated), pargument);
	g_signal_connect(G_OBJECT(progressbar), "value-changed", G_CALLBACK(slider_changed), pargument);
	g_signal_connect(G_OBJECT(window), "destroy", G_CALLBACK(destroy), NULL);	

	gtk_container_add(GTK_CONTAINER(scrolled_win), treeview);

	gtk_box_set_homogeneous(GTK_BOX(vboxv), FALSE);
	
	/* Add objects to the box */
//	gtk_box_pack_start_defaults(GTK_BOX(vboxv), p_button);
	gtk_box_pack_start(GTK_BOX(vboxv), progressbar, FALSE, FALSE, 1);
	gtk_box_pack_start_defaults(GTK_BOX(vboxv), scrolled_win);


	gtk_container_add(GTK_CONTAINER(window), vboxv);

	/* temporary */
	display_library(GTK_TREE_VIEW(treeview), iter, store);
	/* temporary */

	gtk_widget_show_all(window);

	gtk_main();
	return 0;
}
