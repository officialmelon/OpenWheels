#pragma once
// QOL (PC addition): switching the camera between the player's character and the others that ride
// or run with it (Irresponsible Dad's kid, the moped couple's girl, Santa's elves, Irresponsible
// Mom's kids), like the browser game's camera key (C by default, remappable) and a touch button.

namespace qol {

// How many characters the camera can follow in the running level (1 = no choice).
int cameraTargetCount();
// Moves the camera focus to the next target; false when there is nothing to switch to.
bool cycleCameraFocus();

}  // namespace qol
