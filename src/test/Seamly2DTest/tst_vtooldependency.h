/***************************************************************************
 *                                                                         *
 *   Copyright (C) 2026 Seamly2D project                                   *
 *                                                                         *
 *   This program is free software: you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation, either version 3 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 **************************************************************************/

#ifndef TST_VTOOLDEPENDENCY_H
#define TST_VTOOLDEPENDENCY_H

#include <QObject>

class TST_VToolDependency : public QObject
{
    Q_OBJECT

private slots:
    void referenceAttributes();
    void directDependencies();
    void recursiveDependencies();
    void pointOnSplineDependency();
    void structuredDependencies();
    void unusedNodeDependencies();
    void recursiveFormulaDependencies();
    void referenceLengthFormulaIsTracked();
    void referenceLengthSchema();
    void referenceLengthUndo();
    void internalPathReferenceCounts();
    void replaceObjectReferences();
    void dependencyDialog();
    void nodeDependencyActions();
    void globalReplacementDialog();
    void detachedPointReplacement();
    void referenceChangeRequestsFullParse();
    void childReferenceChangeRequestsFullParse();
    void visualChangeRequestsLiteParse();
};

#endif // TST_VTOOLDEPENDENCY_H
