# DQ WebGL wasm32-bare triangle example

This mirrors `examples/wasm_gpu`, but uses WebGL 2 instead of WebGPU. DQ
creates the vertex buffer and GLSL shaders, updates the moving offset uniform,
and emits every frame command. `webgl-interface.js` maps DQ's numeric handles
to WebGL objects; `webgl.js` only starts the module and animation loop.
The live `FrameCounter = …` overlay is rasterized by a browser canvas through a
JavaScript import on every frame, uploaded as a texture, and drawn by DQ as a
blended textured rectangle over the triangle.

Build the module from the repository root:

```sh
build/dq-comp examples/wasm_webgl/triangle/triangle.dqproj
```

Serve this directory over HTTP (for example, `python3 -m http.server` from
`examples/wasm_webgl/triangle`) and open `index.html`. A browser with WebGL 2
support is required; loading a wasm module from a `file://` URL is blocked by
browsers.
