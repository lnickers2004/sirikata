system.require("std/default.em");
system.require("villageSimple/meshes.em");
system.require("std/core/repeatingTimer.em");

meshes = housemeshes.concat(apartmentmeshes).concat(buildingmeshes);

PresQueue = [];
PresCount = 1;
windowSize = 10;
windowCount = 0;

//create a pressence (buiding) at <x,-20,z>
function createPres(mesh,x,z,scale)
{
    system.createPresence({'mesh': mesh, 
                           'pos': <x,-10,z>,
                           'orient': util.Quaternion.fromLookAt(<0,0,1>),
                           'scale': scale,
                           'callback': function (presence) {
                               presence.loadMesh(function() {
                                   bb = presence.meshBounds().across();
                                   pos = <x, bb.y/2-20, z>;
                                   presence.position = pos;
                               });
                           }
                          }
                         );
}

//create a street from <ax,-20,az> to <bx,-20,bz>
function createStreet(mesh,ax,az,bx,bz)
{
    d = 50;
    scale = 1;
    system.createPresence({'mesh': mesh,
                           'pos': <(ax+bx)/2,-20,(az+bz)/2>, 
                           //'scale': 2, // for cube test
                           'callback': function(presence) {
                               presence.loadMesh(function() {
                                   if (ax==bx)
                                       presence.orientation = util.Quaternion.fromLookAt(<0,0,1>);
                                   else if (az==bz)
                                       presence.orientation = util.Quaternion.fromLookAt(<1,0,0>);
                                   bb = presence.meshBounds().across();
                                   pos = <(ax+bx)/2, bb.y/2-20, (az+bz)/2>;
                                   presence.position = pos;
                                   if (ax==bx) 
                                       scale=d/bb.z;
                                   else if (az==bz) 
                                       scale=(d+d*bb.z/bb.x)/bb.x;
                                   presence.setScale(scale);
                               });
                           }
                          }
                         );
}

function createBuildings(x,z,ini_x,ini_z)
{
    d = 50;
    N = meshes.length;
    for(var i=0; i<z; i++) {
        for(var j=0; j<x; j++) {
            n = i*z+j
            xx = -(ini_x+j)*d;
            zz = (ini_z+i)*d;
            PresQueue.push([meshes[n%N],xx,zz,20]);
            //PresQueue.push([cubemesh,xx,zz,20]);
        }
    }
}

function createStreets(x,z,ini_x,ini_z)
{
    d = 50;
    for(var i=0; i<z; i++) {
        for(var j=0; j<x; j++) {
            xx = -(ini_x+j)*d;
            zz = (ini_z+i)*d;
            xxx= -(ini_x+j+1)*d;
            zzz= (ini_z+i+1)*d;
            if(j!=x-1) PresQueue.push([streetmesh, xx, zz, xxx, zz]);
            if(i!=z-1) PresQueue.push([streetmesh, xx, zz, xx, zzz]);
            //if(j!=x-1) PresQueue.push([cubemesh, xx, zz, xxx, zz]);
            //if(i!=z-1) PresQueue.push([cubemesh, xx, zz, xx, zzz]);
        }
    }
}

function CreatePresCB()
{
    while(PresQueue.length>0 && windowCount<=windowSize){
        windowCount++;
        //system.print(windowCount);
        //system.print('----current windowCount\n');
        args = PresQueue.shift();
        if(args.length==4) createPres.apply(this,args);
        else if(args.length==5) createStreet.apply(this,args);
    }
}

//create x by z blocks, with street corner initial position <ini_x*d,-20,ini_z*d>, d is the block size
function createVillage(x,z,ini_x,ini_z)
{
    createBuildings(x,z,ini_x+0.5,ini_z+0.5);
    createStreets(x+1,z+1,ini_x,ini_z);
}


function init()
{
    //createPres(terrainmesh,0,0,1000)
    createVillage(3,3,0,-1);
    system.onPresenceConnected(function()
                               {
                                   windowCount--;
                                   //system.print(windowCount);
                                   //system.print('----current windowCount\n');
                                   PresCount++;
                                   system.print(PresCount);
                                   system.print('----presenced connected\n');

                                   CreatePresCB();
                               }
                              );
    CreatePresCB();

    system.onPresenceDisconnected(function()
                                  {
                                      PresCount--;
                                      system.print(PresCount);
                                      system.print('----Presence Disconnceted\n');
                                  }
                                 );

}
