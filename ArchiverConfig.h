// --------------------------------------------------------
// $Id$
//
// Please refer to NOTICE.txt,
// included as part of this distribution,
// for legal information.
//
// Kay-Uwe Kasemir, kasemir@lanl.gov
// --------------------------------------------------------

#ifndef _ARCHIVER_CONFIG_H__
#define _ARCHIVER_CONFIG_H__

// ----------------------------------------------------------
// General
// ----------------------------------------------------------

// Some even more generic things are configured in ToolsConfig.h!

// This should match Make.ver
#define VERSION 1
#define RELEASE 4

#define VERSION_TXT "1.4"

// ----------------------------------------------------------
// ArchiveEngine
// ----------------------------------------------------------

// Config:
//
// Type of data format to use:
#define ENGINE_ARCHIVE_TYPE BinArchive

// Use password mechanism
// (for stopping the engine over the web)
//#define USE_PASSWD

#define DEFAULT_USER    "engine"
#define DEFAULT_PASS    "password"   

// Define if sigaction call is available in signal.h
//
// On Win32, signal() is good enough.
// On Unix, this does not work in a multithreaded program,
// so sigaction() should be used
#ifndef WIN32
#define HAVE_SIGACTION
#endif

// ----------------------------------------------------------
// ArchiveManager
// ----------------------------------------------------------

// Should ArchiveManager use "MultiArchive" or "BinArchive"
// (when possible.
//  For specifics like listing data headers, creating new file, ...
//  BinArchive is used in any case)
#define MANAGER_USE_MULTI

// ----------------------------------------------------------
// CGIExport
// ----------------------------------------------------------

// Current options: BinArchive, MultiArchive
#define CGIEXPORT_ARCHIVE_TYPE MultiArchive

// Where is GNU-Plot?
#ifdef WIN32
#	define GNUPLOT_PROGRAM "c:\\Progra~1\\Gnuplot3.7\\wgnuplot.exe"
#else
// LEDA @ LANL
//#	define GNUPLOT_PROGRAM "/opt/apache/htdocs/archive/cgi/gnuplot"
// Linux
#	define GNUPLOT_PROGRAM "/usr/bin/gnuplot"
#endif

// The CGI tool adds cgi_body_start.txt and ..end.txt
// to each web page that it generates.
// They are searched for in the directory where the
// start page resides.
//
// The WIN32/MS web server launches the cgi script
// where that web page was, so we are already there.
// Other Web servers launch this program in the cgi
// directory which could be under the archive interface
// like this directory:
//
//    ..../MainPage.htm       <- page that starts this program
//    ..../cgi/CGIExport.cgi  <- that's us now
//    ..../cgi/tmp/.....      <- where we will create our tmp files
// This define needs to be set such that the CGI tool
// gets from its starting point to the directory where the web files are:
#ifndef WIN32
#define WEB_DIRECTORY ".."
#endif

// Temporary Directories:
// The CGI tool creates data files & scripts for GNUPlot,
// then GNUPlot creates an image file.

// 1) Where are those files physically on the disk?
//    Is the path relative to where the web files?
#define USE_RELATIVE_PHYSICAL_PATH
#ifdef WIN32
#define PHYSICAL_PATH "\\cgi\\tmp\\"
#else
#define PHYSICAL_PATH "/cgi/tmp/"
#endif

// 2) Where are those files for the web server,
//    i.e. what is the URL name to send to the web browser?
//    (no WIN32 specific '\' here)
#define USE_RELATIVE_URL
#define URL_PATH "/tmp/"

// ----------------------------------------------------------
// Atac - note that this will be replaced by casi!
// ----------------------------------------------------------

// (The tcl extension has to be compiled seperately,
//  but the sources are configured in here):
#define ATAC_ARCHIVE_TYPE MultiArchive

// ----------------------------------------------------------
// ArchiveExport
// ----------------------------------------------------------

// (This is more of a test program. You should use the CGIExport tool
//  or the tcl/tk extension instead)

#define EXPORT_ARCHIVE_TYPE MultiArchive

// ----------------------------------------------------------
// WinBrowser
// ----------------------------------------------------------

// (This is more of a test program. You should use the CGIExport tool
//  or the tcl/tk extension instead)

#define WINBROWSER_ARCHIVE_TYPE MultiArchive

#endif
