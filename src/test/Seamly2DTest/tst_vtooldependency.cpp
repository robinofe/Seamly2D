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

#include "tst_vtooldependency.h"

#include "../ifc/xml/vabstractpattern.h"
#include "../ifc/xml/vpatternconverter.h"
#include "../ifc/xml/vtoolrecord.h"
#include "../vgeometry/vpointf.h"
#include "../vpatterndb/vcontainer.h"
#include "../vpatterndb/vpiecenode.h"
#include "../vpatterndb/variables/vinternalvariable.h"
#include "../vtools/tools/vabstracttool.h"
#include "../vtools/undocommands/addreferenceline.h"
#include "../vtools/undocommands/savepiecepathoptions.h"
#include "../vtools/undocommands/savetooloptions.h"

#include <QCheckBox>
#include <QDialog>
#include <QPushButton>
#include <QTemporaryFile>
#include <QTextStream>
#include <QTreeWidget>
#include <QtTest>

namespace
{
class DependencyPattern : public VAbstractPattern
{
public:
    void load(const QString &xml, const QVector<VToolRecord> &records = QVector<VToolRecord>())
    {
        QVERIFY(setContent(xml));
        m_activeDraftBlock = QStringLiteral("Block");
        m_history.clear();
        if (records.isEmpty())
        {
            m_history.append(VToolRecord(1, Tool::BasePoint, QStringLiteral("Block")));
            m_history.append(VToolRecord(2, Tool::EndLine, QStringLiteral("Block")));
            m_history.append(VToolRecord(3, Tool::AlongLine, QStringLiteral("Block")));
        }
        else
        {
            m_history = records;
        }
    }

    void CreateEmptyFile() override {}
    void IncrementReferens(quint32 id) const override { increments.append(id); }
    void DecrementReferens(quint32 id) const override { decrements.append(id); }
    QStringList GetCurrentAlphabet() const override { return QStringList(); }
    QString GenerateLabel(const LabelType &, const QString & = QString()) const override { return QString(); }
    QString GenerateSuffix(const QString &) const override { return QString(); }
    void UpdateToolData(const quint32 &, VContainer *) override {}
    void LiteParseTree(const Document &) override {}

    mutable QVector<quint32> increments;
    mutable QVector<quint32> decrements;
};

class ToolVariable : public VInternalVariable
{
public:
    explicit ToolVariable(quint32 toolId)
        : m_toolId(toolId)
    {
    }

    bool Filter(quint32 id) override
    {
        return id == m_toolId;
    }

private:
    quint32 m_toolId;
};

class DependencyTool : public VAbstractTool
{
public:
    DependencyTool(VAbstractPattern *pattern, VContainer *data, quint32 id)
        : VAbstractTool(pattern, data, id)
    {
    }

    using VAbstractTool::showDependencies;

    QString getTagName() const override { return QStringLiteral("point"); }
    void ShowVisualization(bool) override {}
    void FullUpdateFromFile() override {}
    void AllowHover(bool) override {}
    void AllowSelecting(bool) override {}
    void GroupVisibility(quint32, bool) override {}

protected:
    void AddToFile() override {}
    void SetVisualization() override {}
};

const QString patternXml = QStringLiteral(
    "<pattern><draftBlock name=\"Block\"><calculation>"
    "<point id=\"1\" name=\"A\" type=\"single\"/>"
    "<point id=\"2\" name=\"B\" type=\"endLine\" basePoint=\"1\"/>"
    "<point id=\"3\" name=\"C\" type=\"alongLine\" firstPoint=\"2\" secondPoint=\"2\"/>"
    "</calculation></draftBlock></pattern>");
}

void TST_VToolDependency::referenceAttributes()
{
    const QSet<QString> &attributes = VAbstractPattern::objectReferenceAttributes();
    QVERIFY(attributes.contains(QStringLiteral("basePoint")));
    QVERIFY(attributes.contains(QStringLiteral("spline")));
    QVERIFY(attributes.contains(QStringLiteral("splinePath")));
    QVERIFY(attributes.contains(QStringLiteral("path")));
    QVERIFY(attributes.contains(QStringLiteral("idObject")));
}

void TST_VToolDependency::directDependencies()
{
    DependencyPattern pattern;
    pattern.load(patternXml);
    const Unit unit = Unit::Cm;
    VContainer data(nullptr, &unit);

    const QVector<VToolDependency> dependencies = pattern.getDirectDependencies(1, &data);

    QCOMPARE(dependencies.size(), 1);
    QCOMPARE(dependencies.at(0).id, quint32(2));
    QCOMPARE(dependencies.at(0).name, QStringLiteral("B"));
    QCOMPARE(dependencies.at(0).reference, QStringLiteral("base point"));
    QCOMPARE(dependencies.at(0).draftBlockName, QStringLiteral("Block"));
}

void TST_VToolDependency::recursiveDependencies()
{
    DependencyPattern pattern;
    pattern.load(patternXml);
    const Unit unit = Unit::Cm;
    VContainer data(nullptr, &unit);

    const QVector<VToolDependency> dependencies = pattern.getDependentObjectsRecursive(1, &data);

    QCOMPARE(dependencies.size(), 2);
    QCOMPARE(dependencies.at(0).id, quint32(2));
    QCOMPARE(dependencies.at(0).depth, 1);
    QCOMPARE(dependencies.at(1).id, quint32(3));
    QCOMPARE(dependencies.at(1).depth, 2);

    const QVector<VToolDependency> descendants = pattern.getDependentObjectsRecursive(2, &data);
    QCOMPARE(descendants.size(), 1);
    QCOMPARE(descendants.at(0).id, quint32(3));
    QCOMPARE(descendants.at(0).depth, 1);
}

void TST_VToolDependency::structuredDependencies()
{
    const QString xml = QStringLiteral(
        "<pattern><draftBlock name=\"Block\"><calculation>"
        "<point id=\"1\" name=\"A\" type=\"single\"/>"
        "</calculation><modeling>"
        "<operation id=\"5\" type=\"rotation\" center=\"1\"/>"
        "<path id=\"6\" name=\"Pocket path\"><nodes><node idObject=\"1\"/></nodes></path>"
        "</modeling><pieces>"
        "<piece id=\"4\" name=\"Front\"><nodes><node idObject=\"1\"/></nodes>"
        "<iPaths><record path=\"6\"/></iPaths><anchors><record>1</record></anchors></piece>"
        "</pieces></draftBlock></pattern>");

    QVector<VToolRecord> records;
    records << VToolRecord(1, Tool::BasePoint, QStringLiteral("Block"))
            << VToolRecord(4, Tool::Piece, QStringLiteral("Block"))
            << VToolRecord(5, Tool::Rotation, QStringLiteral("Block"))
            << VToolRecord(6, Tool::InternalPath, QStringLiteral("Block"));

    DependencyPattern pattern;
    pattern.load(xml, records);
    const Unit unit = Unit::Cm;
    VContainer data(nullptr, &unit);

    const QVector<VToolDependency> dependencies = pattern.getDirectDependencies(1, &data);
    QCOMPARE(dependencies.size(), 3);

    QSet<quint32> ids;
    for (int i = 0; i < dependencies.size(); ++i)
    {
        ids.insert(dependencies.at(i).id);
    }
    QVERIFY(ids.contains(4));
    QVERIFY(ids.contains(5));
    QVERIFY(ids.contains(6));

    const QVector<VToolDependency> pathDependencies = pattern.getDirectDependencies(6, &data);
    QCOMPARE(pathDependencies.size(), 1);
    QCOMPARE(pathDependencies.at(0).id, quint32(4));

    const QString nodeXml = QStringLiteral(
        "<pattern><draftBlock name=\"Block\"><calculation>"
        "<point id=\"1\" name=\"A18\" type=\"single\"/>"
        "</calculation><modeling>"
        "<point id=\"136\" idObject=\"1\" type=\"modeling\"/>"
        "</modeling></draftBlock></pattern>");
    QVector<VToolRecord> nodeRecords;
    nodeRecords << VToolRecord(1, Tool::BasePoint, QStringLiteral("Block"))
                << VToolRecord(136, Tool::NodePoint, QStringLiteral("Block"));
    pattern.load(nodeXml, nodeRecords);
    data.UpdateGObject(1, new VPointF(0, 0, QStringLiteral("A18"), 0, 0));

    const QVector<VToolDependency> nodeDependencies = pattern.getDirectDependencies(1, &data);
    QCOMPARE(nodeDependencies.size(), 1);
    QCOMPARE(nodeDependencies.at(0).name, QStringLiteral("A18"));
}

void TST_VToolDependency::unusedNodeDependencies()
{
    const QString xml = QStringLiteral(
        "<pattern><draftBlock name=\"Block\"><calculation>"
        "<point id=\"1\" name=\"A18\" type=\"single\"/>"
        "</calculation><modeling>"
        "<point id=\"136\" idObject=\"1\" type=\"modeling\" inUse=\"false\"/>"
        "</modeling></draftBlock></pattern>");
    QVector<VToolRecord> records;
    records << VToolRecord(1, Tool::BasePoint, QStringLiteral("Block"))
            << VToolRecord(136, Tool::NodePoint, QStringLiteral("Block"));

    DependencyPattern pattern;
    pattern.load(xml, records);
    const Unit unit = Unit::Cm;
    VContainer data(nullptr, &unit);
    data.UpdateGObject(1, new VPointF(0, 0, QStringLiteral("A18"), 0, 0));

    QVERIFY(pattern.getDirectDependencies(1, &data).isEmpty());
}

void TST_VToolDependency::recursiveFormulaDependencies()
{
    const QString xml = QStringLiteral(
        "<pattern><variables><variable name=\"Intermediate\" formula=\"source_value\"/></variables>"
        "<draftBlock name=\"Block\"><calculation>"
        "<point id=\"1\" name=\"A\" type=\"single\"/>"
        "<point id=\"4\" name=\"D\" type=\"single\"/>"
        "<point id=\"2\" name=\"B\" type=\"endLine\" basePoint=\"4\" length=\"Intermediate\"/>"
        "</calculation></draftBlock></pattern>");

    QVector<VToolRecord> records;
    records << VToolRecord(1, Tool::BasePoint, QStringLiteral("Block"))
            << VToolRecord(4, Tool::BasePoint, QStringLiteral("Block"))
            << VToolRecord(2, Tool::EndLine, QStringLiteral("Block"));

    DependencyPattern pattern;
    pattern.load(xml, records);
    const Unit unit = Unit::Cm;
    VContainer data(nullptr, &unit);
    data.AddVariable(QStringLiteral("source_value"), new ToolVariable(1));

    const QVector<VToolDependency> direct = pattern.getDirectDependencies(1, &data);
    QCOMPARE(direct.size(), 1);
    QCOMPARE(direct.at(0).id, quint32(NULL_ID));
    QCOMPARE(direct.at(0).name, QStringLiteral("Intermediate"));

    const QVector<VToolDependency> recursive = pattern.getDependentObjectsRecursive(1, &data);
    QCOMPARE(recursive.size(), 2);
    QCOMPARE(recursive.at(0).name, QStringLiteral("Intermediate"));
    QCOMPARE(recursive.at(0).depth, 1);
    QCOMPARE(recursive.at(1).id, quint32(2));
    QCOMPARE(recursive.at(1).depth, 2);
}

void TST_VToolDependency::referenceLengthFormulaIsTracked()
{
    const QString xml = QStringLiteral(
        "<pattern><draftBlock name=\"Block\"><calculation/>"
        "<referenceLines><referenceLine id=\"10\" formula=\"@waist + 2\"/></referenceLines>"
        "</draftBlock></pattern>");
    DependencyPattern pattern;
    pattern.load(xml, {VToolRecord(1, Tool::BasePoint, QStringLiteral("Block"))});

    const QVector<VFormulaField> expressions = pattern.ListExpressions();
    bool found = false;
    for (const VFormulaField &field : expressions)
    {
        if (field.element.tagName() == VAbstractPattern::TagReferenceLine)
        {
            QCOMPARE(field.expression, QStringLiteral("@waist + 2"));
            QCOMPARE(field.attribute, VAbstractPattern::VariableFormula);
            found = true;
        }
    }
    QVERIFY(found);
}

void TST_VToolDependency::referenceLengthSchema()
{
    const QString sample = QStringLiteral(SRCDIR "../../app/share/samples/patterns/trousers.sm2d");
    QFile sampleFile(sample);
    QVERIFY(sampleFile.open(QIODevice::ReadOnly));
    QTemporaryFile source;
    QVERIFY(source.open());
    QCOMPARE(source.write(sampleFile.readAll()), sampleFile.size());
    source.flush();
    sampleFile.close();

    VPatternConverter converter(source.fileName());
    QFile converted(converter.Convert());
    QVERIFY(converted.open(QIODevice::ReadOnly));

    QDomDocument pattern;
    QVERIFY(pattern.setContent(&converted));
    converted.close();

    QDomElement draftBlock = pattern.elementsByTagName(VAbstractPattern::TagDraftBlock).at(0).toElement();
    QVERIFY(!draftBlock.isNull());
    QDomElement referenceLines = pattern.createElement(VAbstractPattern::TagReferenceLines);
    QDomElement referenceLine = pattern.createElement(VAbstractPattern::TagReferenceLine);
    referenceLine.setAttribute(QStringLiteral("id"), QStringLiteral("900000"));
    referenceLine.setAttribute(QStringLiteral("name"), QStringLiteral("Hip check"));
    referenceLine.setAttribute(QStringLiteral("formula"), QStringLiteral("@hip"));
    referenceLine.setAttribute(QStringLiteral("xPos"), QStringLiteral("10"));
    referenceLine.setAttribute(QStringLiteral("yPos"), QStringLiteral("20"));
    referenceLine.setAttribute(QStringLiteral("rotation"), QStringLiteral("0"));
    referenceLine.setAttribute(QStringLiteral("anchorObject"), QStringLiteral("0"));
    referenceLine.setAttribute(QStringLiteral("anchorPosition"), QStringLiteral("0.5"));
    referenceLine.setAttribute(QStringLiteral("orientation"), QStringLiteral("horizontal"));
    referenceLine.setAttribute(QStringLiteral("visible"), QStringLiteral("true"));
    referenceLines.appendChild(referenceLine);
    draftBlock.appendChild(referenceLines);

    QTemporaryFile file;
    QVERIFY(file.open());
    QTextStream output(&file);
    pattern.save(output, 4);
    output.flush();
    file.flush();

    bool valid = true;
    try
    {
        VDomDocument::ValidateXML(VPatternConverter::CurrentSchema, file.fileName());
    }
    catch (...)
    {
        valid = false;
    }
    QVERIFY(valid);
}

void TST_VToolDependency::referenceLengthUndo()
{
    DependencyPattern pattern;
    pattern.load(QStringLiteral(
        "<pattern><draftBlock name=\"Block\"><calculation/><referenceLines/></draftBlock></pattern>"));

    QDomElement element = pattern.createElement(VAbstractPattern::TagReferenceLine);
    element.setAttribute(QStringLiteral("id"), QStringLiteral("10"));
    element.setAttribute(QStringLiteral("name"), QStringLiteral("Hip check"));
    element.setAttribute(QStringLiteral("formula"), QStringLiteral("@hip"));

    AddReferenceLine command(element, &pattern);
    command.redo();
    QVERIFY(!pattern.elementById(10, VAbstractPattern::TagReferenceLine).isNull());

    command.undo();
    QVERIFY(pattern.elementById(10, VAbstractPattern::TagReferenceLine).isNull());

    command.redo();
    QVERIFY(!pattern.elementById(10, VAbstractPattern::TagReferenceLine).isNull());
}

void TST_VToolDependency::internalPathReferenceCounts()
{
    DependencyPattern pattern;
    pattern.load(QStringLiteral(
        "<pattern><draftBlock name=\"Block\"><calculation/><modeling>"
        "<path id=\"6\"><nodes/></path></modeling></draftBlock></pattern>"),
        {VToolRecord(6, Tool::InternalPath, QStringLiteral("Block"))});
    const Unit unit = Unit::Cm;
    VContainer data(nullptr, &unit);

    VPiecePath oldPath;
    oldPath.Append(VPieceNode(1, Tool::NodePoint));
    VPiecePath newPath;
    newPath.Append(VPieceNode(2, Tool::NodePoint));

    SavePiecePathOptions command(NULL_ID, oldPath, newPath, &pattern, &data, 6);
    command.redo();
    QCOMPARE(pattern.increments, QVector<quint32>{2});
    QCOMPARE(pattern.decrements, QVector<quint32>{1});

    pattern.increments.clear();
    pattern.decrements.clear();
    command.undo();
    QCOMPARE(pattern.increments, QVector<quint32>{1});
    QCOMPARE(pattern.decrements, QVector<quint32>{2});
}

void TST_VToolDependency::dependencyDialog()
{
    DependencyPattern pattern;
    pattern.load(patternXml);
    const Unit unit = Unit::Cm;
    VContainer data(nullptr, &unit);
    DependencyTool tool(&pattern, &data, 1);
    QSignalSpy highlightSpy(&pattern, &VAbstractPattern::ShowTool);

    bool dialogFound = false;
    int directRows = -1;
    int recursiveRows = -1;
    QString firstObject;
    QString firstType;
    QString firstReference;
    QString firstSuggestion;
    bool openPropertiesShown = false;
    bool replaceNodeShown = false;

    QTimer::singleShot(0, qApp, [&]()
    {
        auto *dialog = qobject_cast<QDialog *>(QApplication::activeModalWidget());
        dialogFound = dialog != nullptr;
        if (dialog == nullptr)
        {
            return;
        }

        auto *tree = dialog->findChild<QTreeWidget *>();
        auto *recursive = dialog->findChild<QCheckBox *>();
        if (tree != nullptr)
        {
            directRows = tree->topLevelItemCount();
            if (directRows > 0)
            {
                firstObject = tree->topLevelItem(0)->text(0);
                firstType = tree->topLevelItem(0)->text(1);
                firstReference = tree->topLevelItem(0)->text(2);
                firstSuggestion = tree->topLevelItem(0)->text(4);
                tree->itemClicked(tree->topLevelItem(0), 0);
                auto *openButton = dialog->findChild<QPushButton *>(
                    QStringLiteral("dependencyOpenPropertiesButton"));
                auto *replaceButton = dialog->findChild<QPushButton *>(
                    QStringLiteral("dependencyReplaceNodeButton"));
                openPropertiesShown = openButton != nullptr && !openButton->isHidden();
                replaceNodeShown = replaceButton != nullptr && !replaceButton->isHidden();
            }
        }
        if (recursive != nullptr)
        {
            recursive->setChecked(true);
            recursiveRows = tree == nullptr ? -1 : tree->topLevelItemCount();
        }
        dialog->accept();
    });

    tool.showDependencies();

    QVERIFY(dialogFound);
    QCOMPARE(directRows, 1);
    QCOMPARE(recursiveRows, 2);
    QCOMPARE(firstObject, QStringLiteral("B"));
    QVERIFY(!firstType.isEmpty());
    QVERIFY(!firstReference.isEmpty());
    QCOMPARE(firstSuggestion, QStringLiteral("Select the object and replace base point in Properties"));
    QVERIFY(openPropertiesShown);
    QVERIFY(!replaceNodeShown);
    QVERIFY(!highlightSpy.isEmpty());
    QCOMPARE(highlightSpy.at(0).at(0).toUInt(), quint32(2));
    QCOMPARE(highlightSpy.at(0).at(1).toBool(), true);
}

void TST_VToolDependency::referenceChangeRequestsFullParse()
{
    DependencyPattern pattern;
    pattern.load(patternXml);

    const QDomElement oldXml = pattern.elementById(2);
    QDomElement newXml = oldXml.cloneNode(true).toElement();
    newXml.setAttribute(QStringLiteral("basePoint"), QStringLiteral("3"));

    SaveToolOptions command(oldXml, newXml, &pattern, 2);
    QSignalSpy fullParseSpy(&command, &SaveToolOptions::NeedFullParsing);
    QSignalSpy liteParseSpy(&command, &SaveToolOptions::NeedLiteParsing);
    command.redo();

    QCOMPARE(fullParseSpy.size(), 1);
    QCOMPARE(liteParseSpy.size(), 0);
}

void TST_VToolDependency::childReferenceChangeRequestsFullParse()
{
    const QString xml = QStringLiteral(
        "<pattern><draftBlock name=\"Block\"><calculation>"
        "<point id=\"1\" name=\"A\" type=\"single\"/>"
        "</calculation><modeling>"
        "<path id=\"6\" name=\"Pocket path\"><nodes><node idObject=\"1\"/></nodes></path>"
        "</modeling></draftBlock></pattern>");
    QVector<VToolRecord> records;
    records << VToolRecord(1, Tool::BasePoint, QStringLiteral("Block"))
            << VToolRecord(6, Tool::InternalPath, QStringLiteral("Block"));

    DependencyPattern pattern;
    pattern.load(xml, records);

    const QDomElement oldXml = pattern.elementById(6);
    QDomElement newXml = oldXml.cloneNode(true).toElement();
    newXml.firstChildElement(QStringLiteral("nodes")).firstChildElement(QStringLiteral("node"))
        .setAttribute(QStringLiteral("idObject"), QStringLiteral("2"));

    SaveToolOptions command(oldXml, newXml, &pattern, 6);
    QSignalSpy fullParseSpy(&command, &SaveToolOptions::NeedFullParsing);
    QSignalSpy liteParseSpy(&command, &SaveToolOptions::NeedLiteParsing);
    command.redo();

    QCOMPARE(fullParseSpy.size(), 1);
    QCOMPARE(liteParseSpy.size(), 0);
}

void TST_VToolDependency::visualChangeRequestsLiteParse()
{
    DependencyPattern pattern;
    pattern.load(patternXml);

    const QDomElement oldXml = pattern.elementById(2);
    QDomElement newXml = oldXml.cloneNode(true).toElement();
    newXml.setAttribute(QStringLiteral("mx"), QStringLiteral("5"));

    SaveToolOptions command(oldXml, newXml, &pattern, 2);
    QSignalSpy fullParseSpy(&command, &SaveToolOptions::NeedFullParsing);
    QSignalSpy liteParseSpy(&command, &SaveToolOptions::NeedLiteParsing);
    command.redo();

    QCOMPARE(fullParseSpy.size(), 0);
    QCOMPARE(liteParseSpy.size(), 1);
}
