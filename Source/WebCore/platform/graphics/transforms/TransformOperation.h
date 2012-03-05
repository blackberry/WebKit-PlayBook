/*
 * Copyright (C) 2000 Lars Knoll (knoll@kde.org)
 *           (C) 2000 Antti Koivisto (koivisto@kde.org)
 *           (C) 2000 Dirk Mueller (mueller@kde.org)
 * Copyright (C) 2003, 2005, 2006, 2007, 2008 Apple Inc. All rights reserved.
 * Copyright (C) 2006 Graham Dennis (graham.dennis@gmail.com)
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

#ifndef TransformOperation_h
#define TransformOperation_h

#include "FloatSize.h"
#include "TransformationMatrix.h"
#include <wtf/PassRefPtr.h>
#include <wtf/RefCounted.h>

namespace WebCore {

// CSS Transforms (may become part of CSS3)

class TransformOperation : public RefCounted<TransformOperation> {
public:
    enum OperationType {
        SCALE_X, SCALE_Y, SCALE, 
        TRANSLATE_X, TRANSLATE_Y, TRANSLATE, 
        ROTATE,
        ROTATE_Z = ROTATE,
        SKEW_X, SKEW_Y, SKEW, 
        MATRIX,
        SCALE_Z, SCALE_3D,
        TRANSLATE_Z, TRANSLATE_3D,
        ROTATE_X, ROTATE_Y, ROTATE_3D,
        MATRIX_3D,
        PERSPECTIVE,
        IDENTITY, NONE
    };

    virtual ~TransformOperation() { }

    virtual bool operator==(const TransformOperation&) const = 0;
    bool operator!=(const TransformOperation& o) const { return !(*this == o); }

    virtual bool isIdentity() const = 0;

    // Return true if the borderBoxSize was used in the computation, false otherwise.
    virtual bool apply(TransformationMatrix&, const FloatSize& borderBoxSize) const = 0;

    virtual PassRefPtr<TransformOperation> blend(const TransformOperation* from, double progress, bool blendToIdentity = false) = 0;

    virtual OperationType getOperationType() const = 0;
    virtual bool isSameType(const TransformOperation&) const { return false; }
    
    bool is3DOperation() const
    {
        OperationType opType = getOperationType();
        return opType == SCALE_Z ||
               opType == SCALE_3D ||
               opType == TRANSLATE_Z ||
               opType == TRANSLATE_3D ||
#if PLATFORM(BLACKBERRY)
               opType == ROTATE ||
#endif
               opType == ROTATE_X ||
               opType == ROTATE_Y ||
               opType == ROTATE_3D ||
               opType == MATRIX_3D ||
               opType == PERSPECTIVE;
    }

#if PLATFORM(BLACKBERRY)
    virtual String toString() const
    {
        switch (getOperationType()) {
        case SCALE_X: return "SCALE_X";
        case SCALE_Y: return "SCALE_Y";
        case SCALE: return "SCALE";
        case TRANSLATE_X: return "TRANSLATE_X";
        case TRANSLATE_Y: return "TRANSLATE_Y";
        case TRANSLATE: return "TRANSLATE";
        //case ROTATE_Z:
        case ROTATE: return "ROTATE";
        case SKEW_X: return "SKEW_X";
        case SKEW_Y: return "SKEW_Y";
        case SKEW: return "SKEW";
        case MATRIX: return "MATRIX";
        case SCALE_Z: return "SCALE_Z";
        case SCALE_3D: return "SCALE_3D";
        case TRANSLATE_Z: return "TRANSLATE_Z";
        case TRANSLATE_3D: return "TRANSLATE_3D";
        case ROTATE_X: return "ROTATE_X";
        case ROTATE_Y: return "ROTATE_Y";
        case ROTATE_3D: return "ROTATE_3D";
        case MATRIX_3D: return "MATRIX_3D";
        case PERSPECTIVE: return "PERSPECTIVE";
        case IDENTITY: return "IDENTITY";
        case NONE: return "NONE";
        }

        ASSERT_NOT_REACHED();
        return String();
    }
#endif

};

} // namespace WebCore

#endif // TransformOperation_h
