/*
 * Copyright (C) 2009 Torch Mobile Inc. http://www.torchmobile.com/
 */

#include "config.h"
#include "PluginView.h"

#include "Document.h"
#include "DocumentLoader.h"
#include "Element.h"
#include "Event.h"
#include "EventNames.h"
#include "Frame.h"
#include "FrameLoadRequest.h"
#include "FrameLoader.h"
#include "FrameTree.h"
#include "FrameView.h"
#include "GraphicsContext.h"
#include "HTMLNames.h"
#include "HTMLPlugInElement.h"
#include "HostWindow.h"
#include "Image.h"
#include "IntRectRegion.h"
#include "JSDOMBinding.h"
#include "KeyboardEvent.h"
#include "MouseEvent.h"
#include "NotImplemented.h"
#include "BlackBerryPlatformWindow.h"
#include "Page.h"
#include "PlatformContextSkia.h"
#include "PlatformKeyboardEvent.h"
#include "PlatformMouseEvent.h"
#include "PluginDebug.h"
#include "PluginMainThreadScheduler.h"
#include "PluginPackage.h"
#include "RenderLayer.h"
#include "ScriptController.h"
#include "Settings.h"
#include "Touch.h"
#include "TouchEvent.h"
#include "TouchList.h"
#include "WheelEvent.h"
#include "npruntime_impl.h"
#include "runtime_root.h"

#if USE(ACCELERATED_COMPOSITING)
#include "Chrome.h"
#include "ChromeClient.h"
#include "LayerProxyGLES2.h"
#endif

#include <BlackBerryPlatformGraphics.h>
#include <pthread.h>
#include <runtime/JSLock.h>
#include <runtime/JSValue.h>
#include <sys/keycodes.h>

#define UNINIT_COORD 0xffffffff
static unsigned int s_counter = 0;

// -------
class PthreadMutexLocker {
public:
    PthreadMutexLocker(pthread_mutex_t* mutex): m_mutex(mutex)
    {
        pthread_mutex_lock(m_mutex);
    }

    ~PthreadMutexLocker()
    {
        pthread_mutex_unlock(m_mutex);
    }

    pthread_mutex_t* m_mutex;
};

#if USE(ACCELERATED_COMPOSITING)
struct HolePunchData {
    int x, y;
    int w, h;
    RefPtr<WebCore::PlatformLayer> layer;
};

void npSetHolePunchHandler(void *data)
{
    HolePunchData *d = static_cast<HolePunchData*>(data);
    if (d->layer)
        d->layer->setHolePunchRect(WebCore::IntRect(d->x, d->y, d->w, d->h));
    delete d;
}
#endif

// --------
namespace WebCore {

static NPRect toNPRect(const IntRect& rect)
{
    NPRect npRect;
    npRect.top = rect.y();
    npRect.left = rect.x();
    npRect.bottom = rect.y() + rect.height();
    npRect.right = rect.x() + rect.width();
    return npRect;
}

class PluginViewPrivate {
public:
    PluginView* m_view;

public:
    pthread_mutex_t m_backBufferMutex;
    pthread_mutex_t m_frontBufferMutex;

    Olympia::Platform::Graphics::Buffer* m_pluginBuffers[2];
    IntSize m_pluginBufferSize;
    Olympia::Platform::Graphics::BufferType m_pluginBufferType;
    int m_pluginFrontBuffer;

    IntRect m_holePunchRect;
    IntRect m_keepVisibleRect;

#if USE(ACCELERATED_COMPOSITING)
    RefPtr<PlatformLayer> m_platformLayer;
#endif

    bool m_idlePrevented;
    bool m_hasPendingGeometryChange;
    bool m_sentOnLoad;
    bool m_isFullScreen;
    bool m_isFocused;
    bool m_orientationLocked;
    bool m_isBackgroundPlaying;

    std::string m_pluginUniquePrefix;

    IntRectRegion m_invalidateRegion;

public:
    PluginViewPrivate(PluginView* view):
        m_view(view)
        , m_pluginBufferType(Olympia::Platform::Graphics::PluginBufferWithAlpha)
        , m_pluginFrontBuffer(0)
        , m_idlePrevented(false)
        , m_hasPendingGeometryChange(true)
        , m_sentOnLoad(false)
        , m_isFullScreen(false)
        , m_isFocused(false)
        , m_orientationLocked(false)
        , m_isBackgroundPlaying(false)
    {
        m_pluginBuffers[0] = 0;
        m_pluginBuffers[1] = 0;

        pthread_mutexattr_t mutexAttributes;
        pthread_mutexattr_init(&mutexAttributes);
        pthread_mutexattr_settype(&mutexAttributes, PTHREAD_MUTEX_RECURSIVE);

        pthread_mutex_init(&m_backBufferMutex, &mutexAttributes);
        pthread_mutex_init(&m_frontBufferMutex, &mutexAttributes);

        pthread_mutexattr_destroy(&mutexAttributes);

        char uniqueId[50];
        snprintf(uniqueId, 50, "PluginViewBB-%08x%08x-", s_counter++, (int)this);
        m_pluginUniquePrefix = uniqueId;
    }

    ~PluginViewPrivate()
    {
#if USE(ACCELERATED_COMPOSITING)
        if (m_platformLayer)
            m_platformLayer->setPluginView(0);
#endif

        destroyBuffers();

        pthread_mutex_destroy(&m_backBufferMutex);
        pthread_mutex_destroy(&m_frontBufferMutex);
    }

    Olympia::Platform::Graphics::BufferType toBufferType(NPSurfaceFormat format)
    {
        switch (format) {
        case FORMAT_RGB_565:
            return Olympia::Platform::Graphics::PluginBuffer;
        case FORMAT_RGBA_8888:
        default:
            return Olympia::Platform::Graphics::PluginBufferWithAlpha;
        }
    }

    void setZoomFactor(float zoomFactor)
    {
        if (((NPSetWindowCallbackStruct*)m_view->m_npWindow.ws_info)->zoomFactor != zoomFactor)
            m_hasPendingGeometryChange = true;

        ((NPSetWindowCallbackStruct*)m_view->m_npWindow.ws_info)->zoomFactor = zoomFactor;
    }

    void setVisibleRects(const NPRect rects[], int32_t count)
    {
        m_keepVisibleRect = IntRect();

        if (!m_view->parent() || !count)
            return;

        for (int i = 0; i < count; i++) {
            IntRect addRect = IntRect(rects[i].left, rects[i].top, rects[i].right - rects[i].left, rects[i].bottom - rects[i].top);
            m_keepVisibleRect.unite(addRect);
        }

        // Don't cause a possible scroll if the result is an empty rectangle.
        if (m_keepVisibleRect.isEmpty())
            return;

        // Adjust the rect to the parent window and then adjust for scrolling.
        m_keepVisibleRect = m_view->convertToContainingWindow(m_keepVisibleRect);
        FrameView* frameView = static_cast<FrameView*>(m_view->parent());
        m_keepVisibleRect.move(frameView->scrollPosition().x(), frameView->scrollPosition().y());

        frameView->hostWindow()->platformPageClient()->ensureContentVisible();
    }

    void clearVisibleRects()
    {
        if (!m_keepVisibleRect.isEmpty())
            setVisibleRects(0, 0);
    }

    void showKeyboard(bool value)
    {
        FrameView* frameView = static_cast<FrameView*>(m_view->parent());
        frameView->hostWindow()->platformPageClient()->showVirtualKeyboard(value);
    }

    void requestFullScreen()
    {
        if (FrameView* frameView = static_cast<FrameView*>(m_view->parent()))
            if (frameView->hostWindow()->platformPageClient()->shouldPluginEnterFullScreen(m_view, m_pluginUniquePrefix.c_str()))
                m_view->handleFullScreenAllowedEvent();
    }

    void exitFullScreen()
    {
        m_view->handleFullScreenExitEvent();
    }

    void requestCenterFitZoom()
    {
        FrameView* frameView = static_cast<FrameView*>(m_view->parent());

        if (!frameView)
            return;

        frameView->hostWindow()->platformPageClient()->zoomToContentRect(m_view->m_windowRect);
    }

    void lockOrientation(bool landscape)
    {
        FrameView* frameView = static_cast<FrameView*>(m_view->parent());

        if (!frameView)
            return;

        frameView->hostWindow()->platformPageClient()->lockOrientation(landscape);
        m_orientationLocked = true;
    }

    void unlockOrientation()
    {
        if (!m_orientationLocked)
            return;

        FrameView* frameView = static_cast<FrameView*>(m_view->parent());

        if (!frameView)
            return;

        frameView->hostWindow()->platformPageClient()->unlockOrientation();
        m_orientationLocked = false;
    }

    void preventIdle(bool preventIdle)
    {
        FrameView* frameView = static_cast<FrameView*>(m_view->parent());

        if (preventIdle == m_idlePrevented)
            return;

        if (!frameView)
            return;

        frameView->hostWindow()->platformPageClient()->setPreventsScreenDimming(preventIdle);
        m_idlePrevented = preventIdle;
    }

    void setHolePunch(int x, int y, int width, int height)
    {
        if (width > 0 && height > 0)
            m_holePunchRect = IntRect(x, y, width, height);
        else
            m_holePunchRect = IntRect();

        // Clip the hole punch rectangle in case a plugin is 'overzealous', or 
        // does not clean up the hole punch rectangle correctly when exiting 
        // fullscreen (will mask bugs in the plugin).
        m_holePunchRect.intersect(IntRect(IntPoint(0, 0), m_view->frameRect().size()));

#if USE(ACCELERATED_COMPOSITING)
        if (m_platformLayer) {
            IntRect rect = m_holePunchRect;

            // Translate from plugin coordinates to contents coordinates.
            if (!m_holePunchRect.isEmpty())
                rect.move(m_view->frameRect().x(), m_view->frameRect().y());

            HolePunchData *hp = new HolePunchData;
            hp->x = m_holePunchRect.x();
            hp->y = m_holePunchRect.y();
            hp->w = m_holePunchRect.width();
            hp->h = m_holePunchRect.height();
            hp->layer = m_platformLayer;

            // Notify compositing layer and page client in order to be able to
            // punch through composited and non-composited parts of the page.
            callOnMainThread(npSetHolePunchHandler, hp);
        }
#endif
    }

    NPSurface lockBackBuffer()
    {
        pthread_mutex_lock(&m_backBufferMutex);
        Olympia::Platform::Graphics::Buffer* buffer =
            m_pluginBuffers[(m_pluginFrontBuffer + 1) % 2];

        if (!buffer || !Olympia::Platform::Graphics::platformBufferHandle(buffer)) {
            unlockBackBuffer();
            return 0;
        }

        return Olympia::Platform::Graphics::platformBufferHandle(buffer);
    }

    void unlockBackBuffer()
    {
        pthread_mutex_unlock(&m_backBufferMutex);
    }

    Olympia::Platform::Graphics::Buffer* lockReadFrontBufferInternal()
    {
        pthread_mutex_lock(&m_frontBufferMutex);
        return m_pluginBuffers[m_pluginFrontBuffer];
    }

    NPSurface lockReadFrontBuffer()
    {
        Olympia::Platform::Graphics::Buffer* buffer = lockReadFrontBufferInternal();

        if (!buffer || !Olympia::Platform::Graphics::platformBufferHandle(buffer)) {
            unlockReadFrontBuffer();
            return 0;
        }

        return Olympia::Platform::Graphics::platformBufferHandle(buffer);
    }

    void unlockReadFrontBuffer()
    {
        pthread_mutex_unlock(&m_frontBufferMutex);
    }

    void swapBuffers()
    {
        PthreadMutexLocker backLock(&m_backBufferMutex);
        PthreadMutexLocker frontLock(&m_frontBufferMutex);

        m_pluginFrontBuffer = (m_pluginFrontBuffer + 1) % 2;
    }

    bool createBuffers(NPSurfaceFormat format, int width, int height)
    {
        bool success = true;

        PthreadMutexLocker backLock(&m_backBufferMutex);
        PthreadMutexLocker frontLock(&m_frontBufferMutex);

        for (int i = 0; i < 2; i++) {
            if (m_pluginBuffers[i]) {
                Olympia::Platform::Graphics::destroyBuffer(m_pluginBuffers[i]);
                m_pluginBuffers[i] = 0;
            }

            if (width <= 0 || height <= 0)
                success = true;
            else {
                m_pluginBuffers[i] = Olympia::Platform::Graphics::createBuffer(
                    Olympia::Platform::IntSize(width, height),
                    toBufferType(format));

                if (!m_pluginBuffers[i])
                    success = false;
            }
        }

        if (success) {
            m_pluginBufferSize = IntSize(width, height);
            m_pluginBufferType = toBufferType(format);
        }
        else {
            m_pluginBufferSize = IntSize();
            m_pluginBufferType = Olympia::Platform::Graphics::PluginBufferWithAlpha;
            destroyBuffers();
        }

        return success;
    }

    bool resizeBuffers(NPSurfaceFormat format, int width, int height)
    {
        bool success = true;

        // If there is no buffer created, then try to create it.
        if (m_pluginBufferSize.width() == 0 || m_pluginBufferSize.height() == 0)
            return createBuffers(format, width, height);

        if (width == 0 || height == 0)
            return destroyBuffers();

        PthreadMutexLocker backLock(&m_backBufferMutex);
        PthreadMutexLocker frontLock(&m_frontBufferMutex);

        for (int i = 0; (i < 2) && success; i++) {
            success &= Olympia::Platform::Graphics::reallocBuffer(m_pluginBuffers[i],
                Olympia::Platform::IntSize(width, height),
                toBufferType(format));
        }

        if (success) {
            m_pluginBufferSize = IntSize(width, height);
            m_pluginBufferType = toBufferType(format);
            return true;
        }

        // Attempt to undo if we failed to get the new size/format. We can't guarantee
        // we will get it back though.
        if (m_pluginBufferSize.width() == 0 || m_pluginBufferSize.height() == 0) {
            destroyBuffers();
            return false;
        }

        bool undone = true;
        for (int i = 0; i < 2; i++) {
            undone &= Olympia::Platform::Graphics::reallocBuffer(m_pluginBuffers[i],
                m_pluginBufferSize, m_pluginBufferType);
        }

        // If we fail to undo, delete the buffers altogether.
        if (!undone)
            destroyBuffers();

        return false;
    }

    bool destroyBuffers()
    {
        PthreadMutexLocker backLock(&m_backBufferMutex);
        PthreadMutexLocker frontLock(&m_frontBufferMutex);

        for (int i = 0; i < 2; i++) {
            if (m_pluginBuffers[i]) {
                Olympia::Platform::Graphics::destroyBuffer(m_pluginBuffers[i]);
                m_pluginBuffers[i] = 0;
            }
        }
        m_pluginBufferSize = IntSize();

        return true;
    }
};

} // namespace WebCore

// -------

extern "C" {

namespace {
    ////////////////////////////////////////////////////////////////////////////////
    // Static NPAPI Callbacks
    ////////////////////////////////////////////////////////////////////////////////
    void setVisibleRects(NPP instance, const NPRect rects[], int32_t count)
    {
        WebCore::PluginView* pv = (WebCore::PluginView *)instance->ndata;
        WebCore::PluginViewPrivate* priv = pv->getPrivate();
        priv->setVisibleRects(rects, count);
    }

    void clearVisibleRects(NPP instance)
    {
        WebCore::PluginView* pv = (WebCore::PluginView *)instance->ndata;
        WebCore::PluginViewPrivate* priv = pv->getPrivate();
        priv->clearVisibleRects();
    }

    void showKeyboard(NPP instance, bool value)
    {
        WebCore::PluginView* pv = (WebCore::PluginView *)instance->ndata;
        WebCore::PluginViewPrivate* priv = pv->getPrivate();
        priv->showKeyboard(value);
    }

    void requestFullScreen(NPP instance)
    {
        WebCore::PluginView* pv = (WebCore::PluginView *)instance->ndata;
        WebCore::PluginViewPrivate* priv = pv->getPrivate();
        priv->requestFullScreen();
    }

    void exitFullScreen(NPP instance)
    {
        WebCore::PluginView* pv = (WebCore::PluginView *)instance->ndata;
        WebCore::PluginViewPrivate* priv = pv->getPrivate();
        priv->exitFullScreen();
    }

    void requestCenterFitZoom(NPP instance)
    {
        WebCore::PluginView* pv = (WebCore::PluginView *)instance->ndata;
        WebCore::PluginViewPrivate* priv = pv->getPrivate();
        priv->requestCenterFitZoom();
    }

    void lockOrientation(NPP instance, bool landscape)
    {
        WebCore::PluginView* pv = (WebCore::PluginView *)instance->ndata;
        WebCore::PluginViewPrivate* priv = pv->getPrivate();
        priv->lockOrientation(landscape);
    }

    void unlockOrientation(NPP instance)
    {
        WebCore::PluginView* pv = (WebCore::PluginView *)instance->ndata;
        WebCore::PluginViewPrivate* priv = pv->getPrivate();
        priv->unlockOrientation();
    }

    void preventIdle(NPP instance, bool preventIdle)
    {
        WebCore::PluginView* pv = (WebCore::PluginView *)instance->ndata;
        WebCore::PluginViewPrivate* priv = pv->getPrivate();
        priv->preventIdle(preventIdle);
    }

    NPSurface lockBackBuffer(NPP instance)
    {
        WebCore::PluginView* pv = (WebCore::PluginView *)instance->ndata;
        WebCore::PluginViewPrivate* priv = pv->getPrivate();
        return priv->lockBackBuffer();
    }

    void unlockBackBuffer(NPP instance)
    {
        WebCore::PluginView* pv = (WebCore::PluginView *)instance->ndata;
        WebCore::PluginViewPrivate* priv = pv->getPrivate();
        priv->unlockBackBuffer();
    }

    NPSurface lockReadFrontBuffer(NPP instance)
    {
        WebCore::PluginView* pv = (WebCore::PluginView *)instance->ndata;
        WebCore::PluginViewPrivate* priv = pv->getPrivate();
        return priv->lockReadFrontBuffer();
    }

    void unlockReadFrontBuffer(NPP instance)
    {
        WebCore::PluginView* pv = (WebCore::PluginView *)instance->ndata;
        WebCore::PluginViewPrivate* priv = pv->getPrivate();
        priv->unlockReadFrontBuffer();
    }

    void swapBuffers(NPP instance)
    {
        WebCore::PluginView* pv = (WebCore::PluginView *)instance->ndata;
        WebCore::PluginViewPrivate* priv = pv->getPrivate();
        priv->swapBuffers();
    }

    bool createBuffers(NPP instance, NPSurfaceFormat format, int width, int height)
    {
        WebCore::PluginView* pv = (WebCore::PluginView *)instance->ndata;
        WebCore::PluginViewPrivate* priv = pv->getPrivate();
        return priv->createBuffers(format, width, height);
    }

    bool destroyBuffers(NPP instance)
    {
        WebCore::PluginView* pv = (WebCore::PluginView *)instance->ndata;
        WebCore::PluginViewPrivate* priv = pv->getPrivate();
        return priv->destroyBuffers();
    }

    bool resizeBuffers(NPP instance, NPSurfaceFormat format, int width, int height)
    {
        WebCore::PluginView* pv = (WebCore::PluginView *)instance->ndata;
        WebCore::PluginViewPrivate* priv = pv->getPrivate();
        return priv->resizeBuffers(format, width, height);
    }

    void setHolePunch(NPP instance, int x, int y, int width, int height)
    {
        WebCore::PluginView* pv = (WebCore::PluginView *)instance->ndata;
        WebCore::PluginViewPrivate* priv = pv->getPrivate();
        priv->setHolePunch(x, y, width, height);
    }

    NPCallbacks s_NpCallbacks =
    {
        setVisibleRects,
        clearVisibleRects,
        showKeyboard,
        requestFullScreen,
        exitFullScreen,
        requestCenterFitZoom,
        lockOrientation,
        unlockOrientation,
        preventIdle,
        lockBackBuffer,
        unlockBackBuffer,
        lockReadFrontBuffer,
        unlockReadFrontBuffer,
        swapBuffers,
        createBuffers,
        destroyBuffers,
        resizeBuffers,
        setHolePunch
    };
} // anonymous namespace
} // extern "C"

// ------- Standard plugin widget stuff.

using JSC::ExecState;
using JSC::Interpreter;
using JSC::JSLock;
using JSC::JSObject;
using JSC::UString;

using namespace std;

using namespace WTF;

namespace WebCore {

using namespace HTMLNames;

void PluginView::updatePluginWidget()
{
    if (!parent() || !m_private)
        return;

    ASSERT(parent()->isFrameView());
    FrameView* frameView = static_cast<FrameView*>(parent());

    IntRect oldWindowRect = m_windowRect;
    IntRect oldClipRect = m_clipRect;

    m_windowRect = IntRect(frameView->contentsToWindow(frameRect().location()), frameRect().size());

    // Map rect to content coordinate space of main frame.
    m_windowRect.move(root()->scrollOffset());

    m_clipRect = calculateClipRect();
    IntRect f = frameRect();

    // Notify the plugin if it may or may not be on/offscreen.
    handleScrollEvent();

    bool zoomFactorChanged = ((NPSetWindowCallbackStruct*)m_npWindow.ws_info)->zoomFactor
        != frameView->hostWindow()->platformPageClient()->currentZoomFactor();

    if (!zoomFactorChanged && m_windowRect == oldWindowRect && m_clipRect == oldClipRect)
        return;

    // do not call setNPWindowIfNeeded immediately, will be called on paint()
    m_private->m_hasPendingGeometryChange = true;

    // (i) in order to move/resize the plugin window at the same time as the
    // rest of frame during e.g. scrolling, we set the window geometry
    // in the paint() function, but as paint() isn't called when the
    // plugin window is outside the frame which can be caused by a
    // scroll, we need to move/resize immediately.
    // (ii) if we are running layout tests from DRT, paint() won't ever get called
    // so we need to call setNPWindowIfNeeded() if window geometry has changed
    if (!m_windowRect.intersects(frameView->frameRect())
        || (platformPluginWidget() && (m_windowRect != oldWindowRect || m_clipRect != oldClipRect || zoomFactorChanged)))
        setNPWindowIfNeeded();

    // Make sure we get repainted afterwards. This is necessary for downward
    // scrolling to move the plugin widget properly.
    invalidate();
}

void PluginView::setFocus(bool focused)
{
    if (!m_private || m_private->m_isFocused == focused)
        return;

    ASSERT(platformPluginWidget() == platformWidget());
    Widget::setFocus(focused);

    if (focused)
        handleFocusInEvent();
    else
        handleFocusOutEvent();
}

void PluginView::show()
{
    setSelfVisible(true);
    Widget::show();
    updatePluginWidget();
}

void PluginView::hide()
{
    setSelfVisible(false);
    Widget::hide();
    updatePluginWidget();
}

void PluginView::updateBuffer(const IntRect& bufferRect)
{
    if (!m_private)
        return;

    // Update the zoom factor here, it happens right before setNPWindowIfNeeded
    // ensuring that the plugin has every opportunity to get the zoom factor before
    // it paints anything.
    if (FrameView* frameView = static_cast<FrameView*>(parent()))
        m_private->setZoomFactor(frameView->hostWindow()->platformPageClient()->currentZoomFactor());

    setNPWindowIfNeeded();

    // Build and dispatch an event to the plugin to notify it we are about to draw whatever
    // is in the front buffer. This is it's opportunity to do a swap.
    IntRect exposedRect(bufferRect);
    exposedRect.intersect(IntRect(IntPoint(0, 0), frameRect().size()));

    // Only ask the plugin to draw if the exposed rect was explicitly invalidated
    // by the plugin.
    IntRectRegion exposedRegion = IntRectRegion::intersectRegions(m_private->m_invalidateRegion, exposedRect);
    if (!exposedRegion.isEmpty()) {
        m_private->m_invalidateRegion = IntRectRegion::subtractRegions(m_private->m_invalidateRegion, exposedRegion);
        Vector<IntRect> exposedRects = exposedRegion.rects();
        for (unsigned i = 0; i < exposedRects.size(); ++i) {
            NPDrawEvent draw;
            draw.pluginRect = toNPRect(m_windowRect);
            draw.clipRect = toNPRect(m_clipRect);
            draw.drawRect = (NPRect*)alloca(sizeof(NPRect));
            draw.drawRectCount = 1;
            *draw.drawRect = toNPRect(exposedRects.at(i));
            draw.zoomFactor = ((NPSetWindowCallbackStruct*)m_npWindow.ws_info)->zoomFactor;

            NPEvent npEvent;
            npEvent.type = NP_DrawEvent;
            npEvent.data = &draw;

            // TODO: Return early if this fails? Or just repaint?
            dispatchNPEvent(npEvent);
        }
    }
}

void PluginView::paint(GraphicsContext* context, const IntRect& rect)
{
    if (!m_private || !m_isStarted) {
        paintMissingPluginIcon(context, rect);
        return;
    }

    // Update the zoom factor here, it happens right before setNPWindowIfNeeded
    // ensuring that the plugin has every opportunity to get the zoom factor before
    // it paints anything.
    if (FrameView* frameView = static_cast<FrameView*>(parent()))
        m_private->setZoomFactor(frameView->hostWindow()->platformPageClient()->currentZoomFactor());

    if (context->paintingDisabled())
        return;

    setNPWindowIfNeeded();

#if USE(ACCELERATED_COMPOSITING)
    if (m_private->m_platformLayer)
        return;
#endif

    // Build and dispatch an event to the plugin to notify it we are about to draw whatever
    // is in the front buffer. This is it's opportunity to do a swap.
    IntRect rectClip(rect);
    rectClip.intersect(frameRect());

    IntRect exposedRect(rectClip);
    exposedRect.move(-frameRect().x(), -frameRect().y());

    updateBuffer(exposedRect);

    PthreadMutexLocker frontLock(&m_private->m_frontBufferMutex);

    Olympia::Platform::Graphics::Buffer* frontBuffer =
        m_private->m_pluginBuffers[m_private->m_pluginFrontBuffer];

    // Don't paint anything if there is no buffer.
    if (!frontBuffer)
        return;

    const Olympia::Platform::Graphics::BackingImage* backingImage =
        Olympia::Platform::Graphics::lockBufferBackingImage(frontBuffer,
                                                            Olympia::Platform::Graphics::ReadAccess);
    if (!backingImage)
        return;

    // Draw the changed buffer contents to the screen.
    context->save();

#if PLATFORM(SKIA)
    const SkBitmap& pluginImage = *backingImage;
    PlatformGraphicsContext* graphics = context->platformContext();
    ASSERT(graphics);
    SkCanvas* canvas = graphics->canvas();

    // Source rectangle we will draw to the screen.
    SkIRect skSrcRect;
    skSrcRect.set(exposedRect.x(), exposedRect.y(),
                  exposedRect.x() + exposedRect.width(),
                  exposedRect.y() + exposedRect.height());

    // Prepare the hole punch rectangle.
    SkIRect unclippedHolePunchRect;
    unclippedHolePunchRect.set(m_private->m_holePunchRect.x(),
                               m_private->m_holePunchRect.y(),
                               m_private->m_holePunchRect.x() + m_private->m_holePunchRect.width(),
                               m_private->m_holePunchRect.y() + m_private->m_holePunchRect.height());

    // holePunchRect is clipped.
    SkIRect holePunchRect;

    // All source rectangles are scaled by the zoom factor because the source bitmap may be a
    // higher resolution than the 1:1 page. This allows the flash player to scale the content
    // it is drawing to match the scale of the page.
    double zoomFactorH = (double)m_private->m_pluginBufferSize.width() / (double)frameRect().width();
    double zoomFactorW = (double)m_private->m_pluginBufferSize.height() / (double)frameRect().height();
    double zoomFactor = (zoomFactorH + zoomFactorW) / 2.0;

    // This method draws a hole if specified.
    if (!m_private->m_holePunchRect.isEmpty()
        && holePunchRect.intersect(unclippedHolePunchRect, skSrcRect)) {

        // Draw the top chunk if needed.
        if (holePunchRect.fTop > skSrcRect.fTop) {
            SkIRect srcRect;
            srcRect.set(skSrcRect.fLeft * zoomFactor, skSrcRect.fTop * zoomFactor,
                        skSrcRect.fRight * zoomFactor, holePunchRect.fTop * zoomFactor);

            SkRect dstRect;
            dstRect.set(skSrcRect.fLeft, skSrcRect.fTop,
                        skSrcRect.fRight, holePunchRect.fTop);
            dstRect.offset(frameRect().x(), frameRect().y());

            canvas->drawBitmapRect(pluginImage, &srcRect, dstRect);
        }

        // Draw the left chunk if needed.
        if (holePunchRect.fLeft > skSrcRect.fLeft) {
            SkIRect srcRect;
            srcRect.set(skSrcRect.fLeft * zoomFactor, holePunchRect.fTop * zoomFactor,
                        holePunchRect.fLeft * zoomFactor, holePunchRect.fBottom * zoomFactor);

            SkRect dstRect;
            dstRect.set(skSrcRect.fLeft, holePunchRect.fTop,
                        holePunchRect.fLeft, holePunchRect.fBottom);
            dstRect.offset(frameRect().x(), frameRect().y());

            canvas->drawBitmapRect(pluginImage, &srcRect, dstRect);
        }

        // Draw the hole chunk.
        {
            SkPaint paint;
            paint.setXfermodeMode(SkXfermode::kSrc_Mode);

            SkIRect srcRect;
            srcRect.set(holePunchRect.fLeft * zoomFactor, holePunchRect.fTop * zoomFactor,
                        holePunchRect.fRight * zoomFactor, holePunchRect.fBottom * zoomFactor);

            SkRect dstRect;
            dstRect.set(holePunchRect.fLeft, holePunchRect.fTop,
                        holePunchRect.fRight, holePunchRect.fBottom);
            dstRect.offset(frameRect().x(), frameRect().y());

            canvas->drawBitmapRect(pluginImage, &srcRect, dstRect, &paint);
        }

        // Draw the right chunk if needed.
        if (holePunchRect.fRight < skSrcRect.fRight) {
            SkIRect srcRect;
            srcRect.set(holePunchRect.fRight * zoomFactor, holePunchRect.fTop * zoomFactor,
                        skSrcRect.fRight * zoomFactor, holePunchRect.fBottom * zoomFactor);

            SkRect dstRect;
            dstRect.set(holePunchRect.fRight, holePunchRect.fTop,
                        skSrcRect.fRight, holePunchRect.fBottom);
            dstRect.offset(frameRect().x(), frameRect().y());

            canvas->drawBitmapRect(pluginImage, &srcRect, dstRect);
        }

        // Draw the bottom chunk if needed.
        if (holePunchRect.fBottom < skSrcRect.fBottom) {
            SkIRect srcRect;
            srcRect.set(skSrcRect.fLeft * zoomFactor, holePunchRect.fBottom * zoomFactor,
                        skSrcRect.fRight * zoomFactor, skSrcRect.fBottom * zoomFactor);

            SkRect dstRect;
            dstRect.set(skSrcRect.fLeft, holePunchRect.fBottom,
                        skSrcRect.fRight, skSrcRect.fBottom);
            dstRect.offset(frameRect().x(), frameRect().y());

            canvas->drawBitmapRect(pluginImage, &srcRect, dstRect);
        }
    } else {
        SkIRect srcRect;
        srcRect.set(skSrcRect.fLeft * zoomFactor, skSrcRect.fTop * zoomFactor,
                    skSrcRect.fRight * zoomFactor, skSrcRect.fBottom * zoomFactor);

        // Calculate the destination rectangle.
        SkRect dstRect;
        dstRect.set(rectClip.x(), rectClip.y(),
                    rectClip.x() + rectClip.width(),
                    rectClip.y() + rectClip.height());

        // Don't punch a hole.
        canvas->drawBitmapRect(pluginImage, &srcRect, dstRect);
    }
#else
#warning "Implement me!"
    ASSERT_NOT_REACHED();
#endif

    context->restore();

    Olympia::Platform::Graphics::releaseBufferBackingImage(frontBuffer);
}


bool PluginView::dispatchFullScreenNPEvent(NPEvent& event)
{
    if (!m_private)
        return false;

    ASSERT(m_private->m_isFullScreen);
    return dispatchNPEvent(event);
}

// TODO: Unify across ports.
bool PluginView::dispatchNPEvent(NPEvent& event)
{
    // sanity check.
    if (!m_plugin || !m_plugin->pluginFuncs()->event)
        return false;

    PluginView::setCurrentPluginView(this);
    JSC::JSLock::DropAllLocks dropAllLocks(JSC::SilenceAssertionsOnly);
    setCallingPlugin(true);

    bool accepted = m_plugin->pluginFuncs()->event(m_instance, &event);

    setCallingPlugin(false);
    PluginView::setCurrentPluginView(0);

    return accepted;
}

void PluginView::handleKeyboardEvent(KeyboardEvent* event)
{
    NPEvent npEvent;
    NPKeyboardEvent keyEvent;
    const PlatformKeyboardEvent *platformKeyEvent = event->keyEvent();

    keyEvent.modifiers = 0;

    if (platformKeyEvent->shiftKey())
        keyEvent.modifiers |= KEYMOD_SHIFT;

    if (platformKeyEvent->ctrlKey())
        keyEvent.modifiers |= KEYMOD_CTRL;

    if (platformKeyEvent->altKey())
        keyEvent.modifiers |= KEYMOD_ALT;

    if (platformKeyEvent->metaKey())
        keyEvent.modifiers |= KEYMOD_ALTGR;

    keyEvent.cap = platformKeyEvent->unmodifiedCharacter();
    keyEvent.sym = keyEvent.cap;
    keyEvent.scan = 0;
    keyEvent.flags = 0;

    if (platformKeyEvent->type() == PlatformKeyboardEvent::RawKeyDown
            || platformKeyEvent->type() == PlatformKeyboardEvent::KeyDown) {
        keyEvent.flags = KEY_DOWN | KEY_SYM_VALID | KEY_CAP_VALID;
    } else if ( platformKeyEvent->type() == PlatformKeyboardEvent::KeyUp)
        keyEvent.flags = KEY_SYM_VALID | KEY_CAP_VALID;

    npEvent.type = NP_KeyEvent;
    npEvent.data = &keyEvent;
    if (dispatchNPEvent(npEvent))
        event->setDefaultHandled();
}

void PluginView::handleWheelEvent(WheelEvent* event)
{
    if (!m_private)
        return;

    if (!m_private->m_isFocused)
        focusPluginElement();

    NPEvent npEvent;
    NPWheelEvent wheelEvent;

    wheelEvent.x = event->offsetX();
    wheelEvent.y = event->offsetY();

    wheelEvent.flags = 0;

    wheelEvent.xDelta = event->rawDeltaX();
    wheelEvent.yDelta = event->rawDeltaY();

    npEvent.type = NP_WheelEvent;
    npEvent.data = &wheelEvent;

    if (dispatchNPEvent(npEvent)) {
        event->setDefaultHandled();
        event->setPluginHandled();
    }
}

void PluginView::handleTouchEvent(TouchEvent* event)
{
    if (!m_private)
        return;

    if (!m_private->m_isFocused)
        focusPluginElement();

    NPTouchEvent npTouchEvent;

    if (event->isDoubleTap())
        npTouchEvent.type = TOUCH_EVENT_DOUBLETAP;
    else if (event->isTouchHold())
        npTouchEvent.type = TOUCH_EVENT_TOUCHHOLD;
    else if (event->type() == eventNames().touchstartEvent)
        npTouchEvent.type = TOUCH_EVENT_START;
    else if (event->type() == eventNames().touchendEvent)
        npTouchEvent.type = TOUCH_EVENT_END;
    else if (event->type() == eventNames().touchmoveEvent)
        npTouchEvent.type = TOUCH_EVENT_MOVE;
    else if (event->type() == eventNames().touchcancelEvent)
        npTouchEvent.type = TOUCH_EVENT_CANCEL;
    else
        return;

    TouchList* touchList;
    // The touches list is empty if in a touch end event. Use changedTouches instead.
    if (npTouchEvent.type == TOUCH_EVENT_DOUBLETAP || npTouchEvent.type == TOUCH_EVENT_END)
        touchList = event->changedTouches();
    else
        touchList = event->touches();

    npTouchEvent.points = 0;
    npTouchEvent.size = touchList->length();

    if (touchList->length()) {
        npTouchEvent.points = new NPTouchPoint[touchList->length()];
        for (unsigned i = 0; i < touchList->length(); i++) {
            Touch* touchItem = touchList->item(i);
            npTouchEvent.points[i].touchId = touchItem->identifier();
            npTouchEvent.points[i].clientX = touchItem->pageX() - frameRect().x();
            npTouchEvent.points[i].clientY = touchItem->pageY() - frameRect().y();
            npTouchEvent.points[i].screenX = touchItem->screenX();
            npTouchEvent.points[i].screenY = touchItem->screenY();
            npTouchEvent.points[i].pageX = touchItem->pageX();
            npTouchEvent.points[i].pageY = touchItem->pageY();

        }
    }

    NPEvent npEvent;
    npEvent.type = NP_TouchEvent;
    npEvent.data = &npTouchEvent;

    if (dispatchNPEvent(npEvent)) {
        event->setDefaultHandled();
        event->setPluginHandled();
    } else if (npTouchEvent.type == TOUCH_EVENT_DOUBLETAP) {
        // Send Touch Up if double tap not consumed
        npTouchEvent.type = TOUCH_EVENT_END;
        npEvent.data = &npTouchEvent;
        if (dispatchNPEvent(npEvent)) {
            event->setDefaultHandled();
            event->setPluginHandled();
        }
    }

    delete[] npTouchEvent.points;
}

void PluginView::handleMouseEvent(MouseEvent* event)
{
    if (!m_private)
        return;

    if (!m_private->m_isFocused)
        focusPluginElement();

    NPEvent npEvent;
    NPMouseEvent mouseEvent;

    mouseEvent.x = event->offsetX();
    mouseEvent.y = event->offsetY();

    if (event->type() == eventNames().mousedownEvent)
        mouseEvent.type = MOUSE_BUTTON_DOWN;
    else if (event->type() == eventNames().mousemoveEvent)
        mouseEvent.type = MOUSE_MOTION;
    else if (event->type() == eventNames().mouseoutEvent)
        mouseEvent.type = MOUSE_OUTBOUND;
    else if (event->type() == eventNames().mouseoverEvent)
        mouseEvent.type = MOUSE_OVER;
    else if (event->type() == eventNames().mouseupEvent)
        mouseEvent.type = MOUSE_BUTTON_UP;
    else
        return;

    mouseEvent.button = event->button();
    mouseEvent.flags = 0;

    npEvent.type = NP_MouseEvent;
    npEvent.data = &mouseEvent;

    if (dispatchNPEvent(npEvent)) {
        event->setDefaultHandled();
        event->setPluginHandled();
    }
}

void PluginView::handleFocusInEvent()
{
    if (!m_private)
        return;

    if (m_private->m_isFocused)
        return;

    m_private->m_isFocused = true;

    NPEvent npEvent;
    npEvent.type = NP_FocusGainedEvent;
    npEvent.data = 0;
    dispatchNPEvent(npEvent);
}

void PluginView::handleFocusOutEvent()
{
    if (!m_private)
        return;

    if (!m_private->m_isFocused)
        return;

    m_private->m_isFocused = false;

    NPEvent npEvent;
    npEvent.type = NP_FocusLostEvent;
    npEvent.data = 0;
    dispatchNPEvent(npEvent);
}

void PluginView::handlePauseEvent()
{
    NPEvent npEvent;
    npEvent.type = NP_PauseEvent;
    npEvent.data = 0;
    dispatchNPEvent(npEvent);
}

void PluginView::handleResumeEvent()
{
    NPEvent npEvent;
    npEvent.type = NP_ResumeEvent;
    npEvent.data = 0;
    dispatchNPEvent(npEvent);
}

void PluginView::handleScrollEvent()
{
    NPEvent npEvent;
    npEvent.type = m_clipRect.isEmpty() ? NP_OffScreenEvent : NP_OnScreenEvent;
    npEvent.data = 0;
    dispatchNPEvent(npEvent);
}

IntRect PluginView::calculateClipRect() const
{
    FrameView* frameView = static_cast<FrameView*>(parent());
    bool visible = frameView && isVisible();

    // As a special case, if the frameView extent in either dimension is
    // empty, then set visible to true.  This is important for sites like
    // picnik.com which use a hidden iframe (read: width = 0 and height = 0)
    // with an embedded swf to execute some javascript.  Unless we send an
    // on screen event the swf will not execute the javascript and the real
    // site will never load.
    if (frameView && (!frameView->width() || !frameView->height()))
        visible = true;
    else if (visible && frameView->width() && frameView->height()) {
        IntSize windowSize = frameView->hostWindow()->platformPageClient()->viewportSize();

        // Get the clipped rectangle for this player within the current frame.
        IntRect visibleContentRect;
        IntRect contentRect = m_element->renderer()->absoluteClippedOverflowRect();
        FloatPoint contentLocal = m_element->renderer()->absoluteToLocal(FloatPoint(contentRect.location()));

        contentRect.setLocation(roundedIntPoint(contentLocal));
        contentRect.move(frameRect().x(), frameRect().y());

        // Clip against any frames that the widget is inside. Note that if the frames are also clipped
        // by a div, that will not be included in this calculation. That is an improvement that still
        // needs to be made.
        const Widget* current = this;
        while (current->parent() && visible) {
            // Determine if it is visible in this scrollview.
            visibleContentRect = current->parent()->visibleContentRect();

            // Special case for the root ScrollView. Its size does not match the actual window size.
            if (current->parent() == root())
                visibleContentRect.setSize(windowSize);

            contentRect.intersect(visibleContentRect);
            visible = !contentRect.isEmpty();

            // Offset to visible coordinates in scrollview widget's coordinate system (except in the case of
            // the top scroll view).
            if (current->parent()->parent())
                contentRect.move(-visibleContentRect.x(), -visibleContentRect.y());

            current = current->parent();

            // Don't include the offset for the root window or we get the wrong coordinates.
            if (current->parent()) {
                // Move content rect into the parent scrollview's coordinates.
                IntRect curFrameRect = current->frameRect();
                contentRect.move(curFrameRect.x(), curFrameRect.y());
            }
        }

        return contentRect;
    }

    return IntRect();
}

void PluginView::handleOnLoadEvent()
{
    if (!m_private)
        return;

    if (m_private->m_sentOnLoad)
        return;

    m_private->m_sentOnLoad = true;

    NPEvent npEvent;
    npEvent.type = NP_OnLoadEvent;
    npEvent.data = 0;

    dispatchNPEvent(npEvent);

    // Send an initial OnScreen/OffScreen event. It must always come after onLoad is sent.
    handleScrollEvent();
}

void PluginView::handleFreeMemoryEvent()
{
    NPEvent npEvent;
    npEvent.type = NP_FreeMemoryEvent;
    npEvent.data = 0;

    dispatchNPEvent(npEvent);
}

void PluginView::handleBackgroundEvent()
{
    NPEvent npEvent;
    npEvent.type = NP_BackgroundEvent;
    npEvent.data = 0;

    dispatchNPEvent(npEvent);
}

void PluginView::handleForegroundEvent()
{
    NPEvent npEvent;
    npEvent.type = NP_ForegroundEvent;
    npEvent.data = 0;

    dispatchNPEvent(npEvent);
}

void PluginView::handleFullScreenAllowedEvent()
{
    if (!m_private)
        return;

    NPEvent npEvent;
    npEvent.type = NP_FullScreenReadyEvent;
    npEvent.data = 0;

    if (FrameView* frameView = static_cast<FrameView*>(parent())) {
        frameView->hostWindow()->platformPageClient()->didPluginEnterFullScreen(this, m_private->m_pluginUniquePrefix.c_str());

        if (!dispatchNPEvent(npEvent))
            frameView->hostWindow()->platformPageClient()->didPluginExitFullScreen(this, m_private->m_pluginUniquePrefix.c_str());
        else
            m_private->m_isFullScreen = true;
    }
}

void PluginView::handleFullScreenExitEvent()
{
    if (!m_private)
        return;

    NPEvent npEvent;
    npEvent.type = NP_FullScreenExitEvent;
    npEvent.data = 0;

    dispatchNPEvent(npEvent);

    if (FrameView* frameView = static_cast<FrameView*>(parent()))
        frameView->hostWindow()->platformPageClient()->didPluginExitFullScreen(this, m_private->m_pluginUniquePrefix.c_str());

    m_private->m_isFullScreen = false;
    invalidate();
}

void PluginView::handleIdleEvent(bool enterIdle)
{
    NPEvent npEvent;
    npEvent.data = 0;

    if (enterIdle)
        npEvent.type = NP_EnterIdleEvent;
    else
        npEvent.type = NP_ExitIdleEvent;

    dispatchNPEvent(npEvent);
}


void PluginView::handleAppActivatedEvent()
{
    NPEvent npEvent;
    npEvent.data = 0;
    npEvent.type = NP_AppActivatedEvent;

    dispatchNPEvent(npEvent);
}

void PluginView::handleAppDeactivatedEvent()
{
    if (!m_private)
        return;

    // Plugin wants to know that it has to exit fullscreen on deactivation.
    if (m_private->m_isFullScreen)
        handleFullScreenExitEvent();

    NPEvent npEvent;
    npEvent.data = 0;
    npEvent.type = NP_AppDeactivatedEvent;

    dispatchNPEvent(npEvent);
}

void PluginView::handleAppStandbyEvent()
{
    // TODO: This should send an QNP_AppStandbyEvent
    NPEvent npEvent;
    npEvent.data = 0;
    npEvent.type = NP_AppStandByEvent;

    dispatchNPEvent(npEvent);
}

void PluginView::handleOrientationEvent(int angle)
{
    NPEvent npEvent;
    npEvent.type = NP_OrientationEvent;
    npEvent.data = (void*)angle;

    dispatchNPEvent(npEvent);
}

void PluginView::handleSwipeEvent()
{
    if (!m_private)
        return;

    // Plugin only wants to know that it has to exit fullscreen.
    if (m_private->m_isFullScreen)
        handleFullScreenExitEvent();
}

void PluginView::handleScreenPowerEvent(bool powered)
{
    NPEvent npEvent;
    npEvent.data = 0;

    if (powered)
        npEvent.type = NP_ScreenPowerUpEvent;
    else
        npEvent.type = NP_ScreenPowerDownEvent;

    dispatchNPEvent(npEvent);
}

void PluginView::setParent(ScrollView* parentWidget)
{
    // If parentWidget is 0, lets unregister the plugin with the current parent.
    if (m_private && (!parentWidget || parentWidget != parent())) {
        if (FrameView* frameView = static_cast<FrameView*>(parent())) {
            if (m_private->m_isBackgroundPlaying)
                frameView->hostWindow()->platformPageClient()->onPluginStopBackgroundPlay(this, m_private->m_pluginUniquePrefix.c_str());

            if (m_private->m_isFullScreen)
                handleFullScreenExitEvent();

            //This will unlock the idle (if we have locked it).
            m_private->preventIdle(false);

            //This will release any keepVisibleRects if they were set.
            m_private->clearVisibleRects();

#if USE(ACCELERATED_COMPOSITING)
            // If we had a hole punch rect set, clear it.
            if (m_private->m_platformLayer && !m_private->m_holePunchRect.isEmpty())
                m_private->m_platformLayer->setHolePunchRect(IntRect());
#endif
            frameView->hostWindow()->platformPageClient()->registerPlugin(this, false /*shouldRegister*/ );
        }
    }

    Widget::setParent(parentWidget);

    if (parentWidget) {
        init();

        FrameView* frameView = static_cast<FrameView*>(parentWidget);

        if (frameView && m_private) {
            frameView->hostWindow()->platformPageClient()->registerPlugin(this, true /*shouldRegister*/);
            if (frameView->frame()
               && frameView->frame()->loader()
               && frameView->frame()->loader()->frameHasLoaded())
                handleOnLoadEvent();

            if (m_private->m_isBackgroundPlaying)
                frameView->hostWindow()->platformPageClient()->onPluginStartBackgroundPlay(this, m_private->m_pluginUniquePrefix.c_str());
        }
    }
}

void PluginView::setNPWindowRect(const IntRect& rc)
{
    setNPWindowIfNeeded();
}

void PluginView::setNPWindowIfNeeded()
{
    if (!m_private || !m_isStarted || !parent() || !m_plugin->pluginFuncs()->setwindow)
        return;

    // If the plugin didn't load sucessfully, no point in calling setwindow
    if (m_status != PluginStatusLoadedSuccessfully)
        return;

    if (m_private->m_isFullScreen)
        return;

    if (!m_private->m_hasPendingGeometryChange)
        return;

    m_private->m_hasPendingGeometryChange = false;

    FrameView* frameView = static_cast<FrameView*>(parent());

    m_npWindow.x = m_windowRect.x();
    m_npWindow.y = m_windowRect.y();

    m_npWindow.clipRect.left = max(0, m_clipRect.x());
    m_npWindow.clipRect.top = max(0, m_clipRect.y());
    m_npWindow.clipRect.right = max(0, m_clipRect.right());
    m_npWindow.clipRect.bottom = max(0, m_clipRect.bottom());

    if (m_plugin->quirks().contains(PluginQuirkDontCallSetWindowMoreThanOnce)) {
        // FLASH WORKAROUND: Only set initially. Multiple calls to
        // setNPWindow() cause the plugin to crash in windowed mode.
        if (!m_isWindowed || m_npWindow.width == UNINIT_COORD || m_npWindow.height == UNINIT_COORD) {
            m_npWindow.width = m_windowRect.width();
            m_npWindow.height = m_windowRect.height();
        }
    } else {
        m_npWindow.width = m_windowRect.width();
        m_npWindow.height = m_windowRect.height();
    }

    m_npWindow.type = NPWindowTypeDrawable;

    Olympia::Platform::Graphics::Window *window =
        frameView->hostWindow()->platformPageClient()->platformWindow();
    if (window)
        ((NPSetWindowCallbackStruct*)m_npWindow.ws_info)->windowGroup = window->windowGroup();

    PluginView::setCurrentPluginView(this);
    JSC::JSLock::DropAllLocks dropAllLocks(JSC::SilenceAssertionsOnly);
    setCallingPlugin(true);
    // FIXME: Passing zoomFactor to setwindow make windowed plugin scale incorrectly.
    // Handling the zoom factor properly in the plugin side may be a better solution.
    double oldZoomFactor = ((NPSetWindowCallbackStruct*)m_npWindow.ws_info)->zoomFactor;
    ((NPSetWindowCallbackStruct*)m_npWindow.ws_info)->zoomFactor = 1.;
    m_plugin->pluginFuncs()->setwindow(m_instance, &m_npWindow);
    ((NPSetWindowCallbackStruct*)m_npWindow.ws_info)->zoomFactor = oldZoomFactor;
    setCallingPlugin(false);
    PluginView::setCurrentPluginView(0);
}

void PluginView::setParentVisible(bool visible)
{
    if (isParentVisible() == visible)
        return;

    Widget::setParentVisible(visible);

    // TODO: We might want to tell the plugin to hide it's window here, but it doesn't matter
    //       since the window manager should take care of that for us.
}

NPError PluginView::handlePostReadFile(Vector<char>& buffer, uint32_t len, const char* buf)
{
    String filename(buf, len);

    if (filename.startsWith("file:///"))
        filename = filename.substring(8);

    long long size;
    if (!getFileSize(filename, size))
        return NPERR_FILE_NOT_FOUND;

    FILE* fileHandle = fopen((filename.utf8()).data(), "r");
    if (!fileHandle)
        return NPERR_FILE_NOT_FOUND;

    buffer.resize(size);
    int bytesRead = fread(buffer.data(), 1, size, fileHandle);

    fclose(fileHandle);

    if (bytesRead <= 0)
        return NPERR_FILE_NOT_FOUND;

    return NPERR_NO_ERROR;
}

bool PluginView::platformGetValueStatic(NPNVariable variable, void* value, NPError* result)
{
    switch (variable) {
    case NPNVToolkit:
        *static_cast<uint32_t*>(value) = 0;
        *result = NPERR_NO_ERROR;
        return true;

    case NPNVSupportsXEmbedBool:
        *static_cast<NPBool*>(value) = true;
        *result = NPERR_NO_ERROR;
        return true;

    case NPNVjavascriptEnabledBool:
        *static_cast<NPBool*>(value) = true;
        *result = NPERR_NO_ERROR;
        return true;

    case NPNVSupportsWindowless:
        *static_cast<NPBool*>(value) = true;
        *result = NPERR_NO_ERROR;
        return true;

    case NPNVNPCallbacksPtr: {
        *((void **) value) = (void*)&s_NpCallbacks;
        *result = NPERR_NO_ERROR;
        return true;
    }

    default:
        return false;
    }
}

bool PluginView::platformGetValue(NPNVariable variable, void* value, NPError* result)
{
    switch (variable) {
    case NPNVZoomFactor: {
        *((void **) value) = (void*)&(((NPSetWindowCallbackStruct*)m_npWindow.ws_info)->zoomFactor);
        *result = NPERR_NO_ERROR;
        return true;
    }

    case NPNVRootWindowGroup: {
        FrameView* frameView = static_cast<FrameView*>(parent());
        if (frameView) {
            Olympia::Platform::Graphics::Window *window =
                frameView->hostWindow()->platformPageClient()->platformWindow();
            if (window) {
                void** v = (void**) value;
                *v = (void*) window->rootGroup();

                if (*v) {
                    *result = NPERR_NO_ERROR;
                    return true;
                }
            }
        }

        *result = NPERR_GENERIC_ERROR;
        return false;
    }

    case NPNVBrowserWindowGroup: {
        FrameView* frameView = static_cast<FrameView*>(parent());
        if (frameView) {
            Olympia::Platform::Graphics::Window *window =
                frameView->hostWindow()->platformPageClient()->platformWindow();
            if (window) {
                void** v = (void**) value;
                *v = (void*) window->windowGroup();

                if (*v) {
                    *result = NPERR_NO_ERROR;
                    return true;
                }
            }
        }

        *result = NPERR_GENERIC_ERROR;
        return false;
    }

    case NPNVBrowserDisplayContext: {
        FrameView* frameView = static_cast<FrameView*>(parent());
        if (frameView) {
            Olympia::Platform::Graphics::PlatformDisplayContextHandle context =
                Olympia::Platform::Graphics::platformDisplayContext();
            if (context) {
                void** v = (void**) value;
                *v = (void*) context;

                if (*v) {
                    *result = NPERR_NO_ERROR;
                    return true;
                }
            }
        }

        *result = NPERR_GENERIC_ERROR;
        return false;
    }

    case NPNVPluginWindowPrefix: {
        void** v = (void**) value;
        *v = (void*) m_private->m_pluginUniquePrefix.c_str();

        if (*v) {
            *result = NPERR_NO_ERROR;
            return true;
        }

        *result = NPERR_GENERIC_ERROR;
        return false;
    }

    default:
        return platformGetValueStatic(variable, value, result);
        // TODO: Should we return false?
    }
}

// This is a pure virtual inherited from Widget class and all invalidates
// are funneled through this method. We forward them on to either the
// compositing layer or the PluginView::invalidateWindowlessPluginRect method.
void PluginView::invalidateRect(const IntRect& rect)
{
    if (!m_private)
        return;

    // Record the region that we've been asked to invalidate
    m_private->m_invalidateRegion = WebCore::IntRectRegion::unionRegions(rect, m_private->m_invalidateRegion);

#if USE(ACCELERATED_COMPOSITING)
    if (m_private->m_platformLayer) {
        m_private->m_platformLayer->setNeedsDisplay();
        return;
    }
#endif

    invalidateWindowlessPluginRect(rect);
}

void PluginView::invalidateRect(NPRect* rect)
{
    if (!rect) {
        invalidate();
        return;
    }
    IntRect r(rect->left, rect->top, rect->right - rect->left, rect->bottom - rect->top);
    invalidateRect(r);
}

void PluginView::invalidateRegion(NPRegion region)
{
    invalidate();
}

void PluginView::forceRedraw()
{
    invalidate();
}

bool PluginView::platformStart()
{
    ASSERT(m_isStarted);
    ASSERT(m_status == PluginStatusLoadedSuccessfully);

    m_private = new PluginViewPrivate(this);

    if (m_plugin->pluginFuncs()->getvalue) {
        PluginView::setCurrentPluginView(this);
        JSC::JSLock::DropAllLocks dropAllLocks(JSC::SilenceAssertionsOnly);
        setCallingPlugin(true);
        m_plugin->pluginFuncs()->getvalue(m_instance, NPPVpluginNeedsXEmbed, &m_needsXEmbed);
        setCallingPlugin(false);
        PluginView::setCurrentPluginView(0);
    }

#if USE(ACCELERATED_COMPOSITING)
    if (m_parentFrame->page()->chrome()->client()->allowsAcceleratedCompositing()
        && m_parentFrame->page()->settings()
        && m_parentFrame->page()->settings()->acceleratedCompositingEnabled()) {
        m_private->m_platformLayer = LayerProxyGLES2::create(LayerBaseGLES2::Layer, 0);
        m_private->m_platformLayer->setPluginView(this);
        // Trigger layer computation in RenderLayerCompositor
        m_element->setNeedsStyleRecalc(SyntheticStyleChange);
    }
#endif

    m_npWindow.type = NPWindowTypeDrawable;
    m_npWindow.window = 0; // Not used?
    m_npWindow.x = 0;
    m_npWindow.y = 0;
    m_npWindow.width = UNINIT_COORD;
    m_npWindow.height = UNINIT_COORD;
    m_npWindow.ws_info = new NPSetWindowCallbackStruct;
    ((NPSetWindowCallbackStruct*)m_npWindow.ws_info)->zoomFactor = 1.;
    ((NPSetWindowCallbackStruct*)m_npWindow.ws_info)->windowGroup = 0;

    show();

    if (FrameView* frameView = static_cast<FrameView*>(parent()))
        handleOrientationEvent(frameView->hostWindow()->platformPageClient()->orientation());

    if (!(m_plugin->quirks().contains(PluginQuirkDeferFirstSetWindowCall))) {
        updatePluginWidget();
        setNPWindowIfNeeded();
    }

    return true;
}

void PluginView::platformDestroy()
{
    if(!m_private)
        return;

    // This will unlock the idle (if we have locked it).
    m_private->preventIdle(false);

    // This will release any keepVisibleRects if they were set.
    m_private->clearVisibleRects();

    // This will ensure that we unregistered the plugin.
    if (FrameView* frameView = static_cast<FrameView*>(parent())) {
        if (m_private->m_isBackgroundPlaying)
            frameView->hostWindow()->platformPageClient()->onPluginStopBackgroundPlay(this, m_private->m_pluginUniquePrefix.c_str());

        // If we were still fullscreen, ensure we notify everyone we're not.
        if (m_private->m_isFullScreen)
            frameView->hostWindow()->platformPageClient()->didPluginExitFullScreen(this, m_private->m_pluginUniquePrefix.c_str());

        if (m_private->m_orientationLocked)
            frameView->hostWindow()->platformPageClient()->unlockOrientation();

        frameView->hostWindow()->platformPageClient()->registerPlugin(this, false /*shouldRegister*/);
    }

    m_private->m_isBackgroundPlaying = false;
    m_private->m_isFullScreen = false;

    delete m_private;
    m_private = 0;
}

void PluginView::halt()
{
}

void PluginView::restart()
{
}

void PluginView::getWindowInfo(Vector<PluginWindowInfo>& windowList)
{
    if (!m_plugin->pluginFuncs()->getvalue)
        return;

    void* valPtr = 0;

    PluginView::setCurrentPluginView(this);
    JSC::JSLock::DropAllLocks dropAllLocks(JSC::SilenceAssertionsOnly);
    setCallingPlugin(true);
    m_plugin->pluginFuncs()->getvalue(m_instance, NPPVpluginScreenWindow, &valPtr);
    setCallingPlugin(false);
    PluginView::setCurrentPluginView(0);

    if (!valPtr)
        return;

    NPScreenWindowHandles* screenWinHandles = (NPScreenWindowHandles*)valPtr;

    for (int i = 0; i < screenWinHandles->numOfWindows; i++) {
        PluginWindowInfo info;
        info.windowPtr = screenWinHandles->windowHandles[i];

        NPRect* rc = screenWinHandles->windowRects[i];
        info.windowRect = IntRect(rc->left, rc->top, rc->right - rc->left, rc->bottom - rc->top);

        windowList.append(info);
    }
}

Olympia::Platform::Graphics::Buffer* PluginView::lockFrontBufferForRead() const
{
    if (!m_private)
        return NULL;

    Olympia::Platform::Graphics::Buffer* buffer =
        m_private->lockReadFrontBufferInternal();

    if (!buffer)
        m_private->unlockReadFrontBuffer();

    return buffer;
}

void PluginView::unlockFrontBuffer() const
{
    if (!m_private)
        return;

    m_private->unlockReadFrontBuffer();
}

#if USE(ACCELERATED_COMPOSITING)
PlatformLayer* PluginView::platformLayer() const
{
    if (!m_private)
        return NULL;

    return m_private->m_platformLayer.get();
}
#endif

IntRect PluginView::ensureVisibleRect()
{
    if (!m_private)
        return IntRect();

    return m_private->m_keepVisibleRect;
}

void PluginView::setBackgroundPlay(bool value)
{
    if (!m_private)
        return;

    if (m_private->m_isBackgroundPlaying != value) {
        FrameView* frameView = static_cast<FrameView*>(m_private->m_view->parent());
        m_private->m_isBackgroundPlaying = value;
        if (m_private->m_isBackgroundPlaying) {
            frameView->hostWindow()->platformPageClient()->onPluginStartBackgroundPlay(this, m_private->m_pluginUniquePrefix.c_str());
        }
        else {
            frameView->hostWindow()->platformPageClient()->onPluginStopBackgroundPlay(this, m_private->m_pluginUniquePrefix.c_str());
        }
    }
}

} // namespace WebCore

// -------

