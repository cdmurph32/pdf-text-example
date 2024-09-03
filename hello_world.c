#include <fcntl.h>
#include <pdfio.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <sys/mman.h>
#include <time.h>
#include <unistd.h>
#include "pdf.h"


#define MAX_PATH 1024
#define BUFFER_SIZE 500000

#define BUF_ADD(fmt, ...) \
	fprintf(out, fmt, ##__VA_ARGS__);

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

void show_pdf_info(pdf_list_u8_t *data, FILE *out)
{
  pdfio_file_t *pdf;
  time_t       creation_date;
  struct tm    *creation_tm;
  char         creation_text[256];


  // Open the PDF file with the default callbacks...
  pdf = pdfioMemBufOpen(data->ptr, data->len, /*password_cb*/NULL, /*password_cbdata*/NULL, /*error_cb*/NULL, /*error_cbdata*/NULL);
  if (pdf == NULL)
    return;

  // Get the creation date and convert to a string...
  creation_date = pdfioFileGetCreationDate(pdf);
  creation_tm   = localtime(&creation_date);
  strftime(creation_text, sizeof(creation_text), "%c", &creation_tm);

  // Print file information to stdout...
  printf("         Title: %s\n", pdfioFileGetTitle(pdf));
  printf("        Author: %s\n", pdfioFileGetAuthor(pdf));
  printf("    Created On: %s\n", creation_text);
  printf("  Number Pages: %u\n", (unsigned)pdfioFileGetNumPages(pdf));

  // Close the PDF file...
  pdfioFileClose(pdf);
}

wasi_filesystem_types_own_descriptor_t write_to_file(const char *filename, pdf_list_u8_t *write_data, FILE *out) {
	wasi_filesystem_types_own_descriptor_t ret = { -1 };
	wasi_filesystem_types_descriptor_flags_t flags = WASI_FILESYSTEM_TYPES_DESCRIPTOR_FLAGS_WRITE;
	wasi_filesystem_types_open_flags_t open_flags = WASI_FILESYSTEM_TYPES_OPEN_FLAGS_CREATE | WASI_FILESYSTEM_TYPES_OPEN_FLAGS_TRUNCATE;
	wasi_filesystem_types_error_code_t err;
	wasi_filesystem_types_own_descriptor_t fd;
	pdf_string_t path = {
		.ptr = (uint8_t *)filename,
		.len = strlen(filename)
	};
	wasi_filesystem_preopens_list_tuple2_own_descriptor_string_t *directories;
	wasi_filesystem_types_borrow_descriptor_t base_fd;

	wasi_filesystem_preopens_get_directories(directories);
	if (directories->len == 0) {
		BUF_ADD("No filesystem access provided\n");
		return ret;
	}
	base_fd = wasi_filesystem_types_borrow_descriptor(directories->ptr[0].f0);

	// Open the file
	bool success = wasi_filesystem_types_method_descriptor_open_at(
		base_fd,
		WASI_FILESYSTEM_TYPES_PATH_FLAGS_SYMLINK_FOLLOW,
		&path,
		open_flags,
		flags,
		&fd,
		&err
	);

	if (!success) {
		BUF_ADD("Unable to open file %s\n", filename);
		return ret;
	}

	// Write to the file
	wasi_filesystem_types_filesize_t bytes_written;

	success = wasi_filesystem_types_method_descriptor_write(
		wasi_filesystem_types_borrow_descriptor(fd),
		write_data,
		0,
		&bytes_written,
		&err
	);

	if (!success || bytes_written != write_data->len) {
		return ret;
	}

	return fd;
}

void exports_wasi_http_incoming_handler_handle(
	exports_wasi_http_incoming_handler_own_incoming_request_t request,
	exports_wasi_http_incoming_handler_own_response_outparam_t response_out)
{
	FILE *out;
	int fd;
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
		show_pdf_info(&data, out);
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
