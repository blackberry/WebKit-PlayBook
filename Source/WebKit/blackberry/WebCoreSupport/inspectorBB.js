var context = {};
(function () {
    Preferences.ignoreWhitespace = false;
    Preferences.samplingCPUProfiler = true;
    Preferences.debuggerAlwaysEnabled = true;
    Preferences.profilerAlwaysEnabled = true;
    Preferences.canEditScriptSource = false;
    Preferences.onlineDetectionEnabled = false;
    Preferences.nativeInstrumentationEnabled = true;
    // FIXME: Turn this to whatever the value of --enable-file-system for chrome.
    Preferences.fileSystemEnabled = false;
    Preferences.canClearCacheAndCookies = true;
    Preferences.showCookiesTab = true;
})();
InspectorFrontendHost.copyText = function(tmp) {
    var encoded = encodeURI(tmp);
    var text = 'data:text/plain;charset=utf-8,' + encoded;
    window.open(text, "_blank");
}
