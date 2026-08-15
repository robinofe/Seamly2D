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

## Patch design

1. A read-only query in `VAbstractPattern` resolves direct dependents from the existing DOM/history, including ID
   attributes, path/node child elements, operation source items, and formula tokens. It maintains no second graph.
   Recursive lookup also follows chains through custom variables.
2. A dependency dialog shows name, type, the specific reference role, draft block, and a suggested next action.
   Selecting a row highlights the tool through the existing `VAbstractPattern::ShowTool` signal. The **Select
   dependent object** action closes the modal dialog and opens the selected tool in the existing Properties workflow,
   whose object lists already filter points, curves, splines, and paths by compatible geometry type.
3. **Delete** remains reachable for used draw tools and pattern pieces so `deleteTool()` can explain why it is blocked.
   The existing `isUsed()` guard remains authoritative. The base point remains a special case because deleting it
   deletes the complete draft block, including its contents.
4. The dialog can show all descendants recursively. Recursive deletion is deliberately excluded: formulas, pieces,
   internal paths, generated operation objects, and reference-counter propagation require broader deletion-order and
   undo/redo coverage before that can be safe.
5. Automatic replacement is not offered for pattern-piece nodes or formulas. Their stored structure contains
   additional constraints such as contour order, node type, reverse direction, seam allowance, notches, or expression
   tokens. The dialog directs the user to the owning piece or formula instead of applying a partial XML edit.

## Test coverage

- direct and recursive point dependencies
- structured references from pieces, internal paths, and operations
- recursive formula dependencies through a custom variable
- dependency dialog population, recursive toggle, and highlight signal
- full-parse requests when an edited tool replaces a referenced object in an attribute or child element
- lite-parse preservation for changes that do not affect references
