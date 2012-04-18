// Copyright (c) 2012 Sirikata Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can
// be found in the LICENSE file.

#ifndef _SIRIKATA_OH_MQ_OBJECT_QUERY_HANDLER_BASE_HPP_
#define _SIRIKATA_OH_MQ_OBJECT_QUERY_HANDLER_BASE_HPP_

#include "OHLocationServiceCache.hpp"
#include <sirikata/oh/ObjectHostContext.hpp>

namespace Sirikata {
namespace OH {
namespace Manual {

class ManualObjectQueryProcessor;

/** Base class for ObjectQueryProcessors, which all use Libprox query
 *  processors. Just provides some utilities and definitions.
 */
class ObjectQueryHandlerBase : public Service {
public:
    ObjectQueryHandlerBase(ObjectHostContext* ctx, ManualObjectQueryProcessor* parent, const OHDP::SpaceNodeID& space, Network::IOStrandPtr prox_strand, OHLocationServiceCachePtr loc_cache);
    ~ObjectQueryHandlerBase();

protected:
    // Helper types & methods
    enum ObjectClass {
        OBJECT_CLASS_STATIC = 0,
        OBJECT_CLASS_DYNAMIC = 1,
        NUM_OBJECT_CLASSES = 2
    };
    static const std::string& ObjectClassToString(ObjectClass c);
    static bool velocityIsStatic(const Vector3f& vel);

    // BOTH Threads: These are read-only.

    ObjectHostContext* mContext;
    const OHDP::SpaceNodeID mSpaceNodeID;

    // To support a static/dynamic split but also support mixing them for
    // comparison purposes track which we are doing and, for most places, use a
    // simple index to control whether they point to different query handlers or
    // the same one.
    bool mSeparateDynamicObjects;
    int mNumQueryHandlers;


    // MAIN Thread: Utility methods that should only be called from the main
    // strand

    ManualObjectQueryProcessor* mParent;

    // Handle various events in the main thread that are triggered in the prox thread
    void handleAddObjectLocSubscription(const ObjectReference& subscriber, const ObjectReference& observed);
    void handleRemoveObjectLocSubscription(const ObjectReference& subscriber, const ObjectReference& observed);
    void handleRemoveAllObjectLocSubscription(const ObjectReference& subscriber);

    // PROX Thread - Should only be accessed in methods used by the prox thread
    Network::IOStrandPtr mProxStrand;

    OHLocationServiceCachePtr mLocCache;

}; // class ObjectQueryHandlerBase

} // namespace Manual
} // namespace OH
} // namespace Sirikata

#endif //_SIRIKATA_OH_MQ_OBJECT_QUERY_HANDLER_BASE_HPP_
