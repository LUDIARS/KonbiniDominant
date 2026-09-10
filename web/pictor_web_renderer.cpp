#include "pictor_web_renderer.h"
#include "web_shaders.h"
#include <cstddef>
#include <limits>
#include <stdexcept>

namespace konbini::web {
PictorWebRenderer::PictorWebRenderer() {
    if (!context_.initialize()) throw std::runtime_error("Pictor WebGL2 initialization failed");
    context_.resize(1280, 720);
    context_.set_cull_face(false);
    program_ = shaders_.create_program(kVertexShader, kFragmentShader, "konbini-world-hud");
    if (!program_) throw std::runtime_error("Pictor WebGL2 shader compilation failed");
    vao_ = buffers_.create_vao();
    vertices_ = buffers_.create_vertex_buffer(nullptr, 0, ::pictor::WebGLBufferUsage::STREAM);
    indices_ = buffers_.create_index_buffer(nullptr, 0, ::pictor::WebGLBufferUsage::STREAM);
    if (!vao_ || !vertices_ || !indices_) throw std::runtime_error("Pictor WebGL2 buffer allocation failed");
}
PictorWebRenderer::~PictorWebRenderer() {
    buffers_.shutdown();
    shaders_.shutdown();
    context_.shutdown();
}
void PictorWebRenderer::mesh(const render::WorldMesh& mesh, const bool hud,
                             const render::IsometricCamera& camera) {
    if (mesh.indices.empty()) return;
    if (mesh.indices.size() > static_cast<std::size_t>(std::numeric_limits<GLsizei>::max()))
        throw std::runtime_error("WebGL mesh is too large");
    buffers_.bind_vao(vao_);
    buffers_.update_vertex_buffer(vertices_, mesh.vertices.data(),
                                  mesh.vertices.size() * sizeof(render::WorldVertex));
    buffers_.bind_vertex_buffer(vertices_);
    buffers_.bind_index_buffer(indices_);
    // Pictor owns the index buffer lifetime; its current API has no index-update
    // method, so the host uploads to that bound buffer with the standard GL call.
    glBufferData(GL_ELEMENT_ARRAY_BUFFER,
                 static_cast<GLsizeiptr>(mesh.indices.size() * sizeof(std::uint32_t)),
                 mesh.indices.data(), GL_STREAM_DRAW);
    const GLint sizes[] = {3, 3, 4};
    const std::size_t offsets[] = {offsetof(render::WorldVertex, position),
        offsetof(render::WorldVertex, normal), offsetof(render::WorldVertex, color)};
    for (GLuint i = 0; i < 3; ++i) {
        glEnableVertexAttribArray(i);
        glVertexAttribPointer(i, sizes[i], GL_FLOAT, GL_FALSE,
                              sizeof(render::WorldVertex), reinterpret_cast<const void*>(offsets[i]));
    }
    shaders_.use(program_);
    shaders_.set_uniform_1i(program_, "u_hud", hud ? 1 : 0);
    shaders_.set_uniform_mat4(program_, "u_view_projection", camera.viewProjection.data());
    shaders_.set_uniform_2f(program_, "u_extent",
                           static_cast<float>(camera.extent.width),
                           static_cast<float>(camera.extent.height));
    glDrawElements(GL_TRIANGLES, static_cast<GLsizei>(mesh.indices.size()), GL_UNSIGNED_INT, nullptr);
    buffers_.unbind_vao();
}
void PictorWebRenderer::draw(const render::PreparedFrame& frame) {
    if (!frame.world.baseFacilities.empty() || !frame.world.overlayFacilities.empty())
        throw std::runtime_error("Browser host requires the grid-town campaign");
    glDepthMask(GL_TRUE);
    context_.begin_frame(0.08F, 0.08F, 0.10F, 1.0F);
    context_.set_depth_test(true);
    glDepthFunc(GL_LEQUAL);
    context_.set_blend(false);
    mesh(frame.world.storeMesh, false, frame.camera);
    context_.set_blend(true);
    glDepthMask(GL_FALSE);
    mesh(frame.world.overlayMesh, false, frame.camera);
    context_.set_depth_test(false);
    mesh(frame.hud, true, frame.camera);
    glDepthMask(GL_TRUE);
    shaders_.unbind();
    context_.end_frame();
    if (glGetError() != GL_NO_ERROR) throw std::runtime_error("Pictor WebGL2 draw failed");
}
}
