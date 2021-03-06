/*  Sirikata
 *  ObjectHost.cpp
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

#include "SimObjectHost.hpp"
#include <sirikata/core/trace/Trace.hpp>
#include "Object.hpp"
#include <sirikata/core/network/StreamFactory.hpp>
#include <sirikata/core/network/Stream.hpp>
#include <sirikata/core/network/ServerIDMap.hpp>
#include <sirikata/core/util/Random.hpp>
#include <sirikata/core/options/CommonOptions.hpp>

#include <json_spirit/json_spirit.h>

#define OH_LOG(level,msg) SILOG(oh,level,msg)

using namespace Sirikata;
using namespace Sirikata::Network;

namespace Sirikata {

using std::tr1::placeholders::_1;
using std::tr1::placeholders::_2;
using std::tr1::placeholders::_3;

// ObjectHost Implementation

ObjectHost::ObjectHost(ObjectHostContext* ctx, Trace::Trace* trace, ServerIDMap* sidmap)
 : mContext( ctx ),
   mSessionManager(
       ctx, SpaceID::null(), sidmap, // FIXME should have non-null SpaceID
       std::tr1::bind(&ObjectHost::handleObjectConnected, this, _1, _2),
       std::tr1::bind(&ObjectHost::handleObjectMigrated, this, _1, _2, _3),
       std::tr1::bind(&ObjectHost::handleObjectMessage, this, _1, _2),
       std::tr1::bind(&ObjectHost::handleObjectDisconnected, this, _1, _2)
   )
{
    mPingId=0;

    mStreamOptions=Sirikata::Network::StreamFactory::getSingleton().getOptionParser(GetOptionValue<String>("ohstreamlib"))(GetOptionValue<String>("ohstreamoptions"));

    mContext->objectHost = this;
    mContext->add(&mSessionManager);
}


ObjectHost::~ObjectHost() {
}

const ObjectHostContext* ObjectHost::context() const {
    return mContext;
}

void ObjectHost::connect(
    Object* obj, const SolidAngle& init_sa, uint32 init_max_results,
    ConnectedCallback connect_cb,
    MigratedCallback migrate_cb, StreamCreatedCallback stream_created_cb,
    DisconnectedCallback disconnected_cb
)
{
    Sirikata::SerializationCheck::Scoped sc(&mSerialization);

    mObjects[obj->uuid()] = obj;

    TimedMotionVector3f init_loc = obj->location();
    TimedMotionQuaternion init_orient(Time::null(), MotionQuaternion(Quaternion::identity(), Quaternion::identity()));
    BoundingSphere3f init_bounds = obj->bounds();

    SpaceObjectReference sporef(SpaceID::null(),ObjectReference(obj->uuid()));

    // We need to encode for new, generic format, assuming basic solid angle
    // query processor
    String query;
    bool with_query = init_sa != SolidAngle::Max;
    if (with_query) {
        namespace json = json_spirit;
        json::Value json_query = json::Object();
        json_query.put("angle", init_sa.asFloat());
        // Windows doesn't like a normal int here for some reason, seems like a
        // mismatch between boost's typedefs and ours.
        json_query.put("max_results", (boost::uint32_t)init_max_results);
        query = json::write(json_query);
    }

    mSessionManager.connect(
        sporef, init_loc, init_orient, init_bounds, "", "", query,"",
	std::tr1::bind(&ObjectHost::dispatchConnectedCallback, this, _1, _2, _3, connect_cb),
	migrate_cb, stream_created_cb, disconnected_cb
    );
}

void ObjectHost::connect(
    Object* obj,
    ConnectedCallback connect_cb, MigratedCallback migrate_cb,
    StreamCreatedCallback stream_created_cb,
    DisconnectedCallback disconnected_cb
)
{
    Sirikata::SerializationCheck::Scoped sc(&mSerialization);

    mObjects[obj->uuid()] = obj;

    TimedMotionVector3f init_loc = obj->location();
    TimedMotionQuaternion init_orient(Time::null(), MotionQuaternion(Quaternion::identity(), Quaternion::identity()));
    BoundingSphere3f init_bounds = obj->bounds();

    SpaceObjectReference sporef(SpaceID::null(),ObjectReference(obj->uuid()));

    mSessionManager.connect(
        sporef, init_loc, init_orient, init_bounds, "", "", "", "",
	std::tr1::bind(&ObjectHost::dispatchConnectedCallback, this, _1, _2, _3, connect_cb),
        migrate_cb, stream_created_cb, disconnected_cb
    );
}

void ObjectHost::dispatchConnectedCallback(const SpaceID& space, const ObjectReference& objid, const SessionManager::ConnectionInfo& ci, ConnectedCallback cb) {
    cb(space, objid, ci.server, ci.loc, ci.orient, ci.bounds, ci.mesh, ci.physics);
}

void ObjectHost::disconnect(Object* obj) {
    Sirikata::SerializationCheck::Scoped sc(&mSerialization);
    mObjects.erase(obj->uuid());
    mSessionManager.disconnect(SpaceObjectReference(SpaceID::null(),ObjectReference(obj->uuid())));
}

bool ObjectHost::send(const Object* src, const ObjectMessagePort src_port, const UUID& dest, const ObjectMessagePort dest_port, const std::string& payload) {
    Sirikata::SerializationCheck::Scoped sc(&mSerialization);

    return mSessionManager.send(SpaceObjectReference(SpaceID::null(),ObjectReference(src->uuid())), src_port, dest, dest_port, payload);
}
/*
bool ObjectHost::send(const ObjectMessagePort src_port, const UUID& src, const ObjectMessagePort dest_port, const UUID& dest,const std::string& payload) {
    Sirikata::SerializationCheck::Scoped sc(&mSerialization);

    return send(src, src_port, dest, dest_port, payload);
}
*/

void ObjectHost::fillPing(double distance, uint32 payload_size, Sirikata::Protocol::Object::Ping* result) {
    if (distance>=0)
        result->set_distance(distance);
    result->set_id(mPingId++);
    if (payload_size > 0) {
        std::string pl(payload_size, 'a');
        result->set_payload(pl);
    }
}

bool ObjectHost::sendPing(const Time& t, const UUID& src, const UUID& dest, Sirikata::Protocol::Object::Ping* ping_msg) {
    ping_msg->set_ping(t);
    String ping_serialized = serializePBJMessage(*ping_msg);
    bool send_success = mSessionManager.send(SpaceObjectReference(SpaceID::null(),ObjectReference(src)), OBJECT_PORT_PING, dest, OBJECT_PORT_PING, ping_serialized);

    if (send_success)
        CONTEXT_OHTRACE_NO_TIME(pingCreated,
            ping_msg->ping(),
            src,
            t,
            dest,
            ping_msg->id(),
            ping_msg->has_distance()?ping_msg->distance():-1,
            ping_serialized.size()
        );

    return send_success;
}

bool ObjectHost::ping(const Time& t, const UUID& src, const UUID&dest, double distance, uint32 payload_size) {
    Sirikata::SerializationCheck::Scoped sc(&mSerialization);

    Sirikata::Protocol::Object::Ping ping_msg;
    fillPing(distance, payload_size, &ping_msg);

    return sendPing(t, src, dest, &ping_msg);
}


void ObjectHost::handleObjectConnected(const SpaceObjectReference& sporef_objid, ServerID connectedTo) {
    notify(&ObjectHostListener::objectHostConnectedObject, this, mObjects[sporef_objid.object().getAsUUID()], connectedTo);
}

void ObjectHost::handleObjectMigrated(const SpaceObjectReference& sporef_objid, ServerID migratedFrom, ServerID migratedTo) {
    notify(&ObjectHostListener::objectHostMigratedObject, this, sporef_objid.object().getAsUUID(), migratedFrom, migratedTo);
}

void ObjectHost::handleObjectMessage(const SpaceObjectReference& sporef_internalID, Sirikata::Protocol::Object::ObjectMessage* msg) {
    // Possibly tag as ping non-destructively
    if (msg->source_port()==OBJECT_PORT_PING&&msg->dest_port()==OBJECT_PORT_PING) {
        Sirikata::Protocol::Object::Ping ping_msg;
        ping_msg.ParseFromString(msg->payload());
        CONTEXT_OHTRACE_NO_TIME(ping,
            ping_msg.ping(),
            msg->source_object(),
            mContext->simTime(),
            msg->dest_object(),
            ping_msg.has_id()?ping_msg.id():(uint64)-1,
            ping_msg.has_distance()?ping_msg.distance():-1,
            msg->unique(),
            ping_msg.ByteSize()
        );
    }

    if (mRegisteredServices.find(msg->dest_port())!=mRegisteredServices.end()) {
        mRegisteredServices[msg->dest_port()](*msg);
        delete msg;
    }
    else {
        // Otherwise, by default, we just ship it to the correct object
        ObjectMap::iterator obj_it = mObjects.find(msg->dest_object());
        if (obj_it != mObjects.end()) {
            Object* obj = obj_it->second;
            obj->receiveMessage(msg);
        }
        else {
            delete msg;
        }
    }
}

void ObjectHost::handleObjectDisconnected(const SpaceObjectReference& sporef_objid, Disconnect::Code) {
    notify(&ObjectHostListener::objectHostDisconnectedObject, this, mObjects[sporef_objid.object().getAsUUID()]);
}

bool ObjectHost::registerService(uint64 port, const ObjectMessageCallback&cb) {
    Sirikata::SerializationCheck::Scoped sc(&mSerialization);
    if (mRegisteredServices.find(port)!=mRegisteredServices.end())
        return false;
    mRegisteredServices[port]=cb;
    return true;
}
bool ObjectHost::unregisterService(uint64 port) {
    Sirikata::SerializationCheck::Scoped sc(&mSerialization);
    if (mRegisteredServices.find(port)!=mRegisteredServices.end()) {
        mRegisteredServices.erase(mRegisteredServices.find(port));
        return true ;
    }
    return false;
}

ODPSST::StreamPtr ObjectHost::getSpaceStream(const UUID& objectID) {
    return mSessionManager.getSpaceStream(ObjectReference(objectID));
}

} // namespace Sirikata
