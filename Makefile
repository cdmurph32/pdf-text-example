# Build zlib, then pdfio, and finally the pdf_text wasm.

ZLIB_WASM=zlib/libz.wasm.a
PDFIO_WASM=pdfio/libpdfio.a
PDF_WASM=pdf_text.wasm
WASI_SDK_DIR=/opt/wasi-sdk
AR=$(WASI_SDK_DIR)/bin/ar
CC=$(WASI_SDK_DIR)/bin/clang
RANLIB=$(WASI_SDK_DIR)/bin/ranlib

sign: $(PDF_WASM)
	mkdir -p build && wash claims sign -n pdf-text -v 0.1.0 -r 1 pdf_text.wasm && mv pdf_text_s.wasm build/

$(PDF_WASM): $(ZLIB_WASM) $(PDFIO_WASM) pdf_component_type.o
	$(CC) -target wasm32-wasip2 -mexec-model=reactor -D_WASI_EMULATED_MMAN -lwasi-emulated-mman -z stack-size=2097152 pdf.c pdf_text.c pdf_component_type.o -I ./pdfio -L ./pdfio -L ./zlib -lpdfio -lz.wasm -o pdf_text.wasm

$(ZLIB_WASM):
	make -C zlib WASI_SDK_DIR=$(WASI_SDK_DIR)

# Passing LIBS to configure breaks the compiler check for some reason.
$(PDFIO_WASM):
	AR=$(AR) CC=$(CC) RANLIB=$(RANLIB) CFLAGS="-I../zlib/src" ./pdfio/configure --host=wasm32-wasi --srcdir=$(shell pwd)/pdfio --verbose
	cd pdfio && make libpdfio.a LIBS="-L../zlib -lz.wasm -lm"

pdf_component_type.o:
	wit-bindgen c --autodrop-borrows yes ./wit

clean:
	rm -f *.o *.a *.h *.wasm
	make -C zlib clean
