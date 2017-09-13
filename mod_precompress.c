/* Copyright 2017 by SneakyPack.Com
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include "apr.h"
#include "apr_lib.h"
#include "apr_strings.h"
#include "apr_buckets.h"
#include "apr_file_io.h"

#include "apr_hash.h"
#define APR_WANT_IOVEC
#define APR_WANT_STRFUNC
#include "apr_want.h"

#include "httpd.h"
#include "http_log.h"
#include "http_config.h"
#define CORE_PRIVATE
#include "http_request.h"
#include "http_core.h" /* needed for per-directory core-config */
#include "util_filter.h"
#include "http_protocol.h" /* ap_hook_insert_error_filter */

typedef struct
{
	int     enabled;
} precompress_config;

module AP_MODULE_DECLARE_DATA    precompress_module;

static int acceptsGzip(request_rec * r) {
	const char *accepts;
	/* if they don't have the line, then they can't play */
	accepts = apr_table_get(r->headers_in, "Accept-Encoding");
	if (accepts == NULL) {
		return 0;
	}

	char *token = ap_get_token(r->pool, &accepts, 0);
	while (token && token[0] && strcasecmp(token, "gzip")) {
		/* skip parameters, XXX: ;q=foo evaluation? */
		while (*accepts == ';') {
			++accepts;
			ap_get_token(r->pool, &accepts, 1);
		}

		/* retrieve next token */
		if (*accepts == ',') {
			++accepts;
		}
		token = (*accepts) ? ap_get_token(r->pool, &accepts, 0) : NULL;
	}

	/* No acceptable token found. */
	if (token == NULL || token[0] == '\0') {
		return 0;
	}

	return 1;
}

const char *precompressed_set_enabled(cmd_parms *cmd, void *cfg, int arg)
{
	precompress_config *conf = (precompress_config *)cfg;

	conf->enabled = arg ? 1 : 0;

	return NULL;
}

static int gzip_fixup(request_rec *r) {
	precompress_config *config = (precompress_config *)ap_get_module_config(r->per_dir_config, &precompress_module);
	const char * ce = apr_table_get(r->headers_out, "Content-Encoding");
	const char * ct = r->content_type ? r->content_type : apr_table_get(r->headers_out, "Content-Type");
	if (r->method_number != M_GET || config->enabled != 1 || ce && !strcasecmp(ce, "gzip")) {
		return DECLINED;
	}
	
	if (strlen(r->uri) > 3 && !strcasecmp(strchr(r->uri, 0) - 3, ".gz")) {
		return DECLINED;
	}
	if (r->finfo.filetype == APR_NOFILE && !r->handler) {
		// file does not exist; check .gz
		char * name_ptr = apr_pstrcat(r->pool, r->uri, ".gz", NULL);
		request_rec *rr = ap_sub_req_lookup_uri(name_ptr, r, r->output_filters);
		if (rr->status == HTTP_OK
			&& ((rr->handler && !strcmp(rr->handler, "proxy-server"))
				|| rr->finfo.filetype == APR_REG)) {
			// .gz exists
			apr_table_mergen(rr->headers_out, "Content-Encoding", "gzip");
			if (!acceptsGzip(r)) {
				// decompress; must have mod_deflate
				// decompression removes "Content-Encoding"
				ap_add_output_filter("INFLATE", NULL, rr, rr->connection);
			}
			if (ct) {
				rr->content_type = ct;
				apr_table_setn(rr->headers_out, "Content-Type", ct);
			}
			apr_table_mergen(rr->headers_out, "Vary", "Accept-Encoding");
			ap_internal_fast_redirect(rr, r);
			return OK;
		}
		ap_destroy_sub_req(rr);
	}
	return DECLINED;
}

static void register_hooks(apr_pool_t *p)
{
	ap_hook_fixups(gzip_fixup, NULL, NULL, APR_HOOK_LAST);
}

void *create_precompress_dir_conf(apr_pool_t *pool, char *dummy)
{
	precompress_config *cfg = apr_pcalloc(pool, sizeof(precompress_config));
	cfg->enabled = -1;
	return cfg;
}

void *merge_precompress_dir_conf(apr_pool_t *pool, void *BASE, void *ADD)
{
	precompress_config *base = (precompress_config *)BASE;
	precompress_config *add = (precompress_config *)ADD;
	precompress_config *conf = (precompress_config *)create_precompress_dir_conf(pool, "Merged configuration");

	conf->enabled = (add->enabled == -1) ? base->enabled : add->enabled;
	return conf;
}

static const command_rec directives[] =
{
	AP_INIT_FLAG("Precompressed", precompressed_set_enabled, NULL, OR_FILEINFO, "On|Off - Enable or disable precomressed content"),
	{ NULL }
};

module AP_MODULE_DECLARE_DATA    precompress_module =
{
	STANDARD20_MODULE_STUFF,
	create_precompress_dir_conf,    /* Per-directory configuration handler */
	merge_precompress_dir_conf,     /* Merge handler for per-directory configurations */
	NULL,               /* Per-server configuration handler */
	NULL,               /* Merge handler for per-server configurations */
	directives,         /* Any directives we may have for httpd */
	register_hooks      /* Our hook registering function */
};