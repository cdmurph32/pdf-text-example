# pdf-text-example
Wasmcloud demo of a PDF processing service.

### Prerequisites
* [wit-bindgen](https://github.com/bytecodealliance/wit-bindgen)
* [wasi-sdk](https://github.com/WebAssembly/wasi-sdk)
* [wash](https://github.com/wasmCloud/wasmCloud)

```
git clone --recurse-submodules ...
```
### Build
```
make
```

### Run
```
wash up --wadm-manifest ./wadm.yaml -d
```

### Use
```
curl -X POST --data-binary "@pdfio/testfiles/testpdfio.pdf" localhost:8080
```
