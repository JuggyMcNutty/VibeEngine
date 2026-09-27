#pragma once

#include <cstdint>

class UFunction;
class UObject;
class UClass;
class UActor;
class CallArguments;
class NetConnection;

// A call on an actor in a net game (the original's
// AActor::ProcessRemoteFunction, docs/re/network.md, remote functions): true
// when it is not to run here -- sent to the other side, as its class's
// replication condition says, or not run by a simulated proxy because it is
// not a simulated function.
bool NetProcessRemoteFunction(UFunction* function, UObject* instance, CallArguments& args);

// A replicated field's condition, from its class's replication block, as
// the actor stands: the statement at a property's or first declaration's
// offset in the class that declares it.
bool NetReplicationCondition(UClass* cls, uint16_t replicationOffset, UActor* actor);
bool NetReplicationCondition(UFunction* rootFunction, UActor* actor);

// Whether an actor belongs to the connection's player: on a client, its top
// owner the pawn a viewport here plays; on a server, the pawn this client
// plays (bNetOwner as each side sets it).
bool NetOwnedHere(UActor* actor, NetConnection* connection);
