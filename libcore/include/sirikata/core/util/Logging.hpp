/*  Sirikata Utilities -- Sirikata Logging Utility
 *  Logging.hpp
 *
 *  Copyright (c) 2009, Daniel Reiter Horn
 *  All rights reserved.
 *
 *  Redistribution and use in source and binary forms, with or without
 *  modification, are permitted provided that the following conditions are
 *  met:
 *  * Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 *  * Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in
 *    the documentation and/or other materials provided with the
 *    distribution.
 *  * Neither the name of Sirikata nor the names of its contributors may
 *    be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS
 * IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED
 * TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A
 * PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER
 * OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF
 * LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING
 * NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
 * SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef _SIRIKATA_LOGGING_HPP_
#define _SIRIKATA_LOGGING_HPP_

#include <sirikata/core/util/Timer.hpp>
#include <iomanip>

extern "C" SIRIKATA_EXPORT void* Sirikata_Logging_OptionValue_defaultLevel;
extern "C" SIRIKATA_EXPORT void* Sirikata_Logging_OptionValue_atLeastLevel;
extern "C" SIRIKATA_EXPORT void* Sirikata_Logging_OptionValue_moduleLevel;
namespace Sirikata {
class OptionValue;
namespace Logging {
enum LOGGING_LEVEL {
    fatal=1,
    error=8,
    warning=64,
    warn=warning,
    info=512,
    debug=4096,
    detailed=8192,
    insane=32768
};

SIRIKATA_FUNCTION_EXPORT const String& LogModuleString(const char* base);
SIRIKATA_FUNCTION_EXPORT const char* LogLevelString(LOGGING_LEVEL lvl, const char* lvl_as_string);

// Public so the macros work efficiently instead of another call
extern "C" SIRIKATA_EXPORT std::ostream* SirikataLogStream;

/** Set the output file pointer for *all* output, not just SILOG output. This
 *  includes both stdout and stderr.
 */
SIRIKATA_FUNCTION_EXPORT void setOutputFP(FILE* fp);
/** Set the output stream for SILOG output, e.g. to redirect output to a file or
 * in memory.
 */
SIRIKATA_FUNCTION_EXPORT void setLogStream(std::ostream* logfs);
/** Allow logging to finish, e.g. flush output and cleanup output streams. May
 * block.
 */
SIRIKATA_FUNCTION_EXPORT void finishLog();

} }
#if 1
# ifdef DEBUG_ALL
#  define SILOGP(module,lvl) true
# else
//needs to use unsafeAs because the LOGGING_LEVEL typeinfos are not preserved across dll lines
#  define SILOGP(module,lvl) \
    ( \
     std::max( reinterpret_cast<Sirikata::OptionValue*>(Sirikata_Logging_OptionValue_atLeastLevel)->unsafeAs<Sirikata::Logging::LOGGING_LEVEL>(), \
	       reinterpret_cast<Sirikata::OptionValue*>(Sirikata_Logging_OptionValue_defaultLevel)->unsafeAs<Sirikata::Logging::LOGGING_LEVEL>()) \
     >=Sirikata::Logging::lvl &&					\
        ( (reinterpret_cast<Sirikata::OptionValue*>(Sirikata_Logging_OptionValue_moduleLevel)->unsafeAs<std::tr1::unordered_map<std::string,Sirikata::Logging::LOGGING_LEVEL> >().find(#module)==reinterpret_cast<Sirikata::OptionValue*>(Sirikata_Logging_OptionValue_moduleLevel)->unsafeAs<std::tr1::unordered_map<std::string,Sirikata::Logging::LOGGING_LEVEL> >().end() && \
           reinterpret_cast<Sirikata::OptionValue*>(Sirikata_Logging_OptionValue_defaultLevel)->unsafeAs<Sirikata::Logging::LOGGING_LEVEL>()>=(Sirikata::Logging::lvl)) \
		   || (reinterpret_cast<Sirikata::OptionValue*>(Sirikata_Logging_OptionValue_moduleLevel)->unsafeAs<std::tr1::unordered_map<std::string,Sirikata::Logging::LOGGING_LEVEL> >().find(#module)!=reinterpret_cast<Sirikata::OptionValue*>(Sirikata_Logging_OptionValue_moduleLevel)->unsafeAs<std::tr1::unordered_map<std::string,Sirikata::Logging::LOGGING_LEVEL> >().end() && \
              reinterpret_cast<Sirikata::OptionValue*>(Sirikata_Logging_OptionValue_moduleLevel)->unsafeAs<std::tr1::unordered_map<std::string,Sirikata::Logging::LOGGING_LEVEL> >()[#module]>=Sirikata::Logging::lvl)))
# endif
# define SILOGBARE(module,lvl,value)                                    \
    do {                                                                \
        if (SILOGP(module,lvl)) {                                       \
            std::ostringstream __log_stream;                            \
            __log_stream << value;                                      \
            (*Sirikata::Logging::SirikataLogStream) << __log_stream.str() << std::endl; \
        }                                                               \
    } while (0)
#else
# define SILOGP(module,lvl) false
# define SILOGBARE(module,lvl,value)
#endif

#define SILOG(module,lvl,value) SILOGBARE(module,lvl, "[" << std::setw(9) << std::setprecision(3) << std::fixed << Sirikata::Timer::processElapsed().seconds() << ":" << Sirikata::Logging::LogModuleString(#module) << "] " << Sirikata::Logging::LogLevelString(Sirikata::Logging::lvl, #lvl) << ": " << std::resetiosflags(std::ios_base::floatfield | std::ios_base::adjustfield) << value)

#if SIRIKATA_PLATFORM == SIRIKATA_PLATFORM_LINUX
// FIXME only works on GCC
#define NOT_IMPLEMENTED_MSG (Sirikata::String("Not implemented reached in ") + Sirikata::String(__PRETTY_FUNCTION__))
#else
#define NOT_IMPLEMENTED_MSG (Sirikata::String("NOT IMPLEMENTED"))
#endif

#define NOT_IMPLEMENTED(module) SILOG(module,error,NOT_IMPLEMENTED_MSG)


#if SIRIKATA_PLATFORM == SIRIKATA_PLATFORM_LINUX
// FIXME only works on GCC
#define DEPRECATED_MSG (Sirikata::String("DEPRECATED reached in ") + Sirikata::String(__PRETTY_FUNCTION__))
#else
#define DEPRECATED_MSG (Sirikata::String("DEPRECATED"))
#endif

#define DEPRECATED(module) SILOG(module,warning,DEPRECATED_MSG)


#endif
