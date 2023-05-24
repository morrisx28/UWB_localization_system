///
/// \file 				Exception.hpp
/// \author 			Geoffrey Hunter (www.mbedded.ninja) <gbmhunter@gmail.com>
/// \edited             n/a
/// \created			2017-11-09
/// \last-modified		2017-11-27
/// \brief 				Contains the Exception class. File originally from https://github.com/mbedded-ninja/CppUtils.
/// \details
///		See README.md in root dir for more info.

// Header guard
#ifndef BATTERY_MONITOR_EXCEPTION_HPP_
#define BATTERY_MONITOR_EXCEPTION_HPP_

// C++ System headers
#include <iostream>
#include <sstream>
#include <stdexcept>
#include <string>

// Macro definition
#define THROW_EXCEPT(arg) throw Exception(__FILE__, __LINE__, arg);

namespace mn {
namespace CppLinuxSerial {

    class Exception : public std::runtime_error {
      public:
        Exception(const char *file, int line, const std::string &arg) : std::runtime_error(arg) {
            msg_ = static_cast<std::string>(file) + ":" + std::to_string(line) + ": " + arg;
        }

        ~Exception() throw() {}

        const char *what() const throw() override {
            return msg_.c_str();
        }

      private:
        std::string msg_;
    };

} // namespace CppLinuxSerial
} // namespace mn

#endif // BATTERY_MONITOR_EXCEPTION_HPP_