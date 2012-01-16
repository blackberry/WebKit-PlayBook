/*
 * Copyright (C) Research In Motion Limited 2010. All rights reserved.
 */

#include "config.h"
#include "ClientExtension.h"

#include "Frame.h"
#include "JavaScriptCore/JavaScript.h"
#include "JavaScriptCore/API/JSCallbackObject.h"
#include "JavaScriptCore/JSObjectRef.h"
#include "JavaScriptCore/JSStringRef.h"
#include "JavaScriptCore/JSValueRef.h"
#include "OwnArrayPtr.h"
#include "WebPageClient.h"

#include <string>

using namespace WTF;
using namespace WebCore;
using namespace Olympia::WebKit;

using std::string;

static JSValueRef clientExtensionMethod(
    JSContextRef ctx, JSObjectRef functionObject, JSObjectRef thisObject,
    size_t argumentCount, const JSValueRef arguments[], JSValueRef* exception)
{
    JSValueRef jsRetVal = JSValueMakeUndefined(ctx);

    if (argumentCount > 0) {
        char** strArgs = new char*[argumentCount];
        for (unsigned int i = 0; i < argumentCount; ++i) {
            JSStringRef string = JSValueToStringCopy(ctx, arguments[i], 0);
            size_t sizeUTF8 = JSStringGetMaximumUTF8CStringSize(string);
            strArgs[i] = new char[sizeUTF8];
            JSStringGetUTF8CString(string, strArgs[i], sizeUTF8);
            JSStringRelease(string);
        }

        WebPageClient* client = reinterpret_cast<WebPageClient*>(JSObjectGetPrivate(thisObject));

        string retVal = client->invokeClientJavaScriptCallback(strArgs, argumentCount).utf8();

        if (!retVal.empty())
            jsRetVal = JSValueMakeString(ctx, JSStringCreateWithUTF8CString(retVal.c_str()));

        for (unsigned int i = 0; i < argumentCount; ++i)
            delete[] strArgs[i];
        delete[] strArgs;
    }

    return jsRetVal;
}

static void clientExtensionInitialize(JSContextRef context, JSObjectRef object)
{
    UNUSED_PARAM(context);
    UNUSED_PARAM(object);
}

static void clientExtensionFinalize(JSObjectRef object)
{
    UNUSED_PARAM(object);
}

static JSStaticFunction clientExtensionStaticFunctions[] = {
    { "callExtensionMethod", clientExtensionMethod, kJSPropertyAttributeNone },
    { 0, 0, 0 }
};

static JSStaticValue clientExtensionStaticValues[] = {
    { 0, 0, 0, 0 }
};

// FIXME: Revisit the creation of this class and make sure this is the best way to approach it

static JSClassRef clientExtensionClass()
{
    static JSClassRef jsClass;
    if (!jsClass) {
        JSClassDefinition definition = kJSClassDefinitionEmpty;
        definition.staticValues = clientExtensionStaticValues;
        definition.staticFunctions = clientExtensionStaticFunctions;
        definition.initialize = clientExtensionInitialize;
        definition.finalize = clientExtensionFinalize;
        jsClass = JSClassCreate(&definition);
    }
    return jsClass;
}

void attachExtensionObjectToFrame(Frame* frame, WebPageClient* client)
{
    JSC::JSLock lock(JSC::SilenceAssertionsOnly);

    JSDOMWindow* window = frame->script()->windowShell(mainThreadNormalWorld())->window();

    JSC::ExecState* exec = window->globalExec();
    JSContextRef scriptCtx = toRef(exec);

    JSClassRef clientClass = clientExtensionClass();

    JSObjectRef clientClassObject = JSObjectMake(scriptCtx, clientClass, 0);
    JSObjectSetPrivate(clientClassObject, reinterpret_cast<void*>(client));

    JSC::UString name("qnx");

    JSC::PutPropertySlot slot;
    window->put(exec, JSC::Identifier(exec, name), toJS(clientClassObject), slot);
}
