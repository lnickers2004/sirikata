/*  Sirikata
 *  PluginInterface.cpp
 *
 *  Copyright (c) 2009, Ewen Cheslack-Postava
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

#include <sirikata/core/util/Platform.hpp>
#include <sirikata/core/util/RegionWeightCalculator.hpp>
#include <sirikata/core/options/Options.hpp>
#include "ExpIntegral.hpp"

static int weightexp_plugin_refcount = 0;

namespace Sirikata {

static void InitPluginOptions() {
    Sirikata::InitializeClassOptions ico("weightexp",NULL,
        new Sirikata::OptionValue("flatness", "8", Sirikata::OptionValueType<double>(), "k where e^-kx is the bandwidth function and x is the distance between 2 server points"),
        NULL);
}

static RegionWeightCalculator* createRegionWeightCalculator(const String& args) {
    OptionSet* optionsSet = OptionSet::getOptions("weightexp",NULL);
    optionsSet->parse(args);

    double flatness = optionsSet->referenceOption("flatness")->as<double>();

    RegionWeightCalculator* result =
        new RegionWeightCalculator(
            std::tr1::bind(&integralExpFunction,
                flatness,
                std::tr1::placeholders::_1,
                std::tr1::placeholders::_2,
                std::tr1::placeholders::_3,
                std::tr1::placeholders::_4)
        );
    return result;
}

} // namespace Sirikata

SIRIKATA_PLUGIN_EXPORT_C void init() {
    using namespace Sirikata;
    if (weightexp_plugin_refcount==0) {
        InitPluginOptions();
        using std::tr1::placeholders::_1;
#ifdef SIRIKATA_BAD_BOOST_ERF
	SILOG(exp, warn, "Not registering 'gaussian' with RegionWeightCalculatorFactory due to bad Boost.Math library.");
#else
        RegionWeightCalculatorFactory::getSingleton()
            .registerConstructor("gaussian",
                std::tr1::bind(&createRegionWeightCalculator, _1));
#endif
    }
    weightexp_plugin_refcount++;
}

SIRIKATA_PLUGIN_EXPORT_C int increfcount() {
    return ++weightexp_plugin_refcount;
}
SIRIKATA_PLUGIN_EXPORT_C int decrefcount() {
    assert(weightexp_plugin_refcount>0);
    return --weightexp_plugin_refcount;
}

SIRIKATA_PLUGIN_EXPORT_C void destroy() {
    using namespace Sirikata;
    if (weightexp_plugin_refcount==0) {
#ifndef SIRIKATA_BAD_BOOST_ERF
        RegionWeightCalculatorFactory::getSingleton().unregisterConstructor("gaussian");
#endif
    }
}

SIRIKATA_PLUGIN_EXPORT_C const char* name() {
    return "exp";
}

SIRIKATA_PLUGIN_EXPORT_C int refcount() {
    return weightexp_plugin_refcount;
}
