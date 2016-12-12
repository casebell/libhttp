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

static void send_ssi_file(struct mg_connection *, const char *, struct file *, int);


static void do_ssi_include(struct mg_connection *conn, const char *ssi, char *tag, int include_level) {

	char file_name[MG_BUF_LEN];
	char path[512];
	char *p;
	struct file file = STRUCT_FILE_INITIALIZER;
	size_t len;
	int truncated = 0;

	if ( conn == NULL ) return;

	/* sscanf() is safe here, since send_ssi_file() also uses buffer
	 * of size MG_BUF_LEN to get the tag. So strlen(tag) is
	 * always < MG_BUF_LEN. */
	if (sscanf(tag, " virtual=\"%511[^\"]\"", file_name) == 1) {
		/* File name is relative to the webserver root */
		file_name[511] = 0;
		XX_httplib_snprintf(conn, &truncated, path, sizeof(path), "%s/%s", conn->ctx->config[DOCUMENT_ROOT], file_name);

	} else if (sscanf(tag, " abspath=\"%511[^\"]\"", file_name) == 1) {
		/* File name is relative to the webserver working directory
		 * or it is absolute system path */
		file_name[511] = 0;
		XX_httplib_snprintf(conn, &truncated, path, sizeof(path), "%s", file_name);

	} else if (sscanf(tag, " file=\"%511[^\"]\"", file_name) == 1
	           || sscanf(tag, " \"%511[^\"]\"", file_name) == 1) {
		/* File name is relative to the currect document */
		file_name[511] = 0;
		XX_httplib_snprintf(conn, &truncated, path, sizeof(path), "%s", ssi);

		if (!truncated) {
			if ((p = strrchr(path, '/')) != NULL) p[1] = '\0';
			len = strlen(path);
			XX_httplib_snprintf(conn, &truncated, path + len, sizeof(path) - len, "%s", file_name);
		}

	} else {
		mg_cry(conn, "Bad SSI #include: [%s]", tag);
		return;
	}

	if (truncated) {
		mg_cry(conn, "SSI #include path length overflow: [%s]", tag);
		return;
	}

	if (!XX_httplib_fopen(conn, path, "rb", &file)) {
		mg_cry(conn, "Cannot open SSI #include: [%s]: fopen(%s): %s", tag, path, strerror(ERRNO));
	} else {
		XX_httplib_fclose_on_exec(&file, conn);
		if (XX_httplib_match_prefix(conn->ctx->config[SSI_EXTENSIONS], strlen(conn->ctx->config[SSI_EXTENSIONS]), path) > 0) {

			send_ssi_file(conn, path, &file, include_level + 1);
		} else {
			XX_httplib_send_file_data(conn, &file, 0, INT64_MAX);
		}
		XX_httplib_fclose(&file);
	}
}


#if !defined(NO_POPEN)
static void do_ssi_exec(struct mg_connection *conn, char *tag) {

	char cmd[1024] = "";
	struct file file = STRUCT_FILE_INITIALIZER;

	if (sscanf(tag, " \"%1023[^\"]\"", cmd) != 1) {
		mg_cry(conn, "Bad SSI #exec: [%s]", tag);
	} else {
		cmd[1023] = 0;
		if ((file.fp = popen(cmd, "r")) == NULL) {
			mg_cry(conn, "Cannot SSI #exec: [%s]: %s", cmd, strerror(ERRNO));
		} else {
			XX_httplib_send_file_data(conn, &file, 0, INT64_MAX);
			pclose(file.fp);
		}
	}
}
#endif /* !NO_POPEN */


static int mg_fgetc( struct file *filep, int offset ) {

	if ( filep         == NULL                                                              ) return EOF;
	if ( filep->membuf != NULL  &&  offset >= 0  &&  ((unsigned int)(offset)) < filep->size ) return ((const unsigned char *)filep->membuf)[offset];
	if ( filep->fp     != NULL                                                              ) return fgetc( filep->fp );

	return EOF;

}  /* mg_fgetc */


static void send_ssi_file( struct mg_connection *conn, const char *path, struct file *filep, int include_level ) {

	char buf[MG_BUF_LEN];
	int ch;
	int offset;
	int len;
	int in_ssi_tag;

	if (include_level > 10) {
		mg_cry(conn, "SSI #include level is too deep (%s)", path);
		return;
	}

	in_ssi_tag = len = offset = 0;
	while ((ch = mg_fgetc(filep, offset)) != EOF) {
		if (in_ssi_tag && ch == '>') {
			in_ssi_tag = 0;
			buf[len++] = (char)ch;
			buf[len] = '\0';
			/* assert(len <= (int) sizeof(buf)); */
			if (len > (int)sizeof(buf)) break;
			if (len < 6 || memcmp(buf, "<!--#", 5) != 0) {
				/* Not an SSI tag, pass it */
				mg_write( conn, buf, (size_t)len );
			} else {
				if (!memcmp(buf + 5, "include", 7)) {
					do_ssi_include(conn, path, buf + 12, include_level);
#if !defined(NO_POPEN)
				} else if (!memcmp(buf + 5, "exec", 4)) {
					do_ssi_exec(conn, buf + 9);
#endif /* !NO_POPEN */
				} else mg_cry(conn, "%s: unknown SSI " "command: \"%s\"", path, buf);
			}
			len = 0;
		} else if (in_ssi_tag) {
			if (len == 5 && memcmp(buf, "<!--#", 5) != 0) {
				/* Not an SSI tag */
				in_ssi_tag = 0;
			} else if (len == (int)sizeof(buf) - 2) {
				mg_cry(conn, "%s: SSI tag is too large", path);
				len = 0;
			}
			buf[len++] = (char)(ch & 0xff);
		} else if (ch == '<') {
			in_ssi_tag = 1;
			if (len > 0) {
				mg_write(conn, buf, (size_t)len);
			}
			len = 0;
			buf[len++] = (char)(ch & 0xff);
		} else {
			buf[len++] = (char)(ch & 0xff);
			if (len == (int)sizeof(buf)) {
				mg_write(conn, buf, (size_t)len);
				len = 0;
			}
		}
	}

	/* Send the rest of buffered data */
	if (len > 0) mg_write(conn, buf, (size_t)len);
}


void XX_httplib_handle_ssi_file_request( struct mg_connection *conn, const char *path, struct file *filep ) {

	char date[64];
	time_t curtime;
	const char *cors1;
	const char *cors2;
	const char *cors3;

	if ( conn == NULL  ||  path == NULL  ||  filep == NULL ) return;

	curtime = time( NULL );

	if (mg_get_header(conn, "Origin")) {
		/* Cross-origin resource sharing (CORS). */
		cors1 = "Access-Control-Allow-Origin: ";
		cors2 = conn->ctx->config[ACCESS_CONTROL_ALLOW_ORIGIN];
		cors3 = "\r\n";
	} else {
		cors1 = "";
		cors2 = "";
		cors3 = "";
	}

	if (!XX_httplib_fopen(conn, path, "rb", filep)) {
		/* File exists (precondition for calling this function),
		 * but can not be opened by the server. */
		XX_httplib_send_http_error(conn, 500, "Error: Cannot read file\nfopen(%s): %s", path, strerror(ERRNO));
	} else {
		conn->must_close = 1;
		XX_httplib_gmt_time_string(date, sizeof(date), &curtime);
		XX_httplib_fclose_on_exec(filep, conn);
		mg_printf(conn, "HTTP/1.1 200 OK\r\n");
		XX_httplib_send_no_cache_header(conn);
		mg_printf(conn,
		          "%s%s%s"
		          "Date: %s\r\n"
		          "Content-Type: text/html\r\n"
		          "Connection: %s\r\n\r\n",
		          cors1,
		          cors2,
		          cors3,
		          date,
		          XX_httplib_suggest_connection_header(conn));
		send_ssi_file(conn, path, filep, 0);
		XX_httplib_fclose(filep);
	}

}  /* XX_httplib_handle_ssi_file_request */
