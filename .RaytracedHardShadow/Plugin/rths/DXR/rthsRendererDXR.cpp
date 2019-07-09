#include "pch.h"
#include "Foundation/rthsLog.h"
#include "rthsRenderer.h"
#ifdef _WIN32
#include "rthsGfxContextDXR.h"

namespace rths {

class RendererDXR : public RendererBase
{
public:
    RendererDXR(bool deferred);
    ~RendererDXR() override;
    void release() override;

    bool initialized() const override;
    bool valid() const override;
    void render() override; // called from render thread
    void finish() override; // called from render thread
    void frameBegin() override; // called from render thread
    void frameEnd() override; // called from render thread

    bool readbackRenderTarget(void *dst) override;
    const char* getTimestampLog() override;
    void* getRenderTexturePtr() override;

private:
    RenderDataDXR m_render_data;
    bool m_deferred = false;
    bool m_is_initialized = false;
};


RendererDXR::RendererDXR(bool deferred)
    : m_deferred(deferred)
{
    auto do_init = [this]() {
        GfxContextDXR::initializeInstance();
        m_is_initialized = true;
    };

    if (m_deferred)
        AddDeferredCommand(do_init);
    else
        do_init();
}

RendererDXR::~RendererDXR()
{
    GfxContextDXR::finalizeInstance();
}

void RendererDXR::release()
{
    auto do_release = [this]() {
        delete this;
    };

    if (m_deferred)
        AddDeferredCommand(do_release);
    else
        do_release();
}

bool RendererDXR::initialized() const
{
    return m_is_initialized;
}

bool RendererDXR::valid() const
{
    auto ctx = GfxContextDXR::getInstance();
    if (!ctx || !ctx->checkError())
        return false;
    return true;
}

void RendererDXR::render()
{
    if (!valid())
        return;

    auto ctx = GfxContextDXR::getInstance();
    ctx->prepare(m_render_data);
    ctx->setSceneData(m_render_data, m_scene_data);
    ctx->setRenderTarget(m_render_data, m_render_target);
    ctx->setGeometries(m_render_data, m_geometries);
    ctx->flush(m_render_data);
}

void RendererDXR::finish()
{
    if (!valid())
        return;

    auto ctx = GfxContextDXR::getInstance();
    if (!ctx->finish(m_render_data))
        m_render_data.clear();
    clearMeshInstances();
}

void RendererDXR::frameBegin()
{
    if (m_render_data.hasFlag(RenderFlag::DbgForceUpdateAS)) {
        // clear static meshes' BLAS
        for (auto& geom : m_render_data.geometries_prev)
            geom.clearBLAS();

        // mark updated to update deformable meshes' BLAS
        for (auto& geom : m_render_data.geometries_prev)
            geom.inst->base->markUpdated();
    }
}

void RendererDXR::frameEnd()
{
}

bool RendererDXR::readbackRenderTarget(void *dst)
{
    if (!valid())
        return false;

    auto ctx = GfxContextDXR::getInstance();
    return ctx->readbackRenderTarget(m_render_data, dst);
}

const char* RendererDXR::getTimestampLog()
{
#ifdef rthsEnableTimestamp
    if (m_render_data.timestamp)
        return m_render_data.timestamp->getLog().c_str();
#endif // rthsEnableTimestamp
    return nullptr;
}

void* RendererDXR::getRenderTexturePtr()
{
    if (m_render_data.render_target)
        return m_render_data.render_target->texture->resource.GetInterfacePtr();
    return nullptr;
}

IRenderer* CreateRendererDXR(bool deferred)
{
    auto ret = new RendererDXR(deferred);
    if (!deferred && !ret->valid()) {
        delete ret;
        ret = nullptr;
    }
    return ret;
}

} // namespace rths

#else // _WIN32

namespace rths {

IRenderer* CreateRendererDXR(bool deferred)
{
    return nullptr;
}

} // namespace rths

#endif // _WIN32
