#ifndef ADDREFERENCELINE_H
#define ADDREFERENCELINE_H

#include "vundocommand.h"

class AddReferenceLine final : public VUndoCommand
{
    Q_OBJECT

public:
    AddReferenceLine(const QDomElement &xml, VAbstractPattern *doc, QUndoCommand *parent = nullptr);
    void undo() override;
    void redo() override;
};

#endif // ADDREFERENCELINE_H
