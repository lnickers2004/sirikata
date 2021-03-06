// Copyright (c) 2011 Sirikata Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can
// be found in the LICENSE file.

system.require('std/core/namespace.em');

(
function() {

    var ns = Namespace('std.env');

    /** @class Environment synchronizes with an environment in the
     *  space to keep track of global, shared state.
     */
    ns.Environment = function(pres) {
        this._sim = pres.runSimulation('environment');
    };

    ns.Environment.prototype.invoke = function() {
        return this._sim.invoke.apply(this._sim, arguments);
    };

    ns.Environment.prototype.set = function(key, val) {
        return this.invoke('set', key, val);
    };

    ns.Environment.prototype.get = function(key) {
        return this.invoke('get', key);
    };

    ns.Environment.prototype.listen = function(cb) {
        return this.invoke('listen', cb);
    };

})();
