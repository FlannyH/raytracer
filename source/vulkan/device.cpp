#include "../device.h"

#include <array>

namespace gfx {
    Device::Device(const int width, const int height, const bool debug_layer_enabled, const bool gpu_profiling_enabled) {
    }
    
    Device::~Device() {
    }

    void Device::resize_window(const int width, const int height) const {
    }

    void Device::get_window_size(int& width, int& height) const {
    }

    std::shared_ptr<Pipeline> Device::create_raster_pipeline(const std::string& name, const std::string& vertex_shader_path, const std::string& pixel_shader_path, const std::initializer_list<ResourceHandlePair> render_targets, const ResourceHandlePair depth_target) {
        return nullptr;
    }

    std::shared_ptr<Pipeline> Device::create_compute_pipeline(const std::string& name, const std::string& compute_shader_path) {
        return nullptr;
    }

    void Device::begin_frame() {
    }

    void Device::end_frame() {
    }

    void Device::set_graphics_root_constants(const std::vector<uint32_t>& constants) {
    }

    void Device::set_compute_root_constants(const std::vector<uint32_t>& constants) {
    }

    int Device::frame_index() {
        return -1;
    }

    bool Device::supports(RendererFeature feature) {
        return false;
    }

    void Device::begin_raster_pass(std::shared_ptr<Pipeline> pipeline, RasterPassInfo&& render_pass_info) {
    }

    void Device::end_raster_pass() {
    }

    void Device::begin_compute_pass(std::shared_ptr<Pipeline> pipeline, bool async) {
    }

    void Device::end_compute_pass() {
    }

    void Device::dispatch_threadgroups(uint32_t x, uint32_t y, uint32_t z) {
    }

    void Device::draw_vertices(uint32_t n_vertices) {
    }

    ResourceHandlePair Device::load_texture(const std::string& name, uint32_t width, uint32_t height, uint32_t depth, void* data, PixelFormat pixel_format, TextureType type, ResourceUsage usage, int max_mip_levels, int min_resolution) {
        return {};
    }

    ResourceHandlePair Device::load_mesh(const std::string& name, const uint64_t n_triangles, Triangle* tris) {
        return {};
    }

    void debug_scene_graph_nodes(SceneNode* node, int depth = 0) {
    }

    ResourceHandlePair Device::create_buffer(const std::string& name, const size_t size, void* data, ResourceUsage usage) {
        return {};
    }

    ResourceHandlePair Device::create_render_target(const std::string& name, uint32_t width, uint32_t height, PixelFormat pixel_format, std::optional<glm::vec4> clear_color, ResourceUsage extra_usage) {
        return {};
    }

    ResourceHandlePair Device::create_depth_target(const std::string& name, uint32_t width, uint32_t height, PixelFormat pixel_format, float clear_depth) {
        return {};
    }

    void Device::resize_texture(ResourceHandlePair& handle, const uint32_t width, const uint32_t height) {
    }

    void Device::update_buffer(const ResourceHandlePair& buffer, const uint32_t offset, const uint32_t n_bytes, const void* data) {
    }

    void Device::readback_buffer(const ResourceHandlePair& buffer, const uint32_t offset, const uint32_t n_bytes, void* destination) {
    }

    void Device::queue_unload_bindless_resource(ResourceHandlePair resource) {
    }

    D3D12_RESOURCE_STATES resource_usage_to_dx12_state(ResourceUsage usage) {
        return {};
    }

    void Device::use_resource(const ResourceHandlePair &resource, const ResourceUsage usage) {
    }

    void Device::use_resources(const std::initializer_list<ResourceTransitionInfo>& resources) {
    }

    ResourceHandlePair Device::create_acceleration_structure(const std::string& name, const size_t size) {
        return {};
    }

    ResourceHandlePair Device::create_blas(const std::string& name, const ResourceHandlePair& position_buffer, const ResourceHandlePair& index_buffer, const uint32_t vertex_count, const uint32_t index_count) {
        return {};
    }

    ResourceHandlePair Device::create_tlas(const std::string& name, const std::vector<RaytracingInstance>& instances) {
        return {};
    }

    void Device::transition_resource(std::shared_ptr<CommandBuffer> cmd, std::shared_ptr<Resource> resource, D3D12_RESOURCE_STATES new_state, uint32_t subresource) {
    }

    bool Device::should_stay_open() {
        return true;
    }

    void Device::set_full_screen(bool full_screen) {
    }

    int Device::find_dominant_monitor() {
        return -1;
    }

    void Device::clean_up_old_resources() {
    }
    
    void Device::execute_resource_transitions(std::shared_ptr<CommandBuffer> cmd) {
    }
    
    ID3D12Device5* Device::device5() {
        return nullptr;
    }
}