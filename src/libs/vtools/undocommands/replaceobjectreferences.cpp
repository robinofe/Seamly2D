/***************************************************************************
 *                                                                         *
 *   Copyright (C) 2026 Seamly2D project                                   *
 *                                                                         *
 *   This program is free software: you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation, either version 3 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 ***************************************************************************/

#include "replaceobjectreferences.h"

#include "../ifc/exception/vexceptionbadid.h"
#include "../ifc/xml/vabstractpattern.h"
#include "../vpatterndb/vcontainer.h"
#include "../vgeometry/vgobject.h"

#include <QDomAttr>
#include <QDomNamedNodeMap>
#include <QSet>

namespace
{
bool replaceReferences(QDomElement root, const QSet<quint32> &oldIds, quint32 newObjectId)
{
    bool changed = false;
    QList<QDomElement> pending{root};
    const QSet<QString> &referenceAttributes = VAbstractPattern::objectReferenceAttributes();

    while (!pending.isEmpty())
    {
        QDomElement element = pending.takeFirst();
        const QDomNamedNodeMap attributes = element.attributes();
        for (int i = 0; i < attributes.size(); ++i)
        {
            const QDomAttr attribute = attributes.item(i).toAttr();
            if (referenceAttributes.contains(attribute.name()) && oldIds.contains(attribute.value().toUInt()))
            {
                element.setAttribute(attribute.name(), newObjectId);
                changed = true;
            }
        }

        if (element.tagName() == QStringLiteral("record") &&
            element.parentNode().toElement().tagName() == QStringLiteral("anchors") &&
            oldIds.contains(element.text().toUInt()))
        {
            QDomNode value = element.firstChild();
            if (!value.isNull())
            {
                value.setNodeValue(QString::number(newObjectId));
                changed = true;
            }
        }

        QDomElement child = element.firstChildElement();
        while (!child.isNull())
        {
            pending.append(child);
            child = child.nextSiblingElement();
        }
    }
    return changed;
}
} // namespace

ReplaceObjectReferences::ReplaceObjectReferences(quint32 oldToolId, quint32 newObjectId, VAbstractPattern *doc,
                                                 const VContainer *data, QUndoCommand *parent)
    : ReplaceObjectReferences(oldToolId, newObjectId, doc, data, QDomElement(), parent)
{
}

ReplaceObjectReferences::ReplaceObjectReferences(quint32 oldToolId, quint32 newObjectId, VAbstractPattern *doc,
                                                 const VContainer *data, const QDomElement &detachedReplacement,
                                                 QUndoCommand *parent)
    : VUndoCommand(QDomElement(), doc, parent)
{
    setText(tr("Replace object in all direct uses"));

    QSet<quint32> oldIds{oldToolId};
    const auto objects = data->DataGObjects();
    for (auto object = objects->constBegin(); object != objects->constEnd(); ++object)
    {
        if (object.value()->getIdTool() == oldToolId)
        {
            oldIds.insert(object.key());
        }
    }

    QSet<quint32> ownerIds;
    quint32 replacementToolId = NULL_ID;
    try
    {
        replacementToolId = data->GetGObject(newObjectId)->getIdTool();
    }
    catch (const VExceptionBadId &)
    {
    }
    if (!detachedReplacement.isNull() && replacementToolId != NULL_ID)
    {
        // Store detaching and reference replacement in one command so undo cannot leave a half-repaired pattern.
        const QDomElement replacementXml = doc->elementById(replacementToolId);
        if (!replacementXml.isNull())
        {
            m_tools.append({replacementToolId, replacementXml.cloneNode(true).toElement(),
                            detachedReplacement.cloneNode(true).toElement()});
        }
    }
    const QVector<VToolDependency> dependencies = doc->getDirectDependencies(oldToolId, data);
    for (const VToolDependency &dependency : dependencies)
    {
        // Rewriting the replacement's own construction would create a self-reference when it was built from the old
        // object. The chooser normally excludes such descendants; keep this guard in the command as well.
        if (dependency.id == NULL_ID || dependency.id == replacementToolId || ownerIds.contains(dependency.id))
        {
            continue;
        }
        ownerIds.insert(dependency.id);

        const QDomElement oldXml = doc->elementById(dependency.id);
        if (oldXml.isNull())
        {
            continue;
        }
        QDomElement newXml = oldXml.cloneNode(true).toElement();
        if (replaceReferences(newXml, oldIds, newObjectId))
        {
            m_tools.append({dependency.id, oldXml.cloneNode(true).toElement(), newXml});
        }
    }
}

void ReplaceObjectReferences::undo()
{
    apply(false);
}

void ReplaceObjectReferences::redo()
{
    apply(true);
}

int ReplaceObjectReferences::changedToolCount() const
{
    return m_tools.size();
}

void ReplaceObjectReferences::apply(bool useNewXml)
{
    for (const ToolXml &tool : m_tools)
    {
        QDomElement current = doc->elementById(tool.id);
        if (!current.isNull())
        {
            current.parentNode().replaceChild((useNewXml ? tool.newXml : tool.oldXml).cloneNode(true), current);
        }
    }
    emit NeedFullParsing();
}
