#pragma once

class UFunction;
class UObject;
class UActor;
class CallArguments;

// A call on an actor in a net game (the original's
// AActor::ProcessRemoteFunction, docs/re/network.md, remote functions): true
// when it is not to run here -- sent to the other side, as its class's
// replication condition says, or not run by a simulated proxy because it is
// not a simulated function.
bool NetProcessRemoteFunction(UFunction* function, UObject* instance, CallArguments& args);

// A replicated field's condition, from its class's replication block, as
// the actor stands.
bool NetReplicationCondition(UFunction* rootFunction, UActor* actor);
