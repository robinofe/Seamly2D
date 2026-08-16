#include "addreferenceline.h"

#include "../../ifc/xml/vabstractpattern.h"

AddReferenceLine::AddReferenceLine(const QDomElement &xml, VAbstractPattern *doc, QUndoCommand *parent)
    : VUndoCommand(xml, doc, parent)
{
    nodeId = doc->getParameterId(xml);
    setText(tr("add reference length"));
}

void AddReferenceLine::undo()
{
    QDomElement container = doc->createReferenceLines();
    const QDomElement element = doc->elementById(nodeId, VAbstractPattern::TagReferenceLine);
    if (!container.isNull() && !element.isNull())
    {
        container.removeChild(element);
        emit NeedFullParsing();
    }
}

void AddReferenceLine::redo()
{
    QDomElement container = doc->createReferenceLines();
    if (!container.isNull())
    {
        container.appendChild(xml);
        RedoFullParsing();
    }
}
