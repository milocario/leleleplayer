#include <stdio.h>
#include "gui.h"
#include <gdk/gdk.h>

#include <gdk/gdkx.h>
#include <gst/video/videooverlay.h>
#include <gst/video/video.h>

#include <gst/gl/gl.h>
#if GST_GL_HAVE_WINDOW_X11 && defined (GDK_WINDOWING_X11)
#include <gst/gl/x11/gstgldisplay_x11.h>
#endif

gboolean filter_vis_features(GstPluginFeature *feature, gpointer data) {
	GstElementFactory *factory;
   
	if(!GST_IS_ELEMENT_FACTORY (feature))
    	return FALSE;
	factory = GST_ELEMENT_FACTORY (feature);
	if (!g_strrstr(gst_element_factory_get_klass(factory), "Visualization"))
    	return FALSE;

	return TRUE;
}

float distance(struct d4vector v1, struct d4vector v2) {
	float distance;
	distance = sqrt((v1.x - v2.x)*(v1.x - v2.x) + (v1.y - v2.y)*(v1.y - v2.y) +
		(v1.z - v2.z)*(v1.z - v2.z));
	return distance;
}

void destroy(GtkWidget *window, gpointer data) {
	gtk_main_quit ();
}

gboolean ignore_destroy(GtkWidget *window, gpointer data) {
	return TRUE;
}

void free_song(struct song *song) {
	if(song->artist) {
		free(song->artist);
		song->artist = NULL;
	}
	if(song->title) {
		free(song->title);
		song->title = NULL;
	}
	if(song->album) {
		free(song->album);
		song->album = NULL;
	}
	if(song->tracknumber) {
		free(song->tracknumber);
		song->tracknumber = NULL;
	}
	if(song->sample_array) {
		free(song->sample_array);
		song->sample_array = NULL;
	}
}

void explore(GDir *dir, char *folder, FILE *list) {
	const gchar *file;
	gchar *temppath;

	while((dir != NULL) && (file = g_dir_read_name(dir))) {
		temppath = g_build_path("/", folder, file, NULL);
		if( g_file_test(temppath, G_FILE_TEST_IS_REGULAR) && ( g_str_has_suffix(file, "flac") || g_str_has_suffix(file, "mp3") || g_str_has_suffix(file, "ogg") ) ) {
			g_fprintf(list, "%s\n", g_build_path("/", folder, file, NULL));
		}
		else if(g_file_test(temppath, G_FILE_TEST_IS_DIR))
			explore(g_dir_open(g_build_path("/", folder, file, NULL), 0, NULL), g_build_path("/", folder, file, NULL), list);
		g_free(temppath);
	}
	if (file == NULL) {
		g_dir_close(dir);
	}
}

void tags_obtained(GstElement *playbin, gint stream, struct arguments *argument) {
	gint i;
	GstTagList *tags;
	gchar *str, *total_str;
	gint n_audio, n_text, rate, channels;
	GstStructure *s;
	GstPad *pad;
	GstCaps *caps;
	gchar *str_genre, *str_samplerate, *str_bitrate, *str_channels;

	g_object_get(argument->current_song.playbin, "n-audio", &n_audio, NULL);
	g_object_get(argument->current_song.playbin, "n-text", &n_text, NULL);

	g_signal_emit_by_name(argument->current_song.playbin, "get-audio-pad", 0, &pad);
	caps = gst_pad_get_current_caps(pad);

	s = gst_caps_get_structure(caps, 0);

	gst_structure_get_int(s, "channels", &channels);
	gst_structure_get_int(s, "rate", &rate);
	str_channels = g_strdup_printf("Channels: %d", channels);
	gtk_label_set_text(GTK_LABEL(argument->channels_label), str_channels);
	str_samplerate = g_strdup_printf("Sample rate: %dHz", rate);
	gtk_label_set_text(GTK_LABEL(argument->samplerate_label), str_samplerate);
	g_signal_emit_by_name(argument->current_song.playbin, "get-audio-tags", i, &tags);
	if(tags) {
		printf("\naudio stream %d\n", i);
		if(gst_tag_list_get_string(tags, GST_TAG_GENRE, &str)) {
			str_genre = g_strdup_printf("Genre: %s", str);
			gtk_label_set_text(GTK_LABEL(argument->genre_label), str_genre);
		}
		if(gst_tag_list_get_string(tags, GST_TAG_LANGUAGE_CODE, &str)) {
			printf(" language: %s\n", str);
		}
		if(gst_tag_list_get_uint(tags, GST_TAG_BITRATE, &rate)) {
			str_bitrate = g_strdup_printf("Bit rate: %dkB/s", rate/1000);
			gtk_label_set_text(GTK_LABEL(argument->bitrate_label), str_bitrate);
		}
	}
}

gboolean add_artist_to_playlist(gchar *artist, struct arguments *argument) {
	gboolean valid;
	GtkTreeModel *model_library;
	GtkTreeModel *model_playlist;
	GtkTreeIter lib_iter;
	gchar *tempartist;

	model_playlist = gtk_tree_view_get_model(GTK_TREE_VIEW(argument->treeview_playlist));
	model_library = gtk_tree_view_get_model(GTK_TREE_VIEW(argument->treeview_library));

	valid = gtk_tree_model_get_iter_first(model_library, &lib_iter);
	while(valid) {
		gtk_tree_model_get(model_library, &lib_iter, ARTIST, &tempartist, -1);
			
   		if((!strcmp(tempartist, artist)))
			playlist_queue(&lib_iter, model_library, GTK_TREE_VIEW(argument->treeview_playlist), argument);
		valid = gtk_tree_model_iter_next (model_library,
		&lib_iter);
	}
}

gboolean add_album_to_playlist(gchar *album, gchar *artist, struct arguments *argument) {
	gboolean valid;
	GtkTreeModel *model_library;
	GtkTreeModel *model_playlist;
	GtkTreeIter lib_iter;
	gchar *tempalbum;
	gchar *tempartist;

	model_playlist = gtk_tree_view_get_model(GTK_TREE_VIEW(argument->treeview_playlist));
	model_library = gtk_tree_view_get_model(GTK_TREE_VIEW(argument->treeview_library));

	valid = gtk_tree_model_get_iter_first(model_library, &lib_iter);
	while(valid) {
		gtk_tree_model_get(model_library, &lib_iter, ALBUM, &tempalbum, ARTIST, &tempartist, -1);
		
   		if((!strcmp(tempalbum, album))
		&& (!strcmp(tempartist, artist))) {
			playlist_queue(&lib_iter, model_library, GTK_TREE_VIEW(argument->treeview_playlist), argument);
		}
		valid = gtk_tree_model_iter_next (model_library,
		&lib_iter);
	}
}

gboolean play_playlist_song(gchar *title, struct arguments *argument) {
	gboolean valid;
	GtkTreePath *path;
	GtkTreeModel *model_playlist;
	gchar *temptitle;
	
	model_playlist = gtk_tree_view_get_model(GTK_TREE_VIEW(argument->treeview_playlist));
	
	gint rows = gtk_tree_model_iter_n_children(GTK_TREE_MODEL(model_playlist), NULL);
	path = gtk_tree_path_new_from_indices(rows - 1, -1);
	valid = gtk_tree_model_get_iter(GTK_TREE_MODEL(model_playlist), &(argument->iter_playlist), path);

	while(valid) {
		gtk_tree_model_get(model_playlist, &(argument->iter_playlist), TRACK, &temptitle, -1);

   		if((!strcmp(temptitle, title)))
			break;
		valid = gtk_tree_model_iter_previous(model_playlist,
		&argument->iter_playlist);
	}

	start_song(argument);
}

void playlist_queue(GtkTreeIter *iter_to_queue, GtkTreeModel *model_library, GtkTreeView *treeview_playlist, struct arguments *argument) {
	GtkTreeIter iter_playlist;
	gchar *tempfile;
	gfloat tempfloat;
	size_t i;

	gtk_list_store_append(argument->store_playlist, &iter_playlist);
	for(i = 0; i < COLUMNS; ++i) {
		if(i == FORCE) {
			gtk_tree_model_get(model_library, iter_to_queue, i, &tempfloat, -1);
			gtk_list_store_set(argument->store_playlist, &iter_playlist, i, tempfloat,-1);
		}
		else if(i == FORCE_TEMPO) {
			gtk_tree_model_get(model_library, iter_to_queue, i, &tempfloat, -1);
			gtk_list_store_set(argument->store_playlist, &iter_playlist, i, tempfloat,-1);
		}
		else if(i == FORCE_AMP) {
			gtk_tree_model_get(model_library, iter_to_queue, i, &tempfloat, -1);
			gtk_list_store_set(argument->store_playlist, &iter_playlist, i, tempfloat,-1);
		}
		else if(i == FORCE_FREQ) {
			gtk_tree_model_get(model_library, iter_to_queue, i, &tempfloat, -1);
			gtk_list_store_set(argument->store_playlist, &iter_playlist, i, tempfloat,-1);
		}
		else if(i == FORCE_ATK) {
			gtk_tree_model_get(model_library, iter_to_queue, i, &tempfloat, -1);
			gtk_list_store_set(argument->store_playlist, &iter_playlist, i, tempfloat,-1);
		}
		else {
			gtk_tree_model_get(model_library, iter_to_queue, i, &tempfile, -1);
			gtk_list_store_set(argument->store_playlist, &iter_playlist, i, tempfile, -1);
			g_free(tempfile);
		}
	}
	argument->playlist_count++;
}

gboolean get_next_playlist_song(GtkTreeView *treeview_playlist, struct arguments *argument) {
	GtkTreeModel *model_playlist;
	GtkTreeIter tempiter;
	model_playlist = gtk_tree_view_get_model(treeview_playlist);

	if(argument->lelelerandom) {
		return get_lelelerandom_playlist_song(GTK_TREE_VIEW(argument->treeview_playlist), argument);
	}
	else if(argument->random) {
		return get_random_playlist_song(GTK_TREE_VIEW(argument->treeview_playlist), argument);
	}
	else {
		tempiter = argument->iter_playlist;
		if(gtk_tree_model_iter_next(model_playlist, &tempiter)) {
			argument->iter_playlist = tempiter;
			return TRUE;
		}
		else {
			return FALSE;
		}
	}
}

gboolean get_random_playlist_song(GtkTreeView *treeview_playlist, struct arguments *argument) {
	GtkTreeModel *model_playlist;
	GList *temphistory;
	gchar *songstring;
	gchar *tempstring;
	int i, rand;
	gboolean found = true;

	model_playlist = gtk_tree_view_get_model(treeview_playlist);

	do {
		rand = g_random_int_range(0, argument->playlist_count);
		if(gtk_tree_model_get_iter_first(model_playlist, &(argument->iter_playlist))) {
			for(i = 0; i < rand - 1; ++i)
				gtk_tree_model_iter_next(model_playlist, &(argument->iter_playlist));
			songstring = gtk_tree_model_get_string_from_iter(model_playlist, &(argument->iter_playlist));
			found = false;

			for(temphistory = argument->history; temphistory != NULL; temphistory = temphistory->next) {
				tempstring = (gchar*)temphistory->data;
				if(!g_strcmp0(tempstring, songstring)) {
					found = true;
					break;
				}
			}
			g_free(songstring);
		}
		else
			return FALSE;
	} while(found == true);

	return TRUE;
}

gboolean get_lelelerandom_playlist_song(GtkTreeView *treeview_playlist, struct arguments *argument) {
	GtkTreeModel *model_playlist;

	model_playlist = gtk_tree_view_get_model(GTK_TREE_VIEW(argument->treeview_playlist));
	struct d4vector current_force = argument->current_song.force_vector;
	float treshold = 0.45;
	do {
		treshold += 0.01;
		if(!get_random_playlist_song(treeview_playlist, argument))
			return FALSE;
		gtk_tree_model_get(model_playlist, &(argument->iter_playlist), FORCE_TEMPO, &argument->current_song.force_vector.x, 
		FORCE_AMP, &argument->current_song.force_vector.y, FORCE_FREQ, &argument->current_song.force_vector.z, 
		FORCE_ATK, &argument->current_song.force_vector.t, -1);

		printf("%f\n", distance(current_force, argument->current_song.force_vector));
	} while(distance(current_force, argument->current_song.force_vector) >= treshold);
	return TRUE;
}

gboolean get_previous_playlist_song(GtkTreeView *treeview_playlist, struct arguments *argument) {
	GtkTreeModel *model_playlist;
	
	model_playlist = gtk_tree_view_get_model(treeview_playlist);

	if(argument->history && argument->history->next 
	&& gtk_tree_model_get_iter_from_string(model_playlist, &argument->iter_playlist, (gchar*)argument->history->data)) {
		argument->history = g_list_remove(argument->history, argument->history->data);
		return TRUE;
	}
	else
		return FALSE;
}

void clean_playlist(GtkTreeView *treeview_playlist, struct arguments *argument) {
	gtk_list_store_clear(argument->store_playlist);
	argument->playlist_count = 0;	
}

void artist_row_activated(GtkTreeView *treeview, GtkTreePath *path, GtkTreeViewColumn *column, struct arguments *argument) {
	GtkTreeModel *model_artist = gtk_tree_view_get_model(treeview);
	GtkTreeModel *model_library = gtk_tree_view_get_model(GTK_TREE_VIEW(argument->treeview_library));
	GtkTreeModel *model_playlist = gtk_tree_view_get_model(GTK_TREE_VIEW(argument->treeview_playlist));
	gchar *songtitle;
	gchar *songalbum;
	gchar *songartist;
	gchar *temptitle;
	gchar *tempalbum;
	gchar *tempartist;
	GtkTreeIter lib_iter;
	gboolean valid;

	
	clean_playlist(GTK_TREE_VIEW(argument->treeview_playlist), argument);

	if(gtk_tree_path_get_depth(path) == 3) {
		if(!(gtk_tree_model_get_iter(model_artist, &(argument->iter_artist), path))) {
			printf("Error getting artist iter!\n");
			return;
		}
		gtk_tree_model_get(model_artist, &(argument->iter_artist), 0, &songtitle, -1);
		gtk_tree_path_up(path);
		gtk_tree_model_get_iter(model_artist, &(argument->iter_artist), path);
		gtk_tree_model_get(model_artist, &(argument->iter_artist), 0, &songalbum, -1);
		gtk_tree_path_up(path);
		gtk_tree_model_get_iter(model_artist, &(argument->iter_artist), path);
		gtk_tree_model_get(model_artist, &(argument->iter_artist), 0, &songartist, -1);
		songtitle = strstr(songtitle, "  ") + 2;

		add_album_to_playlist(songalbum, songartist, argument);
	
		play_playlist_song(songtitle, argument);
	}
	else if(gtk_tree_path_get_depth(path) == 2) {
		if(!(gtk_tree_model_get_iter(model_artist, &(argument->iter_artist), path))) {
			printf("Error getting artist iter!\n");
			return;
		}
		gtk_tree_model_get_iter(model_artist, &(argument->iter_artist), path);
		gtk_tree_model_get(model_artist, &(argument->iter_artist), 0, &songalbum, -1);
		gtk_tree_path_up(path);
		gtk_tree_model_get_iter(model_artist, &(argument->iter_artist), path);
		gtk_tree_model_get(model_artist, &(argument->iter_artist), 0, &songartist, -1);
		
		add_album_to_playlist(songalbum, songartist, argument);

		gtk_tree_model_get_iter_first(model_playlist, &argument->iter_playlist);
		start_song(argument);
	}
	else if(gtk_tree_path_get_depth(path) == 1) {
		if(!(gtk_tree_model_get_iter(model_artist, &(argument->iter_artist), path))) {
			printf("Error getting artist iter!\n");
			return;
		}
		gtk_tree_model_get(model_artist, &(argument->iter_artist), 0, &songartist, -1);

		add_artist_to_playlist(songartist, argument);
		
		gtk_tree_model_get_iter_first(model_playlist, &argument->iter_playlist);

		start_song(argument);
	}
}
void playlist_row_activated(GtkTreeView *treeview, GtkTreePath *path, GtkTreeViewColumn *column, struct arguments *argument) {
	GtkTreeModel *model_playlist = gtk_tree_view_get_model(treeview);

	if(gtk_tree_model_get_iter(model_playlist, &(argument->iter_playlist), path)) {
		start_song(argument);
	}
}


void lib_row_activated(GtkTreeView *treeview, GtkTreePath *path, GtkTreeViewColumn *column, struct arguments *argument) {
	GtkTreeModel *model_library;
	GtkTreeModel *model_playlist;
	GtkTreeIter iter_library;

	model_library = gtk_tree_view_get_model(treeview);
	model_playlist = gtk_tree_view_get_model(GTK_TREE_VIEW(argument->treeview_playlist));

	clean_playlist(GTK_TREE_VIEW(argument->treeview_playlist), argument);
	gtk_tree_model_get_iter_first(model_library, &iter_library);
	do {
		playlist_queue(&iter_library, model_library, GTK_TREE_VIEW(argument->treeview_playlist), argument);
	} while(gtk_tree_model_iter_next(model_library, &iter_library));

	gtk_tree_model_get_iter(model_playlist, &(argument->iter_playlist), path);
	argument->history = g_list_prepend(argument->history, gtk_tree_model_get_string_from_iter(model_playlist, &argument->iter_playlist));

	start_song(argument);
}

void continue_track(GstElement *playbin, struct arguments *argument) {
	GtkTreeModel *model_playlist;

	model_playlist = gtk_tree_view_get_model(GTK_TREE_VIEW(argument->treeview_playlist));
	
	argument->history = g_list_prepend(argument->history, gtk_tree_model_get_string_from_iter(model_playlist, &argument->iter_playlist));
	if(!argument->repeat) {
		free_song(&argument->current_song); 
		get_next_playlist_song(GTK_TREE_VIEW(argument->treeview_playlist), argument);
	}
	queue_song(argument);
}

void queue_song(struct arguments *argument) {
	char *uri;
	GtkTreeModel *model_playlist;

	model_playlist = gtk_tree_view_get_model(GTK_TREE_VIEW(argument->treeview_playlist));
	gtk_tree_model_get(model_playlist, &(argument->iter_playlist), AFILE, &argument->current_song.filename, 
	TRACKNUMBER, &argument->current_song.tracknumber, TRACK, &argument->current_song.title, 
	ALBUM, &argument->current_song.album, ARTIST, &argument->current_song.artist, 
	FORCE, &argument->current_song.force, FORCE_TEMPO, &argument->current_song.force_vector.x, 
	FORCE_AMP, &argument->current_song.force_vector.y, FORCE_FREQ, &argument->current_song.force_vector.z, 
	FORCE_ATK, &argument->current_song.force_vector.t, -1);

	uri = g_filename_to_uri(argument->current_song.filename, NULL, NULL);

	g_object_set(argument->current_song.playbin, "uri", uri, NULL);
	g_free(uri); 
}

void start_song(struct arguments *argument) {
	char *uri;
	GtkTreeModel *model_playlist;
	GstElement *vis_plugin;
	GstElementFactory *selected_factory = NULL;
	GList *list, *walk;
	guint flags;

	model_playlist = gtk_tree_view_get_model(GTK_TREE_VIEW(argument->treeview_playlist));

	gtk_tree_model_get(model_playlist, &(argument->iter_playlist), AFILE, &argument->current_song.filename, 
	TRACKNUMBER, &argument->current_song.tracknumber, TRACK, &argument->current_song.title, 
	ALBUM, &argument->current_song.album, ARTIST, &argument->current_song.artist, 
	FORCE, &argument->current_song.force, FORCE_TEMPO, &argument->current_song.force_vector.x, 
	FORCE_AMP, &argument->current_song.force_vector.y, FORCE_FREQ, &argument->current_song.force_vector.z, 
	FORCE_ATK, &argument->current_song.force_vector.t, -1);

	gtk_widget_set_sensitive(argument->progressbar, TRUE);
	uri = g_filename_to_uri(argument->current_song.filename, NULL, NULL);
	
	list = gst_registry_feature_filter(gst_registry_get(), filter_vis_features, FALSE, NULL);

	for(walk = list; walk != NULL; walk = g_list_next(walk)) {
		const gchar *name;
		GstElementFactory *factory;

		factory = GST_ELEMENT_FACTORY(walk->data);
		name = gst_element_factory_get_longname(factory);

		if(selected_factory == NULL || g_str_has_prefix(name, "Spectroscope")) {
			selected_factory = factory;
		}
	}

	if(!selected_factory)
		g_printf("No visualization plugins found!\n");

	vis_plugin = gst_element_factory_create(selected_factory, NULL);

	if(!vis_plugin)
		g_printf("Couldn't create the visualizator factory element!\n");

	gst_element_set_state(argument->current_song.playbin, GST_STATE_NULL);
	g_object_set(argument->current_song.playbin, "uri", uri, NULL);
	g_object_get(argument->current_song.playbin, "flags", &flags, NULL);
	flags |= (1 << 3);
	g_object_set(argument->current_song.playbin, "flags", flags, NULL);
	g_object_set(argument->current_song.playbin, "vis-plugin", vis_plugin, NULL);
	g_object_set(argument->current_song.playbin, "force-aspect-ratio", FALSE, NULL);
	gst_element_set_state(argument->current_song.playbin, GST_STATE_PLAYING);
	if(argument->bartag)
			g_source_remove(argument->bartag);
	argument->bartag = g_timeout_add_seconds(1, refresh_progressbar, argument);

	g_free(uri);
}

void pause_song(struct arguments *argument) {
	gtk_button_set_image(GTK_BUTTON(argument->playpause_button), gtk_image_new_from_icon_name("media-playback-start-symbolic", GTK_ICON_SIZE_BUTTON));
	gtk_list_store_set(argument->store_playlist, &(argument->iter_playlist), PLAYING, "▍▍", -1);
	gst_element_set_state(argument->current_song.playbin, GST_STATE_PAUSED);
}

void resume_song(struct arguments *argument) {
	gtk_button_set_image(GTK_BUTTON(argument->playpause_button), gtk_image_new_from_icon_name("media-playback-pause-symbolic", GTK_ICON_SIZE_BUTTON));
	gtk_list_store_set(argument->store_playlist, &(argument->iter_playlist), PLAYING, "▶",-1);
	gst_element_set_state(argument->current_song.playbin, GST_STATE_PLAYING);
}

void toggle_playpause(struct arguments *argument) {
	if(argument->current_song.state == GST_STATE_PLAYING)
		pause_song(argument);
	else if(argument->current_song.state == GST_STATE_PAUSED)
		resume_song(argument);
}

void toggle_random(GtkWidget *button, struct arguments *argument) {
	argument->random = (argument->random == 1 ? 0 : 1);
}

void toggle_lelele(GtkWidget *button, struct arguments *argument) {
	argument->lelelerandom = (argument->lelelerandom == 1 ? 0 : 1);
}

void toggle_repeat(GtkWidget *button, struct arguments *argument) {
	argument->repeat = (argument->repeat == 1 ? 0 : 1);
}


void toggle_playpause_button(GtkWidget *button, struct arguments *argument) { 
	GtkTreeSelection *selection;
	GtkTreeModel *model;
	GtkTreePath *path;
	GtkTreeIter iter;
	
	if(argument->current_song.state == GST_STATE_NULL) {
		selection = gtk_tree_view_get_selection(GTK_TREE_VIEW(argument->treeview_library));
		gtk_tree_view_get_model((GtkTreeView*)(argument->treeview_library));
		if(gtk_tree_selection_get_selected(selection, &model, &iter)) {
			path = gtk_tree_model_get_path(model, &iter);

			lib_row_activated(GTK_TREE_VIEW(argument->treeview_library), path, NULL, argument);
		}		
	}
	else
		toggle_playpause(argument);
}

void next_buttonf(GtkWidget *button, struct arguments *argument) {
	GtkTreeModel *model_playlist;
	model_playlist = gtk_tree_view_get_model(GTK_TREE_VIEW(argument->treeview_playlist));
	gchar *iter_string;
	
	if((iter_string = gtk_tree_model_get_string_from_iter(model_playlist, &argument->iter_playlist))) {
		argument->history = g_list_prepend(argument->history, iter_string);
		free_song(&argument->current_song);
		if(get_next_playlist_song(GTK_TREE_VIEW(argument->treeview_playlist), argument)) {
			start_song(argument);
		}
	}
}

void previous_buttonf(GtkWidget *button, struct arguments *argument) {
	free_song(&argument->current_song);
	if(get_previous_playlist_song(GTK_TREE_VIEW(argument->treeview_playlist), argument)) {
		start_song(argument);
	}
}

void analyze_thread(struct pref_folder_arguments *argument) {
	struct song song;
	char *msg_thread;
	char *line = argument->line;
	FILE *list = argument->list;
	FILE *library = argument->library;
	//FILE *test = fopen("test.txt", "w");
	GAsyncQueue *msg_queue = argument->msg_queue;
	float resnum;

	song.sample_array = NULL;
	song.title = song.artist = song.album = song.tracknumber = NULL;
	while(fgets(line, PATH_MAX, list) != NULL) {
		line[strcspn(line, "\n")] = '\0';
		if((resnum = analyze(line, &song)) != 0) {
	//		fprintf(test, "%f %f %f\n", song.force_vector.x, song.force_vector.y, song.force_vector.z);
			fprintf(library, "%s\n%s\n%s\n%s\n%s\n%f\n%f\n%f\n%f\n%f\n", line, song.tracknumber, song.title, song.album, song.artist, resnum, song.force_vector.x,
				song.force_vector.y, song.force_vector.z, song.force_vector.t);
			msg_thread = g_malloc(strlen(song.title)*sizeof(char) + 1);
			strncpy(msg_thread, song.title, strlen(song.title) + 1);
			g_async_queue_push(msg_queue, msg_thread);
			free_song(&song);
		}
	}
	msg_thread = g_malloc(4);
	g_stpcpy(msg_thread, "end");
	g_async_queue_push(msg_queue, msg_thread);
	//fclose(test);
}

void config_folder_changed(char *folder, GtkWidget *parent) {
	GtkWidget *progressbar, *progressdialog, *area;
	progressbar = gtk_progress_bar_new();
	progressdialog = gtk_dialog_new();
	gtk_window_set_transient_for(GTK_WINDOW(progressdialog), GTK_WINDOW(parent));
	area = gtk_dialog_get_content_area(GTK_DIALOG(progressdialog));
	GDir *dir = g_dir_open (folder, 0, NULL);
	FILE *list;
	FILE *library;
	if(!(list = fopen("list.txt", "w+"))) {
		g_warning("Couldn't write config file");
		return;
	}
	if(!(library = fopen("library.txt", "w"))) {
		g_warning("Couldn't write library file");
		return;
	}
	explore(dir, folder, list);
	int nblines = 0;
	char line[PATH_MAX];
	int count = 0;
	GAsyncQueue *msg_queue = g_async_queue_new();
	char *msg;
	struct pref_folder_arguments argument;

	fseek(list, 0, SEEK_SET);

	while(fgets(line, PATH_MAX, list) != NULL)
		nblines++;

	fseek(list, 0, SEEK_SET);

	g_signal_connect(G_OBJECT(progressdialog), "delete-event", G_CALLBACK(ignore_destroy), NULL);

	gtk_progress_bar_set_ellipsize(GTK_PROGRESS_BAR(progressbar), PANGO_ELLIPSIZE_END);
	gtk_box_set_homogeneous(GTK_BOX(area), TRUE);
	gtk_widget_set_size_request(progressbar, 300, 20);
	gtk_box_pack_start(GTK_BOX(area), progressbar, TRUE, TRUE, 0);
	gtk_progress_bar_set_show_text(GTK_PROGRESS_BAR(progressbar), 1);
	//gtk_window_set_keep_above(GTK_WINDOW(progressdialog), TRUE);
	gtk_window_set_deletable(GTK_WINDOW(progressdialog), FALSE);
	gtk_window_set_modal(GTK_WINDOW(progressdialog), TRUE);
	gtk_widget_show_all(progressdialog);

	argument.msg_queue = msg_queue;
	argument.line = line;
	argument.list = list;
	argument.library = library;
	msg = NULL;
	
	g_thread_new("analyze", (GThreadFunc)analyze_thread, &argument);

	do {
		if((msg != NULL) && strcmp(msg, "end")) {
			gtk_progress_bar_set_text(GTK_PROGRESS_BAR(progressbar), msg);
			count++;
			gtk_progress_bar_set_fraction(GTK_PROGRESS_BAR(progressbar), (float)count/(float)nblines);
			g_free(msg);
			msg = NULL;
		}
		while (gtk_events_pending ())
  			gtk_main_iteration ();
	} while(((msg = g_async_queue_pop(msg_queue)) == NULL) || strcmp(msg, "end")); 

	gtk_widget_destroy(progressdialog);
	g_free(msg);
	g_async_queue_unref(msg_queue);
	fclose(list);
	g_remove("list.txt");
	fclose(library);
}

void folder_chooser(GtkWidget *button, struct pref_arguments *argument) {
	GtkWidget *dialog;
	gint res;
	GtkFileChooserAction action = GTK_FILE_CHOOSER_ACTION_SELECT_FOLDER;

	dialog = gtk_file_chooser_dialog_new("Choose library folder", GTK_WINDOW(argument->window),
		action, "Cancel", GTK_RESPONSE_CANCEL, "Open", GTK_RESPONSE_ACCEPT, NULL);

	res = gtk_dialog_run(GTK_DIALOG(dialog));

	if(res == GTK_RESPONSE_ACCEPT) {
		GtkFileChooser *chooser;
		chooser = GTK_FILE_CHOOSER(dialog);
		argument->folder = gtk_file_chooser_get_filename(chooser);
	}
	else
		argument->folder = NULL;
	gtk_widget_destroy(dialog);
	gtk_entry_set_text((GtkEntry*)argument->library_entry, argument->folder);
}
	
 void preferences_callback(GtkMenuItem *preferences, struct pref_arguments *argument) {
	GtkWidget *dialog, *label, *area, *vbox, *hbox, *library_entry, *browse_button, *window_temp;
	GtkDialogFlags flags = GTK_DIALOG_MODAL | GTK_DIALOG_DESTROY_WITH_PARENT;	
	gint res;

	label = gtk_label_new("");
	gtk_label_set_markup(GTK_LABEL(label), "<span weight=\"bold\">Select library location:</span>");
	browse_button = gtk_button_new_with_label("Browse...");
	library_entry = gtk_entry_new();
	vbox = gtk_box_new(GTK_ORIENTATION_VERTICAL, 5);
	hbox = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 5);
	dialog = gtk_dialog_new_with_buttons("Preferences", GTK_WINDOW(argument->window), flags, "Cancel", GTK_RESPONSE_REJECT, "Save", GTK_RESPONSE_ACCEPT, NULL);
	area = gtk_dialog_get_content_area(GTK_DIALOG(dialog));

	g_signal_connect(G_OBJECT(browse_button), "clicked", G_CALLBACK(folder_chooser), argument);
	argument->library_entry = library_entry;
	window_temp = argument->window;
	argument->window = dialog;

	gtk_box_set_homogeneous(GTK_BOX(vbox), TRUE);
	gtk_box_set_homogeneous(GTK_BOX(hbox), FALSE);

	gtk_box_pack_start(GTK_BOX(vbox), label, FALSE, FALSE, 0);
	gtk_box_pack_start(GTK_BOX(vbox), hbox, FALSE, FALSE, 0);
		gtk_box_pack_start(GTK_BOX(hbox), library_entry, TRUE, TRUE, 0);
		gtk_box_pack_start(GTK_BOX(hbox), browse_button, FALSE, FALSE, 0);
	gtk_container_add(GTK_CONTAINER(area), vbox);

	gtk_widget_set_size_request(dialog, 400, 120);
	gtk_widget_show_all(dialog);
	res = gtk_dialog_run(GTK_DIALOG(dialog));

	if(argument->folder != NULL && res == GTK_RESPONSE_ACCEPT) {
		config_folder_changed(argument->folder, dialog);
		display_library(GTK_TREE_VIEW(argument->treeview), argument->store_library);
	} 
	gtk_widget_destroy(dialog); 

	argument->window = window_temp;
}

void state_changed(GstBus *bus, GstMessage *msg, struct arguments *argument) {
	GstState old_state, new_state, pending_state;

	if(GST_MESSAGE_SRC(msg) == GST_OBJECT(argument->current_song.playbin)) {
		gst_message_parse_state_changed(msg, &old_state, &new_state, &pending_state);
    	argument->current_song.state = new_state; 
	}
}

void refresh_ui(GstBus *bus, GstMessage *msg, struct arguments *argument) {
	GtkTreeSelection *selection;
	GtkTreePath *path;
	GtkTreeViewColumn *column;
	GtkTreeModel *model_playlist;
	GtkTreeIter temp_iter;

	GstFormat fmt = GST_FORMAT_TIME;
	
	if((model_playlist = gtk_tree_view_get_model(GTK_TREE_VIEW(argument->treeview_playlist)))) {
		selection = gtk_tree_view_get_selection(GTK_TREE_VIEW(argument->treeview_playlist));
		path = gtk_tree_model_get_path(model_playlist, &argument->iter_playlist);
		column = gtk_tree_view_get_column(GTK_TREE_VIEW(argument->treeview_playlist), PLAYING);

		gtk_tree_model_get_iter_first(gtk_tree_view_get_model(GTK_TREE_VIEW(argument->treeview_playlist)), &(temp_iter));

		do {
			gtk_list_store_set(argument->store_playlist, &(temp_iter), PLAYING, "", -1);
		}
		while(gtk_tree_model_iter_next(model_playlist, &(temp_iter)));
		gtk_button_set_image(GTK_BUTTON(argument->playpause_button), gtk_image_new_from_icon_name("media-playback-pause-symbolic", GTK_ICON_SIZE_BUTTON));
		gtk_list_store_set(argument->store_playlist, &(argument->iter_playlist), PLAYING, "▶", -1);
		gtk_tree_selection_select_iter(selection, &(argument->iter_playlist));
		gtk_tree_view_scroll_to_cell(GTK_TREE_VIEW(argument->treeview_playlist), path, column, 1, 0.3, 1);
	
		gtk_label_set_text(GTK_LABEL(argument->album_label), argument->current_song.album);
		gtk_label_set_text(GTK_LABEL(argument->artist_label), argument->current_song.artist);
		gtk_label_set_text(GTK_LABEL(argument->title_label), argument->current_song.title);
	}
	while(!gst_element_query_duration(argument->current_song.playbin, fmt,
		&(argument->current_song.duration)))
		;
	gst_element_query_position(argument->current_song.playbin, 
		fmt, &(argument->current_song.current));

	g_signal_handler_block(argument->progressbar, argument->progressbar_update_signal_id);
	gtk_adjustment_configure(argument->adjust, 0, 0, argument->current_song.duration/GST_SECOND, 
		1, 1, 1);
	gtk_adjustment_changed(argument->adjust);
	g_signal_handler_unblock(argument->progressbar, argument->progressbar_update_signal_id);
	refresh_progressbar(argument);
	
}

void ui_playlist_changed(GtkTreeModel *playlist_model, GtkTreePath *path, GtkTreeIter *iter, GtkNotebook *libnotebook) {
	gtk_notebook_set_current_page(libnotebook, 2);
}

 gboolean refresh_progressbar(gpointer pargument) {
	struct arguments *argument = (struct arguments*)pargument;
	GstFormat fmt = GST_FORMAT_TIME;

	if(argument->current_song.state < GST_STATE_PAUSED) {
		return TRUE;
	}
	
	if(gst_element_query_position(argument->current_song.playbin, 
		fmt, &(argument->current_song.current))) {
		g_signal_handler_block(argument->progressbar, argument->progressbar_update_signal_id);
		gtk_adjustment_configure(argument->adjust, argument->current_song.current/GST_SECOND, 0, argument->current_song.duration/GST_SECOND, 
			1, 1, 1);
		gtk_adjustment_changed(argument->adjust);
		g_signal_handler_unblock(argument->progressbar, argument->progressbar_update_signal_id);
		return TRUE;
	}
	else
		return FALSE;
}

void slider_changed(GtkRange *progressbar, struct arguments *argument) {
	if(!gst_element_seek(argument->current_song.playbin, 1.0, GST_FORMAT_TIME, GST_SEEK_FLAG_FLUSH, 
		GST_SEEK_TYPE_SET, gtk_adjustment_get_value(argument->adjust)*GST_SECOND, GST_SEEK_TYPE_NONE,
		GST_CLOCK_TIME_NONE)) 
		g_warning("Seek failed!\n");
} 

void volume_scale_changed(GtkScaleButton* volume_scale, struct arguments *argument) {
	float vol = pow(gtk_scale_button_get_value(volume_scale), 3);
	if(argument->current_song.state != GST_STATE_NULL)
		g_object_set(argument->current_song.playbin, "volume", vol, NULL);
}

void setup_tree_view_renderer_artist(GtkWidget *treeview, GtkTreeStore *treestore, GtkTreeModel *model_library) {
	GtkCellRenderer *renderer;
	GtkTreeViewColumn *column;

	renderer = gtk_cell_renderer_text_new();
	column = gtk_tree_view_column_new_with_attributes("Artist", renderer, "text", COLUMN_ARTIST, NULL);
	gtk_tree_view_append_column(GTK_TREE_VIEW(treeview), column);
	g_object_set(renderer, "size", 13*PANGO_SCALE, NULL);
	
	GtkTreeIter toplevel, child, lowlevel, tempiter_artist;
	gchar *tempartist1, *tempartist2;
	gchar *tempalbum1, *tempalbum2;
	gchar *temptrack;
	gchar *temptracknumber;

	tempartist1 = tempartist2 = tempalbum1 = tempalbum2 = temptrack = temptracknumber = NULL;

	if(gtk_tree_model_get_iter_first(model_library, &tempiter_artist)) {
		do {
			gtk_tree_model_get(model_library, &tempiter_artist, ARTIST, &tempartist1, -1);
			if(g_strcmp0(tempartist1, tempartist2)) {
			 	gtk_tree_store_append(treestore, &toplevel, NULL);
  				gtk_tree_store_set(treestore, &toplevel,
                     COLUMN_ARTIST, tempartist1,
                     -1);
			
				gtk_tree_model_get(model_library, &tempiter_artist, ARTIST, &tempartist2, -1);
			}
			gtk_tree_model_get(model_library, &tempiter_artist, ALBUM, &tempalbum1, -1);
			if(g_strcmp0(tempalbum1, tempalbum2)) {
				gtk_tree_store_append(treestore, &child, &toplevel);
  				gtk_tree_store_set(treestore, &child,
                  	COLUMN_ARTIST, tempalbum1,
                    -1);
					gtk_tree_model_get(model_library, &tempiter_artist, ALBUM, &tempalbum2, -1);
			}
			gtk_tree_model_get(model_library, &tempiter_artist, TRACK, &temptrack, -1);
			gtk_tree_model_get(model_library, &tempiter_artist, TRACKNUMBER, &temptracknumber, -1);
				gchar *song = g_strconcat(temptracknumber, "  ", temptrack, NULL);
				gtk_tree_store_append(treestore, &lowlevel, &child);
  					gtk_tree_store_set(treestore, &lowlevel,
                    	COLUMN_ARTIST, song,
                    -1);
			g_free(song);
			
		} while(gtk_tree_model_iter_next(model_library, &tempiter_artist));
	}
}

void setup_tree_view_renderer_play_lib(GtkWidget *treeview) {
	GtkCellRenderer *renderer;
	GtkTreeViewColumn *column;

	renderer = gtk_cell_renderer_text_new();
	column = gtk_tree_view_column_new_with_attributes("", renderer, "text", PLAYING, NULL);
	gtk_tree_view_column_set_sort_column_id(column, PLAYING);
	gtk_tree_view_column_set_sizing(column, GTK_TREE_VIEW_COLUMN_FIXED);
	gtk_tree_view_append_column(GTK_TREE_VIEW(treeview), column);

	renderer = gtk_cell_renderer_text_new();
	column = gtk_tree_view_column_new_with_attributes("N°", renderer, "text", TRACKNUMBER, NULL);
	gtk_tree_view_column_set_resizable (column, TRUE);
	gtk_tree_view_column_set_sort_column_id(column, TRACKNUMBER);
	gtk_tree_view_column_set_sizing(column, GTK_TREE_VIEW_COLUMN_FIXED);
	gtk_tree_view_column_set_fixed_width(column, 50);
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
	column = gtk_tree_view_column_new_with_attributes("Force", renderer, "text", TEXTFORCE, NULL);
	gtk_tree_view_column_set_resizable (column, TRUE);
	gtk_tree_view_column_set_sort_column_id(column, TEXTFORCE);
	gtk_tree_view_column_set_sizing(column, GTK_TREE_VIEW_COLUMN_FIXED);
	gtk_tree_view_column_set_fixed_width(column, 70);
	gtk_tree_view_append_column(GTK_TREE_VIEW(treeview), column);

	gtk_tree_view_columns_autosize(GTK_TREE_VIEW(treeview));
}

void display_library(GtkTreeView *treeview, GtkListStore *store) {
	GtkTreeIter iter;
	FILE *library;
	char tempfile[PATH_MAX];
	char temptrack[PATH_MAX];
	char tempalbum[PATH_MAX];
	char tempartist[PATH_MAX];
	char temptracknumber[PATH_MAX];
	char tempforce[PATH_MAX];
	float tempforcef;
	char tempforce_env[PATH_MAX];
	float tempforce_envf;
	char tempforce_amp[PATH_MAX];
	float tempforce_ampf;
	char tempforce_freq[PATH_MAX];
	float tempforce_freqf;
	char tempforce_atk[PATH_MAX];
	float tempforce_atkf;

	gtk_list_store_clear(store);

	if((library = fopen("library.txt", "r")) != NULL) {
		while(fgets(tempfile, PATH_MAX, library) != NULL) {
			tempfile[strcspn(tempfile, "\n")] = '\0';

			if(!fgets(temptracknumber, PATH_MAX, library)
			|| !fgets(temptrack, PATH_MAX, library)
			|| !fgets(tempalbum, PATH_MAX, library)
			|| !fgets(tempartist, PATH_MAX, library)
			|| !fgets(tempforce, PATH_MAX, library)
			|| !fgets(tempforce_env, PATH_MAX, library)
			|| !fgets(tempforce_amp, PATH_MAX, library)
			|| !fgets(tempforce_freq, PATH_MAX, library)
			|| !fgets(tempforce_atk, PATH_MAX, library)) {
				printf("Wrong config file format\n");
				return;
			}
			
			temptracknumber[strcspn(temptracknumber, "\n")] = '\0';

			temptrack[strcspn(temptrack, "\n")] = '\0';

			tempalbum[strcspn(tempalbum, "\n")] = '\0';

			tempartist[strcspn(tempartist, "\n")] = '\0';

			tempforce[strcspn(tempforce, "\n")] = '\0';
			tempforcef = atof(tempforce);

			tempforce_env[strcspn(tempforce_env, "\n")] = '\0';
			tempforce_envf = atof(tempforce_env);

			tempforce_amp[strcspn(tempforce_amp, "\n")] = '\0';
			tempforce_ampf = atof(tempforce_amp);

			tempforce_freq[strcspn(tempforce_freq, "\n")] = '\0';
			tempforce_freqf = atof(tempforce_freq);

			tempforce_atk[strcspn(tempforce_atk, "\n")] = '\0';
			tempforce_atkf = atof(tempforce_atk);

			if(atof(tempforce) > 0)
				strcpy(tempforce, "Loud");
			else if(atof(tempforce) < 0) {
				strcpy(tempforce, "Calm");
			}
			else
				strcpy(tempforce, "Can't conclude");

			gtk_list_store_append(store, &iter);
			gtk_list_store_set(store, &iter, PLAYING, "", TRACKNUMBER, temptracknumber, TRACK, temptrack, ALBUM, tempalbum, ARTIST, tempartist, FORCE, tempforcef, FORCE_TEMPO, tempforce_envf, FORCE_AMP, tempforce_ampf, FORCE_FREQ, tempforce_freqf, FORCE_ATK, tempforce_atkf, TEXTFORCE, tempforce, AFILE, tempfile, -1);
		}
		fclose(library);
	}
}

gint sort_iter_compare_func(GtkTreeModel *model, GtkTreeIter *a, GtkTreeIter *b, gpointer userdata) {
	gchar *track1, *track2;
	gtk_tree_model_get(model, a, TRACKNUMBER, &track1, -1);
	gtk_tree_model_get(model, b, TRACKNUMBER, &track2, -1);

	if (atof(track1) > atof(track2)) {
		g_free(track1);
		g_free(track2);
		return 1;
	}
	else if(atof(track1) < atof(track2)) {
		g_free(track1);
		g_free(track2);
		return -1;
	}
	else {
		g_free(track1);
		g_free(track2);
		return 0;
	}
}

gint sort_artist_album_tracks(GtkTreeModel *model, GtkTreeIter *a, GtkTreeIter *b, gpointer userdata) {
	gchar *artist1, *artist2;
	gchar *album1, *album2;
	gchar *track1, *track2;
	int retval;

	gtk_tree_model_get(model, a, ARTIST, &artist1, -1);
	gtk_tree_model_get(model, b, ARTIST, &artist2, -1);
	gtk_tree_model_get(model, a, ALBUM, &album1, -1);
	gtk_tree_model_get(model, b, ALBUM, &album2, -1);
	gtk_tree_model_get(model, a, TRACKNUMBER, &track1, -1);
	gtk_tree_model_get(model, b, TRACKNUMBER, &track2, -1);


	if(strcmp(artist1, artist2) > 0) {
		retval = 1;
	}
	else if(strcmp(artist1, artist2) < 0)
		retval = -1;
	else {
		if(strcmp(album1, album2) > 0)
			retval = 1;
		else if(strcmp(album1, album2) < 0)
			retval = -1;
		else {
			if(atof(track1) > atof(track2))
				retval = 1;
			else if(atof(track1) < atof(track2))
				retval = -1;
			else
				retval = 0;
		}
	}
	g_free(artist1);
	g_free(artist2);
	g_free(album1);
	g_free(album2);
	g_free(track1);
	g_free(track2);
	
	return retval;
}

gint sort_force(GtkTreeModel *model, GtkTreeIter *a, GtkTreeIter *b, gpointer userdata) {
	gfloat force1, force2;
	gtk_tree_model_get(model, a, FORCE, &force1, -1);
	gtk_tree_model_get(model, b, FORCE, &force2, -1);

	if(force1 > force2)
		return 1;
	else if(force1 < force2)
		return -1;
	else
		return 0;
}

int main(int argc, char **argv) {
	struct arguments argument;
	struct arguments *pargument = &argument;
	struct pref_arguments pref_arguments;

	GtkWidget *window, *treeview_library, *treeview_playlist, *treeview_artist, *library_panel, *artist_panel, *playlist_panel, *vboxv,
		*playbox, *volumebox, *randombox, *repeat_button, *random_button, *lelele_button, *labelbox, *next_button, *previous_button, *menubar, *edit, *editmenu, 
		*preferences, *libnotebook, *mediainfo_expander, *mediainfo_box, *mediainfo_labelbox, *area;
	
	GtkTreeModel *model_playlist;
	GtkTreeModel *model_library;
	GtkTreeSortable *sortable;
	const gchar *volume[5] = {
		"audio-volume-muted-symbolic",
		"audio-volume-high-symbolic",
		"audio-volume-low-symbolic",
		"audio-volume-medium-symbolic",
		NULL
	};

	GstElement *gtk_sink;
	GstBus *bus;

	pargument->lelelerandom = 0;
	pargument->random = 0;
	pargument->repeat = 0;
	pargument->first = 1;
	pargument->playlist_count = 0;
	pargument->iter_library.stamp = 0;
	pargument->current_song.duration = GST_CLOCK_TIME_NONE;
	pargument->elapsed = g_timer_new();
	pargument->current_song.state = GST_STATE_NULL;
	pargument->current_song.artist = pargument->current_song.title = pargument->current_song.album = pargument->current_song.tracknumber = NULL;
	pargument->history = NULL;
	pargument->current_song.sample_array = NULL;
	pargument->bartag = 0;
	gtk_init(&argc, &argv);
	
	gst_init(&argc, &argv);
	
	window = gtk_window_new(GTK_WINDOW_TOPLEVEL);
	gtk_window_set_title(GTK_WINDOW(window), "lelele player");
	gtk_widget_set_size_request(window, 900, 700);

	pargument->current_song.playbin = gst_element_factory_make("playbin", "playbin");
	if(!pargument->current_song.playbin)
		g_error("Not all elements could be created.\n");
	bus = gst_element_get_bus(pargument->current_song.playbin);
	gst_bus_add_signal_watch(bus);
	
	if((gtk_sink = gst_element_factory_make("gtksink", NULL))) {
		//GstElement *video_sink;
		//video_sink = gst_element_factory_make("glsinkbin", NULL);
		//g_object_set(video_sink, "sink", gtk_sink, NULL);
		g_object_get(gtk_sink, "widget", &area, NULL);

		g_object_set(pargument->current_song.playbin, "video-sink", gtk_sink, NULL);	
		//g_object_set(pargument->current_song.playbin, "video-sink", gtk_sink, NULL);
	}	


	library_panel = gtk_scrolled_window_new(NULL, NULL);
	playlist_panel = gtk_scrolled_window_new(NULL, NULL);
	artist_panel = gtk_scrolled_window_new(NULL, NULL);

	pargument->store_library = gtk_list_store_new(COLUMNS, G_TYPE_STRING, G_TYPE_STRING, G_TYPE_STRING, G_TYPE_STRING, G_TYPE_STRING, G_TYPE_FLOAT, G_TYPE_FLOAT, G_TYPE_FLOAT, G_TYPE_FLOAT, G_TYPE_FLOAT, G_TYPE_STRING, G_TYPE_STRING);
	sortable = GTK_TREE_SORTABLE(pargument->store_library);
	gtk_tree_sortable_set_sort_column_id(sortable, TRACKNUMBER, GTK_SORT_ASCENDING);
	treeview_library = gtk_tree_view_new_with_model(GTK_TREE_MODEL(pargument->store_library));
	gtk_tree_sortable_set_sort_func(sortable, TRACKNUMBER, sort_iter_compare_func, NULL, NULL); 
	gtk_tree_sortable_set_sort_func(sortable, TEXTFORCE, sort_force, NULL, NULL);
	gtk_tree_sortable_set_sort_func(sortable, ARTIST, sort_artist_album_tracks, NULL, NULL);
	setup_tree_view_renderer_play_lib(treeview_library);	
	pargument->treeview_library = treeview_library;
	display_library(GTK_TREE_VIEW(treeview_library), pargument->store_library);
	model_library = gtk_tree_view_get_model(GTK_TREE_VIEW(treeview_library));

	pargument->store_playlist = gtk_list_store_new(COLUMNS, G_TYPE_STRING, G_TYPE_STRING, G_TYPE_STRING, G_TYPE_STRING, G_TYPE_STRING, G_TYPE_FLOAT, G_TYPE_FLOAT, G_TYPE_FLOAT, G_TYPE_FLOAT, G_TYPE_FLOAT, G_TYPE_STRING, G_TYPE_STRING);
	treeview_playlist = gtk_tree_view_new_with_model(GTK_TREE_MODEL(pargument->store_playlist));
	setup_tree_view_renderer_play_lib(treeview_playlist);
	gtk_tree_view_set_reorderable(GTK_TREE_VIEW(treeview_playlist), TRUE);
	gtk_tree_view_set_headers_clickable(GTK_TREE_VIEW(treeview_playlist), FALSE);
	pargument->treeview_playlist = treeview_playlist;
	model_playlist = gtk_tree_view_get_model(GTK_TREE_VIEW(treeview_playlist));

	pargument->store_artist = gtk_tree_store_new(NUM_COLS_ARTIST, G_TYPE_STRING);
	treeview_artist = gtk_tree_view_new_with_model(GTK_TREE_MODEL(pargument->store_artist));
	gtk_tree_sortable_set_sort_column_id(sortable, ARTIST, GTK_SORT_ASCENDING);
	setup_tree_view_renderer_artist(treeview_artist, pargument->store_artist, model_library);
	pargument->treeview_artist = treeview_artist;

	gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(library_panel), GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC);
	gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(playlist_panel), GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC);
	gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(artist_panel), GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC);
	pargument->elapsed = g_timer_new();
	playbox = gtk_button_box_new(GTK_ORIENTATION_HORIZONTAL);
	randombox = gtk_button_box_new(GTK_ORIENTATION_HORIZONTAL);
	volumebox= gtk_button_box_new(GTK_ORIENTATION_HORIZONTAL);
	pargument->playpause_button= gtk_button_new();
	random_button = gtk_toggle_button_new();
	gtk_button_set_image(GTK_BUTTON(random_button), gtk_image_new_from_icon_name("media-playlist-shuffle-symbolic", GTK_ICON_SIZE_BUTTON));
	gtk_widget_set_tooltip_text(random_button, "Standard random button");
	repeat_button = gtk_toggle_button_new();
	gtk_button_set_image(GTK_BUTTON(repeat_button), gtk_image_new_from_icon_name("media-playlist-repeat-symbolic", GTK_ICON_SIZE_BUTTON));
	lelele_button = gtk_toggle_button_new();
	gtk_button_set_image(GTK_BUTTON(lelele_button), gtk_image_new_from_file("../images/lelelerandom.svg"));
	gtk_widget_set_tooltip_text(lelele_button, "Random smoothly over songs");
	next_button = gtk_button_new();
	gtk_button_set_image(GTK_BUTTON(next_button), gtk_image_new_from_icon_name("media-skip-forward-symbolic", GTK_ICON_SIZE_BUTTON));
	previous_button = gtk_button_new();
	gtk_button_set_image(GTK_BUTTON(previous_button), gtk_image_new_from_icon_name("media-skip-backward-symbolic", GTK_ICON_SIZE_BUTTON));
	pargument->playpause_button = gtk_button_new();
	gtk_button_set_image(GTK_BUTTON(pargument->playpause_button), gtk_image_new_from_icon_name("media-playback-start-symbolic", GTK_ICON_SIZE_BUTTON));
	gtk_window_set_icon_from_file(GTK_WINDOW(window), "../images/lelele.svg", NULL);
	pargument->adjust = (GtkAdjustment*)gtk_adjustment_new(0, 0, 100, 1, 1, 1);
	pargument->progressbar = gtk_scale_new(GTK_ORIENTATION_HORIZONTAL, pargument->adjust);
	pargument->volume_scale = gtk_scale_button_new(GTK_ICON_SIZE_BUTTON, 0, 1, 0.1, volume);
	menubar = gtk_menu_bar_new();
	edit = gtk_menu_item_new_with_label("Edit");
	editmenu = gtk_menu_new();
	gtk_scale_set_draw_value((GtkScale*)pargument->progressbar, FALSE);
	gtk_widget_set_sensitive(pargument->progressbar, FALSE);
	vboxv = gtk_box_new(GTK_ORIENTATION_VERTICAL, 5);
	labelbox = gtk_box_new(GTK_ORIENTATION_VERTICAL, 5);
	pargument->title_label = gtk_label_new("");
	pargument->album_label = gtk_label_new("");
	gtk_label_set_markup(GTK_LABEL(pargument->album_label), "<span foreground=\"grey\">No song currently playing</span>");
	pargument->artist_label = gtk_label_new("");
	pargument->genre_label = gtk_label_new("Genre:");
	pargument->samplerate_label = gtk_label_new("Sample rate:");
	pargument->bitrate_label = gtk_label_new("Bitrate:");
	pargument->channels_label = gtk_label_new("Channels:");
	libnotebook = gtk_notebook_new();
	mediainfo_expander = gtk_expander_new("Visualizer/Mediainfo");
	mediainfo_labelbox = gtk_box_new(GTK_ORIENTATION_VERTICAL, 5);
	mediainfo_box = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 5);
	gtk_widget_set_size_request(area, -1, 50);

	gtk_menu_item_set_submenu(GTK_MENU_ITEM(edit), editmenu);
	gtk_menu_shell_append(GTK_MENU_SHELL(menubar), edit);

	preferences = gtk_menu_item_new_with_label("Preferences");
	gtk_menu_shell_append(GTK_MENU_SHELL(editmenu), preferences);

	pref_arguments.window = window;
	pref_arguments.treeview = treeview_library;
	pref_arguments.store_library = pargument->store_library;

	/* Signal management */
	g_signal_connect(G_OBJECT(bus), "message::state-changed", G_CALLBACK(state_changed), pargument);
	g_signal_connect(G_OBJECT(pargument->current_song.playbin), "about-to-finish", G_CALLBACK(continue_track), pargument);
	g_signal_connect(G_OBJECT(bus), "message::stream-start", G_CALLBACK(refresh_ui), pargument);
	g_signal_connect(G_OBJECT(pargument->current_song.playbin), "audio-tags-changed", G_CALLBACK(tags_obtained), pargument);
	g_signal_connect(G_OBJECT(pargument->current_song.playbin), "text-tags-changed", G_CALLBACK(tags_obtained), pargument);
	g_signal_connect(G_OBJECT(pargument->playpause_button), "clicked", G_CALLBACK(toggle_playpause_button), pargument);
	g_signal_connect(G_OBJECT(pargument->volume_scale), "value-changed", G_CALLBACK(volume_scale_changed), pargument);
	g_signal_connect(G_OBJECT(random_button), "clicked", G_CALLBACK(toggle_random), pargument);
	g_signal_connect(G_OBJECT(repeat_button), "clicked", G_CALLBACK(toggle_repeat), pargument);
	g_signal_connect(G_OBJECT(lelele_button), "clicked", G_CALLBACK(toggle_lelele), pargument);
	g_signal_connect(G_OBJECT(next_button), "clicked", G_CALLBACK(next_buttonf), pargument);
	g_signal_connect(G_OBJECT(previous_button), "clicked", G_CALLBACK(previous_buttonf), pargument);
	g_signal_connect(G_OBJECT(preferences), "activate", G_CALLBACK(preferences_callback), &pref_arguments);
	g_signal_connect(G_OBJECT(treeview_library), "row-activated", G_CALLBACK(lib_row_activated), pargument);
	g_signal_connect(G_OBJECT(treeview_playlist), "row-activated", G_CALLBACK(playlist_row_activated), pargument);
	g_signal_connect(G_OBJECT(treeview_artist), "row-activated", G_CALLBACK(artist_row_activated), pargument);
	g_signal_connect(G_OBJECT(model_playlist), "row-inserted", G_CALLBACK(ui_playlist_changed), libnotebook);
	pargument->progressbar_update_signal_id = g_signal_connect(G_OBJECT(pargument->progressbar), 
	"value-changed", G_CALLBACK(slider_changed), pargument);
	g_signal_connect(G_OBJECT(window), "destroy", G_CALLBACK(destroy), NULL);	

	gtk_container_add(GTK_CONTAINER(library_panel), treeview_library);
	gtk_container_add(GTK_CONTAINER(playlist_panel), treeview_playlist);
	gtk_container_add(GTK_CONTAINER(artist_panel), treeview_artist);

	gtk_box_set_homogeneous(GTK_BOX(vboxv), FALSE);
	
	/* Add objects to the box */
	gtk_box_pack_start(GTK_BOX(vboxv), menubar, FALSE, FALSE, 1);
	gtk_box_pack_start(GTK_BOX(vboxv), labelbox, FALSE, FALSE, 1);
		gtk_box_pack_start(GTK_BOX(labelbox), pargument->title_label, FALSE, FALSE, 1);
		gtk_box_pack_start(GTK_BOX(labelbox), pargument->album_label, FALSE, FALSE, 1);
		gtk_box_pack_start(GTK_BOX(labelbox), pargument->artist_label, FALSE, FALSE, 1);
	gtk_box_pack_start(GTK_BOX(vboxv), playbox, FALSE, FALSE, 1);
		gtk_button_box_set_layout(GTK_BUTTON_BOX(playbox), GTK_BUTTONBOX_CENTER);
		gtk_box_pack_start(GTK_BOX(playbox), previous_button, FALSE, FALSE, 1);
		gtk_box_pack_start(GTK_BOX(playbox), pargument->playpause_button, FALSE, FALSE, 1);
		gtk_box_pack_start(GTK_BOX(playbox), next_button, FALSE, FALSE, 1);
		gtk_button_box_set_child_non_homogeneous(GTK_BUTTON_BOX(playbox), pargument->playpause_button, TRUE);
		gtk_button_box_set_child_non_homogeneous(GTK_BUTTON_BOX(playbox), next_button, TRUE);
		gtk_button_box_set_child_non_homogeneous(GTK_BUTTON_BOX(playbox), previous_button, TRUE);
		gtk_box_set_spacing(GTK_BOX(playbox), 5);
	gtk_box_pack_start(GTK_BOX(vboxv), randombox, FALSE, FALSE, 1);
		gtk_button_box_set_layout(GTK_BUTTON_BOX(randombox), GTK_BUTTONBOX_CENTER);
		gtk_box_pack_start(GTK_BOX(randombox), lelele_button, FALSE, FALSE, 1);
		gtk_box_pack_start(GTK_BOX(randombox), repeat_button, FALSE, FALSE, 1);
		gtk_box_pack_start(GTK_BOX(randombox), random_button, FALSE, FALSE, 1);
		gtk_button_box_set_child_non_homogeneous(GTK_BUTTON_BOX(randombox), repeat_button, TRUE);
		gtk_button_box_set_child_non_homogeneous(GTK_BUTTON_BOX(randombox), lelele_button, TRUE);
		gtk_button_box_set_child_non_homogeneous(GTK_BUTTON_BOX(randombox), random_button, TRUE);
		gtk_box_set_spacing(GTK_BOX(randombox), 5);
	gtk_box_pack_start(GTK_BOX(vboxv), volumebox, FALSE, FALSE, 1);
		gtk_button_box_set_layout(GTK_BUTTON_BOX(volumebox), GTK_BUTTONBOX_CENTER);
		gtk_box_pack_start(GTK_BOX(volumebox), pargument->volume_scale, FALSE, FALSE, 1);
		gtk_button_box_set_child_non_homogeneous(GTK_BUTTON_BOX(volumebox), pargument->volume_scale, TRUE);
	gtk_box_pack_start(GTK_BOX(vboxv), mediainfo_expander, FALSE, FALSE, 1);
	gtk_container_add(GTK_CONTAINER(mediainfo_expander), mediainfo_box);
		gtk_box_pack_start(GTK_BOX(mediainfo_box), area, TRUE, TRUE, 1);
		gtk_box_pack_start(GTK_BOX(mediainfo_box), mediainfo_labelbox, FALSE, FALSE, 1);
			gtk_box_pack_start(GTK_BOX(mediainfo_labelbox), gtk_label_new("More information:"), FALSE, FALSE, 1);
			gtk_box_pack_start(GTK_BOX(mediainfo_labelbox), pargument->genre_label,FALSE, FALSE, 1);
			gtk_box_pack_start(GTK_BOX(mediainfo_labelbox), pargument->bitrate_label, FALSE, FALSE, 1);
			gtk_box_pack_start(GTK_BOX(mediainfo_labelbox), pargument->channels_label, FALSE, FALSE, 1);
			gtk_box_pack_start(GTK_BOX(mediainfo_labelbox), pargument->samplerate_label, FALSE, FALSE, 1);
	gtk_box_pack_start(GTK_BOX(vboxv), pargument->progressbar, FALSE, FALSE, 1);
	gtk_box_pack_start(GTK_BOX(vboxv), libnotebook, TRUE, TRUE, 1);
		gtk_notebook_append_page(GTK_NOTEBOOK(libnotebook), library_panel, gtk_label_new("Library"));
		gtk_notebook_append_page(GTK_NOTEBOOK(libnotebook), artist_panel, gtk_label_new("Artists"));
		gtk_notebook_append_page(GTK_NOTEBOOK(libnotebook), playlist_panel, gtk_label_new("Playlist"));

	gtk_scale_button_set_value(GTK_SCALE_BUTTON(pargument->volume_scale), 0.4);
	gtk_container_add(GTK_CONTAINER(window), vboxv);
	gtk_widget_show_all(window);

	gtk_main();
	return 0;
}
