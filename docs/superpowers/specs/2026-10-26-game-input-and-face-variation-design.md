# Game Input and Face Variation Design

## Intent

Make the voxel game easier to use and read during play by adding an explicit
cursor lock/pause toggle, a visible frame-rate counter, and stable texture
orientation variation on block faces.

## Goals

- Start with the cursor locked to the center of the game window and mouse look
  active.
- Pressing Alt releases the cursor and pauses gameplay. While paused, keyboard
  movement, mouse look, block breaking/placing, and infinite-world chunk
  streaming do not run. Pressing Alt again relocks the cursor and resumes play.
- Keep a small FPS counter visible in the top-left while playing and paused.
- Rotate existing face textures by deterministic quarter turns so their
  orientation varies by block and face. Keep every grass-block side face in its
  current orientation and using its current grass-side tile.
- Preserve both finite and infinite world behavior and the existing controls
  while gameplay is active.

## Non-goals

- Adding new texture assets, block types, a pause menu, world saving, or a
  configurable settings screen.
- Changing the material tile chosen for any block face.

## Approaches

- Random atlas-tile selection would create stronger variation, but could make
  dirt or stone faces display another material and would conflict with keeping
  block identity clear.
- Random texture rotation preserves the selected material and gives visual
  variation without new assets. This is the selected approach; use a stable
  hash of world block coordinates and face direction so mesh rebuilds do not
  change the appearance. Grass side faces retain their existing orientation.

## Implementation calls

- Use GLFW's disabled cursor mode while locked to hide and confine the pointer
  and preserve GLFW's relative mouse look. Center it at startup and on relock;
  use normal cursor mode when released.
- Track the lock state explicitly. Alt is the only gameplay-related key
  processed while paused. On relock, reset first-mouse tracking to avoid a
  sudden camera jump.
- Continue rendering while paused so the FPS counter remains visible, but skip
  player input/physics, chunk streaming, raycast edits, and mouse look.
- Draw the FPS counter using a small built-in bitmap glyph set and the existing
  color-only shader path; do not add a font dependency or external font asset.
- Update the displayed FPS on a short interval (about once per second) to keep
  the number readable and avoid unnecessary HUD rebuilds.

## Constraints and acceptance checks

- C++17, GLFW, and the current OpenGL renderer remain in use.
- Verify cursor starts locked; Alt unlocks and pauses; Alt relocks and resumes;
  no gameplay action occurs while unlocked; the FPS text appears at the
  top-left in both states; face rotations are stable after chunk remeshing; and
  grass side textures remain unchanged.
- Rebuild the Release target and run the existing CTest suite after changes.
- The game remains a finite/infinite voxel game; this change does not alter
  world generation rules.
