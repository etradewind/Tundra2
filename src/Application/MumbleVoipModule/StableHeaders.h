// For conditions of distribution and use, see copyright notice in license.txt

#pragma once

#include "CoreDefines.h"

// If we don't do this, we get "boost/asio/detail/socket_types.hpp(27) : fatal error C1189: #error :
// WinSock.h has already been included" for many files (User.cpp, Connection.cpp, etc.).
#ifdef WIN32
#include <Winsock2.h>
#endif

#ifdef PCH_ENABLED

#include "Framework.h"
#include <QtCore>
#include <QtGui>

#endif
