#include "analyze.h"

#include <stdbool.h>
#include <gtk/gtk.h>
#include <glib/gprintf.h>
#include <glib/gerror.h>
#include <gdk-pixbuf/gdk-pixbuf.h>
#include <glib/gstdio.h>
#include <glib.h>
#include <limits.h>

enum {
	COLUMN_ARTIST = 0,
	NUM_COLS_ARTIST
};

int calm;
int count;

enum {
	PLAYING = 0,
	TRACKNUMBER,
	TRACK,
	ALBUM,
	ARTIST,
	FORCE,
	FORCE_TEMPO,
	FORCE_AMP,
	FORCE_FREQ,
	FORCE_ATK,
	TEXTFORCE,
	AFILE,
	COLUMNS
};

struct arguments {
	gboolean lelelerandom;
	gboolean random;
	gboolean repeat;
	struct song current_song;
	int first;
	int bartag;
	int playlist_count;
	gulong progressbar_update_signal_id;
	GTimer *elapsed;
	GList *history;
	GtkWidget *treeview_library;
	GtkWidget *treeview_artist;
	GtkWidget *progressbar;
	GtkWidget *treeview_playlist;
	GtkWidget *playpause_button;
	GtkTreePath *path;
	GtkTreeIter iter_playlist;
	GtkTreeIter iter_library;
	GtkTreeIter iter_artist;
	GtkListStore *store_library;
	GtkListStore *store_playlist;
	GtkTreeStore *store_artist;
	GtkTreeViewColumn *column;
	GtkWidget *album_label, *title_label, *artist_label;
	GtkAdjustment *adjust;
	GtkWidget *volume_scale;
};

struct pref_arguments {
        GtkWidget *window;
        GtkWidget *treeview;
		gchar *folder;
        GtkListStore *store_library;
		GtkWidget *library_entry;
};

struct pref_folder_arguments {
	GAsyncQueue *msg_queue;
	char *line;
	FILE *list;
	FILE *library;
};

static void setup_tree_view_renderer_play_lib(GtkWidget *);
/**
* Description: Sets up the treeview renderer with columns "playing", "tracknumber", "track",
* "album", "artist", "force", and sizes it properly
* Arguments: GtkWidget *treeview: The TreeView to set up
*/
static void setup_tree_view_renderer_artist(GtkWidget *, GtkTreeStore *, GtkTreeModel *);
/**
* Description: Sets up the treeview renderer like this: artist->album->songs
* Arguments: GtkWidget *treeview: the TreeView to set up
* Arguments: GtkTreeStore *treestore: the treestore associated with the TreeView
* Arguments: GtkTreeModel *model_library: the model associated with the library treeview (NOT the artist treeview)
*/
static void continue_track(GstElement *, struct arguments *);
/**
* Description: Queue next playlist song, and don't play it immediately: useful for gapless
* Arguments: struct arguments *argument: the global argument struct containing:
* -the songs history: history
* -the struct_song current_song to set up: current_song 
* -the playlist treeview (in order to instert songs there): treeview_playlist
* -the playlist iter (in order to know what is the next song)
* Arguments: GstElement *playbin: the playbin where the song is queued
*/
static gboolean refresh_progressbar(gpointer);
/**
* Description: Refresh the GtkScale progress bar (the song progress bar)
* Arguments: struct arguments *argument: the global argument struct containing:
* -the song state: current_song.state
* -the progressbar widget: progressbar
* -the playbin, in order to get the time elapsed in the song: playbin
*/
static void lib_row_activated(GtkTreeView *, GtkTreePath *, GtkTreeViewColumn *, struct arguments *);
/**
* Description: Callback function called when the user selects (double-click/enter) a song in the library
* Arguments: GtkTreeView *treeview: the library treeview
* Arguments: GtkTreePath *path: the path of the selected item in the treeview (useless)
* Arguments: GtkTreeViewColumn *column: the column of the selected item in the treeview
* Arguments: struct arguments *argument: the global argument struct containing:
* -the playlist treeview (in order to insert songs there): treeview_playlist
* -the struct song to set up: current_song
* -the launched timer tag: bartag
*/
static void playlist_row_activated(GtkTreeView *, GtkTreePath *, GtkTreeViewColumn *, struct arguments *);
/**
* Description: Callback function called when the user selects (double-click/enter) a song in the playlist
* Arguments: GtkTreeView *treeview: the playlist treeview
* Arguments: GtkTreePath *path: the path of the selected item in the treeview (useless)
* Arguments: GtkTreeViewColumn *column: the column of the selected item in the treeview
* Arguments: struct arguments *argument: the global argument struct containing:
* -the struct song to set up: current_song
* -the launched timer tag: bartag
*/
static void artist_row_activated(GtkTreeView *, GtkTreePath *, GtkTreeViewColumn *, struct arguments *);
static void toggle_playpause_button(GtkWidget *, struct arguments *);
static void next_buttonf(GtkWidget *, struct arguments *);
static void previous_buttonf(GtkWidget *, struct arguments *);
static void destroy(GtkWidget *, gpointer);
static void config_folder_changed(char *, GtkWidget *);
static void preferences_callback(GtkMenuItem *preferences, struct pref_arguments *);
static void analyze_thread(struct pref_folder_arguments *);
static void state_changed(GstBus *, GstMessage *, struct arguments *);
static void slider_changed(GtkRange *, struct arguments *);
static void volume_scale_changed(GtkScaleButton*, struct arguments *);
static void refresh_ui(GstBus *, GstMessage *, struct arguments *);
static void ui_playlist_changed(GtkTreeModel *, GtkTreePath *, GtkTreeIter *, GtkNotebook *);
static void toggle_lelele(GtkWidget *button, struct arguments *);
static void toggle_random(GtkWidget *button, struct arguments *);
int bufferize(struct song, struct arguments *);
float distance(struct d4vector, struct d4vector);
void pause_song(struct arguments *);
void start_song(struct arguments *);
void resume_song(struct arguments *);
void play_song(struct arguments *);
void queue_song(struct arguments *);
void free_song(struct song *);
void explore(GDir *dir, char *folder, FILE *list);
void folder_chooser(GtkWidget *, struct pref_arguments *);
void display_library(GtkTreeView *, GtkListStore *);
void playlist_queue(GtkTreeIter *, GtkTreeModel *, GtkTreeView *, struct arguments *);
void get_playlist_song(GtkTreeView *, struct song *, struct arguments *);
void clean_playlist(GtkTreeView *, struct arguments *);
gboolean get_next_playlist_song(GtkTreeView *, struct song *, struct arguments *);
gboolean get_random_playlist_song(GtkTreeView *, struct song *, struct arguments *);
gboolean get_lelelerandom_playlist_song(GtkTreeView *, struct song *, struct arguments *);
gboolean get_previous_playlist_song(GtkTreeView *, struct song *, struct arguments *);
