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

#ifndef REPLACEOBJECTREFERENCES_H
#define REPLACEOBJECTREFERENCES_H

#include "vundocommand.h"

#include <QVector>

class VContainer;

class ReplaceObjectReferences : public VUndoCommand
{
    Q_OBJECT

public:
    ReplaceObjectReferences(quint32 oldToolId, quint32 newObjectId, VAbstractPattern *doc, const VContainer *data,
                            QUndoCommand *parent = nullptr);

    void undo() override;
    void redo() override;

    int changedToolCount() const;

private:
    struct ToolXml
    {
        quint32     id{NULL_ID};
        QDomElement oldXml{};
        QDomElement newXml{};
    };

    void apply(bool useNewXml);

    QVector<ToolXml> m_tools{};
};

#endif // REPLACEOBJECTREFERENCES_H
