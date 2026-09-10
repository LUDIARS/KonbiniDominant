#pragma once
#include "konbini/render/prepared_frame.h"
#include "pictor/webgl/webgl_buffer.h"
#include "pictor/webgl/webgl_context.h"
#include "pictor/webgl/webgl_shader.h"

namespace konbini::web {
class PictorWebRenderer {
public:
    PictorWebRenderer();
    ~PictorWebRenderer();
    PictorWebRenderer(const PictorWebRenderer&) = delete;
    PictorWebRenderer& operator=(const PictorWebRenderer&) = delete;
    void draw(const render::PreparedFrame& frame);
private:
    void mesh(const render::WorldMesh& mesh, bool hud,
              const render::IsometricCamera& camera);
    ::pictor::WebGLContext context_;
    ::pictor::WebGLShaderManager shaders_;
    ::pictor::WebGLBufferManager buffers_;
    ::pictor::WebGLProgramHandle program_ = 0;
    ::pictor::WebGLBufferHandle vertices_ = 0, indices_ = 0;
    GLuint vao_ = 0;
};
}
