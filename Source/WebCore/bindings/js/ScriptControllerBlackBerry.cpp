/*
 * Copyright (C) 2009 Torch Mobile Inc. http://www.torchmobile.com/
 */


#include "config.h"
#include "ScriptController.h"

#include "Bridge.h"
#include "PluginView.h"
#include "runtime_root.h"

namespace WebCore {

PassRefPtr<JSC::Bindings::Instance> ScriptController::createScriptInstanceForWidget(Widget* widget)
{
    if (!widget->isPluginView())
        return 0;

    return static_cast<PluginView*>(widget)->bindingInstance();
}

}
