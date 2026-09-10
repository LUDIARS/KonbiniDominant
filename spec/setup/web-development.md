# Web development

Install and activate the official Emscripten 6.0.9 SDK in a build directory.
Use Ninja (Visual Studio's bundled ninja.exe is also supported). No permanent
PATH changes or global SDK installation are required.

    python tools/build_web.py --emsdk /path/to/emsdk --ninja /path/to/ninja

The default output is build-web/dist. Optional --pictor-source and --ergo-source
reuse local checkouts at the exact pinned commits; modified dependencies fail
configuration. The Web target is selected only by the Emscripten toolchain.
Native CMake configurations retain the existing Vulkan targets.

Serve the entire dist directory from a static HTTP(S) host. index.html,
index.js, index.wasm, index.data, browser.js and style.css must remain together.
Use application/wasm for .wasm and application/octet-stream for .data. Do not
open index.html via file://. No application backend, account, or database is
required. HTTPS is recommended for deployed pages; fullscreen support is optional.

The committed tools/run_web_playtest.py stages output under the project body and
claims the Cc testing slot. Its Node harness uses playwright-core 1.63.0 and an
installed Microsoft Edge. Run only from the project body, with CONCORDIA_URL and
the current LICTOR_PORT available. It invokes no LLM and starts no listening server.

    python tools/run_web_playtest.py --project-body /project/body --dist /checkout/build-web/dist --playwright /project/body/build-web-browser/node_modules/playwright-core

The source must be clean and the build revision must match HEAD. Rebuild/relink
after committing. Test evidence contains the full campaign report, actual canvas
screenshots, browser errors, asset hashes and the claim release marker. The normal
page runs at 1x. Explicit ?autoplay=1&speed=100 enables the existing C++ test pilot.

The Web ZIP is a static distribution, not a mobile native package. Deployment
destination and real Android/iOS browser validation are separate from desktop
browser acceptance.
