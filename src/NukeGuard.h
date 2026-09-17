// NukeGuard.h - The client-side half of the reversible "nuke" kill switch.
//
// A nuked client must not run: on the next launch it exits before any window is
// created, so nothing appears - it simply does not start. This is not a literal
// file deletion (which would be unrecoverable); instead a small marker on disk
// gates the launch, and the server is the authority on whether the nuke is still
// in effect. An admin's "Restore Client" clears the server flag, and the next
// launch sees it, removes the marker, and starts normally with data intact.
#pragma once

#include <QString>

namespace NukeGuard {

/// True when the marker is present, i.e. this client was nuked and has not yet
/// confirmed a restore.
bool isEngaged();

/// Writes the marker (storing \a sessionToken so the launch check can ask the
/// server whether the nuke still stands). Called when a nuke command arrives.
void engage(const QString &sessionToken);

/// Removes the marker - the client may run again.
void disengage();

/// The session token saved in the marker, or empty.
QString storedToken();

/// Blocking check used at launch: asks the server whether this client is still
/// nuked. Returns true (stay nuked) unless the server explicitly says it has
/// been restored, so a nuke cannot be escaped by pulling the network cable.
bool stillNukedOnServer();

} // namespace NukeGuard
