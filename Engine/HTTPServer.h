// -*- c++ -*-
// $Id$
//
// Please refer to NOTICE.txt,
// included as part of this distribution,
// for legal information.
//
// kasemir@lanl.gov

#if !defined(HTTP_SERVER_H_)
#define HTTP_SERVER_H_

#include <epicsTime.h>
#include <epicsThread.h>
#include <ToolsConfig.h>
#include <NetTools.h>
#include "HTMLPage.h"

/// \addtogroup Engine
/// @{


/// An in-memory web server.

/// Creates a HTTPClientConnection
/// for each incoming, well, client.
///
/// The HTTPClientConnection then needs to handle
/// the incoming requests and produce appropriate
/// and hopefully strikingly beatiful HTML pages.
class HTTPServer : public epicsThreadRunable
{
public:
    /// Create a HTTPServer.
    static HTTPServer *create(short port);
    
    virtual ~HTTPServer();

    /// Start accepting connections (launch thread)
    void start();

    void run();

    void serverinfo(SOCKET socket);
    
private:
    HTTPServer(SOCKET socket);
    void cleanup();

    epicsThread                           thread;
    bool                                  go;
    SOCKET                                socket;
    stdList<class HTTPClientConnection *> clients;
    size_t                                total_clients;
};

typedef void (*PathHandler) (class HTTPClientConnection *connection,
                             const stdString &full_path);


/// Used by HTTPClientConnection to dispatch client requests

/// Terminator: entry with path = 0.
///
///
typedef struct
{
    const char  *path;      // Path for this handler
    size_t      path_len;   // _Relevant_ portion of path to check (if > 0)
    PathHandler handler;    // Handler to call
}   PathHandlerList;

/// Handler for a HTTPServer's client.

/// Handles input and dispatches
/// to a PathHandler from PathList.
/// It's deleted when the connection is closed.
class HTTPClientConnection : public epicsThreadRunable
{
public:
    static PathHandlerList  *handlers;

    HTTPClientConnection(HTTPServer *server, SOCKET socket, int num);
    virtual ~HTTPClientConnection();

    HTTPServer *getServer()
    {   return server; }
    
    SOCKET getSocket()
    {   return socket; }
            
    int getNum()
    {   return num; }
    
    bool isDone()
    {   return done; }    

    /// Predefined PathHandlers
    void error(const stdString &message);
    void pathError(const stdString &path);
 
    void start()
    {  thread.start(); }

    void run();

    const epicsTime &getBirthTime()
    {   return birthtime; }

private:
    epicsThread              thread; // .. that handles this connection
    HTTPServer              *server;
    epicsTime                birthtime;
    int                      num;    // unique sequence number of this conn.
    bool                     done;   // has run() finished running?
    SOCKET                   socket;
    stdVector<stdString>     input_line;
    char                     line[2048];
    unsigned int             dest;  // index of next unused char in _line

    // Result: done, i.e. connection can be closed?
    bool handleInput();

    // return: full request that I can handle?
    bool analyzeInput();

    void dumpInput(HTMLPage &page);
};

/// @}

#endif // !defined(HTTP_SERVER_H_)
