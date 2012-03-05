/*
 * Copyright (C) Research In Motion Limited 2011. All rights reserved.
 * Copyright (C) 2007, 2008, 2009 Apple Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1.  Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 * 2.  Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution.
 * 3.  Neither the name of Apple Computer, Inc. ("Apple") nor the names of
 *     its contributors may be used to endorse or promote products derived
 *     from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE AND ITS CONTRIBUTORS "AS IS" AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL APPLE OR ITS CONTRIBUTORS BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"
#include "LayerAnimation.h"

#include "IdentityTransformOperation.h"
#include "TransformationMatrix.h"

#include <algorithm>

namespace WebCore {

using namespace std;

// FIXME: Copy pasted from AnimationBase.cpp, TransformOperations blendFunc(const AnimationBase* anim, const TransformOperations& from, const TransformOperations& to, double progress)
// Ideally, AnimationBase should be refactored to increase code reuse once this part of the BlackBerry port is upstream.
TransformationMatrix LayerAnimation::blendTransform(const TransformOperations* from, const TransformOperations* to, double progress) const
{
    TransformationMatrix t;

    if (m_transformFunctionListValid) {
        // A trick to avoid touching the refcount of shared TransformOperations on the wrong thread
        // Since TransforOperation is not ThreadSafeRefCounted, we are only allowed to touch the ref
        // count of shared operations when the WebKit thread and compositing thread are in sync.
        Vector<TransformOperation*> result;
        Vector<RefPtr<TransformOperation> > owned;

        unsigned fromSize = from->operations().size();
        unsigned toSize = to->operations().size();
        unsigned size = max(fromSize, toSize);
        for (unsigned i = 0; i < size; i++) {
            TransformOperation* fromOp = (i < fromSize) ? from->operations()[i].get() : 0;
            TransformOperation* toOp = (i < toSize) ? to->operations()[i].get() : 0;
            RefPtr<TransformOperation> blendedOp = toOp ? toOp->blend(fromOp, progress) : (fromOp ? fromOp->blend(0, progress, true) : PassRefPtr<TransformOperation>(0));
            if (blendedOp) {
                result.append(blendedOp.get());
                owned.append(blendedOp);
            } else {
                RefPtr<TransformOperation> identityOp = IdentityTransformOperation::create();
                owned.append(identityOp);
                if (progress > 0.5)
                    result.append(toOp ? toOp : identityOp.get());
                else
                    result.append(fromOp ? fromOp : identityOp.get());
            }
        }

        IntSize sz = boxSize();
        for (unsigned i = 0; i < result.size(); ++i)
            result[i]->apply(t, sz);
    } else {
        // Convert the TransformOperations into matrices
        TransformationMatrix fromT;
        from->apply(boxSize(), fromT);
        to->apply(boxSize(), t);

        t.blend(fromT, progress);
    }

    return t;
}

float LayerAnimation::blendOpacity(float from, float to, double progress) const
{
    float opacity = from + (to - from) * progress;

    return max(0.0f, min(opacity, 1.0f));
}

// FIXME: Copy pasted from WebCore::KeyframeAnimation!!!
// Ideally, AnimationBase and related classes should be refactored to increase code reuse once this part of the BlackBerry port is upstream.
void LayerAnimation::validateTransformLists()
{
    m_transformFunctionListValid = false;

    if (m_values.size() < 2 || property() != AnimatedPropertyWebkitTransform)
        return;

    // Empty transforms match anything, so find the first non-empty entry as the reference
    size_t numKeyframes = m_values.size();
    size_t firstNonEmptyTransformKeyframeIndex = numKeyframes;

    for (size_t i = 0; i < numKeyframes; ++i) {
        const TransformAnimationValue* currentKeyframe = static_cast<const TransformAnimationValue*>(m_values.at(i));
        if (currentKeyframe->value()->size()) {
            firstNonEmptyTransformKeyframeIndex = i;
            break;
        }
    }

    if (firstNonEmptyTransformKeyframeIndex == numKeyframes)
        return;

    const TransformOperations* firstVal = static_cast<const TransformAnimationValue*>(m_values.at(firstNonEmptyTransformKeyframeIndex))->value();

    // See if the keyframes are valid
    for (size_t i = firstNonEmptyTransformKeyframeIndex + 1; i < numKeyframes; ++i) {
        const TransformAnimationValue* currentKeyframe = static_cast<const TransformAnimationValue*>(m_values.at(i));
        const TransformOperations* val = currentKeyframe->value();

        // A null transform matches anything
        if (val->operations().isEmpty())
            continue;

        // If the sizes of the function lists don't match, the lists don't match
        if (firstVal->operations().size() != val->operations().size())
            return;

        // If the types of each function are not the same, the lists don't match
        for (size_t j = 0; j < firstVal->operations().size(); ++j) {
            if (!firstVal->operations()[j]->isSameType(*val->operations()[j]))
                return;
        }
    }

    // Keyframes are valid
    m_transformFunctionListValid = true;
}

}
