// For conditions of distribution and use, see copyright notice in license.txt
#ifndef incl_OpenSimProtocolModule_NetMessageException_h
#define incl_OpenSimProtocolModule_NetMessageException_h

#include <cstdlib>
#include <cstring>
#include <string>

#include "CoreException.h"

/**
 * NetMessageException is exception type which is throwed in cases if you use NetMessage interface and a problem arises.
 * @see Core::Exception
 */
 
class NetMessageException : public Core::Exception
{
public:
    NetMessageException() : Core::Exception(), strMessage_(""), type_(ET_None) {}
    virtual ~NetMessageException() throw () {} 
    
    /// Enumeration of the exception types.
    enum ExceptionType 
    {
        ET_None = 0,
        ET_VariableTypeMismatch,
        ET_BlockInstanceCountNotRead,
        ET_BlockInstanceCountAlreadyRead,
        ET_EnumCount
    };

    /**
     * Override constructor. 
     * @param message is error message. 
     */
    NetMessageException(const std::string& message) : Core::Exception(), strMessage_(message) {}

    /**
     * Override constructor. 
     * @param type Exception type enum. 
     */
    NetMessageException(ExceptionType type) : Core::Exception(), type_(type) {}
    
    /**
     * Returns error message for caller. 
     * @return message which contains error. 
     */
    std::string GetMessage() const { return strMessage_; }
    
    /**
     * Sets error message. 
     * @param message is string which contains error message.
     */
    void SetMessage(const std::string& message) { strMessage_ = message; }

    /**
     * Sets error message. 
     * @return Exception type enum.
     */
    const ExceptionType GetType() const  { return type_; }

    /** Utility function for converting the connection state enum to string.
     * @param The connection state enum.
     * @return The login state as a string.
     */
    const std::string &What() const
    {
        static const std::string exception_strings[ET_EnumCount] = {
            "None"
            "Tried to read wrong variable type",
            "Current block is variable: use ReadCurrentBlockInstanceCount first in order to proceed"
            "This block's instance count is already read",
            };

        return exception_strings[type_];
    }

private:
    /// Exception error message.
    std::string strMessage_;
    
    /// Exception type.
    ExceptionType type_;

};
#endif 
