// For conditions of distribution and use, see copyright notice in license.txt

#ifndef incl_Core_StringUtil_h
#define incl_Core_StringUtil_h

#include <iomanip>

#include <Poco/LocalDateTime.h>

namespace Core
{
    static std::wstring ToWString(const std::string &str)
    {
        std::wstring w_str(str.length(), L' ');
        std::copy(str.begin(), str.end(), w_str.begin());
        return w_str;
    }

    //! Converts value to a string. May throw boost::bad_lexical_cast.
    template <class T>
    static std::string ToString(const T &val) { return boost::lexical_cast<std::string>(val); }

    //! Converts string to a primitive type, such as int or float. May throw boost::bad_lexical_cast.
    template <typename T>
    static T ParseString(const std::string &val) { return boost::lexical_cast<T>(val); }
    
    //! Get the current time as a string.
    static std::string GetLocalTimeString()
    {
        Poco::LocalDateTime *time = new Poco::LocalDateTime(); 
        std::stringstream ss;
        
        ss << std::setw(2) << time->hour() << std::setfill('0') << ":" <<
            std::setw(2) << time->minute() << std::setfill('0') << ":" <<
            std::setw(2) << time->second() << std::setfill('0');
            
        SAFE_DELETE(time);
        
        return ss.str();
    }
    
    //! Get the current date and time as a string.
    static std::string GetLocalDateTimeString()
    {
        Poco::LocalDateTime *time = new Poco::LocalDateTime();
        std::stringstream ss;
        
        ss << std::setw(2) << time->day() << std::setfill('0') << "/" <<
            std::setw(2) << time->month() << std::setfill('0') << "/" <<
            std::setw(4) << time->year() << std::setfill('0') << " " <<
            std::setw(2) << time->hour() << std::setfill('0') << ":" <<
            std::setw(2) << time->minute() << std::setfill('0') << ":" <<
            std::setw(2) << time->second() << std::setfill('0');
        
        SAFE_DELETE(time);
        
        return ss.str();
    }
}
 
#endif


