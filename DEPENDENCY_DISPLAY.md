# Object dependency display

## Existing mechanism

- `VDataTool::_referens` is the deletion guard. Tool creation increments the referenced parent through
  `VPattern::IncrementReferens()`, and removal decrements it.
- `VDrawTool::isUsed()` extends the numeric reference counter with formula lookup for variables exported by the tool.
- `VDrawTool::ContextMenu()` previously disabled **Delete** when `isUsed()` was true.
- `VAbstractTool::deleteTool()` performs the same guard check and previously only logged
  `Can't delete, tool has children.`
- The serialized pattern DOM is the canonical source for actual references. `VToolRecord` supplies tool type and
  draft-block location, while `VContainer` supplies names and maps object IDs back to their creating tool IDs.
- A full parse rebuilds the counters. `SaveToolOptions` requests a full parse when a stored object reference changes,
  preventing stale counters after editing or undo/redo.
- Internal-path edits update both sides of the counter change: removed nodes are decremented and newly referenced nodes
  are incremented, with the inverse operations on Undo.

## Patch design

1. A read-only query in `VAbstractPattern` resolves direct dependents from the existing DOM/history, including ID
   attributes, path/node child elements, operation source items, and formula tokens. It maintains no second graph.
   Recursive lookup also follows chains through custom variables.
2. A dependency dialog shows name, type, the specific reference role, draft block, and a suggested next action.
   Selecting a row highlights the tool through the existing `VAbstractPattern::ShowTool` signal. The available action
   is chosen after selection: open Properties, edit the piece, detach a piece section, select existing replacement
   geometry, or draw new replacement geometry. After a guided piece action closes, the dependency dialog opens again
   with the updated result. A blocked delete goes directly to this dialog instead of showing a warning first.
3. **Delete** remains reachable for used draw tools and pattern pieces so `deleteTool()` can explain why it is blocked.
   The existing `isUsed()` guard remains authoritative. The base point remains a special case because deleting it
   deletes the complete draft block, including its contents.
4. The dialog can show all descendants recursively. Recursive deletion is deliberately excluded: formulas, pieces,
   internal paths, generated operation objects, and reference-counter propagation require broader deletion-order and
   undo/redo coverage before that can be safe.
5. Piece contours and internal paths use a separate guided replacement. One or more adjacent entries can be replaced
   by a different number of points, lines, curves, or splines, including removal without replacement. The old path is
   retained until the candidate passes continuity, direction, and point checks, then one undoable command swaps it.
   New geometry can be drawn with the standard point, line, curve, spline, and arc tools while a small guide remains
   open. The new objects are preselected for review; cancelling the guide rolls its drawing steps back through Undo.
   Detaching removes only the piece reference, so the underlying construction remains visible on the draft. This
   staging provides the safe part of a freeze workflow without storing a permanently detached piece state.
6. A compatible one-to-one replacement can update every direct object reference in one undoable command. The command
   rewrites construction-tool attributes, piece-node source objects, and piece anchors, then requests a full parse so
   the existing reference counters are rebuilt. The dependency dialog offers this action for ordinary construction
   geometry as well as piece nodes. The user can choose an existing object of the same type or create one with the
   normal drafting tools; the confirmation lists all affected owners. Formula expressions are reported but are not
   rewritten by name because a replacement object does not necessarily export equivalent variables. One-to-many
   geometry remains in the guided piece-section workflow where continuity and piece settings can be reviewed.
   Objects that already depend on the old geometry are excluded as replacements, preventing a self-reference or
   dependency cycle. The creation guide names rejected objects and explains this relationship. Closing the guide keeps
   newly drawn construction and returns to the refreshed recursive dependency list. This also reveals new geometry
   attached indirectly through a curve or spline. Point replacement offers the common point construction tools instead
   of assuming one point method. A newly created point that depends on the old construction can be detached at its
   current position and used as the replacement in the same undoable command; rollback is available only through the
   explicit discard action.
7. The dependency list supports multiple node selection. Adjacent selected entries from the same piece path are
   handed to the existing section replacement, allowing them to be detached or replaced together.
8. History marks independent roots, unused geometry, and dependent objects. Clicking a row keeps using the existing
   scene highlight, so independent construction geometry is visible both as a list and in the draft.
9. Reference lengths are non-printing drafting aids. Their length uses the standard formula variables, and their
   position can be free, attached to a point, or placed along a line with free, horizontal, vertical, or line-aligned
   orientation.

## Test coverage

- direct and recursive point dependencies
- structured references from pieces, internal paths, and operations
- recursive formula dependencies through a custom variable
- dependency dialog population, recursive toggle, and highlight signal
- replacement, create, and detach actions for piece nodes
- atomic replacement of direct construction, piece-node, and anchor references, including Undo
- full-parse requests when an edited tool replaces a referenced object in an attribute or child element
- lite-parse preservation for changes that do not affect references
- flexible piece-path section replacement, collapse, and removal
- reference-length formula discovery
