#include "pdf.h"

void handle( wasi_http_types_own_incoming_request_t* request, wasi_http_types_own_response_outparam_t* response_output_param) {
  wasi_http_types_method_t* method;
  wasi_http_types_borrow_incoming_request_t request_borrow = wasi_http_types_borrow_incoming_request(*request);

  wasi_http_types_own_outgoing_response_t response = wasi_http_types_constructor_outgoing_response(wasi_http_types_constructor_fields());
  wasi_http_types_borrow_outgoing_response_t response_borrow = wasi_http_types_borrow_outgoing_response(response);
  wasi_http_types_own_outgoing_body_t* response_body;

  wasi_http_types_method_incoming_request_method(request_borrow, method);
  if (method == WASI_HTTP_TYPES_METHOD_GET) {
    wasi_http_types_method_outgoing_response_set_status_code(response_borrow, 200);
  } else {
    wasi_http_types_method_outgoing_response_set_status_code(response_borrow, 400);
  }
  wasi_http_types_method_outgoing_response_body(response_borrow, response_body);
  wasi_http_types_outgoing_body_write(body).blocking_write_and_flus
}
            
