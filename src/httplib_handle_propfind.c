/* 
 * Copyright (c) 2016 Lammert Bies
 * Copyright (c) 2013-2016 the Civetweb developers
 * Copyright (c) 2004-2013 Sergey Lyubka
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 */

#include "httplib_main.h"
#include "httplib_string.h"
#include "httplib_utils.h"

#if !defined(NO_FILES)

/*
 * static void print_props( struct mg_connection *conn, const char *uri, struct file *filep );
 *
 * The function print_props() writes the PROPFIND properties for a collection
 * event.
 */

static void print_props( struct mg_connection *conn, const char *uri, struct file *filep ) {

	char mtime[64];

	if ( conn == NULL  ||  uri == NULL  ||  filep == NULL ) return;

	XX_httplib_gmt_time_string(mtime, sizeof(mtime), &filep->last_modified);
	conn->num_bytes_sent +=
	    mg_printf(conn,
	              "<d:response>"
	              "<d:href>%s</d:href>"
	              "<d:propstat>"
	              "<d:prop>"
	              "<d:resourcetype>%s</d:resourcetype>"
	              "<d:getcontentlength>%" INT64_FMT "</d:getcontentlength>"
	              "<d:getlastmodified>%s</d:getlastmodified>"
	              "</d:prop>"
	              "<d:status>HTTP/1.1 200 OK</d:status>"
	              "</d:propstat>"
	              "</d:response>\n",
	              uri,
	              filep->is_directory ? "<d:collection/>" : "",
	              filep->size,
	              mtime);

}  /* print_props */



/*
 * static void print_dav_dir_entry( struct de *de, void *data );
 *
 * The function print_dav_dir_entry() is used to send the properties of a
 * webdav directory to the remote client.
 */

static void print_dav_dir_entry( struct de *de, void *data ) {

	char href[PATH_MAX];
	char href_encoded[PATH_MAX * 3 /* worst case */];
	int truncated;

	struct mg_connection *conn = (struct mg_connection *)data;
	if (!de || !conn) return;

	XX_httplib_snprintf(conn, &truncated, href, sizeof(href), "%s%s", conn->request_info.local_uri, de->file_name);

	if (!truncated) {
		mg_url_encode(href, href_encoded, PATH_MAX * 3);
		print_props(conn, href_encoded, &de->file);
	}

}  /* print_dav_dir_entry */



/*
 * void XX_httplib_handle_propfind( struct mg_connection *conn, const char *path, struct file *filep );
 *
 * The function XX_httlib_handle_propfind() handles a propfind request.
 */

void XX_httplib_handle_propfind( struct mg_connection *conn, const char *path, struct file *filep ) {

	const char *depth = mg_get_header(conn, "Depth");
	char date[64];
	time_t curtime = time(NULL);

	XX_httplib_gmt_time_string(date, sizeof(date), &curtime);

	if (!conn || !path || !filep || !conn->ctx) return;

	conn->must_close = 1;
	conn->status_code = 207;
	mg_printf(conn, "HTTP/1.1 207 Multi-Status\r\n" "Date: %s\r\n", date);
	XX_httplib_send_static_cache_header(conn);
	mg_printf(conn, "Connection: %s\r\n" "Content-Type: text/xml; charset=utf-8\r\n\r\n", XX_httplib_suggest_connection_header(conn));

	conn->num_bytes_sent += mg_printf(conn, "<?xml version=\"1.0\" encoding=\"utf-8\"?>" "<d:multistatus xmlns:d='DAV:'>\n");

	/* Print properties for the requested resource itself */
	print_props(conn, conn->request_info.local_uri, filep);

	/* If it is a directory, print directory entries too if Depth is not 0 */
	if (filep && filep->is_directory
	    && !mg_strcasecmp(conn->ctx->config[ENABLE_DIRECTORY_LISTING], "yes")
	    && (depth == NULL || strcmp(depth, "0") != 0)) {
		XX_httplib_scan_directory(conn, path, conn, &print_dav_dir_entry);
	}

	conn->num_bytes_sent += mg_printf(conn, "%s\n", "</d:multistatus>");

}  /* XX_httplib_handle_propfind */

#endif
