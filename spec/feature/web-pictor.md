# Pictor browser campaign

## Contract

The browser edition runs the existing C++ campaign in WebAssembly. Pictor is
mandatory: its pinned WebGLContext, WebGLShaderManager and WebGLBufferManager
own real browser GPU resources. The host issues game-specific mesh draws through
these components. No separate JavaScript game rules or streaming server are used.

Windows/Android/iOS keep the Pictor Vulkan path. GameSession emits a PreparedFrame
containing the same camera, world draw list and vector HUD for both hosts. Native
entry points retain their existing API through native_game_session.cpp. Snapshot
observation still runs after every simulation tick, including accelerated ticks,
so the Ergo construction animation survives multi-tick frames.

## Browser behavior

- Initial drawing buffer: 1280 x 720, scaled as a 16:9 canvas.
- Pointer Events map CSS coordinates back to drawing-buffer coordinates.
- Mouse and touch share the existing pointer controller and TouchContacts.
- Chain selection, one-chain grid placement, connected stores, skills, all campaign
  phases, Aion warning, construction particles and results share native code.
- Portrait orientation displays a rotation prompt and suspends simulation.
- Visibility/focus changes cancel gestures and drain elapsed time; no catch-up
  simulation is applied after returning to the page.
- Context loss and initialization errors display an explicit reload state.
- Only the campaign grid is supported. Legacy Figmentum building-city content is
  rejected rather than silently replaced or omitted.

The camera remains in the established Vulkan clip convention. Only the WebGL
vertex shader converts its Y axis and depth range. World shading is encoded to
sRGB for the default browser drawing buffer; the existing HUD remains unlit.

## Dependency boundary

Pictor c088e8d1b7b9e2625b7a8d923c89d4d684566c16 and Ergo
771b027f0e5492015b27f54c3bab1fd5c1ae4790 are unchanged. The Web build compiles the
upstream optional pictor_webgl source list and the two Ergo CPU particle sources.
Their root desktop CMake setup also discovers GLFW/Vulkan, so the browser module
selection is owned by web/dependencies.cmake. Exact clean Git-source verification
is retained. No source files in either dependency are patched.

## Acceptance

Build both WebAssembly and Windows after changes to the shared session. Validate
mouse placement, landscape touch placement, portrait pause/resume, and a complete
100x campaign with the existing deterministic behavior tree. The browser harness
uses a real Microsoft Edge WebGL2 context and supplies static HTTP responses from
assets staged under the project body; it creates no listening service and makes
no LLM calls. Claim/release and source/asset hashes accompany every recorded run.

Mobile viewport emulation is not Android/iOS hardware or Safari certification.
A static build is not proof of a deployed public URL.

## Tool references

- [Emscripten installation](https://emscripten.org/docs/getting_started/downloads.html)
- [Emscripten browser event loop](https://emscripten.org/docs/porting/emscripten-runtime-environment.html)
- [Playwright browser support](https://playwright.dev/docs/browsers)
