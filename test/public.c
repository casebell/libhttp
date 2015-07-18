/* Copyright (c) 2015 the Civetweb developers
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

#include <stdlib.h>

#include "public.h"
#include <civetweb.h>

#if defined(_WIN32)
#include <Windows.h>
#define mg_sleep(x) (Sleep(x))
#else
#include <unistd.h>
#define mg_sleep(x) (usleep((x)*1000))
#endif

/* This unit test file uses the excellent Check unit testing library.
 * The API documentation is available here:
 * http://check.sourceforge.net/doc/check_html/index.html
 */

START_TEST (test_mg_get_cookie)
{
  char buf[20];

  ck_assert_int_eq(-2, mg_get_cookie("", "foo", NULL, sizeof(buf)));
  ck_assert_int_eq(-2, mg_get_cookie("", "foo", buf, 0));
  ck_assert_int_eq(-1, mg_get_cookie("", "foo", buf, sizeof(buf)));
  ck_assert_int_eq(-1, mg_get_cookie("", NULL, buf, sizeof(buf)));
  ck_assert_int_eq(1, mg_get_cookie("a=1; b=2; c; d", "a", buf, sizeof(buf)));
  ck_assert_str_eq("1", buf);
  ck_assert_int_eq(1, mg_get_cookie("a=1; b=2; c; d", "b", buf, sizeof(buf)));
  ck_assert_str_eq("2", buf);
  ck_assert_int_eq(3, mg_get_cookie("a=1; b=123", "b", buf, sizeof(buf)));
  ck_assert_str_eq("123", buf);
  ck_assert_int_eq(-1, mg_get_cookie("a=1; b=2; c; d", "c", buf, sizeof(buf)));
}
END_TEST


START_TEST (test_mg_start_stop_server)
{
  struct mg_context *ctx;
  const char *OPTIONS[] = {
    "document_root", ".",
    "listening_ports", "8080",
    NULL,
  };

  ctx = mg_start(NULL, NULL, OPTIONS);
  ck_assert(ctx != NULL);
  mg_sleep(1000);
  mg_stop(ctx);
}
END_TEST


Suite * make_public_suite (void) {

  Suite * const suite = suite_create("Public");
  TCase * const cookies = tcase_create("Cookies");
  TCase * const startserver = tcase_create("StartServer");

  tcase_add_test(cookies, test_mg_get_cookie);
  suite_add_tcase(suite, cookies);

  tcase_add_test(startserver, test_mg_start_stop_server);
  suite_add_tcase(suite, startserver);

  return suite;
}