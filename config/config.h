#pragma once

// The beginning of the paths is strictly from '/'
// The MCONFIG_MODULES_FOLDER folder is the folder containing your '.so' modules.
// MCONFIG_HTTP_MAIN_HTML_PAGE_PATH is what '/' will be replaced with during a 'GET /' request
// MCONFIG_HTTP_..._FOLDER: the folder corresponding to a specific file type
// MCONFIG_TLS_CERTIFICATE_FOLDER: the folder for your TLS certificates

// Do not use blocking operations in server API modules.

#define MCONFIG_SERVER_NAME "mserver"
#define MCONFIG_MODULES_FOLDER "modules"
#define MCONFIG_HTTP_MAIN_HTML_PAGE_PATH "/index.html"

#define MCONFIG_TLS_CERTIFICATE_FOLDER "TLS-certificate"
#define MCONFIG_TLS_CERTIFICATE_NAME MCONFIG_SERVER_NAME

#define MCONFIG_START_MAX_CONNECTIONS 25
#define MCONFIG_SERVER_PORT 8080

#define MCONFIG_HTTP_HTML_FOLDER "/"
#define MCONFIG_HTTP_CSS_FOLDER "/css/"
#define MCONFIG_HTTP_JS_FOLDER "/js/"
#define MCONFIG_HTTP_TXT_FOLDER "/txt/"
#define MCONFIG_HTTP_JSON_FOLDER "/json/"
#define MCONFIG_HTTP_XML_FOLDER "/xml/"
#define MCONFIG_HTTP_JPEG_FOLDER "/images/"
#define MCONFIG_HTTP_PNG_FOLDER "/images/"
#define MCONFIG_HTTP_GIF_FOLDER "/images/"
#define MCONFIG_HTTP_SVG_FOLDER "/images/"
#define MCONFIG_HTTP_WEBP_FOLDER "/images/"
#define MCONFIG_HTTP_XICON_FOLDER "/images/"
#define MCONFIG_HTTP_PDF_FOLDER "/docs/"
#define MCONFIG_HTTP_ZIP_FOLDER "/files/"