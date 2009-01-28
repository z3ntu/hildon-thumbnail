#include <gtk/gtk.h>
#include <hildon-albumart-factory.h>


GtkWindow *window;
GtkHBox *box;
GtkVBox *hbox;
GtkImage *image, *imaget;
GtkEntry *atext;
GtkEntry *btext;
GtkButton *button;


#ifdef OLDAPI
static void
on_art_back (HildonAlbumartFactoryHandle handle, gpointer user_data, GdkPixbuf *albumart, GError *error)
{
	if (albumart) {
		gtk_image_set_from_pixbuf (user_data, albumart);
	}
}
#else
static void 
on_art_back (HildonAlbumartFactory *self, GdkPixbuf *albumart, GError *error, gpointer user_data)
{
	if (albumart) {
		gtk_image_set_from_pixbuf (user_data, albumart);
	}
}
#endif

static void
on_button_clicked (GtkButton *button, gpointer user_data)
{
	gchar *album, *artist;

	album = gtk_entry_get_text (btext);
	artist = gtk_entry_get_text (atext);

#ifdef OLDAPI
	hildon_albumart_factory_load(artist, album, "album", on_art_back, image);
#else
	HildonAlbumartFactory *f = hildon_albumart_factory_get_instance ();
	HildonAlbumartRequest *r1, *r2;

	r1 = hildon_albumart_factory_queue (f, artist, album, "album",
		on_art_back, image, NULL);

	r2 = hildon_albumart_factory_queue_thumbnail (f, artist, album, "album",
		256, 256, TRUE,
		on_art_back, imaget, NULL);


	g_print ("Requesting Nelly!\n");

	hildon_albumart_factory_queue_thumbnail(
                 f,
                 "Nelly Furtado",
                 "2008 Grammy Nominees",
                 "album",
                 1, 1, TRUE,
                 on_art_back, imaget, NULL);


	g_print ("%s\n", hildon_albumart_get_path("Nelly Furtado",
                 "2008 Grammy Nominees", "album"));

	g_object_unref (f);
	g_object_unref (r1);
	g_object_unref (r2);
#endif

}

int
main(int argc, char **argv) 
{

	gtk_init (&argc, &argv);

	window = gtk_window_new (GTK_WINDOW_TOPLEVEL);
	box = gtk_vbox_new (FALSE, 5);

	hbox = gtk_hbox_new (FALSE, 5);

	atext = gtk_entry_new ();
	gtk_box_pack_start (GTK_BOX (hbox), GTK_WIDGET (atext), FALSE, TRUE, 0);
	btext = gtk_entry_new ();
	gtk_box_pack_start (GTK_BOX (hbox), GTK_WIDGET (btext), FALSE, TRUE, 0);

	gtk_box_pack_start (GTK_BOX (box), GTK_WIDGET (hbox), FALSE, TRUE, 0);

	image = gtk_image_new ();
	gtk_box_pack_start (GTK_BOX (box), GTK_WIDGET (image), TRUE, TRUE, 0);

	imaget = gtk_image_new ();
	gtk_box_pack_start (GTK_BOX (box), GTK_WIDGET (imaget), TRUE, TRUE, 0);

	button = gtk_button_new_with_label ("Get album art");
	gtk_box_pack_start (GTK_BOX (box), GTK_WIDGET (button), FALSE, TRUE, 0);

	g_signal_connect (G_OBJECT (button), "clicked", 
					  G_CALLBACK (on_button_clicked), NULL);

	gtk_container_add (GTK_CONTAINER (window), GTK_WIDGET (box));
	gtk_widget_show_all (GTK_WIDGET (window));
	gtk_main();
}
