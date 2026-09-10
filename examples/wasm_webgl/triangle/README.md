# DQ WebGL wasm32-bare triangle example

This mirrors `examples/wasm_gpu`, but uses WebGL 2 instead of WebGPU. DQ
creates the vertex buffer and GLSL shaders, updates the moving offset uniform,
and emits every frame command. `webgl-interface.js` maps DQ's numeric handles
to WebGL objects; `webgl.js` only starts the module and animation loop.
DQ builds the live `FrameCounter = …` string. A generic JavaScript service
rasterizes the supplied UTF-8 bytes through a browser canvas on every frame,
uploads them as a texture, and DQ draws that texture as a blended rectangle over
the triangle.

Build the module from the repository root:

```sh
build/dq-comp examples/wasm_webgl/triangle/triangle.dqproj
```

Serve this directory over HTTP (for example, `python3 -m http.server` from
`examples/wasm_webgl/triangle`) and open `index.html`. A browser with WebGL 2
support is required; loading a wasm module from a `file://` URL is blocked by
browsers.
