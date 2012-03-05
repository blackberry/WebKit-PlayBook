/*
 * Copyright (C) 2010 Apple Inc. All rights reserved.
 * Copyright (C) 2010 University of Szeged. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "PlatformWebView.h"
#include "qquickwebview.h"
#include "qquickwebpage.h"

#include <QApplication>
#include <QDeclarativeProperty>
#include <QQuickView>
#include <qwindowsysteminterface_qpa.h>

namespace WTR {

class WrapperWindow : public QQuickView {
    Q_OBJECT
public:
    WrapperWindow(QQuickWebView* view)
        : QQuickView(QUrl("data:text/plain,import QtQuick 2.0\nItem { objectName: 'root' }"))
        , m_view(view)
    {
        connect(this, SIGNAL(statusChanged(QQuickView::Status)), SLOT(handleStatusChanged(QQuickView::Status)));
    }

private slots:
    void handleStatusChanged(QQuickView::Status status)
    {
        if (status != QQuickView::Ready)
            return;

        setGeometry(0, 0, 800, 600);
        setResizeMode(QQuickView::SizeRootObjectToView);

        m_view->setParentItem(rootObject());
        QDeclarativeProperty::write(m_view, "anchors.fill", qVariantFromValue(rootObject()));

        QWindowSystemInterface::handleWindowActivated(this);
        m_view->page()->setFocus(true);
    }

private:
    QQuickWebView* m_view;
};

PlatformWebView::PlatformWebView(WKContextRef contextRef, WKPageGroupRef pageGroupRef)
    : m_view(new QQuickWebView(contextRef, pageGroupRef))
    , m_window(new WrapperWindow(m_view))
    , m_windowIsKey(true)
{
}

PlatformWebView::~PlatformWebView()
{
    delete m_window;
}

void PlatformWebView::resizeTo(unsigned width, unsigned height)
{
    m_window->resize(width, height);
}

WKPageRef PlatformWebView::page()
{
    return m_view->wkView()->page()->pageRef();
}

void PlatformWebView::focus()
{
    m_view->setFocus(Qt::OtherFocusReason);
}

WKRect PlatformWebView::windowFrame()
{
    // Implement.

    WKRect wkFrame;
    wkFrame.origin.x = 0;
    wkFrame.origin.y = 0;
    wkFrame.size.width = 0;
    wkFrame.size.height = 0;
    return wkFrame;
}

void PlatformWebView::setWindowFrame(WKRect)
{
    // Implement.
}

bool PlatformWebView::sendEvent(QEvent* event)
{
    return QCoreApplication::sendEvent(m_view->page(), event);
}

void PlatformWebView::postEvent(QEvent* event)
{
    QCoreApplication::postEvent(m_view->page(), event);
}

void PlatformWebView::addChromeInputField()
{
}

void PlatformWebView::removeChromeInputField()
{
}

void PlatformWebView::makeWebViewFirstResponder()
{
}

WKRetainPtr<WKImageRef> PlatformWebView::windowSnapshotImage()
{
    // FIXME: implement to capture pixels in the UI process,
    // which may be necessary to capture things like 3D transforms.
    return 0;
}

} // namespace WTR

#include "PlatformWebViewQt.moc"
