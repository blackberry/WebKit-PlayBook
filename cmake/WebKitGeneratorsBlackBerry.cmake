SET(BlackBerry_STYLE_SHEET
  ${WEBCORE_DIR}/css/themeBlackBerry.css
  ${WEBCORE_DIR}/css/mediaControlsBlackBerry.css
)

IF(CMAKE_SYSTEM_NAME MATCHES Nessus)
  SET(PREPROCESSOR
    --preprocessor="armcc -P -E"
  )
ELSEIF (CMAKE_SYSTEM_NAME MATCHES QNX)
  SET(WEBKIT_BUILDINFO_GENERATOR perl ${CMAKE_SOURCE_DIR}/WebKitTools/Scripts/generate-buildinfo)
ENDIF(CMAKE_SYSTEM_NAME MATCHES Nessus)

