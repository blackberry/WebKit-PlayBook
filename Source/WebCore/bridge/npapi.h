/* -*- Mode: C; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 1998
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

#ifndef npapi_h_
#define npapi_h_

#if defined(__OS2__)
#pragma pack(1)
#endif

#include "nptypes.h"

#if defined(__OS2__) || defined(OS2)
#ifndef XP_OS2
#define XP_OS2 1
#endif
#endif

#ifdef INCLUDE_JAVA
#include "jri.h"                /* Java Runtime Interface */
#else
#define jref    void *
#define JRIEnv  void
#endif

#if defined(_WIN32) && !defined(__SYMBIAN32__)
#include <windows.h>
#ifndef XP_WIN
#define XP_WIN 1
#endif
#endif

#if defined(__SYMBIAN32__)
#ifndef XP_SYMBIAN
#define XP_SYMBIAN 1
#endif
#endif

#if defined(__APPLE_CC__) && !defined(XP_UNIX)
#ifndef XP_MACOSX
#define XP_MACOSX 1
#endif
#endif

#if defined(XP_MACOSX) && defined(__LP64__)
#define NP_NO_QUICKDRAW
#define NP_NO_CARBON
#endif

#if defined(XP_MACOSX)
#include <ApplicationServices/ApplicationServices.h>
#include <OpenGL/OpenGL.h>
#ifndef NP_NO_CARBON
#include <Carbon/Carbon.h>
#endif
#endif

#if defined(__QNXNTO__) && !defined(XP_UNIX)
#define XP_UNIX
#endif

#if defined(XP_UNIX)
#if defined(X_PROTOCOL)
#include <X11/Xlib.h>
#include <X11/Xutil.h>
#endif
#include <stdio.h>
#if defined(__QNXNTO__)
#include <sys/neutrino.h>
#endif
#endif

#if defined(XP_SYMBIAN)
#include <QEvent>
#include <QRegion>
#endif

/*----------------------------------------------------------------------*/
/*                        Plugin Version Constants                      */
/*----------------------------------------------------------------------*/

#define NP_VERSION_MAJOR 0
#define NP_VERSION_MINOR 24


/* The OS/2 version of Netscape uses RC_DATA to define the
   mime types, file extensions, etc that are required.
   Use a vertical bar to separate types, end types with \0.
   FileVersion and ProductVersion are 32bit ints, all other
   entries are strings that MUST be terminated with a \0.

AN EXAMPLE:

RCDATA NP_INFO_ProductVersion { 1,0,0,1,}

RCDATA NP_INFO_MIMEType    { "video/x-video|",
                             "video/x-flick\0" }
RCDATA NP_INFO_FileExtents { "avi|",
                             "flc\0" }
RCDATA NP_INFO_FileOpenName{ "MMOS2 video player(*.avi)|",
                             "MMOS2 Flc/Fli player(*.flc)\0" }

RCDATA NP_INFO_FileVersion       { 1,0,0,1 }
RCDATA NP_INFO_CompanyName       { "Netscape Communications\0" }
RCDATA NP_INFO_FileDescription   { "NPAVI32 Extension DLL\0"
RCDATA NP_INFO_InternalName      { "NPAVI32\0" )
RCDATA NP_INFO_LegalCopyright    { "Copyright Netscape Communications \251 1996\0"
RCDATA NP_INFO_OriginalFilename  { "NVAPI32.DLL" }
RCDATA NP_INFO_ProductName       { "NPAVI32 Dynamic Link Library\0" }
*/
/* RC_DATA types for version info - required */
#define NP_INFO_ProductVersion      1
#define NP_INFO_MIMEType            2
#define NP_INFO_FileOpenName        3
#define NP_INFO_FileExtents         4
/* RC_DATA types for version info - used if found */
#define NP_INFO_FileDescription     5
#define NP_INFO_ProductName         6
/* RC_DATA types for version info - optional */
#define NP_INFO_CompanyName         7
#define NP_INFO_FileVersion         8
#define NP_INFO_InternalName        9
#define NP_INFO_LegalCopyright      10
#define NP_INFO_OriginalFilename    11

#ifndef RC_INVOKED

/*----------------------------------------------------------------------*/
/*                       Definition of Basic Types                      */
/*----------------------------------------------------------------------*/
#ifdef __QNXNTO__
typedef unsigned int uint32;
typedef short int16;
typedef int int32;
#endif

#ifndef FALSE
#define FALSE (0)
#endif
#ifndef TRUE
#define TRUE (1)
#endif
#ifndef NULL
#define NULL (0L)
#endif

typedef unsigned char NPBool;
typedef int16_t       NPError;
typedef int16_t       NPReason;
typedef char*         NPMIMEType;

/*----------------------------------------------------------------------*/
/*                       Structures and definitions                     */
/*----------------------------------------------------------------------*/

#if !defined(__LP64__)
#if defined(XP_MACOSX)
#pragma options align=mac68k
#endif
#endif /* __LP64__ */

/*
 *  NPP is a plug-in's opaque instance handle
 */
typedef struct _NPP
{
  void* pdata;      /* plug-in private data */
  void* ndata;      /* netscape private data */
} NPP_t;

typedef NPP_t*  NPP;

typedef struct _NPStream
{
  void*    pdata; /* plug-in private data */
  void*    ndata; /* netscape private data */
  const    char* url;
  uint32_t end;
  uint32_t lastmodified;
  void*    notifyData;
  const    char* headers; /* Response headers from host.
                           * Exists only for >= NPVERS_HAS_RESPONSE_HEADERS.
                           * Used for HTTP only; NULL for non-HTTP.
                           * Available from NPP_NewStream onwards.
                           * Plugin should copy this data before storing it.
                           * Includes HTTP status line and all headers,
                           * preferably verbatim as received from server,
                           * headers formatted as in HTTP ("Header: Value"),
                           * and newlines (\n, NOT \r\n) separating lines.
                           * Terminated by \n\0 (NOT \n\n\0). */
} NPStream;

typedef struct _NPByteRange
{
  int32_t  offset; /* negative offset means from the end */
  uint32_t length;
  struct _NPByteRange* next;
} NPByteRange;

typedef struct _NPSavedData
{
  int32_t len;
  void*   buf;
} NPSavedData;

typedef struct _NPRect
{
  uint16_t top;
  uint16_t left;
  uint16_t bottom;
  uint16_t right;
} NPRect;

typedef struct _NPSize
{
  int32_t width;
  int32_t height;
} NPSize;

typedef enum {
  NPFocusNext = 0,
  NPFocusPrevious = 1
} NPFocusDirection;

/* Return values for NPP_HandleEvent */
#define kNPEventNotHandled 0
#define kNPEventHandled 1
/* Exact meaning must be spec'd in event model. */
#define kNPEventStartIME 2

#if defined(XP_UNIX)
/*
 * Unix specific structures and definitions
 */

/*
 * Callback Structures.
 *
 * These are used to pass additional platform specific information.
 */
enum {
  NP_SETWINDOW = 1,
  NP_PRINT
};

typedef struct
{
  int32_t type;
} NPAnyCallbackStruct;

#if defined(__QNXNTO__)

typedef unsigned int uint32;
typedef short int16;
typedef int int32;

typedef struct
{
    int32           type;
    void            *display;    // TODO: MVN: display?
    void            *visual;
    void            *colormap;    // TODO: MVN: colormap?
    unsigned int    depth;
    const char      *windowGroup; // winmgr group
    double          zoomFactor;
} NPSetWindowCallbackStruct;
#else
typedef struct
{
  int32_t      type;
#if defined(X_PROTOCOL)
  Display*     display;
  Visual*      visual;
  Colormap     colormap;
#endif
  unsigned int depth;
} NPSetWindowCallbackStruct;
#endif

typedef struct
{
  int32_t type;
  FILE* fp;
} NPPrintCallbackStruct;

#endif /* XP_UNIX */

#if defined(XP_MACOSX)
typedef enum {
#ifndef NP_NO_QUICKDRAW
  NPDrawingModelQuickDraw = 0,
#endif
  NPDrawingModelCoreGraphics = 1,
  NPDrawingModelOpenGL = 2,
  NPDrawingModelCoreAnimation = 3
} NPDrawingModel;

typedef enum {
#ifndef NP_NO_CARBON
  NPEventModelCarbon = 0,
#endif
  NPEventModelCocoa = 1
} NPEventModel;
#endif

/*
 *   The following masks are applied on certain platforms to NPNV and
 *   NPPV selectors that pass around pointers to COM interfaces. Newer
 *   compilers on some platforms may generate vtables that are not
 *   compatible with older compilers. To prevent older plugins from
 *   not understanding a new browser's ABI, these masks change the
 *   values of those selectors on those platforms. To remain backwards
 *   compatible with different versions of the browser, plugins can
 *   use these masks to dynamically determine and use the correct C++
 *   ABI that the browser is expecting. This does not apply to Windows
 *   as Microsoft's COM ABI will likely not change.
 */

#define NP_ABI_GCC3_MASK  0x10000000
/*
 *   gcc 3.x generated vtables on UNIX and OSX are incompatible with
 *   previous compilers.
 */
#if (defined(XP_UNIX) && defined(__GNUC__) && (__GNUC__ >= 3))
#define _NP_ABI_MIXIN_FOR_GCC3 NP_ABI_GCC3_MASK
#else
#define _NP_ABI_MIXIN_FOR_GCC3 0
#endif

#if defined(XP_MACOSX)
#define NP_ABI_MACHO_MASK 0x01000000
#define _NP_ABI_MIXIN_FOR_MACHO NP_ABI_MACHO_MASK
#else
#define _NP_ABI_MIXIN_FOR_MACHO 0
#endif

#define NP_ABI_MASK (_NP_ABI_MIXIN_FOR_GCC3 | _NP_ABI_MIXIN_FOR_MACHO)

/*
 * List of variable names for which NPP_GetValue shall be implemented
 */
typedef enum {
  NPPVpluginNameString = 1,
  NPPVpluginDescriptionString,
  NPPVpluginWindowBool,
  NPPVpluginTransparentBool,
  NPPVjavaClass,                /* Not implemented in WebKit */
  NPPVpluginWindowSize,         /* Not implemented in WebKit */
  NPPVpluginTimerInterval,      /* Not implemented in WebKit */
  NPPVpluginScriptableInstance = (10 | NP_ABI_MASK), /* Not implemented in WebKit */
  NPPVpluginScriptableIID = 11, /* Not implemented in WebKit */
  NPPVjavascriptPushCallerBool = 12,  /* Not implemented in WebKit */
  NPPVpluginKeepLibraryInMemory = 13, /* Not implemented in WebKit */
  NPPVpluginNeedsXEmbed         = 14, /* Not implemented in WebKit */

  /* Get the NPObject for scripting the plugin. Introduced in NPAPI minor version 14.
   */
  NPPVpluginScriptableNPObject  = 15,

  /* Get the plugin value (as \0-terminated UTF-8 string data) for
   * form submission if the plugin is part of a form. Use
   * NPN_MemAlloc() to allocate memory for the string data. Introduced
   * in NPAPI minor version 15.
   */
  NPPVformValue = 16,    /* Not implemented in WebKit */

  NPPVpluginUrlRequestsDisplayedBool = 17, /* Not implemented in WebKit */

  /* Checks if the plugin is interested in receiving the http body of
   * all http requests (including failed ones, http status != 200).
   */
  NPPVpluginWantsAllNetworkStreams = 18,

  /* Browsers can retrieve a native ATK accessibility plug ID via this variable. */
  NPPVpluginNativeAccessibleAtkPlugId = 19,

  /* Checks to see if the plug-in would like the browser to load the "src" attribute. */
  NPPVpluginCancelSrcStream = 20

#if defined(XP_MACOSX)
  /* Used for negotiating drawing models */
  , NPPVpluginDrawingModel = 1000
  /* Used for negotiating event models */
  , NPPVpluginEventModel = 1001
  /* In the NPDrawingModelCoreAnimation drawing model, the browser asks the plug-in for a Core Animation layer. */
  , NPPVpluginCoreAnimationLayer = 1003
#endif

#if defined(MOZ_PLATFORM_MAEMO) && (MOZ_PLATFORM_MAEMO >= 5)
  , NPPVpluginWindowlessLocalBool = 2002
#endif

#if defined(__QNXNTO__)
    /* Used when the plugin returns 0 from NPN_WriteReady and wishes the browser
     * to wait a certain amount of millis before calling NPN_WriteReady again.
     */
    , NPPVdataDeliveryDelayMs = 100

    /* Get handles of all the Screen windows the plugin created, including the video window */
    , NPPVpluginScreenWindow = 101

    /* The plugin is playing even though it's received a background event; i.e., it may
     * need the container to remain fully active when it normally wouldn't do so
     */
    , NPPVbackgroundPlayBool = 102
#endif
} NPPVariable;

/*
 * List of variable names for which NPN_GetValue should be implemented.
 */
typedef enum {
  NPNVxDisplay = 1,
  NPNVxtAppContext,
  NPNVnetscapeWindow,
  NPNVjavascriptEnabledBool,
  NPNVasdEnabledBool,
  NPNVisOfflineBool,

  NPNVserviceManager = (10 | NP_ABI_MASK),  /* Not implemented in WebKit */
  NPNVDOMElement     = (11 | NP_ABI_MASK),  /* Not implemented in WebKit */
  NPNVDOMWindow      = (12 | NP_ABI_MASK),  /* Not implemented in WebKit */
  NPNVToolkit        = (13 | NP_ABI_MASK),  /* Not implemented in WebKit */
  NPNVSupportsXEmbedBool = 14,              /* Not implemented in WebKit */

  /* Get the NPObject wrapper for the browser window. */
  NPNVWindowNPObject = 15,

  /* Get the NPObject wrapper for the plugins DOM element. */
  NPNVPluginElementNPObject = 16,

  NPNVSupportsWindowless = 17,

  NPNVprivateModeBool = 18

#if defined(XP_MACOSX)
  /* Used for negotiating drawing models */
  , NPNVpluginDrawingModel = 1000
#ifndef NP_NO_QUICKDRAW
  , NPNVsupportsQuickDrawBool = 2000
#endif
  , NPNVsupportsCoreGraphicsBool = 2001
  , NPNVsupportsOpenGLBool = 2002
  , NPNVsupportsCoreAnimationBool = 2003
#ifndef NP_NO_CARBON
  , NPNVsupportsCarbonBool = 3000 /* TRUE if the browser supports the Carbon event model */
#endif
  , NPNVsupportsCocoaBool = 3001 /* TRUE if the browser supports the Cocoa event model */
#endif /* XP_MACOSX */
#if defined(MOZ_PLATFORM_MAEMO) && (MOZ_PLATFORM_MAEMO >= 5)
  , NPNVSupportsWindowlessLocal = 2002
#endif

#if defined(__QNXNTO__)
  , NPNVZoomFactor = 4000
  , NPNVNPCallbacksPtr = 4001
  , NPNVRootWindowGroup = 4002
  , NPNVBrowserWindowGroup = 4003
  , NPNVBrowserDisplayContext = 4004
  , NPNVPluginWindowPrefix = 4005
#endif
} NPNVariable;

typedef enum {
  NPNURLVCookie = 501,
  NPNURLVProxy
} NPNURLVariable;

/*
 * The type of Toolkit the widgets use
 */
typedef enum {
  NPNVGtk12 = 1,
  NPNVGtk2
} NPNToolkitType;

/*
 * The type of a NPWindow - it specifies the type of the data structure
 * returned in the window field.
 */
typedef enum {
  NPWindowTypeWindow = 1,
  NPWindowTypeDrawable
} NPWindowType;

typedef struct _NPWindow
{
  void* window;  /* Platform specific window handle */
                 /* OS/2: x - Position of bottom left corner */
                 /* OS/2: y - relative to visible netscape window */
  int32_t  x;      /* Position of top left corner relative */
  int32_t  y;      /* to a netscape page. */
  uint32_t width;  /* Maximum window size */
  uint32_t height;
  NPRect   clipRect; /* Clipping rectangle in port coordinates */
#if defined(XP_UNIX) || defined(XP_SYMBIAN)
  void * ws_info; /* Platform-dependent additonal data */
#endif /* XP_UNIX || XP_SYMBIAN */
  NPWindowType type; /* Is this a window or a drawable? */
} NPWindow;

typedef struct _NPImageExpose
{
  char*    data;       /* image pointer */
  int32_t  stride;     /* Stride of data image pointer */
  int32_t  depth;      /* Depth of image pointer */
  int32_t  x;          /* Expose x */
  int32_t  y;          /* Expose y */
  uint32_t width;      /* Expose width */
  uint32_t height;     /* Expose height */
  NPSize   dataSize;   /* Data buffer size */
  float    translateX; /* translate X matrix value */
  float    translateY; /* translate Y matrix value */
  float    scaleX;     /* scale X matrix value */
  float    scaleY;     /* scale Y matrix value */
} NPImageExpose;

typedef struct _NPFullPrint
{
  NPBool pluginPrinted;/* Set TRUE if plugin handled fullscreen printing */
  NPBool printOne;     /* TRUE if plugin should print one copy to default
                          printer */
  void* platformPrint; /* Platform-specific printing info */
} NPFullPrint;

typedef struct _NPEmbedPrint
{
  NPWindow window;
  void* platformPrint; /* Platform-specific printing info */
} NPEmbedPrint;

typedef struct _NPPrint
{
  uint16_t mode;               /* NP_FULL or NP_EMBED */
  union
  {
    NPFullPrint fullPrint;   /* if mode is NP_FULL */
    NPEmbedPrint embedPrint; /* if mode is NP_EMBED */
  } print;
} NPPrint;

#if defined(XP_MACOSX)
#ifndef NP_NO_CARBON
typedef EventRecord NPEvent;
#else
typedef void*  NPEvent;
#endif
#elif defined(XP_SYMBIAN)
typedef QEvent NPEvent;
#elif defined(XP_WIN)
typedef struct _NPEvent
{
  uint16_t event;
  uintptr_t wParam;
  uintptr_t lParam;
} NPEvent;
#elif defined(XP_OS2)
typedef struct _NPEvent
{
  uint32_t event;
  uint32_t wParam;
  uint32_t lParam;
} NPEvent;
#elif defined (XP_UNIX) && defined (X_PROTOCOL)
typedef XEvent NPEvent;
#elif defined(__QNXNTO__)

typedef enum {
    MOUSE_EVENT_UNDEFINED = -1, /*  Undefined mouse event */
    MOUSE_BUTTON_DOWN = 0,      /* A button was pressed on the mouse within the plugin */
    MOUSE_BUTTON_UP,            /* A button was release on the mouse within the plugin */
    MOUSE_MOTION,               /* The mouse was moved with the plugin with or without a button */
    MOUSE_OUTBOUND,             /* The mouse is leaving the plugin */
    MOUSE_OVER                  /* The mouse moved over the plugin */
} NPMouseEventType;

typedef enum {
    TOUCH_EVENT_UNDEFINED = -1, /* Undefined touch event */
    TOUCH_EVENT_START     = 0,  /* All touches listed started. */
    TOUCH_EVENT_END       = 1,  /* All touches listed ended. */
    TOUCH_EVENT_MOVE      = 2,  /* All touches listed moved. */
    TOUCH_EVENT_CANCEL    = 3,  /* All touches listed were cancelled. */
    TOUCH_EVENT_DOUBLETAP = 4,  /* DoubleTapEvent. Consuming this prevents block zoom in browser. */
    TOUCH_EVENT_TOUCHHOLD = 5   /* TouchHold Event. Consuming this prevents context menu in browser. */
} NPTouchEventType;

typedef enum {
    NP_DrawEvent = 0,           /* data = NPDrawEvent* */
    NP_KeyEvent,                /* data = NPKeyboardEvent* */
    NP_MouseEvent,              /* data = NPMouseEvent* */
    NP_TouchEvent,              /* data = NPTouchEvent* */
    NP_OrientationEvent,        /* data = int (angle = 0, 90, 180, 270) */
    NP_FocusGainedEvent,        /* data = reserved (should be 0) */
    NP_FocusLostEvent,          /* data = reserved (should be 0) */
    NP_PauseEvent,              /* data = reserved (should be 0) */
    NP_ResumeEvent,             /* data = reserved (should be 0) */
    NP_FreeMemoryEvent,         /* data = reserved (should be 0) */
    NP_OnLoadEvent,             /* data = reserved (should be 0) */
    NP_OnScreenEvent,           /* data = reserved (should be 0) */
    NP_OffScreenEvent,          /* data = reserved (should be 0) */
    NP_ForegroundEvent,         /* data = reserved (should be 0) */
    NP_BackgroundEvent,         /* data = reserved (should be 0) */
    NP_FullScreenReadyEvent,    /* data = reserved (should be 0) */
    NP_EnterIdleEvent,          /* data = reserved (should be 0) */
    NP_ExitIdleEvent,           /* data = reserved (should be 0) */
    NP_FullScreenExitEvent,     /* data = reserved (should be 0) */
    NP_AppActivatedEvent,       /* data = reserved (should be 0) */
    NP_AppDeactivatedEvent,     /* data = reserved (should be 0) */
    NP_ScreenPowerDownEvent,    /* data = reserved (should be 0) */
    NP_ScreenPowerUpEvent,      /* data = reserved (should be 0) */
    NP_AppStandByEvent,         /* data = reserved (should be 0) */
    NP_WheelEvent,              /* data = NPWheelEvent* */

    // Entirely defined by Plugin.
    NP_CustomEvent = 1024       /* data = NPCustomEvent* */
} NPEventType;

typedef enum {
    FORMAT_RGBA_8888,
    FORMAT_RGB_565
} NPSurfaceFormat;

typedef struct
{
  int32_t numOfWindows;        /* Number of windows in the handles and rects lists. */
  void** windowHandles;        /* List of pointers to screen windows */
  NPRect** windowRects;        /* List of poitners to rectangles defining the window dimensions */
} NPScreenWindowHandles;

typedef struct np_event_t {
    NPEventType type;          /* The plugin event type. */
    void *data;                /* The data ascociated with the event */
} NPEvent;

typedef void* NPSurface;

/** 
 * After returning from NPDrawEvent, the contents of the front buffer will be drawn to the backing store
 * in the areas specified by drawRect, clipped by clipRect. This is a good time to do a swap.
 */
typedef struct {
    NPRect      pluginRect;    /* The containing rectangle for the plugin */
    NPRect      clipRect;      /* The clip rectangle for the plugin */
    NPRect*     drawRect;      /* The areas of the plugin that should be drawn */
    int         drawRectCount; /* Number of rectangles to draw */
    double      zoomFactor;    /* Zoom factor of the page (pluginRect * zoomFactor == screen pixels). */
} NPDrawEvent;

typedef struct {
    int x;                     /* The x position of the mouse */
    int y;                     /* The y position of the mouse */
    NPMouseEventType type;     /* The mouse event type.  See mouse event type enum above */
    unsigned short button;     /* If a button was pressed, the type of button it was */
    int flags;                 /* The flags for the mouse event */
} NPMouseEvent;

typedef struct {
    int        modifiers;      /* See KEYMOD_* in <sys/keycodes.h> */
    int        cap;            /* The key cap.  Check flags to see if valid */
    int        sym;            /* The key sym.  Check flags to see if valid */
    int        scan;           /* The key scan code.  Check flags to see if valid */
    int        flags;          /* See KEY_* in <sys/keycodes.h> */
} NPKeyboardEvent;

typedef struct {
    int touchId;                /* Identifier of the touch point. */
    int clientX;                /* Position in the plugin coordinate space. */
    int clientY;
    int screenX;                /* Position in screen coordinate space. */
    int screenY;
    int pageX;                  /* Position in page coordinate space. */
    int pageY;
} NPTouchPoint;

typedef struct {
    NPTouchEventType type;      /* Type of touch event. */
    int size;                   /* Number of Touch Points */
    NPTouchPoint* points;
} NPTouchEvent;

typedef struct {
    int x;                     /* The x position of the mouse */
    int y;                     /* The y position of the mouse */
    int flags;                 /* The flags for the wheel event */
    int xDelta;                /* Horizontal wheel movement */
    int yDelta;                /* Vertical wheel movement */
} NPWheelEvent;

typedef struct {
    int other[5];               /* For holding custom data */
} NPCustomEvent;

typedef struct {
    /** Registers a set of rectangles that the plugin would like to keep on
        screen. The rectangles are listed in order of priority with the highest
        priority rectangle in location rects[0].  The browser will attempt to keep
        as many of the rectangles on screen as possible and will scroll them into
        view in response to the invocation of this method and other various events.
        The count specifies how many rectangles are in the array. If the count is
        zero it signals the browser that any existing rectangles should be cleared
        and no rectangles will be tracked.
     */
    void (*setVisibleRects)(NPP instance, const NPRect rects[], int32_t count);

    /** Clears any rectangles that are being tracked as a result of a call to
        setVisibleRects. This call is equivalent to setVisibleRect(inst, NULL, 0).
     */
    void (*clearVisibleRects)(NPP instance);

    /** Given a boolean value of true the device will be requested to provide
        a keyboard. A value of false will result in a request to hide the
        keyboard. Further, the on-screen keyboard will not be displayed if a
        physical keyboard is active.
     */
    void (*showKeyboard)(NPP instance, bool value);

    /** Called when a plugin wishes to enter into full screen mode (or has). All input events
     *  are sent to the plugin instance.
     */
    void (*requestFullScreen)(NPP instance);

    /** Called when a plugin wishes to exit from full screen mode.
     */
    void (*exitFullScreen)(NPP instance);

    /** Called when a plugin wishes to be zoomed and centered in the current view.
     */
    void (*requestCenterFitZoom)(NPP instance);

    /**
     * Called when a plugin wishes to change the screen orientation. Orientation can be locked
     * in portrait or landscape. When the orientation has changed the plugin will received an
     * an orientation changed message. The message may not come through if the application prevents
     * the orientation change. You must still call unlockOrientation in any event.
     */
    void (*lockOrientation)(NPP instance, bool landscape);

    /**
     * Called when a plugin is done with locked screen orientation.
     */
    void (*unlockOrientation)(NPP instance);

    /**
     * Call to lock/unlock idling. If you do not unlock before your plugin is destroyed,
     * it will be unlocked for you. Locks are ref-counted.
     */
    void (*preventIdle)(NPP instance, bool preventIdle);

    /**
     * Called to lock the back buffer. Must only be called once and matched with a call to
     * unlockBackBuffer. Never block on the WebKit thread while the surface is locked, if the
     * plugin is destroyed it will wait for the buffer to be unlocked before completing destruction.
     */
    NPSurface (*lockBackBuffer)(NPP instance);

    /**
     * Called to unlock the back buffer.
     */
    void (*unlockBackBuffer)(NPP instance);

    /** 
     * Called to read the front buffer. The front buffer is not destroyed until this
     * function returns. Never block on the WebKit thread while the surface is locked, if the
     * plugin is destroyed it will wait for the buffer to be unlocked before completing destruction.
     * You must NOT write to the front buffer or tearing may result.
     */
    NPSurface (*lockReadFrontBuffer)(NPP instance);

    /**
     * Called to unlock the front buffer when reading is complete.
     */
    void (*unlockReadFrontBuffer)(NPP instance);

    /**
     * Called to swap the front and back buffers. Must not be called while the backBuffers are
     * locked. This function may block if the browser is currently reading from the front buffer
     * to perform a paint operation.
     */
    void (*swapBuffers)(NPP instance);

    /**
     * Called to recreate the front and back buffers. Must not be called while the backBuffers are
     * locked. This function may block if the browser is currently reading from the front buffer
     * to perform a paint operation. If buffers exist already, they are destroyed. If the buffers
     * could not be created, this function will return false.
     */
    bool (*createBuffers)(NPP instance, NPSurfaceFormat format, int width, int height);

    /**
     * Called to destroy the front and back buffers. Must not be called while the backBuffers are
     * locked. This function may block if the browser is currently reading from the front buffer
     * to perform a paint operation. If buffers do not exist, this function has no effect.
     */
    bool (*destroyBuffers)(NPP instance);

    /**
     * Called to resize the front and back buffers. Must not be called while the backBuffers are
     * locked. This function may block if the browser is currently reading from the front buffer
     * to perform a paint operation. If the buffers could not be resized, this function will return
     * false and the buffers will retain their original configuration.
     */
    bool (*resizeBuffers)(NPP instance, NPSurfaceFormat format, int width, int height);

    /**
     * Call to set a transparent rectangle in the browser. The hole will go through the window
     * and all content. Use to show video that is displayed behind the browser. To clear the hole
     * punch rectangle, pass -1 for all dimensions. Coordinates are in plugin space (0,0-width,height)
     */
    void (*setHolePunch)(NPP instance, int x, int y, int width, int height);
} NPCallbacks;

typedef np_event_t  NPEvent;
#else
typedef void*  NPEvent;
#endif

#if defined(XP_MACOSX)
typedef void* NPRegion;
#ifndef NP_NO_QUICKDRAW
typedef RgnHandle NPQDRegion;
#endif
typedef CGPathRef NPCGRegion;
#elif defined(XP_WIN)
typedef HRGN NPRegion;
#elif defined(XP_UNIX) && defined (X_PROTOCOL)
typedef Region NPRegion;
#elif defined(XP_SYMBIAN)
typedef QRegion* NPRegion;
#else
typedef void *NPRegion;
#endif

typedef struct _NPNSString NPNSString;
typedef struct _NPNSWindow NPNSWindow;
typedef struct _NPNSMenu   NPNSMenu;

#if defined(XP_MACOSX)
typedef NPNSMenu NPMenu;
#else
typedef void *NPMenu;
#endif

typedef enum {
  NPCoordinateSpacePlugin = 1,
  NPCoordinateSpaceWindow,
  NPCoordinateSpaceFlippedWindow,
  NPCoordinateSpaceScreen,
  NPCoordinateSpaceFlippedScreen
} NPCoordinateSpace;

#if defined(XP_MACOSX)

#ifndef NP_NO_QUICKDRAW
typedef struct NP_Port
{
  CGrafPtr port;
  int32_t portx; /* position inside the topmost window */
  int32_t porty;
} NP_Port;
#endif /* NP_NO_QUICKDRAW */

/*
 * NP_CGContext is the type of the NPWindow's 'window' when the plugin specifies NPDrawingModelCoreGraphics
 * as its drawing model.
 */

typedef struct NP_CGContext
{
  CGContextRef context;
#ifdef NP_NO_CARBON
  NPNSWindow *window;
#else
  void *window; /* A WindowRef or NULL for the Cocoa event model. */
#endif
} NP_CGContext;

/*
 * NP_GLContext is the type of the NPWindow's 'window' when the plugin specifies NPDrawingModelOpenGL as its
 * drawing model.
 */

typedef struct NP_GLContext
{
  CGLContextObj context;
#ifdef NP_NO_CARBON
  NPNSWindow *window;
#else
  void *window; /* Can be either an NSWindow or a WindowRef depending on the event model */
#endif
} NP_GLContext;

typedef enum {
  NPCocoaEventDrawRect = 1,
  NPCocoaEventMouseDown,
  NPCocoaEventMouseUp,
  NPCocoaEventMouseMoved,
  NPCocoaEventMouseEntered,
  NPCocoaEventMouseExited,
  NPCocoaEventMouseDragged,
  NPCocoaEventKeyDown,
  NPCocoaEventKeyUp,
  NPCocoaEventFlagsChanged,
  NPCocoaEventFocusChanged,
  NPCocoaEventWindowFocusChanged,
  NPCocoaEventScrollWheel,
  NPCocoaEventTextInput
} NPCocoaEventType;

typedef struct _NPCocoaEvent {
  NPCocoaEventType type;
  uint32_t version;
  union {
    struct {
      uint32_t modifierFlags;
      double   pluginX;
      double   pluginY;
      int32_t  buttonNumber;
      int32_t  clickCount;
      double   deltaX;
      double   deltaY;
      double   deltaZ;
    } mouse;
    struct {
      uint32_t    modifierFlags;
      NPNSString *characters;
      NPNSString *charactersIgnoringModifiers;
      NPBool      isARepeat;
      uint16_t    keyCode;
    } key;
    struct {
      CGContextRef context;
      double x;
      double y;
      double width;
      double height;
    } draw;
    struct {
      NPBool hasFocus;
    } focus;
    struct {
      NPNSString *text;
    } text;
  } data;
} NPCocoaEvent;

#ifndef NP_NO_CARBON
/* Non-standard event types that can be passed to HandleEvent */
enum NPEventType {
  NPEventType_GetFocusEvent = (osEvt + 16),
  NPEventType_LoseFocusEvent,
  NPEventType_AdjustCursorEvent,
  NPEventType_MenuCommandEvent,
  NPEventType_ClippingChangedEvent,
  NPEventType_ScrollingBeginsEvent = 1000,
  NPEventType_ScrollingEndsEvent
};
#endif /* NP_NO_CARBON */

#endif /* XP_MACOSX */

/*
 * Values for mode passed to NPP_New:
 */
#define NP_EMBED 1
#define NP_FULL  2

/*
 * Values for stream type passed to NPP_NewStream:
 */
#define NP_NORMAL     1
#define NP_SEEK       2
#define NP_ASFILE     3
#define NP_ASFILEONLY 4

#define NP_MAXREADY (((unsigned)(~0)<<1)>>1)

/*
 * Flags for NPP_ClearSiteData.
 */
#define NP_CLEAR_ALL   0
#define NP_CLEAR_CACHE (1 << 0)

#if !defined(__LP64__)
#if defined(XP_MACOSX)
#pragma options align=reset
#endif
#endif /* __LP64__ */

/*----------------------------------------------------------------------*/
/*       Error and Reason Code definitions                              */
/*----------------------------------------------------------------------*/

/*
 * Values of type NPError:
 */
#define NPERR_BASE                         0
#define NPERR_NO_ERROR                    (NPERR_BASE + 0)
#define NPERR_GENERIC_ERROR               (NPERR_BASE + 1)
#define NPERR_INVALID_INSTANCE_ERROR      (NPERR_BASE + 2)
#define NPERR_INVALID_FUNCTABLE_ERROR     (NPERR_BASE + 3)
#define NPERR_MODULE_LOAD_FAILED_ERROR    (NPERR_BASE + 4)
#define NPERR_OUT_OF_MEMORY_ERROR         (NPERR_BASE + 5)
#define NPERR_INVALID_PLUGIN_ERROR        (NPERR_BASE + 6)
#define NPERR_INVALID_PLUGIN_DIR_ERROR    (NPERR_BASE + 7)
#define NPERR_INCOMPATIBLE_VERSION_ERROR  (NPERR_BASE + 8)
#define NPERR_INVALID_PARAM               (NPERR_BASE + 9)
#define NPERR_INVALID_URL                 (NPERR_BASE + 10)
#define NPERR_FILE_NOT_FOUND              (NPERR_BASE + 11)
#define NPERR_NO_DATA                     (NPERR_BASE + 12)
#define NPERR_STREAM_NOT_SEEKABLE         (NPERR_BASE + 13)

/*
 * Values of type NPReason:
 */
#define NPRES_BASE          0
#define NPRES_DONE         (NPRES_BASE + 0)
#define NPRES_NETWORK_ERR  (NPRES_BASE + 1)
#define NPRES_USER_BREAK   (NPRES_BASE + 2)

/*
 * Don't use these obsolete error codes any more.
 */
#define NP_NOERR  NP_NOERR_is_obsolete_use_NPERR_NO_ERROR
#define NP_EINVAL NP_EINVAL_is_obsolete_use_NPERR_GENERIC_ERROR
#define NP_EABORT NP_EABORT_is_obsolete_use_NPRES_USER_BREAK

/*
 * Version feature information
 */
#define NPVERS_HAS_STREAMOUTPUT             8
#define NPVERS_HAS_NOTIFICATION             9
#define NPVERS_HAS_LIVECONNECT              9
#define NPVERS_WIN16_HAS_LIVECONNECT        9
#define NPVERS_68K_HAS_LIVECONNECT          11
#define NPVERS_HAS_WINDOWLESS               11
#define NPVERS_HAS_XPCONNECT_SCRIPTING      13  /* Not implemented in WebKit */
#define NPVERS_HAS_NPRUNTIME_SCRIPTING      14
#define NPVERS_HAS_FORM_VALUES              15  /* Not implemented in WebKit; see bug 13061 */
#define NPVERS_HAS_POPUPS_ENABLED_STATE     16  /* Not implemented in WebKit */
#define NPVERS_HAS_RESPONSE_HEADERS         17
#define NPVERS_HAS_NPOBJECT_ENUM            18
#define NPVERS_HAS_PLUGIN_THREAD_ASYNC_CALL 19
#define NPVERS_HAS_ALL_NETWORK_STREAMS      20
#define NPVERS_HAS_URL_AND_AUTH_INFO        21
#define NPVERS_HAS_PRIVATE_MODE             22
#define NPVERS_MACOSX_HAS_EVENT_MODELS      23
#define NPVERS_HAS_CANCEL_SRC_STREAM        24
#define NPVERS_HAS_ADVANCED_KEY_HANDLING    25
#define NPVERS_HAS_URL_REDIRECT_HANDLING    26
#define NPVERS_HAS_CLEAR_SITE_DATA          27

/*----------------------------------------------------------------------*/
/*                        Function Prototypes                           */
/*----------------------------------------------------------------------*/

#if defined(__OS2__)
#define NP_LOADDS _System
#else
#define NP_LOADDS
#endif

#ifdef __cplusplus
extern "C" {
#endif

/* NPP_* functions are provided by the plugin and called by the navigator. */

#if defined(XP_UNIX)
char* NPP_GetMIMEDescription(void);
#endif

NPError NP_LOADDS NPP_Initialize(void);
void    NP_LOADDS NPP_Shutdown(void);
NPError NP_LOADDS NPP_New(NPMIMEType pluginType, NPP instance,
                          uint16_t mode, int16_t argc, char* argn[],
                          char* argv[], NPSavedData* saved);
NPError NP_LOADDS NPP_Destroy(NPP instance, NPSavedData** save);
NPError NP_LOADDS NPP_SetWindow(NPP instance, NPWindow* window);
NPError NP_LOADDS NPP_NewStream(NPP instance, NPMIMEType type,
                                NPStream* stream, NPBool seekable,
                                uint16_t* stype);
NPError NP_LOADDS NPP_DestroyStream(NPP instance, NPStream* stream,
                                    NPReason reason);
int32_t NP_LOADDS NPP_WriteReady(NPP instance, NPStream* stream);
int32_t NP_LOADDS NPP_Write(NPP instance, NPStream* stream, int32_t offset,
                            int32_t len, void* buffer);
void    NP_LOADDS NPP_StreamAsFile(NPP instance, NPStream* stream,
                                   const char* fname);
void    NP_LOADDS NPP_Print(NPP instance, NPPrint* platformPrint);
int16_t NP_LOADDS NPP_HandleEvent(NPP instance, void* event);
void    NP_LOADDS NPP_URLNotify(NPP instance, const char* url,
                                NPReason reason, void* notifyData);
jref    NP_LOADDS NPP_GetJavaClass(void);
NPError NP_LOADDS NPP_GetValue(NPP instance, NPPVariable variable, void *value);
NPError NP_LOADDS NPP_SetValue(NPP instance, NPNVariable variable, void *value);
NPBool  NP_LOADDS NPP_GotFocus(NPP instance, NPFocusDirection direction);
void    NP_LOADDS NPP_LostFocus(NPP instance);
void    NP_LOADDS NPP_URLRedirectNotify(NPP instance, const char* url, int32_t status, void* notifyData);
NPError NP_LOADDS NPP_ClearSiteData(const char* site, uint64_t flags, uint64_t maxAge);
char**  NP_LOADDS NPP_GetSitesWithData(void);

/* NPN_* functions are provided by the navigator and called by the plugin. */
void        NP_LOADDS NPN_Version(int* plugin_major, int* plugin_minor,
                                  int* netscape_major, int* netscape_minor);
NPError     NP_LOADDS NPN_GetURLNotify(NPP instance, const char* url,
                                       const char* target, void* notifyData);
NPError     NP_LOADDS NPN_GetURL(NPP instance, const char* url,
                                 const char* target);
NPError     NP_LOADDS NPN_PostURLNotify(NPP instance, const char* url,
                                        const char* target, uint32_t len,
                                        const char* buf, NPBool file,
                                        void* notifyData);
NPError     NP_LOADDS NPN_PostURL(NPP instance, const char* url,
                                  const char* target, uint32_t len,
                                  const char* buf, NPBool file);
NPError     NP_LOADDS NPN_RequestRead(NPStream* stream, NPByteRange* rangeList);
NPError     NP_LOADDS NPN_NewStream(NPP instance, NPMIMEType type,
                                    const char* target, NPStream** stream);
int32_t     NP_LOADDS NPN_Write(NPP instance, NPStream* stream, int32_t len,
                                void* buffer);
NPError     NP_LOADDS NPN_DestroyStream(NPP instance, NPStream* stream,
                                        NPReason reason);
void        NP_LOADDS NPN_Status(NPP instance, const char* message);
const char* NP_LOADDS NPN_UserAgent(NPP instance);
void*       NP_LOADDS NPN_MemAlloc(uint32_t size);
void        NP_LOADDS NPN_MemFree(void* ptr);
uint32_t    NP_LOADDS NPN_MemFlush(uint32_t size);
void        NP_LOADDS NPN_ReloadPlugins(NPBool reloadPages);
JRIEnv*     NP_LOADDS NPN_GetJavaEnv(void);
jref        NP_LOADDS NPN_GetJavaPeer(NPP instance);
NPError     NP_LOADDS NPN_GetValue(NPP instance, NPNVariable variable,
                                   void *value);
NPError     NP_LOADDS NPN_SetValue(NPP instance, NPPVariable variable,
                                   void *value);
void        NP_LOADDS NPN_InvalidateRect(NPP instance, NPRect *invalidRect);
void        NP_LOADDS NPN_InvalidateRegion(NPP instance,
                                           NPRegion invalidRegion);
void        NP_LOADDS NPN_ForceRedraw(NPP instance);
void        NP_LOADDS NPN_PushPopupsEnabledState(NPP instance, NPBool enabled);
void        NP_LOADDS NPN_PopPopupsEnabledState(NPP instance);
void        NP_LOADDS NPN_PluginThreadAsyncCall(NPP instance,
                                                void (*func) (void *),
                                                void *userData);
NPError     NP_LOADDS NPN_GetValueForURL(NPP instance, NPNURLVariable variable,
                                         const char *url, char **value,
                                         uint32_t *len);
NPError     NP_LOADDS NPN_SetValueForURL(NPP instance, NPNURLVariable variable,
                                         const char *url, const char *value,
                                         uint32_t len);
NPError     NP_LOADDS NPN_GetAuthenticationInfo(NPP instance,
                                                const char *protocol,
                                                const char *host, int32_t port,
                                                const char *scheme,
                                                const char *realm,
                                                char **username, uint32_t *ulen,
                                                char **password,
                                                uint32_t *plen);
uint32_t    NP_LOADDS NPN_ScheduleTimer(NPP instance, uint32_t interval, NPBool repeat, void (*timerFunc)(NPP npp, uint32_t timerID));
void        NP_LOADDS NPN_UnscheduleTimer(NPP instance, uint32_t timerID);
NPError     NP_LOADDS NPN_PopUpContextMenu(NPP instance, NPMenu* menu);
NPBool      NP_LOADDS NPN_ConvertPoint(NPP instance, double sourceX, double sourceY, NPCoordinateSpace sourceSpace, double *destX, double *destY, NPCoordinateSpace destSpace);

#ifdef __cplusplus
}  /* end extern "C" */
#endif

#endif /* RC_INVOKED */
#if defined(__OS2__)
#pragma pack()
#endif

#endif /* npapi_h_ */
