#include "cairo_buffer.hpp"

namespace miquoverview {

const struct wlr_buffer_impl OverviewCairoBuffer::s_buffer_impl = {
    .destroy = OverviewCairoBuffer::buffer_destroy,
    .get_dmabuf = nullptr,
    .get_shm = nullptr,
    .begin_data_ptr_access = OverviewCairoBuffer::buffer_begin_data_ptr_access,
    .end_data_ptr_access = OverviewCairoBuffer::buffer_end_data_ptr_access,
};

OverviewCairoBuffer* OverviewCairoBuffer::create(int width, int height) {
    if (width <= 0 || height <= 0) return nullptr;
    return new OverviewCairoBuffer(width, height);
}

OverviewCairoBuffer::OverviewCairoBuffer(int width, int height)
    : m_width(width), m_height(height)
{
    wlr_buffer_init(&m_base, &s_buffer_impl, width, height);
    m_surface = cairo_image_surface_create(CAIRO_FORMAT_ARGB32, width, height);
    m_cr = cairo_create(m_surface);
}

OverviewCairoBuffer::~OverviewCairoBuffer() {
    if (m_cr) {
        cairo_destroy(m_cr);
        m_cr = nullptr;
    }
    if (m_surface) {
        cairo_surface_destroy(m_surface);
        m_surface = nullptr;
    }
}

void OverviewCairoBuffer::clear() {
    if (!m_cr) return;
    cairo_save(m_cr);
    cairo_set_operator(m_cr, CAIRO_OPERATOR_CLEAR);
    cairo_paint(m_cr);
    cairo_restore(m_cr);
}

void OverviewCairoBuffer::drop() {
    wlr_buffer_drop(&m_base);
}

void OverviewCairoBuffer::buffer_destroy(struct wlr_buffer* buffer) {
    OverviewCairoBuffer* self = wl_container_of(buffer, self, m_base);
    delete self;
}

bool OverviewCairoBuffer::buffer_begin_data_ptr_access(struct wlr_buffer* buffer, uint32_t flags,
                                                      void** data, uint32_t* format, size_t* stride)
{
    OverviewCairoBuffer* self = wl_container_of(buffer, self, m_base);
    if (!self->m_surface) return false;
    *data = cairo_image_surface_get_data(self->m_surface);
    *stride = cairo_image_surface_get_stride(self->m_surface);
    *format = DRM_FORMAT_ARGB8888;
    return true;
}

void OverviewCairoBuffer::buffer_end_data_ptr_access(struct wlr_buffer* buffer) {
    // No-op for direct image surface memory
}

} // namespace miquoverview
