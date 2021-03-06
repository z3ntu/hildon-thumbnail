/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*
 * This file is part of hildon-albumart package
 *
 * Copyright (C) 2005 Nokia Corporation.  All Rights reserved.
 *
 * Contact: Marius Vollmer <marius.vollmer@nokia.com>
 * Author: Philip Van Hoof <philip@codeminded.be>
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public License
 * version 2.1 as published by the Free Software Foundation.
 *
 * This library is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA
 * 02110-1301 USA
 *
 */
#include <gio/gio.h>
#include <glib/gfileutils.h>
#include <gdk-pixbuf/gdk-pixbuf.h>
#include <stdlib.h>

#include "hildon-albumart-factory.h"
#include "hildon-thumbnail-factory.h"
#include "albumart-client.h"

#include "utils.h"

#define ALBUMARTER_SERVICE      "com.nokia.albumart"
#define ALBUMARTER_PATH         "/com/nokia/albumart/Requester"
#define ALBUMARTER_INTERFACE    "com.nokia.albumart.Requester"

#define REQUEST_GET_PRIVATE(obj) (G_TYPE_INSTANCE_GET_PRIVATE ((obj), HILDON_TYPE_ALBUMART_REQUEST, HildonAlbumartRequestPrivate))
#define FACTORY_GET_PRIVATE(obj) (G_TYPE_INSTANCE_GET_PRIVATE ((obj), HILDON_TYPE_ALBUMART_FACTORY, HildonAlbumartFactoryPrivate))


typedef struct {
	gchar *artist_or_title;
	gchar *album;
	gchar *kind;
	gchar *key;
	guint width, height;
	HildonAlbumartRequestCallback callback;
	GDestroyNotify destroy;
	HildonAlbumartFactory *factory;
	gpointer user_data;
	HildonAlbumartRequest *real;
} HildonAlbumartRequestPrivate;

typedef struct {
	DBusGProxy *proxy;
	GHashTable *tasks;
} HildonAlbumartFactoryPrivate;



static void
create_pixbuf_and_callback (HildonAlbumartRequestPrivate *r_priv)
{
	GFile *filei = NULL;
	GInputStream *stream = NULL;
	GdkPixbuf *pixbuf = NULL;
	gchar *path;
	GError *error = NULL;

	hildon_thumbnail_util_get_albumart_path (r_priv->artist_or_title, 
						 r_priv->album, r_priv->kind, 
						 &path);

	if (r_priv->callback) {
		filei = g_file_new_for_path (path);
		stream = G_INPUT_STREAM (g_file_read (filei, NULL, &error));

		if (error)
			goto error_handler;

		pixbuf = my_gdk_pixbuf_new_from_stream (stream, NULL, &error);
	}

	error_handler:

	/* Callback user function, passing the pixbuf and error */

	if (r_priv->callback)
		r_priv->callback (r_priv->factory, pixbuf, error, r_priv->user_data);

	/* Cleanup */

	if (filei)
		g_object_unref (filei);

	if (stream) {
		g_input_stream_close (G_INPUT_STREAM (stream), NULL, NULL);
		g_object_unref (stream);
	}

	if (error)
		g_error_free (error);

	if (pixbuf)
		gdk_pixbuf_unref (pixbuf);

	g_free (path);

	if (r_priv->destroy)
		r_priv->destroy (r_priv->user_data);

}


static void
on_task_finished (DBusGProxy *proxy,
		  guint       handle,
		  gpointer    user_data)
{
	HildonAlbumartFactoryPrivate *f_priv = FACTORY_GET_PRIVATE (user_data);
	gchar *key = g_strdup_printf ("%d", handle);
	HildonAlbumartRequest *request = g_hash_table_lookup (f_priv->tasks, key);

	if (request) {
		HildonAlbumartRequestPrivate *r_priv = REQUEST_GET_PRIVATE (request);
		create_pixbuf_and_callback (r_priv);
		g_hash_table_remove (f_priv->tasks, key);
	}

	g_free (key);
}

/* static gboolean
 * have_all_for_request_cb (gpointer user_data)
 * {
 * 	HildonAlbumartRequestPrivate *r_priv = REQUEST_GET_PRIVATE (user_data);
 * 	create_pixbuf_and_callback (r_priv);
 * 	return FALSE;
 * }
 **/

static void
hildon_albumart_factory_init (HildonAlbumartFactory *self)
{
	GError *error = NULL;
	DBusGConnection *connection;
	HildonAlbumartFactoryPrivate *f_priv = FACTORY_GET_PRIVATE (self);

	connection = dbus_g_bus_get (DBUS_BUS_SESSION, &error);

	if (error) {
		g_critical ("Can't initialize HildonAlbumarter: %s\n", error->message);
		g_error_free (error);
		return;
	}

	f_priv->tasks = g_hash_table_new_full (g_str_hash, g_str_equal,
				       (GDestroyNotify) g_free,
				       (GDestroyNotify) g_object_unref);

	f_priv->proxy = dbus_g_proxy_new_for_name (connection, 
				   ALBUMARTER_SERVICE,
				   ALBUMARTER_PATH,
				   ALBUMARTER_INTERFACE);

	dbus_g_proxy_add_signal (f_priv->proxy, "Finished", 
				G_TYPE_UINT, G_TYPE_INVALID);

	dbus_g_proxy_connect_signal (f_priv->proxy, "Finished",
			     G_CALLBACK (on_task_finished),
			     g_object_ref (self),
			     (GClosureNotify) g_object_unref);
}

static void
hildon_albumart_request_init (HildonAlbumartRequest *self)
{
	HildonAlbumartRequestPrivate *r_priv = REQUEST_GET_PRIVATE (self);

	r_priv->key = NULL;
	r_priv->artist_or_title = NULL;
	r_priv->album = NULL;
	r_priv->kind = NULL;
	r_priv->factory = NULL;
	r_priv->callback = NULL;
	r_priv->destroy = NULL;
	r_priv->user_data = NULL;
	r_priv->real = NULL;
}


static void
hildon_albumart_factory_finalize (GObject *object)
{
	HildonAlbumartFactoryPrivate *f_priv = FACTORY_GET_PRIVATE (object);

	g_object_unref (f_priv->proxy);
	g_hash_table_unref (f_priv->tasks);
}

static void
hildon_albumart_request_finalize (GObject *object)
{
	HildonAlbumartRequestPrivate *r_priv = REQUEST_GET_PRIVATE (object);

	g_free (r_priv->artist_or_title);
	g_free (r_priv->album);
	g_free (r_priv->kind);

	g_free (r_priv->key);
	if (r_priv->factory)
		g_object_unref (r_priv->factory);
	if (r_priv->real)
		g_object_unref (r_priv->real);
}

static void
hildon_albumart_request_class_init (HildonAlbumartRequestClass *klass)
{
	GObjectClass *object_class = G_OBJECT_CLASS (klass);

	object_class->finalize = hildon_albumart_request_finalize;

	g_type_class_add_private (object_class, sizeof (HildonAlbumartRequestPrivate));
}

static void
hildon_albumart_factory_class_init (HildonAlbumartFactoryClass *klass)
{
	GObjectClass *object_class = G_OBJECT_CLASS (klass);

	object_class->finalize = hildon_albumart_factory_finalize;

	g_type_class_add_private (object_class, sizeof (HildonAlbumartFactoryPrivate));
}

static gboolean waiting_for_cb = FALSE;

static void 
on_got_handle (DBusGProxy *proxy, guint OUT_handle, GError *error, gpointer userdata)
{
	HildonAlbumartRequest *request = userdata;
	HildonAlbumartRequestPrivate *r_priv = REQUEST_GET_PRIVATE (request);
	HildonAlbumartFactoryPrivate *f_priv = FACTORY_GET_PRIVATE (r_priv->factory);

	gchar *key = g_strdup_printf ("%d", OUT_handle);
	r_priv->key = g_strdup (key);
	g_hash_table_replace (f_priv->tasks, key, 
			      g_object_ref (request));
	g_object_unref (request);

	waiting_for_cb = FALSE;
}

HildonAlbumartRequest*
hildon_albumart_factory_queue (HildonAlbumartFactory *self,
				  const gchar *artist_or_title, const gchar *album, const gchar *kind,
				  HildonAlbumartRequestCallback callback,
				  gpointer user_data,
				  GDestroyNotify destroy)
{
	HildonAlbumartRequest *request = g_object_new (HILDON_TYPE_ALBUMART_REQUEST, NULL);
	HildonAlbumartRequestPrivate *r_priv = REQUEST_GET_PRIVATE (request);
	HildonAlbumartFactoryPrivate *f_priv = FACTORY_GET_PRIVATE (self);


	g_debug ("Albumart n request for %s,%s,%s\n",
		 artist_or_title, album, kind);

	r_priv->artist_or_title = g_strdup (artist_or_title);
	r_priv->album = g_strdup (album);
	r_priv->kind = g_strdup (kind);
	r_priv->factory = g_object_ref (self);
	r_priv->user_data = user_data;
	r_priv->callback = callback;
	r_priv->destroy = destroy;

	waiting_for_cb = TRUE;

	com_nokia_albumart_Requester_queue_async (f_priv->proxy, 
						  r_priv->artist_or_title, 
						  r_priv->album, 
						  r_priv->kind, 0, 
						  on_got_handle,
						  g_object_ref (request));

	return request;
}

typedef struct {
	HildonAlbumartRequest *request;
	guint width;
	guint height;
	gboolean cropped;
} ArtReqInfo;


static void 
thumb_callback (HildonThumbnailFactoryHandle handle, gpointer user_data, GdkPixbuf *thumbnail, GError *error)
{
	ArtReqInfo *info = user_data;
	HildonAlbumartRequestPrivate *r_priv = REQUEST_GET_PRIVATE (info->request);

	if (r_priv->callback) 
		r_priv->callback (r_priv->factory, thumbnail, error, r_priv->user_data);

	if (r_priv->destroy)
		r_priv->destroy (r_priv->user_data);

	g_object_unref (info->request);

	g_slice_free (ArtReqInfo, info);
}

static void 
intercept_callback (HildonAlbumartFactory *self, GdkPixbuf *albumart, GError *error, gpointer user_data)
{
	ArtReqInfo *info = user_data;
	HildonAlbumartRequestPrivate *r_priv = REQUEST_GET_PRIVATE (info->request);

	if (!error) {
		gchar *uri, *path;

		hildon_thumbnail_util_get_albumart_path (r_priv->artist_or_title, 
							 r_priv->album, r_priv->kind, 
							 &path);

		uri = g_strdup_printf ("file://%s", path);

		hildon_thumbnail_factory_load_custom (uri, "image/jpeg",
						      info->width, info->height,
						      thumb_callback, 
						      info, 
						      info->cropped?HILDON_THUMBNAIL_FLAG_CROP:0, -1);

		g_free (path);
		g_free (uri);
	} else {
		thumb_callback (NULL, user_data, NULL, error);
	}

}

HildonAlbumartRequest*
hildon_albumart_factory_queue_thumbnail (HildonAlbumartFactory *self,
					  const gchar *artist_or_title, const gchar *album, const gchar *kind,
					  guint width, guint height, gboolean cropped,
					  HildonAlbumartRequestCallback callback,
					  gpointer user_data,
					  GDestroyNotify destroy)
{
	ArtReqInfo *info = g_slice_new0 (ArtReqInfo);
	HildonAlbumartRequest *request = g_object_new (HILDON_TYPE_ALBUMART_REQUEST, NULL);
	HildonAlbumartRequestPrivate *r_priv = REQUEST_GET_PRIVATE (request);


	g_debug ("Albumart nt request for %s,%s,%s\n",
		 artist_or_title, album, kind);

	r_priv->artist_or_title = g_strdup (artist_or_title);
	r_priv->album = g_strdup (album);
	r_priv->kind = g_strdup (kind);
	r_priv->factory = g_object_ref (self);
	r_priv->user_data = user_data;
	r_priv->callback = callback;
	r_priv->destroy = destroy;

	info->request = g_object_ref (request);
	info->width = width;
	info->height = height;
	info->cropped = cropped;

	r_priv->real = hildon_albumart_factory_queue (self, artist_or_title, 
						      album, kind,
						      intercept_callback,
						      info,
						      NULL);

	return request;
}


void 
hildon_albumart_factory_join (HildonAlbumartFactory *self)
{
	HildonAlbumartFactoryPrivate *f_priv = FACTORY_GET_PRIVATE (self);

	while (waiting_for_cb)
		g_main_context_iteration (NULL, FALSE);

	while(g_hash_table_size (f_priv->tasks) != 0) {
		g_main_context_iteration (NULL, FALSE);
	}
}

static void 
on_unqueued (DBusGProxy *proxy, GError *error, gpointer userdata)
{
	HildonAlbumartRequest *request = userdata;
	HildonAlbumartRequestPrivate *r_priv = REQUEST_GET_PRIVATE (request);
	HildonAlbumartFactoryPrivate *f_priv = FACTORY_GET_PRIVATE (r_priv->factory);

	g_hash_table_remove (f_priv->tasks, r_priv->key);
	g_object_unref (request);
}

void 
hildon_albumart_request_unqueue (HildonAlbumartRequest *self)
{
	HildonAlbumartRequestPrivate *r_priv = REQUEST_GET_PRIVATE (self);

	if (r_priv->real) {
		hildon_albumart_request_unqueue (r_priv->real);
	} else {
		HildonAlbumartFactoryPrivate *f_priv = FACTORY_GET_PRIVATE (r_priv->factory);
		guint handle = atoi (r_priv->key);
		com_nokia_albumart_Requester_unqueue_async (f_priv->proxy, handle,
							    on_unqueued, 
							    g_object_ref (self));
	}
}

void 
hildon_albumart_request_join (HildonAlbumartRequest *self)
{
	HildonAlbumartRequestPrivate *r_priv = REQUEST_GET_PRIVATE (self);

	if (r_priv->real) {
		hildon_albumart_request_join (r_priv->real);
	} else {
		HildonAlbumartFactoryPrivate *f_priv = FACTORY_GET_PRIVATE (r_priv->factory);
		HildonAlbumartRequest *found = g_hash_table_lookup (f_priv->tasks, r_priv->key);

		while (waiting_for_cb)
			g_main_context_iteration (NULL, FALSE);

		while (found) {
			g_main_context_iteration (NULL, FALSE);
			found = g_hash_table_lookup (f_priv->tasks, r_priv->key);
		}
	}
}

static GStaticMutex mutex = G_STATIC_MUTEX_INIT;
static HildonAlbumartFactory *singleton = NULL;

HildonAlbumartFactory* 
hildon_albumart_factory_get_instance (void)
{
	g_static_mutex_lock (&mutex);

	if (!singleton) {
		singleton = g_object_new (HILDON_TYPE_ALBUMART_FACTORY, NULL);
	}

	g_static_mutex_unlock (&mutex);

	return g_object_ref (singleton);
}

G_DEFINE_TYPE (HildonAlbumartFactory, hildon_albumart_factory, G_TYPE_OBJECT)

G_DEFINE_TYPE (HildonAlbumartRequest, hildon_albumart_request, G_TYPE_OBJECT)
