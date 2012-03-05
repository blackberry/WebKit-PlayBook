/*
 * Copyright (C) 2009 Torch Mobile Inc. http://www.torchmobile.com/
 * Copyright (C) 2009, 2010, 2011 Research In Motion Limited. All rights reserved.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
 */

#include "config.h"
#include "EditorClientBlackBerry.h"

#include "DOMSupport.h"
#include "DumpRenderTreeClient.h"
#include "EditCommand.h"
#include "FocusController.h"
#include "Frame.h"
#include "HTMLElement.h"
#include "HTMLInputElement.h"
#include "HTMLNames.h"
#include "HTMLTextAreaElement.h"
#include "InputHandler.h"
#include "KeyboardEvent.h"
#include "Node.h"
#include "NotImplemented.h"
#include "Page.h"
#include "PlatformKeyboardEvent.h"
#include "RenderObject.h"
#include "SelectionHandler.h"
#include "WebPage.h"
#include "WebPageClient.h"
#include "WebPage_p.h"
#include "WindowsKeyboardCodes.h"
#include <BlackBerryPlatformLog.h>

namespace WebCore {

// Arbitrary depth limit for the undo stack, to keep it from using
// unbounded memory. This is the maximum number of distinct undoable
// actions -- unbroken stretches of typed characters are coalesced
// into a single action only when not interrupted by string replacements
// triggered by replaceText calls.
static const size_t maximumUndoStackDepth = 1000;

EditorClientBlackBerry::EditorClientBlackBerry(BlackBerry::WebKit::WebPage* page)
    : m_page(page)
    , m_waitingForCursorFocus(false)
    , m_spellCheckState(SpellCheckDefault)
    , m_inRedo(false)
{
}

void EditorClientBlackBerry::pageDestroyed()
{
    delete this;
}

bool EditorClientBlackBerry::shouldDeleteRange(Range* range)
{
    if (m_page->d->m_dumpRenderTree)
        return m_page->d->m_dumpRenderTree->shouldDeleteDOMRange(range);
    return true;
}

bool EditorClientBlackBerry::shouldShowDeleteInterface(HTMLElement*)
{
    notImplemented();
    return false;
}

bool EditorClientBlackBerry::smartInsertDeleteEnabled()
{
    notImplemented();
    return false;
}

bool EditorClientBlackBerry::isSelectTrailingWhitespaceEnabled()
{
    if (m_page->d->m_dumpRenderTree)
        return m_page->d->m_dumpRenderTree->isSelectTrailingWhitespaceEnabled();
    return false;
}

void EditorClientBlackBerry::enableSpellChecking(bool enable)
{
    m_spellCheckState = enable ? SpellCheckDefault : SpellCheckOff;
}

bool EditorClientBlackBerry::shouldSpellCheckFocusedField()
{
    const Frame* frame = m_page->d->focusedOrMainFrame();
    if (!frame || !frame->document() || !frame->editor())
        return false;

    const Node* node = frame->document()->focusedNode();
    // NOTE: This logic is taken from EditorClientImpl::shouldSpellcheckByDefault
    // If |node| is null, we default to allowing spellchecking. This is done in
    // order to mitigate the issue when the user clicks outside the textbox, as a
    // result of which |node| becomes null, resulting in all the spell check
    // markers being deleted. Also, the Frame will decide not to do spellchecking
    // if the user can't edit - so returning true here will not cause any problems
    // to the Frame's behavior.
    if (!node)
        return true;

    // If the field does not support autocomplete, do not do spellchecking.
    if (node->isElementNode()) {
        const Element* element = static_cast<const Element*>(node);
        if (element->hasTagName(HTMLNames::inputTag) && !BlackBerry::WebKit::DOMSupport::elementSupportsAutocomplete(element))
            return false;
    }

    // Check if the node disables spell checking
    return frame->editor()->isSpellCheckingEnabledInFocusedNode();
}

bool EditorClientBlackBerry::isContinuousSpellCheckingEnabled()
{
    if (m_spellCheckState == SpellCheckOff)
        return false;
    if (m_spellCheckState == SpellCheckOn)
        return true;
    return shouldSpellCheckFocusedField();
}

void EditorClientBlackBerry::toggleContinuousSpellChecking()
{
    // Use the current state to determine how to toggle, if it hasn't
    // been explicitly set, it will toggle based on the field type.
    if (isContinuousSpellCheckingEnabled())
        m_spellCheckState = SpellCheckOff;
    else
        m_spellCheckState = SpellCheckOn;
}

bool EditorClientBlackBerry::isGrammarCheckingEnabled()
{
    notImplemented();
    return false;
}

void EditorClientBlackBerry::toggleGrammarChecking()
{
    notImplemented();
}

int EditorClientBlackBerry::spellCheckerDocumentTag()
{
    notImplemented();
    return 0;
}

bool EditorClientBlackBerry::shouldBeginEditing(Range* range)
{
    if (m_page->d->m_dumpRenderTree)
        return m_page->d->m_dumpRenderTree->shouldBeginEditingInDOMRange(range);

    return m_page->d->m_inputHandler->shouldAcceptInputFocus();
}

bool EditorClientBlackBerry::shouldEndEditing(Range* range)
{
    if (m_page->d->m_dumpRenderTree)
        return m_page->d->m_dumpRenderTree->shouldEndEditingInDOMRange(range);
    return true;
}

bool EditorClientBlackBerry::shouldInsertNode(Node* node, Range* range, EditorInsertAction insertAction)
{
    if (m_page->d->m_dumpRenderTree)
        return m_page->d->m_dumpRenderTree->shouldInsertNode(node, range, static_cast<int>(insertAction));
    return true;
}

bool EditorClientBlackBerry::shouldInsertText(const WTF::String& text, Range* range, EditorInsertAction insertAction)
{
    if (m_page->d->m_dumpRenderTree)
        return m_page->d->m_dumpRenderTree->shouldInsertText(text, range, static_cast<int>(insertAction));
    return true;
}

bool EditorClientBlackBerry::shouldChangeSelectedRange(Range* fromRange, Range* toRange, EAffinity affinity, bool stillSelecting)
{
    if (m_page->d->m_dumpRenderTree)
        return m_page->d->m_dumpRenderTree->shouldChangeSelectedDOMRangeToDOMRangeAffinityStillSelecting(fromRange, toRange, static_cast<int>(affinity), stillSelecting);

    Frame* frame = m_page->d->m_page->focusController()->focusedOrMainFrame();
    if (frame && frame->document()) {
        if (frame->document()->focusedNode() && frame->document()->focusedNode()->hasTagName(HTMLNames::selectTag))
            return false;

        // Update the focus state to re-show the keyboard if needed.
        // FIXME, this should be removed and strictly be a show keyboard call if
        // focus is active.
        if (m_page->d->m_inputHandler->isInputMode())
            m_page->d->m_inputHandler->nodeFocused(frame->document()->focusedNode());
    }

    return true;
}

bool EditorClientBlackBerry::shouldApplyStyle(CSSStyleDeclaration*, Range*)
{
    notImplemented();
    return true;
}

bool EditorClientBlackBerry::shouldMoveRangeAfterDelete(Range*, Range*)
{
    notImplemented();
    return true;
}

void EditorClientBlackBerry::didBeginEditing()
{
    if (m_page->d->m_dumpRenderTree)
        m_page->d->m_dumpRenderTree->didBeginEditing();
}

void EditorClientBlackBerry::respondToChangedContents()
{
    if (m_page->d->m_dumpRenderTree)
        m_page->d->m_dumpRenderTree->didChange();
}

void EditorClientBlackBerry::respondToChangedSelection(Frame* frame)
{
    if (m_waitingForCursorFocus)
        m_waitingForCursorFocus = false;
    else
        m_page->d->selectionChanged(frame);

    if (m_page->d->m_dumpRenderTree)
        m_page->d->m_dumpRenderTree->didChangeSelection();
}

void EditorClientBlackBerry::didEndEditing()
{
    if (m_page->d->m_dumpRenderTree)
        m_page->d->m_dumpRenderTree->didEndEditing();
}

void EditorClientBlackBerry::respondToSelectionAppearanceChange()
{
    m_page->d->m_selectionHandler->selectionPositionChanged();
}

void EditorClientBlackBerry::didWriteSelectionToPasteboard()
{
    notImplemented();
}

void EditorClientBlackBerry::didSetSelectionTypesForPasteboard()
{
    notImplemented();
}

void EditorClientBlackBerry::registerCommandForUndo(PassRefPtr<EditCommand> command)
{
    if (m_undoStack.size() == maximumUndoStackDepth)
        m_undoStack.removeFirst(); // drop oldest item off the far end
    if (!m_inRedo)
        m_redoStack.clear();
    m_undoStack.append(command);
}

void EditorClientBlackBerry::registerCommandForRedo(PassRefPtr<EditCommand> command)
{
    m_redoStack.append(command);
}

void EditorClientBlackBerry::clearUndoRedoOperations()
{
    m_undoStack.clear();
    m_redoStack.clear();
}

bool EditorClientBlackBerry::canUndo() const
{
    return !m_undoStack.isEmpty();
}

bool EditorClientBlackBerry::canRedo() const
{
    return !m_redoStack.isEmpty();
}

bool EditorClientBlackBerry::canCopyCut(Frame*, bool defaultValue) const
{
    return defaultValue;
}

bool EditorClientBlackBerry::canPaste(Frame*, bool defaultValue) const
{
    return defaultValue;
}

void EditorClientBlackBerry::undo()
{
    if (canUndo()) {
        EditCommandStack::iterator back = --m_undoStack.end();
        RefPtr<EditCommand> command(*back);
        m_undoStack.remove(back);

        command->unapply();
        // unapply will call us back to push this command onto the redo stack.
    }
}

void EditorClientBlackBerry::redo()
{
    if (canRedo()) {
        EditCommandStack::iterator back = --m_redoStack.end();
        RefPtr<EditCommand> command(*back);
        m_redoStack.remove(back);

        ASSERT(!m_inRedo);
        m_inRedo = true;
        command->reapply();
        // reapply will call us back to push this command onto the undo stack.
        m_inRedo = false;
    }
}

static const unsigned CtrlKey = 1 << 0;
static const unsigned AltKey = 1 << 1;
static const unsigned ShiftKey = 1 << 2;

struct KeyDownEntry {
    unsigned virtualKey;
    unsigned modifiers;
    const char* name;
};

static const KeyDownEntry keyDownEntries[] = {
    { VK_LEFT,   0,                  "MoveLeft"                                    },
    { VK_LEFT,   ShiftKey,           "MoveLeftAndModifySelection"                  },
    { VK_LEFT,   CtrlKey,            "MoveWordLeft"                                },
    { VK_LEFT,   CtrlKey | ShiftKey, "MoveWordLeftAndModifySelection"              },
    { VK_RIGHT,  0,                  "MoveRight"                                   },
    { VK_RIGHT,  ShiftKey,           "MoveRightAndModifySelection"                 },
    { VK_RIGHT,  CtrlKey,            "MoveWordRight"                               },
    { VK_RIGHT,  CtrlKey | ShiftKey, "MoveWordRightAndModifySelection"             },
    { VK_UP,     0,                  "MoveUp"                                      },
    { VK_UP,     ShiftKey,           "MoveUpAndModifySelection"                    },
    { VK_DOWN,   0,                  "MoveDown"                                    },
    { VK_DOWN,   ShiftKey,           "MoveDownAndModifySelection"                  },
    { VK_PRIOR,  0,                  "MovePageUp"                                  },
    { VK_PRIOR,  ShiftKey,           "MovePageUpAndModifySelection"                },
    { VK_NEXT,   0,                  "MovePageDown"                                },
    { VK_NEXT,   ShiftKey,           "MovePageDownAndModifySelection"              },
    { VK_HOME,   0,                  "MoveToBeginningOfLine"                       },
    { VK_HOME,   ShiftKey,           "MoveToBeginningOfLineAndModifySelection"     },
    { VK_HOME,   CtrlKey,            "MoveToBeginningOfDocument"                   },
    { VK_HOME,   CtrlKey | ShiftKey, "MoveToBeginningOfDocumentAndModifySelection" },
    { VK_END,    0,                  "MoveToEndOfLine"                             },
    { VK_END,    ShiftKey,           "MoveToEndOfLineAndModifySelection"           },
    { VK_END,    CtrlKey,            "MoveToEndOfDocument"                         },
    { VK_END,    CtrlKey | ShiftKey, "MoveToEndOfDocumentAndModifySelection"       },

    { 'B',       CtrlKey,            "ToggleBold"                                  },
    { 'I',       CtrlKey,            "ToggleItalic"                                },

#if OS(QNX)
    { 'C',       CtrlKey,            "Copy"                                        },
    { 'V',       CtrlKey,            "Paste"                                       },
    { 'X',       CtrlKey,            "Cut"                                         },
    { VK_INSERT, CtrlKey,            "Copy"                                        },
    { VK_DELETE, ShiftKey,           "Cut"                                         },
    { VK_INSERT, ShiftKey,           "Paste"                                       },
#endif

    { 'A',       CtrlKey,            "SelectAll"                                   },
    { 'Z',       CtrlKey,            "Undo"                                        },
    { 'Z',       CtrlKey | ShiftKey, "Redo"                                        },
};

const char* EditorClientBlackBerry::interpretKeyEvent(const KeyboardEvent* event)
{
    ASSERT(event->type() == eventNames().keydownEvent);

    static HashMap<int, const char*>* keyDownCommandsMap = 0;

    if (!keyDownCommandsMap) {
        keyDownCommandsMap = new HashMap<int, const char*>;

        for (unsigned i = 0; i < WTF_ARRAY_LENGTH(keyDownEntries); i++)
            keyDownCommandsMap->set(keyDownEntries[i].modifiers << 16 | keyDownEntries[i].virtualKey, keyDownEntries[i].name);
    }

    unsigned modifiers = 0;
    if (event->shiftKey())
        modifiers |= ShiftKey;
    if (event->altKey())
        modifiers |= AltKey;
    if (event->ctrlKey())
        modifiers |= CtrlKey;

    int mapKey = modifiers << 16 | event->keyCode();
    return mapKey ? keyDownCommandsMap->get(mapKey) : 0;
}

void EditorClientBlackBerry::handleKeyboardEvent(KeyboardEvent* event)
{
    Node* node = event->target()->toNode();
    ASSERT(node);
    Frame* frame = node->document()->frame();
    ASSERT(frame);

    ASSERT(m_page->d->m_inputHandler);

    const PlatformKeyboardEvent* platformEvent = event->keyEvent();
    if (!platformEvent)
        return;

    Node* start = frame->selection()->start().anchorNode();
    if (!start)
        return;

    if (start->isContentEditable()) {

        // Text insertion commands should only be triggered from keypressEvent.
        // There is an assert guaranteeing this in
        // EventHandler::handleTextInputEvent. Note that windowsVirtualKeyCode
        // is not set for keypressEvent: special keys should have been already
        // handled in keydownEvent, which is called first.
        if (event->type() == eventNames().keypressEvent) {
            if (event->charCode() == '\r') { // \n has been converted to \r in PlatformKeyboardEventBlackBerry
                if (frame->editor()->command("InsertNewline").execute(event))
                    event->setDefaultHandled();
            } else if ((event->charCode() >= ' ' || event->charCode() == '\t') // Don't insert null or control characters as they can result in unexpected behaviour
                    && !platformEvent->text().isEmpty()) {
                if (frame->editor()->insertText(platformEvent->text(), event))
                    event->setDefaultHandled();
            }
            return;
        }

        if (event->type() != eventNames().keydownEvent)
            return;

        bool handledEvent = false;
        String commandName = interpretKeyEvent(event);

        if (!commandName.isEmpty())
            handledEvent = frame->editor()->command(commandName).execute();
        else {
            switch (platformEvent->windowsVirtualKeyCode()) {
            case VK_BACK:
                if (platformEvent->ctrlKey())
                    handledEvent = frame->editor()->deleteWithDirection(DirectionBackward, WordGranularity, false, true);
                else
                    handledEvent = frame->editor()->deleteWithDirection(DirectionBackward, CharacterGranularity, false, true);
                break;
            case VK_DELETE:
                if (platformEvent->ctrlKey())
                    handledEvent = frame->editor()->deleteWithDirection(DirectionForward, WordGranularity, false, true);
                else
                    handledEvent = frame->editor()->deleteWithDirection(DirectionForward, CharacterGranularity, false, true);
                break;
            case VK_ESCAPE:
                handledEvent = frame->page()->focusController()->setFocusedNode(0, frame);
                break;
            case VK_TAB:
            default:
                return;
            }
        }
        if (handledEvent)
            event->setDefaultHandled();
    }
}

void EditorClientBlackBerry::handleInputMethodKeydown(KeyboardEvent*)
{
    notImplemented();
}

void EditorClientBlackBerry::textFieldDidBeginEditing(Element*)
{
    notImplemented();
}

void EditorClientBlackBerry::textFieldDidEndEditing(Element*)
{
    notImplemented();
}

void EditorClientBlackBerry::textDidChangeInTextField(Element*)
{
    notImplemented();
}

bool EditorClientBlackBerry::doTextFieldCommandFromEvent(Element*, KeyboardEvent*)
{
    notImplemented();
    return false;
}

void EditorClientBlackBerry::textWillBeDeletedInTextField(Element*)
{
    notImplemented();
}

void EditorClientBlackBerry::textDidChangeInTextArea(Element*)
{
    notImplemented();
}

void EditorClientBlackBerry::ignoreWordInSpellDocument(const WTF::String&)
{
    notImplemented();
}

void EditorClientBlackBerry::learnWord(const WTF::String&)
{
    notImplemented();
}

void EditorClientBlackBerry::checkSpellingOfString(const UChar* text, int textLength, int* misspellLocation, int* misspellLength)
{
    m_page->client()->checkSpellingOfString(text, textLength, *misspellLocation, *misspellLength);
}

WTF::String EditorClientBlackBerry::getAutoCorrectSuggestionForMisspelledWord(const WTF::String& misspelledWord)
{
    notImplemented();
    return WTF::String();
};

void EditorClientBlackBerry::checkGrammarOfString(const UChar*, int, WTF::Vector<GrammarDetail, 0u>&, int*, int*)
{
    notImplemented();
}

void EditorClientBlackBerry::requestCheckingOfString(SpellChecker*, int, TextCheckingTypeMask, const String&)
{
    notImplemented();
}

TextCheckerClient* EditorClientBlackBerry::textChecker()
{
    return this;
}

void EditorClientBlackBerry::updateSpellingUIWithGrammarString(const WTF::String&, const GrammarDetail&)
{
    notImplemented();
}

void EditorClientBlackBerry::updateSpellingUIWithMisspelledWord(const WTF::String&)
{
    notImplemented();
}

void EditorClientBlackBerry::showSpellingUI(bool)
{
    notImplemented();
}

bool EditorClientBlackBerry::spellingUIIsShowing()
{
    notImplemented();
    return false;
}

void EditorClientBlackBerry::getGuessesForWord(const WTF::String&, WTF::Vector<WTF::String, 0u>&)
{
    notImplemented();
}

void EditorClientBlackBerry::getGuessesForWord(const String&, const String&, Vector<String>&)
{
    notImplemented();
}

void EditorClientBlackBerry::willSetInputMethodState()
{
    notImplemented();
}

void EditorClientBlackBerry::setInputMethodState(bool active)
{
    Frame* frame = m_page->d->m_page->focusController()->focusedOrMainFrame();
    // Determines whether or not to provide input assistance. Password fields
    // do not have this flag active, so it needs to be overridden.
    if (frame && frame->document()) {
        if (Node* focusNode = frame->document()->focusedNode()) {
            if (!active && focusNode->hasTagName(HTMLNames::inputTag)
                && static_cast<HTMLInputElement*>(focusNode)->isPasswordField())
                    active = true;

            if (active) {
                m_page->d->m_inputHandler->nodeFocused(focusNode);
                return;
            }
        }
    }
    // No frame or document or a node that doesn't support IME.
    m_page->d->m_inputHandler->nodeFocused(0);
}

} // namespace WebCore
