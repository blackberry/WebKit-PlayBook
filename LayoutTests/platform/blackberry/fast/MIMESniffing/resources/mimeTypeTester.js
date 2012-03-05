if (window.layoutTestController) {
    layoutTestController.dumpAsText();
    layoutTestController.dumpResourceResponseMIMETypes();
    layoutTestController.waitUntilDone();
}

var testImage;

function startTest(resourcePath)
{
    testImage = document.getElementById("testImage");
    testImage.src = resourcePath;
}

function imageLoaded()
{
    finish();
}

function finish()
{
    if (window.layoutTestController)
        layoutTestController.notifyDone();
}
