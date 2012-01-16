/*
 * Copyright (C) 1999 Lars Knoll (knoll@kde.org)
 *           (C) 1999 Antti Koivisto (koivisto@kde.org)
 *           (C) 2000 Dirk Mueller (mueller@kde.org)
 *           (C) 2006 Allan Sandfeld Jensen (kde@carewolf.com)
 *           (C) 2006 Samuel Weinig (sam.weinig@gmail.com)
 * Copyright (C) 2003, 2004, 2005, 2006, 2008, 2009, 2010, 2011 Apple Inc. All rights reserved.
 * Copyright (C) 2010 Google Inc. All rights reserved.
 * Copyright (C) Research In Motion Limited 2010. All rights reserved.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public License
 * along with this library; see the file COPYING.LIB.  If not, write to
 * the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA 02110-1301, USA.
 *
 */

#include "config.h"
#include "RenderImage.h"

#include "Frame.h"
#include "GraphicsContext.h"
#include "HTMLAreaElement.h"
#include "HTMLImageElement.h"
#include "HTMLInputElement.h"
#include "HTMLMapElement.h"
#include "HTMLNames.h"
#include "HitTestResult.h"
#include "Page.h"
#include "RenderLayer.h"
#include "RenderView.h"
#include "SelectionController.h"
#include "Settings.h"
#include <wtf/UnusedParam.h>

#if ENABLE(WML)
#include "WMLImageElement.h"
#include "WMLNames.h"
#endif

using namespace std;

namespace WebCore {

using namespace HTMLNames;

RenderImage::RenderImage(Node* node)
    : RenderReplaced(node, IntSize(0, 0))
    , m_needsToSetSizeForAltText(false)
{
    updateAltText();

    view()->frameView()->setIsVisuallyNonEmpty();
}

RenderImage::~RenderImage()
{
    ASSERT(m_imageResource);
    m_imageResource->shutdown();
}

void RenderImage::setImageResource(PassOwnPtr<RenderImageResource> imageResource)
{
    ASSERT(!m_imageResource);
    m_imageResource = imageResource;
    m_imageResource->initialize(this);
}

// If we'll be displaying either alt text or an image, add some padding.
static const unsigned short paddingWidth = 4;
static const unsigned short paddingHeight = 4;

// Alt text is restricted to this maximum size, in pixels.  These are
// signed integers because they are compared with other signed values.
static const int maxAltTextWidth = 1024;
static const int maxAltTextHeight = 256;

IntSize RenderImage::imageSizeForError(Image* newImage) const
{
    ASSERT_ARG(newImage, newImage);

    // imageSize() returns 0 for the error image. We need the true size of the
    // error image, so we have to get it by grabbing image() directly.
    return IntSize(newImage->width() * style()->effectiveZoom(), newImage->height() * style()->effectiveZoom());
}

// Sets the image height and width to fit the alt text.  Returns true if the
// image size changed.
bool RenderImage::setImageSizeForAltText(CachedImage* newImage /* = 0 */)
{
    Image* image = newImage ? newImage->image() : 0;

    // if newImage exists, we want to include padding even if newImage->image() is null
    bool extraPadding = newImage;

    return setImageSizeForAltText(image, extraPadding);
}

bool RenderImage::setImageSizeForAltText(Image* newImage, bool extraPadding)
{
    IntSize imageSize;
    if (newImage)
        imageSize = imageSizeForError(newImage);

    // we have an alt and the user meant it (its not a text we invented)
    if (!m_altText.isEmpty()) {
        const Font& font = style()->font();
        IntSize textSize(min(font.width(TextRun(m_altText.characters(), m_altText.length())), maxAltTextWidth), min(font.height(), maxAltTextHeight));
        imageSize = imageSize.expandedTo(textSize);
    }

    // If we'll be displaying either text or an image, add a little padding.
    if (!m_altText.isEmpty() || newImage || extraPadding)
        imageSize.expand(paddingWidth, paddingHeight);

    if (imageSize == intrinsicSize())
        return false;

    setIntrinsicSize(imageSize);
    return true;
}

static bool shouldDrawBorderWhileLoading(RenderImage* renderImage)
{
    ASSERT(renderImage);
    ASSERT(renderImage->document());
    Settings* settings = renderImage->document()->settings();
    return settings ? settings->shouldDrawBorderWhileLoadingImages() : false;
}

void RenderImage::styleDidChange(StyleDifference diff, const RenderStyle* oldStyle)
{
    RenderReplaced::styleDidChange(diff, oldStyle);
    if (m_needsToSetSizeForAltText) {
        if (!m_altText.isEmpty() && setImageSizeForAltText(m_imageResource->cachedImage()))
            imageDimensionsChanged(true /* imageSizeChanged */);
        m_needsToSetSizeForAltText = false;
    } else if (style() && shouldDrawBorderWhileLoading(this) && !(m_imageResource->cachedImage() && m_imageResource->cachedImage()->image())) {
        // We need a size to draw the border around while loading.  The broken image size makes a good default.
        // Do not call imageDimensionsChanged here.  It should only be called when the actual size of the image object changes, not just the placeholder size.
        setImageSizeForAltText(CachedImage::brokenImage(), true);
    }
}

void RenderImage::imageChanged(WrappedImagePtr newImage, const IntRect* rect)
{
    if (documentBeingDestroyed())
        return;

    if (hasBoxDecorations() || hasMask())
        RenderReplaced::imageChanged(newImage, rect);
    
    if (!m_imageResource)
        return;

    if (newImage != m_imageResource->imagePtr() || !newImage)
        return;

    bool imageSizeChanged = false;

    // Set image dimensions, taking into account the size of the alt text.
    if (m_imageResource->errorOccurred()) {
        if (!m_altText.isEmpty() && document()->isPendingStyleRecalc()) {
            ASSERT(node());
            if (node()) {
                m_needsToSetSizeForAltText = true;
                node()->setNeedsStyleRecalc(SyntheticStyleChange);
            }
            return;
        }
        imageSizeChanged = setImageSizeForAltText(m_imageResource->cachedImage());
    }

    imageDimensionsChanged(imageSizeChanged, rect);
}

void RenderImage::imageDimensionsChanged(bool imageSizeChanged, const IntRect* rect)
{
    bool shouldRepaint = true;

    if (m_imageResource->imageSize(style()->effectiveZoom()) != intrinsicSize() || imageSizeChanged) {
        if (!m_imageResource->errorOccurred())
            setIntrinsicSize(m_imageResource->imageSize(style()->effectiveZoom()));

        // In the case of generated image content using :before/:after, we might not be in the
        // render tree yet.  In that case, we don't need to worry about check for layout, since we'll get a
        // layout when we get added in to the render tree hierarchy later.
        if (containingBlock()) {
            // lets see if we need to relayout at all..
            int oldwidth = width();
            int oldheight = height();
            if (!preferredLogicalWidthsDirty())
                setPreferredLogicalWidthsDirty(true);
            computeLogicalWidth();
            computeLogicalHeight();

            if (imageSizeChanged || width() != oldwidth || height() != oldheight) {
                shouldRepaint = false;
                if (!selfNeedsLayout())
                    setNeedsLayout(true);
            }

            setWidth(oldwidth);
            setHeight(oldheight);
        }
    }

    if (shouldRepaint) {
        IntRect repaintRect;
        if (rect) {
            // The image changed rect is in source image coordinates (pre-zooming),
            // so map from the bounds of the image to the contentsBox.
            repaintRect = enclosingIntRect(mapRect(*rect, FloatRect(FloatPoint(), m_imageResource->imageSize(1.0f)), contentBoxRect()));
            // Guard against too-large changed rects.
            repaintRect.intersect(contentBoxRect());
        } else
            repaintRect = contentBoxRect();
        
        repaintRectangle(repaintRect);

#if USE(ACCELERATED_COMPOSITING)
        if (hasLayer()) {
            // Tell any potential compositing layers that the image needs updating.
            layer()->rendererContentChanged();
        }
#endif
    }
}

void RenderImage::notifyFinished(CachedResource* newImage)
{
    if (!m_imageResource)
        return;
    
    if (documentBeingDestroyed())
        return;

#if USE(ACCELERATED_COMPOSITING)
    if (newImage == m_imageResource->cachedImage() && hasLayer()) {
        // tell any potential compositing layers
        // that the image is done and they can reference it directly.
        layer()->rendererContentChanged();
    }
#else
    UNUSED_PARAM(newImage);
#endif
}

void RenderImage::paintReplaced(PaintInfo& paintInfo, int tx, int ty)
{
    int cWidth = contentWidth();
    int cHeight = contentHeight();
    int leftBorder = borderLeft();
    int topBorder = borderTop();
    int leftPad = paddingLeft();
    int topPad = paddingTop();

    GraphicsContext* context = paintInfo.context;

    Image* img = 0;
    bool drawBorder = false;

    if (cWidth > 0 && cHeight > 0) {
        if (m_imageResource->hasImage() && !m_imageResource->errorOccurred())
            img = m_imageResource->image(cWidth, cHeight);

        if (shouldDrawBorderWhileLoading(this)) {
            // draw the border whenever there is no image data to display
            drawBorder = (!img || img->isNull());
        } else {
            // draw the border only when loading is finished and there is still no image
            drawBorder = (!m_imageResource->hasImage() || m_imageResource->errorOccurred()
#if PLATFORM(BLACKBERRY)
                            // For the RIM-platform, we also draw the alternative text when network image loading is suppressed.
                            // loading is suppressed.
                            || (img->isNull() && document()->settings() && !document()->settings()->loadsImagesAutomatically())
#endif
                         );
        }
    }

    if (drawBorder) {
        if (paintInfo.phase == PaintPhaseSelection)
            return;

        if (cWidth > 2 && cHeight > 2) {
            context->save();

            // Draw an outline rect where the image should be.
            context->setStrokeStyle(SolidStroke);
            context->setStrokeThickness(1.0);
            context->setStrokeColor(Color::lightGray, style()->colorSpace());

            context->strokeRect(IntRect(tx + leftBorder + leftPad, ty + topBorder + topPad, cWidth, cHeight));

            bool errorPictureDrawn = false;
            int imageX = 0;
            int imageY = 0;
            // When calculating the usable dimensions, exclude the pixels of
            // the ouline rect so the error image/alt text doesn't draw on it.
            int usableWidth = cWidth - 2;
            int usableHeight = cHeight - 2;

            Image* image = m_imageResource->image();

            if (m_imageResource->errorOccurred() && !image->isNull() && usableWidth >= image->width() && usableHeight >= image->height()) {
                // Center the error image, accounting for border and padding.
                int centerX = (usableWidth - image->width()) / 2;
                if (centerX < 0)
                    centerX = 0;
                int centerY = (usableHeight - image->height()) / 2;
                if (centerY < 0)
                    centerY = 0;
                imageX = leftBorder + leftPad + centerX + 1;
                imageY = topBorder + topPad + centerY + 1;
                context->drawImage(image, style()->colorSpace(), IntPoint(tx + imageX, ty + imageY));
                errorPictureDrawn = true;
            }

            if (!m_altText.isEmpty()) {
                String text = document()->displayStringModifiedByEncoding(m_altText);
                context->setFillColor(style()->visitedDependentColor(CSSPropertyColor), style()->colorSpace());
                int ax = tx + leftBorder + leftPad;
                int ay = ty + topBorder + topPad;
                const Font& font = style()->font();
                int ascent = font.ascent();

                // Only draw the alt text if it'll fit within the content box,
                // and only if it fits above the error image.
                TextRun textRun(text.characters(), text.length());
                int textWidth = font.width(textRun);
                if (errorPictureDrawn) {
                    if (usableWidth >= textWidth && font.height() <= imageY)
                        context->drawText(style()->font(), textRun, IntPoint(ax, ay + ascent));
                } else if (usableWidth >= textWidth && cHeight >= font.height())
                    context->drawText(style()->font(), textRun, IntPoint(ax, ay + ascent));
            }

            context->restore();
        }
    } else if (img && !img->isNull()) {

#if PLATFORM(MAC)
        if (style()->highlight() != nullAtom && !paintInfo.context->paintingDisabled())
            paintCustomHighlight(tx - x(), ty - y(), style()->highlight(), true);
#endif

        IntSize contentSize(cWidth, cHeight);
        IntRect rect(IntPoint(tx + leftBorder + leftPad, ty + topBorder + topPad), contentSize);
        paintIntoRect(context, rect);
    }
}

void RenderImage::paint(PaintInfo& paintInfo, int tx, int ty)
{
    RenderReplaced::paint(paintInfo, tx, ty);
    
    if (paintInfo.phase == PaintPhaseOutline)
        paintAreaElementFocusRing(paintInfo);
}
    
void RenderImage::paintAreaElementFocusRing(PaintInfo& paintInfo)
{
    Document* document = this->document();
    
    if (document->printing() || !document->frame()->selection()->isFocusedAndActive())
        return;
    
    if (paintInfo.context->paintingDisabled() && !paintInfo.context->updatingControlTints())
        return;

    Node* focusedNode = document->focusedNode();
    if (!focusedNode || !focusedNode->hasTagName(areaTag))
        return;

    HTMLAreaElement* areaElement = static_cast<HTMLAreaElement*>(focusedNode);
    if (areaElement->imageElement() != node())
        return;

    // Even if the theme handles focus ring drawing for entire elements, it won't do it for
    // an area within an image, so we don't call RenderTheme::supportsFocusRing here.

    Path path = areaElement->getPath(this);
    if (path.isEmpty())
        return;

    // FIXME: Do we need additional code to clip the path to the image's bounding box?

    RenderStyle* areaElementStyle = areaElement->computedStyle();
    paintInfo.context->drawFocusRing(path, areaElementStyle->outlineWidth(), areaElementStyle->outlineOffset(),
        areaElementStyle->visitedDependentColor(CSSPropertyOutlineColor));
}

void RenderImage::areaElementFocusChanged(HTMLAreaElement* element)
{
    ASSERT_UNUSED(element, element->imageElement() == node());

    // It would be more efficient to only repaint the focus ring rectangle
    // for the passed-in area element. That would require adding functions
    // to the area element class.
    repaint();
}

void RenderImage::paintIntoRect(GraphicsContext* context, const IntRect& rect)
{
    if (!m_imageResource->hasImage() || m_imageResource->errorOccurred() || rect.width() <= 0 || rect.height() <= 0)
        return;

    Image* img = m_imageResource->image(rect.width(), rect.height());
    if (!img || img->isNull())
        return;

    HTMLImageElement* imageElt = (node() && node()->hasTagName(imgTag)) ? static_cast<HTMLImageElement*>(node()) : 0;
    CompositeOperator compositeOperator = imageElt ? imageElt->compositeOperator() : CompositeSourceOver;
    bool useLowQualityScaling = shouldPaintAtLowQuality(context, m_imageResource->image(), 0, rect.size());
    context->drawImage(m_imageResource->image(rect.width(), rect.height()), style()->colorSpace(), rect, compositeOperator, useLowQualityScaling);
}

int RenderImage::minimumReplacedHeight() const
{
    return m_imageResource->errorOccurred() ? intrinsicSize().height() : 0;
}

HTMLMapElement* RenderImage::imageMap() const
{
    HTMLImageElement* i = node() && node()->hasTagName(imgTag) ? static_cast<HTMLImageElement*>(node()) : 0;
    return i ? i->document()->getImageMap(i->fastGetAttribute(usemapAttr)) : 0;
}

bool RenderImage::nodeAtPoint(const HitTestRequest& request, HitTestResult& result, int x, int y, int tx, int ty, HitTestAction hitTestAction)
{
    HitTestResult tempResult(result.point(), result.topPadding(), result.rightPadding(), result.bottomPadding(), result.leftPadding());
    bool inside = RenderReplaced::nodeAtPoint(request, tempResult, x, y, tx, ty, hitTestAction);

    if (tempResult.innerNode() && node()) {
        if (HTMLMapElement* map = imageMap()) {
            IntRect contentBox = contentBoxRect();
            float zoom = style()->effectiveZoom();
            int mapX = lroundf((x - tx - this->x() - contentBox.x()) / zoom);
            int mapY = lroundf((y - ty - this->y() - contentBox.y()) / zoom);
            if (map->mapMouseEvent(mapX, mapY, contentBox.size(), tempResult))
                tempResult.setInnerNonSharedNode(node());
        }
    }

    if (!inside && result.isRectBasedTest())
        result.append(tempResult);
    if (inside)
        result = tempResult;
    return inside;
}

void RenderImage::updateAltText()
{
    if (!node())
        return;

    if (node()->hasTagName(inputTag))
        m_altText = static_cast<HTMLInputElement*>(node())->altText();
    else if (node()->hasTagName(imgTag))
        m_altText = static_cast<HTMLImageElement*>(node())->altText();
#if ENABLE(WML)
    else if (node()->hasTagName(WMLNames::imgTag))
        m_altText = static_cast<WMLImageElement*>(node())->altText();
#endif
}

bool RenderImage::isLogicalWidthSpecified() const
{
    switch (style()->logicalWidth().type()) {
        case Fixed:
        case Percent:
            return true;
        case Auto:
        case Relative: // FIXME: Shouldn't this case return true?
        case Static:
        case Intrinsic:
        case MinIntrinsic:
            return false;
    }
    ASSERT(false);
    return false;
}

bool RenderImage::isLogicalHeightSpecified() const
{
    switch (style()->logicalHeight().type()) {
        case Fixed:
        case Percent:
            return true;
        case Auto:
        case Relative: // FIXME: Shouldn't this case return true?
        case Static:
        case Intrinsic:
        case MinIntrinsic:
            return false;
    }
    ASSERT(false);
    return false;
}

int RenderImage::computeReplacedLogicalWidth(bool includeMaxWidth) const
{
    if (m_imageResource->imageHasRelativeWidth())
        if (RenderObject* cb = isPositioned() ? container() : containingBlock()) {
            if (cb->isBox())
                m_imageResource->setImageContainerSize(IntSize(toRenderBox(cb)->availableWidth(), toRenderBox(cb)->availableHeight()));
        }

    int logicalWidth;
    if (isLogicalWidthSpecified())
        logicalWidth = computeReplacedLogicalWidthUsing(style()->logicalWidth());
    else if (m_imageResource->usesImageContainerSize()) {
        IntSize size = m_imageResource->imageSize(style()->effectiveZoom());
        logicalWidth = style()->isHorizontalWritingMode() ? size.width() : size.height();
    } else if (m_imageResource->imageHasRelativeWidth())
        logicalWidth = 0; // If the image is relatively-sized, set the width to 0 until there is a set container size.
    else
        logicalWidth = calcAspectRatioLogicalWidth();

    int minLogicalWidth = computeReplacedLogicalWidthUsing(style()->logicalMinWidth());
    int maxLogicalWidth = !includeMaxWidth || style()->logicalMaxWidth().isUndefined() ? logicalWidth : computeReplacedLogicalWidthUsing(style()->logicalMaxWidth());

    return max(minLogicalWidth, min(logicalWidth, maxLogicalWidth));
}

int RenderImage::computeReplacedLogicalHeight() const
{
    int logicalHeight;
    if (isLogicalHeightSpecified())
        logicalHeight = computeReplacedLogicalHeightUsing(style()->logicalHeight());
    else if (m_imageResource->usesImageContainerSize()) {
        IntSize size = m_imageResource->imageSize(style()->effectiveZoom());
        logicalHeight = style()->isHorizontalWritingMode() ? size.height() : size.width();
    } else if (m_imageResource->imageHasRelativeHeight())
        logicalHeight = 0; // If the image is relatively-sized, set the height to 0 until there is a set container size.
    else
        logicalHeight = calcAspectRatioLogicalHeight();

    int minLogicalHeight = computeReplacedLogicalHeightUsing(style()->logicalMinHeight());
    int maxLogicalHeight = style()->logicalMaxHeight().isUndefined() ? logicalHeight : computeReplacedLogicalHeightUsing(style()->logicalMaxHeight());

    return max(minLogicalHeight, min(logicalHeight, maxLogicalHeight));
}

int RenderImage::calcAspectRatioLogicalWidth() const
{
    int intrinsicWidth = intrinsicLogicalWidth();
    int intrinsicHeight = intrinsicLogicalHeight();
    if (!intrinsicHeight)
        return 0;
    if (!m_imageResource->hasImage() || m_imageResource->errorOccurred())
        return intrinsicWidth; // Don't bother scaling.
    return RenderBox::computeReplacedLogicalHeight() * intrinsicWidth / intrinsicHeight;
}

int RenderImage::calcAspectRatioLogicalHeight() const
{
    int intrinsicWidth = intrinsicLogicalWidth();
    int intrinsicHeight = intrinsicLogicalHeight();
    if (!intrinsicWidth)
        return 0;
    if (!m_imageResource->hasImage() || m_imageResource->errorOccurred())
        return intrinsicHeight; // Don't bother scaling.
    return RenderBox::computeReplacedLogicalWidth() * intrinsicHeight / intrinsicWidth;
}

} // namespace WebCore
