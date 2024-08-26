#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include "pdf.h"

static const struct {
	const char *method;
} http_method_map[] = {
	[WASI_HTTP_TYPES_METHOD_GET]	 = { "GET"     },
	[WASI_HTTP_TYPES_METHOD_HEAD]	 = { "HEAD"    },
	[WASI_HTTP_TYPES_METHOD_POST]    = { "POST"    },
	[WASI_HTTP_TYPES_METHOD_PUT]	 = { "PUT"     },
	[WASI_HTTP_TYPES_METHOD_DELETE]	 = { "DELETE"  },
	[WASI_HTTP_TYPES_METHOD_CONNECT] = { "CONNECT" },
	[WASI_HTTP_TYPES_METHOD_OPTIONS] = { "OPTIONS" },
	[WASI_HTTP_TYPES_METHOD_TRACE]	 = { "TRACE"   },
	[WASI_HTTP_TYPES_METHOD_PATCH]   = { "PATCH"   },
	[WASI_HTTP_TYPES_METHOD_OTHER]	 = { "OTHER"   }
};

void exports_wasi_http_incoming_handler_handle(
	exports_wasi_http_incoming_handler_own_incoming_request_t request,
	exports_wasi_http_incoming_handler_own_response_outparam_t response_out)
{
	FILE *out;
	size_t size;
	char *out_ptr;
	char *ptr;
	char clen[32];
	wasi_http_types_borrow_incoming_request_t b_req;
	wasi_http_types_own_headers_t hdrs;
	wasi_http_types_borrow_fields_t b_hdrs;
	wasi_http_types_own_incoming_body_t r_body;
	wasi_http_types_borrow_incoming_body_t b_r_body;
	wasi_http_types_own_outgoing_response_t resp;
	wasi_http_types_borrow_outgoing_response_t b_resp;
	wasi_http_types_own_fields_t fields;
	wasi_http_types_borrow_fields_t b_fields;
	wasi_http_types_result_own_outgoing_response_error_code_t rerr = { };
	wasi_http_types_own_outgoing_body_t body;
	wasi_http_types_borrow_outgoing_body_t b_body;
	wasi_io_streams_own_output_stream_t out_stream;
	wasi_io_streams_borrow_output_stream_t b_out_stream;
	pdf_list_u8_t stream_data;
	wasi_io_streams_stream_error_t stream_err;
	wasi_http_types_header_error_t hdr_err;
	wasi_http_types_field_key_t key;
	wasi_http_types_field_value_t value;
	wasi_http_types_method_t method;
	pdf_list_tuple2_field_key_field_value_t fvk;
	wasi_http_types_own_input_stream_t in_stream;
	wasi_io_streams_borrow_input_stream_t b_in_stream;
	pdf_list_u8_t data;
	wasi_io_streams_stream_error_t in_stream_err;
	pdf_string_t prstr;

	b_req = wasi_http_types_borrow_incoming_request(request);

	out = open_memstream(&out_ptr, &size);

#define BUF_ADD(fmt, ...) \
	fprintf(out, fmt, ##__VA_ARGS__);

	BUF_ADD("*** WasmCloud with C ***\n\n");

	BUF_ADD("[Request Info]\n");
	wasi_http_types_method_incoming_request_path_with_query(b_req, &prstr);
	BUF_ADD("REQUEST_PATH = %.*s\n", (int)prstr.len, prstr.ptr);
	wasi_http_types_method_incoming_request_method(b_req, &method);
	BUF_ADD("METHOD       = %s\n", http_method_map[method.tag].method);
	ptr = memchr(prstr.ptr, '?', prstr.len);
	BUF_ADD("QUERY        = %.*s\n",
	 ptr ? (int)(((char *)(prstr.ptr + prstr.len)) - ptr - 1) : 0,
	 ptr ? ptr + 1 : "");

	BUF_ADD("\n[Request Headers]\n");
	hdrs = wasi_http_types_method_incoming_request_headers(b_req);
	b_hdrs = wasi_http_types_borrow_fields(hdrs);

	wasi_http_types_method_fields_entries(b_hdrs, &fvk);
	for (size_t i = 0; i < fvk.len; i++)
		BUF_ADD("%.*s = %.*s\n",
	  (int)fvk.ptr[i].f0.len, fvk.ptr[i].f0.ptr,
	  (int)fvk.ptr[i].f1.len, fvk.ptr[i].f1.ptr);

	pdf_list_tuple2_field_key_field_value_free(&fvk);

	wasi_http_types_method_incoming_request_consume(b_req, &r_body);
	b_r_body = wasi_http_types_borrow_incoming_body(r_body);
	wasi_http_types_method_incoming_body_stream(b_r_body, &in_stream);
	b_in_stream = wasi_io_streams_borrow_input_stream(in_stream);

	bool ok = wasi_io_streams_method_input_stream_blocking_read(b_in_stream,     
							     8 * 1024*1024,   
							     &data,           
							     &in_stream_err);

	if (method.tag == WASI_HTTP_TYPES_METHOD_POST ||
		method.tag == WASI_HTTP_TYPES_METHOD_PUT) {
		BUF_ADD("\n[%s data]\n",
	  http_method_map[method.tag].method);
		BUF_ADD("%.*s\n", (int)data.len, data.ptr);
	}

	fclose(out);

	fields = wasi_http_types_constructor_fields();
	b_fields = wasi_http_types_borrow_fields(fields); 

	pdf_string_set((pdf_string_t *)&key, "Content-Length");
	sprintf(clen, "%zu", size);
	pdf_string_set((pdf_string_t *)&value, clen);

	wasi_http_types_method_fields_append(b_fields, &key, &value,
				      &hdr_err);

	resp = wasi_http_types_constructor_outgoing_response(fields);

	b_resp = wasi_http_types_borrow_outgoing_response(resp);
	wasi_http_types_method_outgoing_response_body(b_resp, &body);
	b_body = wasi_http_types_borrow_outgoing_body(body);

	wasi_http_types_method_outgoing_body_write(b_body, &out_stream);
	b_out_stream = wasi_io_streams_borrow_output_stream(out_stream);

	stream_data.len = size;
	stream_data.ptr = (uint8_t *)out_ptr;
	ok = wasi_io_streams_method_output_stream_blocking_write_and_flush(     
		b_out_stream,   
		&stream_data,   
		&stream_err);

	free(out_ptr);

	wasi_http_types_result_own_outgoing_response_error_code_t result = {
		.is_err = false,
		.val = {
			.ok = resp
		}
	};
	wasi_http_types_static_response_outparam_set(response_out, &result);
}
