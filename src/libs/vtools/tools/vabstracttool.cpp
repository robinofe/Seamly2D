/***************************************************************************
 **  @file   vabstracttool.cpp
 **  @author Douglas S Caskey
 **  @date   17 Sep, 2023
 **
 **  @copyright
 **  Copyright (C) 2017 - 2023 Seamly, LLC
 **  https://github.com/fashionfreedom/seamly2d
 **
 **  @brief
 **  Seamly2D is free software: you can redistribute it and/or modify
 **  it under the terms of the GNU General Public License as published by
 **  the Free Software Foundation, either version 3 of the License, or
 **  (at your option) any later version.
 **
 **  Seamly2D is distributed in the hope that it will be useful,
 **  but WITHOUT ANY WARRANTY; without even the implied warranty of
 **  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 **  GNU General Public License for more details.
 **
 **  You should have received a copy of the GNU General Public License
 **  along with Seamly2D. If not, see <http://www.gnu.org/licenses/>.
 **************************************************************************/

/************************************************************************
 **  @file   vabstracttool.cpp
 **  @author Roman Telezhynskyi <dismine(at)gmail.com>
 **  @date   November 15, 2013
 **
 **  @brief
 **  @copyright
 **  This source code is part of the Valentina project, a pattern making
 **  program, whose allow create and modeling patterns of clothing.
 **  Copyright (C) 2013 Valentina project
 **  <https://bitbucket.org/dismine/valentina> All Rights Reserved.
 **
 **  Valentina is free software: you can redistribute it and/or modify
 **  it under the terms of the GNU General Public License as published by
 **  the Free Software Foundation, either version 3 of the License, or
 **  (at your option) any later version.
 **
 **  Valentina is distributed in the hope that it will be useful,
 **  but WITHOUT ANY WARRANTY; without even the implied warranty of
 **  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 **  GNU General Public License for more details.
 **
 **  You should have received a copy of the GNU General Public License
 **  along with Seamly2D.  If not, see <http://www.gnu.org/licenses/>.
 **
 *************************************************************************/

#include "vabstracttool.h"

#include <QBrush>
#include <QApplication>
#include <QClipboard>
#include <QCoreApplication>
#include <QDialog>
#include <QDialogButtonBox>
#include <QDomDocument>
#include <QCheckBox>
#include <QComboBox>
#include <QFlags>
#include <QGraphicsEllipseItem>
#include <QGraphicsLineItem>
#include <QHash>
#include <QHBoxLayout>
#include <QIcon>
#include <QLineF>
#include <QMessageBox>
#include <QLabel>
#include <QListWidget>
#include <QTreeWidget>
#include <QVBoxLayout>
#include <QPainter>
#include <QPen>
#include <QPixmap>
#include <QPlainTextEdit>
#include <QPoint>
#include <QPointF>
#include <QPushButton>
#include <QRectF>
#include <QSharedPointer>
#include <QString>
#include <QStyle>
#include <QTextStream>
#include <QTimer>
#include <QToolButton>
#include <QUndoStack>
#include <QVector>
#include <new>
#include <qnumeric.h>

#include "../vgeometry/vpointf.h"
#include "../vpropertyexplorer/checkablemessagebox.h"
#include "../vwidgets/vmaingraphicsview.h"
#include "../ifc/exception/vexception.h"
#include "../ifc/exception/vexceptionbadid.h"
#include "../ifc/exception/vexceptionundo.h"
#include "../ifc/xml/vtoolrecord.h"
#include "../undocommands/deltool.h"
#include "../undocommands/replaceobjectreferences.h"
#include "pattern_piece_tool.h"
#include "../vgeometry/../ifc/ifcdef.h"
#include "../vgeometry/vgeometrydef.h"
#include "../vgeometry/vgobject.h"
#include "../vgeometry/vcubicbezier.h"
#include "../vgeometry/vcubicbezierpath.h"
#include "../vgeometry/vsplinepath.h"
#include "../vgeometry/varc.h"
#include "../vgeometry/vellipticalarc.h"
#include "../vmisc/vcommonsettings.h"
#include "../vmisc/logging.h"
#include "../vpatterndb/vcontainer.h"
#include "../vpatterndb/vpiecenode.h"
#include "../vpatterndb/calculator.h"
#include "../vwidgets/vgraphicssimpletextitem.h"
#include "nodeDetails/nodedetails.h"
#include "../dialogs/support/dialogundo.h"
#include "../dialogs/support/edit_formula_dialog.h"

template <class T> class QSharedPointer;

bool VAbstractTool::m_suppressContextMenu = false;
const QString VAbstractTool::AttrInUse = QStringLiteral("inUse");

namespace
{
//---------------------------------------------------------------------------------------------------------------------
QString GeometryTypeName(GOType type)
{
    switch (type)
    {
        case GOType::Point: return QStringLiteral("Point");
        case GOType::Arc: return QStringLiteral("Arc");
        case GOType::EllipticalArc: return QStringLiteral("EllipticalArc");
        case GOType::Spline: return QStringLiteral("Spline");
        case GOType::SplinePath: return QStringLiteral("SplinePath");
        case GOType::CubicBezier: return QStringLiteral("CubicBezier");
        case GOType::CubicBezierPath: return QStringLiteral("CubicBezierPath");
        case GOType::Unknown: return QStringLiteral("Unknown");
        case GOType::Curve: return QStringLiteral("Curve");
        case GOType::Path: return QStringLiteral("Path");
        case GOType::AllCurves: return QStringLiteral("AllCurves");
    }
    return QStringLiteral("Invalid(%1)").arg(static_cast<int>(type));
}

//---------------------------------------------------------------------------------------------------------------------
QString DrawModeName(Draw mode)
{
    switch (mode)
    {
        case Draw::Calculation: return QStringLiteral("Calculation");
        case Draw::Modeling: return QStringLiteral("Modeling");
        case Draw::Layout: return QStringLiteral("Layout");
    }
    return QStringLiteral("Invalid(%1)").arg(static_cast<int>(mode));
}

//---------------------------------------------------------------------------------------------------------------------
QString ReplacementDiagnostics(VAbstractPattern *doc, const VContainer *data,
                               const QSharedPointer<VGObject> &source, quint32 sourceObjectId,
                               const QSet<quint32> &trackedObjects, bool allowTrackedPointDetachment,
                               bool requireTrackedObject)
{
    QString report;
    QTextStream out(&report);
    out << "Seamly2D replacement diagnostics v1\n";
    out << "Diagnostic build marker: replacement-diagnostics-2026-08-17\n";
    out << "Executable: " << QCoreApplication::applicationFilePath() << '\n';
    out << "Application version: " << QCoreApplication::applicationVersion() << '\n';
    out << "Qt version: " << qVersion() << "\n\n";
    out << "Source object ID: " << sourceObjectId << '\n';
    out << "Source name: " << source->name() << '\n';
    out << "Source geometry type: " << GeometryTypeName(source->getType()) << " ("
        << static_cast<int>(source->getType()) << ")\n";
    out << "Source tool ID: " << source->getIdTool() << "\n\n";

    QSet<quint32> descendantTools;
    const QVector<VToolDependency> descendants =
        doc->getDependentObjectsRecursive(source->getIdTool(), data);
    out << "Dependency descendants: " << descendants.size() << '\n';
    for (const VToolDependency &dependency : descendants)
    {
        if (dependency.id != NULL_ID)
        {
            descendantTools.insert(dependency.id);
        }
        out << "  tool=" << dependency.id << ", depth=" << dependency.depth
            << ", name=" << dependency.name << ", type=" << dependency.typeName
            << ", reference=" << dependency.reference << '\n';
    }

    const auto geometry = data->DataGObjects();
    out << "\nGeometry object count: " << geometry->size() << '\n';
    out << "Tracked/new object IDs:";
    for (quint32 id : trackedObjects)
    {
        out << ' ' << id;
    }
    out << "\n\nCandidate evaluation:\n";

    bool foundRelevantObject = false;
    for (auto object = geometry->constBegin(); object != geometry->constEnd(); ++object)
    {
        const QSharedPointer<VGObject> candidate = object.value();
        const bool tracked = trackedObjects.contains(object.key());
        const bool sameType = candidate->getType() == source->getType();
        if (!tracked && !sameType)
        {
            continue;
        }
        foundRelevantObject = true;
        const bool descendant = descendantTools.contains(candidate->getIdTool());
        const bool canDetach = allowTrackedPointDetachment && tracked && source->getType() == GOType::Point;
        QString result = QStringLiteral("eligible");
        if (candidate->getMode() != Draw::Calculation)
        {
            result = QStringLiteral("excluded: mode is not Calculation");
        }
        else if (candidate->getIdObject() != NULL_ID)
        {
            result = QStringLiteral("excluded: idObject is not 0");
        }
        else if (candidate->getIdTool() == source->getIdTool())
        {
            result = QStringLiteral("excluded: same tool as source");
        }
        else if (requireTrackedObject && !tracked)
        {
            result = QStringLiteral("excluded: object existed before replacement session");
        }
        else if (!sameType)
        {
            result = QStringLiteral("excluded: geometry type differs from source");
        }
        else if (descendant && !canDetach)
        {
            result = QStringLiteral("excluded: descendant of source");
        }
        else if (descendant)
        {
            result = QStringLiteral("eligible: tracked point will be detached");
        }

        out << "- object ID: " << object.key() << '\n';
        out << "  name: " << candidate->name() << '\n';
        out << "  geometry type: " << GeometryTypeName(candidate->getType()) << " ("
            << static_cast<int>(candidate->getType()) << ")\n";
        out << "  tool ID: " << candidate->getIdTool() << '\n';
        out << "  mode: " << DrawModeName(candidate->getMode()) << " ("
            << static_cast<int>(candidate->getMode()) << ")\n";
        out << "  idObject: " << candidate->getIdObject() << '\n';
        out << "  tracked/new: " << (tracked ? "yes" : "no") << '\n';
        out << "  descendant: " << (descendant ? "yes" : "no") << '\n';
        out << "  result: " << result << '\n';

        const QDomElement element = doc->elementById(candidate->getIdTool());
        if (element.isNull())
        {
            out << "  XML: <not found>\n";
        }
        else
        {
            QDomDocument snippet;
            snippet.appendChild(snippet.importNode(element, true));
            QString xml = snippet.toString(-1).trimmed();
            xml.replace('\n', ' ');
            out << "  XML: " << xml << '\n';
        }
    }
    if (!foundRelevantObject)
    {
        out << "  <no tracked or type-compatible geometry objects found>\n";
    }
    return report;
}

//---------------------------------------------------------------------------------------------------------------------
quint32 CreateNodeSpline(VContainer *data, quint32 id)
{
    if (data->GetGObject(id)->getType() == GOType::Spline)
    {
        return VAbstractTool::CreateNode<VSpline>(data, id);
    }
    else
    {
        return VAbstractTool::CreateNode<VCubicBezier>(data, id);
    }
}

//---------------------------------------------------------------------------------------------------------------------
quint32 CreateNodeSplinePath(VContainer *data, quint32 id)
{
    if (data->GetGObject(id)->getType() == GOType::SplinePath)
    {
        return VAbstractTool::CreateNode<VSplinePath>(data, id);
    }
    else
    {
        return VAbstractTool::CreateNode<VCubicBezierPath>(data, id);
    }
}
}//static functions

//---------------------------------------------------------------------------------------------------------------------
/**
 * @brief VAbstractTool container.
 * @param doc dom document container.
 * @param data container with data.
 * @param id object id in container.
 * @param parent parent object.
 */
VAbstractTool::VAbstractTool(VAbstractPattern *doc, VContainer *data, quint32 id, QObject *parent)
    : VDataTool(data, parent)
    , doc(doc)
    , m_id(id)
    , vis()
    , selectionType(SelectionType::ByMouseRelease)
{
    SCASSERT(doc != nullptr)
    connect(this, &VAbstractTool::toolHasChanges, this->doc, &VAbstractPattern::haveLiteChange);
    connect(this->doc, &VAbstractPattern::FullUpdateFromFile, this, &VAbstractTool::FullUpdateFromFile);
    connect(this, &VAbstractTool::LiteUpdateTree, this->doc, &VAbstractPattern::LiteParseTree);
}

//---------------------------------------------------------------------------------------------------------------------
VAbstractTool::~VAbstractTool()
{
    if (not vis.isNull())
    {
        delete vis;
    }
}

//---------------------------------------------------------------------------------------------------------------------
/**
 * @brief CheckFormula check formula.
 *
 * Try calculate formula. If find error show dialog that allow user try fix formula. If user can't throw exception. In
 * successes case return result calculation and fixed formula string. If formula ok don't touch formula.
 *
 * @param toolId [in] tool's id.
 * @param formula [in|out] string with formula.
 * @param data [in] container with variables. Need for math parser.
 * @throw QmuParserError.
 * @return result of calculation formula.
 */
qreal VAbstractTool::CheckFormula(const quint32 &toolId, QString &formula, VContainer *data)
{
    SCASSERT(data != nullptr)
    qreal result = 0;
    try
    {
        QScopedPointer<Calculator> cal(new Calculator());
        result = cal->EvalFormula(data->DataVariables(), formula);

        if (qIsInf(result) || qIsNaN(result))
        {
            qWarning() << "Invalid the formula value";
            return 0;
        }
    }
    catch (qmu::QmuParserError &error)
    {
        qDebug() << "\nMath parser error:\n"
                   << "--------------------------------------\n"
                   << "Message:     " << error.GetMsg()  << "\n"
                   << "Expression:  " << error.GetExpr() << "\n"
                   << "--------------------------------------";

        if (qApp->isAppInGUIMode())
        {
            QScopedPointer<DialogUndo> dialogUndo(new DialogUndo(qApp->getMainWindow()));
            forever
            {
                if (dialogUndo->exec() == QDialog::Accepted)
                {
                    const UndoButton resultUndo = dialogUndo->Result();
                    if (resultUndo == UndoButton::Fix)
                    {
                        auto *dialog = new EditFormulaDialog(data, toolId, ToolDialog, qApp->getMainWindow());
                        dialog->setWindowTitle(tr("Edit wrong formula"));
                        dialog->SetFormula(formula);
                        if (dialog->exec() == QDialog::Accepted)
                        {
                            formula = dialog->GetFormula();
                            /* Need delete dialog here because parser in dialog don't allow use correct separator for
                             * parsing here. */
                            delete dialog;
                            QScopedPointer<Calculator> cal1(new Calculator());
                            result = cal1->EvalFormula(data->DataVariables(), formula);

                            if (qIsInf(result) || qIsNaN(result))
                            {
                                qWarning() << "Invalid the formula value";
                                return 0;
                            }

                            break;
                        }
                        else
                        {
                            delete dialog;
                        }
                    }
                    else
                    {
                        throw VExceptionUndo(QString("Undo wrong formula %1").arg(formula));
                    }
                }
                else
                {
                    throw;
                }
            }
        }
        else
        {
            throw;
        }
    }
    return result;
}

//---------------------------------------------------------------------------------------------------------------------
/**
 * @brief deleteTool full delete object from scene and file.
 */
void VAbstractTool::deleteTool(bool ask)
{
    qCDebug(vTool, "Deleting abstract tool.");
    if (not isUsed())
    {
        qCDebug(vTool, "No children.");
        qApp->getSceneView()->itemClicked(nullptr);
        if (ask)
        {
            qCDebug(vTool, "Asking.");
            if (ConfirmDeletion() == QMessageBox::No)
            {
                qCDebug(vTool, "User said no.");
                return;
            }
        }

        qCDebug(vTool, "Begin deleting.");
        DelTool *delTool = new DelTool(doc, m_id);
        connect(delTool, &DelTool::NeedFullParsing, doc, &VAbstractPattern::NeedFullParsing);
        qApp->getUndoStack()->push(delTool);

        // Throw exception, this will help prevent case when we forget to immediately quit function.
        VExceptionToolWasDeleted e("Tool was used after deleting.");
        throw e;
    }
    else
    {
        // The dependency dialog contains the reason and the available repair actions. Avoid a separate warning first.
        showDependencies();
    }
}

//---------------------------------------------------------------------------------------------------------------------
void VAbstractTool::showDependencies()
{
    showDependencies(false);
}

//---------------------------------------------------------------------------------------------------------------------
void VAbstractTool::showDependencies(bool showAllDescendants)
{
    enum class DependencyAction
    {
        None,
        Select,
        EditPiece,
        ReplaceNode,
        CreateReplacement,
        RemoveNode,
        ReplaceEverywhere,
        CreateEverywhere
    };

    QDialog dialog(qApp->getMainWindow());
    dialog.setWindowTitle(tr("Dependencies"));
    dialog.resize(720, 360);

    auto *layout = new QVBoxLayout(&dialog);
    QString objectName = tr("Object %1").arg(m_id);
    try
    {
        objectName = getData()->GetGObject(m_id)->name();
    }
    catch (const VExceptionBadId &)
    {
        try
        {
            objectName = getData()->GetPiece(m_id).GetName();
        }
        catch (const VExceptionBadId &)
        {
            // Some tools, for example lines and operations, have no object stored under the tool id.
        }
    }
    auto *label = new QLabel(&dialog);
    label->setWordWrap(true);
    layout->addWidget(label);

    auto *recursive = new QCheckBox(tr("Show all descendants"), &dialog);
    recursive->setChecked(showAllDescendants);
    layout->addWidget(recursive);

    auto *tree = new QTreeWidget(&dialog);
    tree->setColumnCount(5);
    tree->setSelectionMode(QAbstractItemView::ExtendedSelection);
    tree->setHeaderLabels(QStringList() << tr("Object") << tr("Type") << tr("Dependency") << tr("Draft block")
                                             << tr("Suggested action"));
    tree->setRootIsDecorated(false);
    layout->addWidget(tree);

    quint32 selectedToolId = NULL_ID;
    Tool selectedToolType = Tool::LAST_ONE_DO_NOT_USE;
    QVector<quint32> selectedNodeIds;
    DependencyAction selectedAction = DependencyAction::None;

    auto *actionLabel = new QLabel(tr("Select an object to see the available next steps."), &dialog);
    actionLabel->setWordWrap(true);
    layout->addWidget(actionLabel);

    auto *globalActionLayout = new QHBoxLayout;
    auto *replaceEverywhereButton = new QPushButton(tr("Replace %1 everywhere...").arg(objectName), &dialog);
    auto *createEverywhereButton = new QPushButton(tr("Create replacement for %1...").arg(objectName), &dialog);
    globalActionLayout->addWidget(replaceEverywhereButton);
    globalActionLayout->addWidget(createEverywhereButton);
    globalActionLayout->addStretch();
    layout->addLayout(globalActionLayout);

    auto *actionLayout = new QHBoxLayout;
    auto *selectButton = new QPushButton(tr("Open properties"), &dialog);
    auto *replaceButton = new QPushButton(tr("Select replacement..."), &dialog);
    auto *createButton = new QPushButton(tr("Create replacement geometry..."), &dialog);
    auto *removeButton = new QPushButton(tr("Detach from pattern piece..."), &dialog);
    auto *editPieceButton = new QPushButton(tr("Edit pattern piece"), &dialog);
    replaceEverywhereButton->setObjectName(QStringLiteral("dependencyReplaceEverywhereButton"));
    createEverywhereButton->setObjectName(QStringLiteral("dependencyCreateEverywhereButton"));
    selectButton->setObjectName(QStringLiteral("dependencyOpenPropertiesButton"));
    replaceButton->setObjectName(QStringLiteral("dependencyReplaceNodeButton"));
    createButton->setObjectName(QStringLiteral("dependencyCreateReplacementButton"));
    removeButton->setObjectName(QStringLiteral("dependencyRemoveNodeButton"));
    editPieceButton->setObjectName(QStringLiteral("dependencyEditPieceButton"));
    actionLayout->addWidget(selectButton);
    actionLayout->addWidget(replaceButton);
    actionLayout->addWidget(createButton);
    actionLayout->addWidget(removeButton);
    actionLayout->addWidget(editPieceButton);
    actionLayout->addStretch();
    layout->addLayout(actionLayout);

    selectButton->hide();
    replaceButton->hide();
    createButton->hide();
    removeButton->hide();
    editPieceButton->hide();

    bool canReplaceEverywhere = false;
    bool canCreateEverywhere = false;
    try
    {
        const QSharedPointer<VGObject> source = getData()->GetGObject(m_id);
        canReplaceEverywhere = source->getMode() == Draw::Calculation;
        switch (source->getType())
        {
            case GOType::Point:
            case GOType::Spline:
            case GOType::SplinePath:
            case GOType::CubicBezier:
            case GOType::CubicBezierPath:
            case GOType::Arc:
            case GOType::EllipticalArc:
                canCreateEverywhere = canReplaceEverywhere;
                break;
            default:
                break;
        }
    }
    catch (const VExceptionBadId &)
    {
    }
    replaceEverywhereButton->setVisible(canReplaceEverywhere);
    createEverywhereButton->setVisible(canCreateEverywhere);

    auto *buttons = new QDialogButtonBox(QDialogButtonBox::Close, &dialog);

    auto clearActions = [this, actionLabel, selectButton, replaceButton, createButton, removeButton, editPieceButton]()
    {
        actionLabel->setText(tr("Select an object to see the available next steps."));
        selectButton->hide();
        replaceButton->hide();
        createButton->hide();
        removeButton->hide();
        editPieceButton->hide();
    };

    auto populate = [this, tree, recursive, label, objectName, clearActions]()
    {
        tree->clear();
        clearActions();
        const QVector<VToolDependency> dependencies = recursive->isChecked()
                ? doc->getDependentObjectsRecursive(m_id, getData())
                : doc->getDirectDependencies(m_id, getData());
        label->setText(dependencies.isEmpty()
                           ? tr("No dependent objects were found for %1.").arg(objectName)
                           : tr("%1 cannot be deleted because these objects depend on it.").arg(objectName));
        for (const VToolDependency &dependency : dependencies)
        {
            const QString indentation(qMax(0, dependency.depth - 1) * 3, QLatin1Char(' '));
            QString suggestion = tr("Select the object and replace %1 in Properties").arg(dependency.reference);
            if (dependency.id == NULL_ID)
            {
                suggestion = tr("Edit the formula");
            }
            else if (dependency.type == Tool::Piece)
            {
                suggestion = tr("Edit the pattern piece");
            }
            else if (dependency.type == Tool::NodePoint || dependency.type == Tool::NodeArc ||
                     dependency.type == Tool::NodeElArc || dependency.type == Tool::NodeSpline ||
                     dependency.type == Tool::NodeSplinePath)
            {
                suggestion = tr("Replace or remove the node in the pattern piece");
            }
            auto *item = new QTreeWidgetItem(tree, QStringList() << indentation + dependency.name
                                                                 << dependency.typeName << dependency.reference
                                                                 << dependency.draftBlockName << suggestion);
            item->setData(0, Qt::UserRole, dependency.id);
            item->setData(0, Qt::UserRole + 1, static_cast<int>(dependency.type));
        }
        tree->resizeColumnToContents(0);
        tree->resizeColumnToContents(1);
        tree->resizeColumnToContents(2);
    };

    connect(recursive, &QCheckBox::toggled, &dialog, populate);
    connect(tree, &QTreeWidget::itemClicked, &dialog,
            [this, tree, actionLabel, selectButton, replaceButton, createButton, removeButton, editPieceButton,
             &selectedToolId, &selectedToolType, &selectedNodeIds](QTreeWidgetItem *item)
    {
        selectedToolId = item->data(0, Qt::UserRole).toUInt();
        selectedToolType = static_cast<Tool>(item->data(0, Qt::UserRole + 1).toInt());
        if (tree->selectedItems().isEmpty())
        {
            item->setSelected(true);
        }
        selectedNodeIds.clear();
        bool allNodes = !tree->selectedItems().isEmpty();
        for (QTreeWidgetItem *selectedItem : tree->selectedItems())
        {
            const Tool type = static_cast<Tool>(selectedItem->data(0, Qt::UserRole + 1).toInt());
            const bool isSelectedNode = type == Tool::NodePoint || type == Tool::NodeArc ||
                                        type == Tool::NodeElArc || type == Tool::NodeSpline ||
                                        type == Tool::NodeSplinePath;
            allNodes = allNodes && isSelectedNode;
            if (isSelectedNode)
            {
                selectedNodeIds.append(selectedItem->data(0, Qt::UserRole).toUInt());
            }
        }
        const bool isNode = allNodes;

        selectButton->setVisible(selectedToolId != NULL_ID && !isNode && selectedToolType != Tool::Piece);
        replaceButton->setVisible(isNode);
        createButton->setVisible(isNode);
        removeButton->setVisible(isNode);
        editPieceButton->setVisible(isNode || selectedToolType == Tool::Piece);
        replaceButton->setText(selectedNodeIds.size() > 1 ? tr("Replace selected section...")
                                                         : tr("Select replacement..."));
        removeButton->setText(selectedNodeIds.size() > 1 ? tr("Detach selected section...")
                                                        : tr("Detach from pattern piece..."));
        actionLabel->setText(isNode
                                 ? selectedNodeIds.size() > 1
                                       ? tr("The selected nodes can be detached or replaced together when they form one adjacent section.")
                                       : tr("This node or an adjacent section can be replaced by one or more points and curves. Changes can be undone.")
                                 : selectedToolType == Tool::Piece
                                       ? tr("Open the pattern piece to change the references listed above.")
                                       : selectedToolId == NULL_ID
                                             ? tr("Open the formula editor for this variable and replace the reference there.")
                                             : tr("Open Properties and replace the referenced object."));
        for (int i = 0; i < tree->topLevelItemCount(); ++i)
        {
            const quint32 id = tree->topLevelItem(i)->data(0, Qt::UserRole).toUInt();
            if (id != NULL_ID)
            {
                emit doc->ShowTool(id, tree->topLevelItem(i) == item);
            }
        }
    });

    connect(selectButton, &QPushButton::clicked, &dialog, [&dialog, &selectedAction]()
    {
        selectedAction = DependencyAction::Select;
        dialog.accept();
    });
    connect(replaceEverywhereButton, &QPushButton::clicked, &dialog, [&dialog, &selectedAction]()
    {
        selectedAction = DependencyAction::ReplaceEverywhere;
        dialog.accept();
    });
    connect(createEverywhereButton, &QPushButton::clicked, &dialog, [&dialog, &selectedAction]()
    {
        selectedAction = DependencyAction::CreateEverywhere;
        dialog.accept();
    });
    connect(replaceButton, &QPushButton::clicked, &dialog, [&dialog, &selectedAction]()
    {
        selectedAction = DependencyAction::ReplaceNode;
        dialog.accept();
    });
    connect(createButton, &QPushButton::clicked, &dialog, [&dialog, &selectedAction]()
    {
        selectedAction = DependencyAction::CreateReplacement;
        dialog.accept();
    });
    connect(removeButton, &QPushButton::clicked, &dialog, [&dialog, &selectedAction]()
    {
        selectedAction = DependencyAction::RemoveNode;
        dialog.accept();
    });
    connect(editPieceButton, &QPushButton::clicked, &dialog, [&dialog, &selectedAction]()
    {
        selectedAction = DependencyAction::EditPiece;
        dialog.accept();
    });
    connect(buttons, &QDialogButtonBox::rejected, &dialog, &QDialog::reject);
    layout->addWidget(buttons);

    populate();
    dialog.exec();
    const QVector<VToolDependency> dependencies = doc->getDependentObjectsRecursive(m_id, getData());
    for (const VToolDependency &dependency : dependencies)
    {
        if (dependency.id != NULL_ID)
        {
            emit doc->ShowTool(dependency.id, false);
        }
    }

    if (selectedAction == DependencyAction::ReplaceEverywhere)
    {
        replaceObjectEverywhere();
        QTimer::singleShot(0, this, [this]() { showDependencies(); });
        return;
    }
    if (selectedAction == DependencyAction::CreateEverywhere)
    {
        createReplacementObject();
        return;
    }

    quint32 selectedPathId = NULL_ID;
    auto findPieceTool = [this, selectedToolId, selectedToolType, &selectedPathId]() -> PatternPieceTool *
    {
        quint32 pieceId = selectedToolType == Tool::Piece ? selectedToolId : NULL_ID;
        if (pieceId == NULL_ID)
        {
            const QVector<VToolDependency> dependencies = doc->getDirectDependencies(selectedToolId, getData());
            for (const VToolDependency &dependency : dependencies)
            {
                if (dependency.type == Tool::Piece)
                {
                    pieceId = dependency.id;
                    break;
                }
                if (dependency.type == Tool::InternalPath)
                {
                    selectedPathId = dependency.id;
                    pieceId = getData()->pieceIdOfPath(selectedPathId);
                    break;
                }
            }
        }

        if (pieceId == NULL_ID)
        {
            return nullptr;
        }
        try
        {
            return qobject_cast<PatternPieceTool *>(VAbstractPattern::getTool(pieceId));
        }
        catch (const VExceptionBadId &)
        {
            return nullptr;
        }
    };

    if (selectedAction == DependencyAction::ReplaceNode || selectedAction == DependencyAction::CreateReplacement ||
        selectedAction == DependencyAction::RemoveNode ||
        selectedAction == DependencyAction::EditPiece)
    {
        PatternPieceTool *pieceTool = findPieceTool();
        if (pieceTool == nullptr)
        {
            QMessageBox::information(qApp->getMainWindow(), tr("Dependencies"),
                                     tr("The pattern piece is in another draft block. Open that draft block and try again."));
            return;
        }

        if (selectedAction == DependencyAction::CreateReplacement)
        {
            connect(pieceTool, &PatternPieceTool::replacementGeometrySessionClosed, this, [this, pieceTool]()
            {
                disconnect(pieceTool, &PatternPieceTool::replacementGeometrySessionClosed, this, nullptr);
                QTimer::singleShot(0, this, [this]() { showDependencies(); });
            });
            pieceTool->createReplacementGeometry(selectedToolId, selectedPathId, selectedNodeIds);
            return;
        }

        if (selectedAction == DependencyAction::ReplaceNode || selectedAction == DependencyAction::RemoveNode)
        {
            pieceTool->replacePieceNode(selectedToolId, selectedPathId,
                                        selectedAction == DependencyAction::RemoveNode, QVector<quint32>(),
                                        selectedNodeIds);
            QTimer::singleShot(0, this, [this]() { showDependencies(); });
            return;
        }

        connect(pieceTool, &PatternPieceTool::piecePropertiesClosed, this, [this, pieceTool]()
        {
            disconnect(pieceTool, &PatternPieceTool::piecePropertiesClosed, this, nullptr);
            QTimer::singleShot(0, this, [this]() { showDependencies(); });
        });
        pieceTool->editPieceProperties(selectedToolType == Tool::Piece ? NULL_ID : selectedToolId);
        return;
    }

    if (selectedAction == DependencyAction::Select && selectedToolId != NULL_ID)
    {
        try
        {
            auto *item = dynamic_cast<QGraphicsItem *>(VAbstractPattern::getTool(selectedToolId));
            if (item != nullptr)
            {
                item->setSelected(true);
                qApp->getSceneView()->itemClicked(item);
            }
        }
        catch (const VExceptionBadId &)
        {
            // A dependency from another draft block may not be loaded in the current scene.
        }
    }
}

//---------------------------------------------------------------------------------------------------------------------
bool VAbstractTool::replaceObjectEverywhere(const QVector<quint32> &suggestedObjects, bool detachSuggestedObjects)
{
    QSharedPointer<VGObject> source;
    try
    {
        source = getData()->GetGObject(m_id);
    }
    catch (const VExceptionBadId &)
    {
        return false;
    }

    QDialog dialog(qApp->getMainWindow());
    dialog.setWindowTitle(tr("Replace %1 everywhere").arg(source->name()));
    dialog.resize(720, 500);
    auto *layout = new QVBoxLayout(&dialog);
    auto *description = new QLabel(
        detachSuggestedObjects
            ? tr("Choose the newly created point. If it depends on the old construction, it will first be detached "
                 "at its current position. Every direct object reference listed below will then be changed together.")
            : tr("Choose one compatible object. Every direct object reference listed below will be changed together. "
                 "The old object remains on the draft and becomes independent when no references remain."), &dialog);
    description->setWordWrap(true);
    layout->addWidget(description);

    layout->addWidget(new QLabel(tr("Replacement object"), &dialog));
    auto *objects = new QListWidget(&dialog);
    objects->setObjectName(QStringLiteral("dependencyGlobalReplacementList"));
    QSet<quint32> descendantTools;
    const QVector<VToolDependency> descendants = doc->getDependentObjectsRecursive(source->getIdTool(), getData());
    for (const VToolDependency &dependency : descendants)
    {
        if (dependency.id != NULL_ID)
        {
            descendantTools.insert(dependency.id);
        }
    }
    const auto geometry = getData()->DataGObjects();
    for (auto object = geometry->constBegin(); object != geometry->constEnd(); ++object)
    {
        const QSharedPointer<VGObject> candidate = object.value();
        const bool descendant = descendantTools.contains(candidate->getIdTool());
        const bool canDetach = detachSuggestedObjects && suggestedObjects.contains(object.key()) &&
                               source->getType() == GOType::Point;
        if (candidate->getMode() != Draw::Calculation || candidate->getIdObject() != NULL_ID ||
            candidate->getIdTool() == source->getIdTool() || candidate->getType() != source->getType() ||
            (descendant && !canDetach))
        {
            continue;
        }
        auto *item = new QListWidgetItem(canDetach
                                             ? tr("%1 (detach at current position)").arg(candidate->name())
                                             : candidate->name(), objects);
        item->setData(Qt::UserRole, object.key());
        item->setData(Qt::UserRole + 1, canDetach);
        if (suggestedObjects.contains(object.key()))
        {
            objects->setCurrentItem(item);
        }
    }
    objects->sortItems();
    layout->addWidget(objects);

    layout->addWidget(new QLabel(tr("References that will be updated"), &dialog));
    auto *affectedTree = new QTreeWidget(&dialog);
    affectedTree->setColumnCount(3);
    affectedTree->setHeaderLabels(QStringList() << tr("Object") << tr("Type") << tr("Dependency"));
    affectedTree->setRootIsDecorated(false);
    bool hasFormulaReference = false;
    const QVector<VToolDependency> dependencies = doc->getDirectDependencies(source->getIdTool(), getData());
    for (const VToolDependency &dependency : dependencies)
    {
        new QTreeWidgetItem(affectedTree, QStringList() << dependency.name << dependency.typeName
                                                        << dependency.reference);
        hasFormulaReference = hasFormulaReference ||
                              dependency.reference.contains(tr("formula"), Qt::CaseInsensitive);
    }
    affectedTree->resizeColumnToContents(0);
    affectedTree->resizeColumnToContents(1);
    layout->addWidget(affectedTree);

    auto *status = new QLabel(&dialog);
    status->setWordWrap(true);
    if (hasFormulaReference)
    {
        status->setText(tr("Formula references are shown for review but cannot be renamed safely. They will continue "
                           "to refer to the old object's variables."));
    }
    else if (objects->count() == 0)
    {
        status->setText(tr("No compatible existing object was found. Close this window and choose 'Create replacement'."));
    }
    layout->addWidget(status);

    QSet<quint32> diagnosticObjects;
    for (quint32 id : suggestedObjects)
    {
        diagnosticObjects.insert(id);
    }
    auto *diagnosticButtons = new QHBoxLayout;
    auto *showDiagnosticsButton = new QPushButton(tr("Refresh diagnostics"), &dialog);
    auto *copyDiagnosticsButton = new QPushButton(tr("Copy diagnostics"), &dialog);
    diagnosticButtons->addWidget(showDiagnosticsButton);
    diagnosticButtons->addWidget(copyDiagnosticsButton);
    diagnosticButtons->addStretch();
    layout->addLayout(diagnosticButtons);
    auto *diagnostics = new QPlainTextEdit(&dialog);
    diagnostics->setObjectName(QStringLiteral("replacementDiagnosticsText"));
    diagnostics->setReadOnly(true);
    diagnostics->setLineWrapMode(QPlainTextEdit::NoWrap);
    diagnostics->setMinimumHeight(180);
    diagnostics->setVisible(objects->count() == 0);
    layout->addWidget(diagnostics);
    const auto refreshDiagnostics = [this, diagnostics, source, diagnosticObjects, detachSuggestedObjects]()
    {
        diagnostics->setPlainText(ReplacementDiagnostics(doc, getData(), source, m_id, diagnosticObjects,
                                                         detachSuggestedObjects, false));
    };
    if (diagnostics->isVisible())
    {
        refreshDiagnostics();
    }
    connect(showDiagnosticsButton, &QPushButton::clicked, &dialog,
            [diagnostics, refreshDiagnostics]()
    {
        refreshDiagnostics();
        diagnostics->show();
    });
    connect(copyDiagnosticsButton, &QPushButton::clicked, &dialog,
            [diagnostics, refreshDiagnostics]()
    {
        refreshDiagnostics();
        QApplication::clipboard()->setText(diagnostics->toPlainText());
        diagnostics->setVisible(true);
    });

    auto *buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, &dialog);
    QPushButton *okButton = buttons->button(QDialogButtonBox::Ok);
    okButton->setText(objects->currentItem() != nullptr &&
                              objects->currentItem()->data(Qt::UserRole + 1).toBool()
                          ? tr("Detach and replace everywhere")
                          : tr("Replace everywhere"));
    okButton->setEnabled(objects->currentItem() != nullptr);
    layout->addWidget(buttons);
    connect(objects, &QListWidget::currentItemChanged, &dialog,
            [this, okButton](QListWidgetItem *current, QListWidgetItem *previous)
    {
        if (previous != nullptr)
        {
            emit doc->ShowTool(previous->data(Qt::UserRole).toUInt(), false);
        }
        okButton->setEnabled(current != nullptr);
        okButton->setText(current != nullptr && current->data(Qt::UserRole + 1).toBool()
                              ? tr("Detach and replace everywhere")
                              : tr("Replace everywhere"));
        if (current != nullptr)
        {
            emit doc->ShowTool(current->data(Qt::UserRole).toUInt(), true);
        }
    });
    connect(buttons, &QDialogButtonBox::accepted, &dialog, &QDialog::accept);
    connect(buttons, &QDialogButtonBox::rejected, &dialog, &QDialog::reject);

    const int dialogResult = dialog.exec();
    const quint32 highlightedId = objects->currentItem() == nullptr
                                      ? NULL_ID
                                      : objects->currentItem()->data(Qt::UserRole).toUInt();
    if (highlightedId != NULL_ID)
    {
        emit doc->ShowTool(highlightedId, false);
    }
    if (dialogResult != QDialog::Accepted || objects->currentItem() == nullptr)
    {
        return false;
    }
    const quint32 replacementId = objects->currentItem()->data(Qt::UserRole).toUInt();
    QDomElement detachedReplacement;
    if (objects->currentItem()->data(Qt::UserRole + 1).toBool())
    {
        const QSharedPointer<VPointF> point =
            qSharedPointerDynamicCast<VPointF>(getData()->GetGObject(replacementId));
        if (point.isNull())
        {
            QMessageBox::information(qApp->getMainWindow(), tr("Replace object everywhere"),
                                     tr("This point cannot be detached safely because no independent draft base "
                                        "point is available."));
            return false;
        }
        const QDomElement oldXml = doc->elementById(point->getIdTool());
        const QDomElement calculation = oldXml.parentNode().toElement();
        QDomElement basePointXml = calculation.firstChildElement(VAbstractPattern::TagPoint);
        while (!basePointXml.isNull() && basePointXml.attribute(AttrType) != QStringLiteral("single"))
        {
            basePointXml = basePointXml.nextSiblingElement(VAbstractPattern::TagPoint);
        }
        const quint32 basePointId = basePointXml.attribute(QStringLiteral("id")).toUInt();
        if (oldXml.isNull() || basePointId == NULL_ID || basePointId == source->getIdTool())
        {
            QMessageBox::information(qApp->getMainWindow(), tr("Replace object everywhere"),
                                     tr("This point cannot be detached safely because no independent draft base "
                                        "point is available."));
            return false;
        }

        try
        {
            const QSharedPointer<VPointF> basePoint = getData()->GeometricObject<VPointF>(basePointId);
            const QLineF offset(static_cast<QPointF>(*basePoint), static_cast<QPointF>(*point));
            // An end-line point preserves the position while depending only on the draft block's root point.
            detachedReplacement = doc->createElement(VAbstractPattern::TagPoint);
            doc->SetAttribute(detachedReplacement, QStringLiteral("id"), point->getIdTool());
            doc->SetAttribute(detachedReplacement, AttrType, QStringLiteral("endLine"));
            doc->SetAttribute(detachedReplacement, AttrName, point->name());
            doc->SetAttribute(detachedReplacement, AttrMx, qApp->fromPixel(point->mx()));
            doc->SetAttribute(detachedReplacement, AttrMy, qApp->fromPixel(point->my()));
            doc->SetAttribute<bool>(detachedReplacement, AttrShowPointName, point->isShowPointName());
            doc->SetAttribute(detachedReplacement, AttrLineType, LineTypeNone);
            doc->SetAttribute(detachedReplacement, AttrLineWeight, DefaultLineWeight);
            doc->SetAttribute(detachedReplacement, AttrLineColor, ColorBlack);
            doc->SetAttribute(detachedReplacement, AttrLength,
                              QString::number(qApp->fromPixel(offset.length()), 'g', 15));
            doc->SetAttribute(detachedReplacement, AttrAngle,
                              QString::number(offset.length() > 0 ? offset.angle() : 0.0, 'g', 15));
            doc->SetAttribute(detachedReplacement, AttrBasePoint, basePointId);
        }
        catch (const VExceptionBadId &)
        {
            QMessageBox::information(qApp->getMainWindow(), tr("Replace object everywhere"),
                                     tr("This point cannot be detached safely because no independent draft base "
                                        "point is available."));
            return false;
        }
    }

    auto *command = new ReplaceObjectReferences(source->getIdTool(), replacementId, doc, getData(),
                                                detachedReplacement);
    if (command->changedToolCount() == 0)
    {
        delete command;
        QMessageBox::information(qApp->getMainWindow(), tr("Replace object everywhere"),
                                 tr("No compatible direct object reference was found."));
        return false;
    }
    connect(command, &ReplaceObjectReferences::NeedFullParsing, doc, &VAbstractPattern::NeedFullParsing);
    qApp->getUndoStack()->push(command);
    return true;
}

//---------------------------------------------------------------------------------------------------------------------
void VAbstractTool::createReplacementObject()
{
    QSharedPointer<VGObject> source;
    try
    {
        source = getData()->GetGObject(m_id);
    }
    catch (const VExceptionBadId &)
    {
        return;
    }

    const int initialUndoIndex = qApp->getUndoStack()->index();
    QSet<quint32> existingObjects;
    const auto objects = getData()->DataGObjects();
    for (auto object = objects->constBegin(); object != objects->constEnd(); ++object)
    {
        existingObjects.insert(object.key());
    }

    auto *guide = new QDialog(qApp->getMainWindow(), Qt::Tool);
    guide->setAttribute(Qt::WA_DeleteOnClose);
    guide->setWindowTitle(tr("Create replacement for %1").arg(source->name()));
    guide->resize(540, 230);
    auto *layout = new QVBoxLayout(guide);
    auto *description = new QLabel(
        tr("Create a compatible replacement with the normal drawing tools. Finish the active tool, then choose "
           "'Select new object'. The replacement must not use %1 itself, directly or indirectly. The affected "
           "references are shown once more before anything changes.").arg(source->name()), guide);
    description->setWordWrap(true);
    layout->addWidget(description);

    auto *toolLayout = new QHBoxLayout;
    QList<QPair<QString, QString>> tools;
    switch (source->getType())
    {
        case GOType::Point:
            tools.append(qMakePair(tr("Point by length and angle"),
                                   QStringLiteral("pointAtDistanceAngle_ToolButton")));
            tools.append(qMakePair(tr("Point on line"), QStringLiteral("alongLine_ToolButton")));
            tools.append(qMakePair(tr("Midpoint on line"), QStringLiteral("midpoint_ToolButton")));
            tools.append(qMakePair(tr("Perpendicular point"), QStringLiteral("normal_ToolButton")));
            tools.append(qMakePair(tr("Point on curve"), QStringLiteral("pointAlongCurve_ToolButton")));
            tools.append(qMakePair(tr("Point on spline"), QStringLiteral("pointAlongSpline_ToolButton")));
            tools.append(qMakePair(tr("Point on arc"), QStringLiteral("pointAlongArc_ToolButton")));
            tools.append(qMakePair(tr("Line intersection"), QStringLiteral("lineIntersect_ToolButton")));
            tools.append(qMakePair(tr("Curve intersection"),
                                   QStringLiteral("pointOfIntersectionCurves_ToolButton")));
            break;
        case GOType::Spline:
            tools.append(qMakePair(tr("Curve"), QStringLiteral("curve_ToolButton")));
            break;
        case GOType::SplinePath:
            tools.append(qMakePair(tr("Spline"), QStringLiteral("spline_ToolButton")));
            break;
        case GOType::CubicBezier:
            tools.append(qMakePair(tr("Fixed curve"), QStringLiteral("curveWithCPs_ToolButton")));
            break;
        case GOType::CubicBezierPath:
            tools.append(qMakePair(tr("Fixed spline"), QStringLiteral("splineWithCPs_ToolButton")));
            break;
        case GOType::Arc:
            tools.append(qMakePair(tr("Arc"), QStringLiteral("arc_ToolButton")));
            break;
        case GOType::EllipticalArc:
            tools.append(qMakePair(tr("Elliptical arc"), QStringLiteral("ellipticalArc_ToolButton")));
            break;
        default:
            break;
    }
    if (source->getType() == GOType::Point)
    {
        auto *pointTypes = new QComboBox(guide);
        pointTypes->setObjectName(QStringLiteral("replacementPointTypeCombo"));
        for (const auto &tool : tools)
        {
            pointTypes->addItem(tool.first, tool.second);
        }
        auto *button = new QPushButton(tr("Start point tool"), guide);
        button->setObjectName(QStringLiteral("replacementStartPointToolButton"));
        toolLayout->addWidget(pointTypes);
        toolLayout->addWidget(button);
        connect(button, &QPushButton::clicked, guide, [guide, pointTypes]()
        {
            const QString objectName = pointTypes->currentData().toString();
            if (auto *toolButton = qApp->getMainWindow()->findChild<QToolButton *>(objectName))
            {
                toolButton->click();
                guide->raise();
            }
        });
    }
    else
    {
        for (const auto &tool : tools)
        {
            auto *button = new QPushButton(tool.first, guide);
            toolLayout->addWidget(button);
            connect(button, &QPushButton::clicked, guide, [guide, tool]()
            {
                if (auto *toolButton = qApp->getMainWindow()->findChild<QToolButton *>(tool.second))
                {
                    toolButton->click();
                    guide->raise();
                }
            });
        }
    }
    layout->addLayout(toolLayout);

    auto *status = new QLabel(tr("Only newly created geometry of the same type as the old object can be selected."),
                              guide);
    status->setWordWrap(true);
    layout->addWidget(status);

    auto *diagnosticButtons = new QHBoxLayout;
    auto *showDiagnosticsButton = new QPushButton(tr("Refresh diagnostics"), guide);
    auto *copyDiagnosticsButton = new QPushButton(tr("Copy diagnostics"), guide);
    diagnosticButtons->addWidget(showDiagnosticsButton);
    diagnosticButtons->addWidget(copyDiagnosticsButton);
    diagnosticButtons->addStretch();
    layout->addLayout(diagnosticButtons);
    auto *diagnostics = new QPlainTextEdit(guide);
    diagnostics->setObjectName(QStringLiteral("replacementDiagnosticsText"));
    diagnostics->setReadOnly(true);
    diagnostics->setLineWrapMode(QPlainTextEdit::NoWrap);
    diagnostics->setMinimumHeight(180);
    diagnostics->hide();
    layout->addWidget(diagnostics);
    const auto refreshDiagnostics = [this, diagnostics, existingObjects, source]()
    {
        QSet<quint32> newObjects;
        const auto currentObjects = getData()->DataGObjects();
        for (auto object = currentObjects->constBegin(); object != currentObjects->constEnd(); ++object)
        {
            if (!existingObjects.contains(object.key()))
            {
                newObjects.insert(object.key());
            }
        }
        diagnostics->setPlainText(ReplacementDiagnostics(doc, getData(), source, m_id, newObjects, true, true));
    };
    connect(showDiagnosticsButton, &QPushButton::clicked, guide,
            [diagnostics, refreshDiagnostics]()
    {
        refreshDiagnostics();
        diagnostics->show();
    });
    connect(copyDiagnosticsButton, &QPushButton::clicked, guide,
            [diagnostics, refreshDiagnostics]()
    {
        refreshDiagnostics();
        QApplication::clipboard()->setText(diagnostics->toPlainText());
        diagnostics->show();
    });

    auto *buttons = new QDialogButtonBox(guide);
    auto *finishButton = buttons->addButton(tr("Select new object"), QDialogButtonBox::AcceptRole);
    auto *keepButton = buttons->addButton(tr("Keep geometry and close"), QDialogButtonBox::RejectRole);
    auto *discardButton = buttons->addButton(tr("Discard new geometry"), QDialogButtonBox::DestructiveRole);
    layout->addWidget(buttons);
    connect(keepButton, &QPushButton::clicked, guide, &QDialog::reject);
    connect(discardButton, &QPushButton::clicked, guide, [guide]()
    {
        guide->setProperty("discardCreatedGeometry", true);
        guide->reject();
    });
    connect(finishButton, &QPushButton::clicked, guide,
            [this, guide, status, diagnostics, refreshDiagnostics, existingObjects, source]()
    {
        QVector<quint32> createdObjects;
        QVector<quint32> dependentCreatedObjects;
        QStringList dependentCreatedObjectNames;
        QSet<quint32> descendantTools;
        const QVector<VToolDependency> descendants =
            doc->getDependentObjectsRecursive(source->getIdTool(), getData());
        for (const VToolDependency &dependency : descendants)
        {
            if (dependency.id != NULL_ID)
            {
                descendantTools.insert(dependency.id);
            }
        }
        const auto currentObjects = getData()->DataGObjects();
        for (auto object = currentObjects->constBegin(); object != currentObjects->constEnd(); ++object)
        {
            const QSharedPointer<VGObject> candidate = object.value();
            const bool newCompatibleType = !existingObjects.contains(object.key()) &&
                                           candidate->getMode() == Draw::Calculation &&
                                           candidate->getIdObject() == NULL_ID &&
                                           candidate->getType() == source->getType();
            if (newCompatibleType && descendantTools.contains(candidate->getIdTool()))
            {
                dependentCreatedObjects.append(object.key());
                dependentCreatedObjectNames.append(candidate->name());
            }
            else if (newCompatibleType)
            {
                createdObjects.append(object.key());
            }
        }
        if (createdObjects.isEmpty())
        {
            if (!dependentCreatedObjects.isEmpty() && source->getType() == GOType::Point)
            {
                guide->setProperty("replacementSelectionStarted", true);
                guide->accept();
                replaceObjectEverywhere(dependentCreatedObjects, true);
                QTimer::singleShot(0, this, [this]() { showDependencies(true); });
                return;
            }
            status->setText(dependentCreatedObjects.isEmpty()
                                ? tr("No new object of the required type was found. Finish the active drawing tool "
                                     "and try again.")
                                : tr("%1 cannot replace %2 because it depends on %2. It will be shown under 'Show "
                                     "all descendants'. Create the replacement from a different point or "
                                     "construction chain.")
                                      .arg(dependentCreatedObjectNames.join(QStringLiteral(", ")), source->name()));
            refreshDiagnostics();
            diagnostics->show();
            return;
        }

        guide->setProperty("replacementSelectionStarted", true);
        guide->accept();
        replaceObjectEverywhere(createdObjects);
        QTimer::singleShot(0, this, [this]() { showDependencies(); });
    });
    connect(guide, &QDialog::finished, this, [this, guide, initialUndoIndex](int)
    {
        const bool discardCreatedGeometry = guide->property("discardCreatedGeometry").toBool();
        if (discardCreatedGeometry)
        {
            while (qApp->getUndoStack()->index() > initialUndoIndex)
            {
                qApp->getUndoStack()->undo();
            }
        }
        if (!guide->property("replacementSelectionStarted").toBool())
        {
            QTimer::singleShot(0, this, [this, discardCreatedGeometry]()
            {
                showDependencies(!discardCreatedGeometry);
            });
        }
    });
    guide->show();
}

//---------------------------------------------------------------------------------------------------------------------
int VAbstractTool::ConfirmDeletion()
{
    if (false == qApp->Settings()->getConfirmItemDelete())
    {
        return QMessageBox::Yes;
    }

    Utils::CheckableMessageBox msgBox(qApp->getMainWindow());
    msgBox.setWindowTitle(tr("Confirm deletion"));
    msgBox.setText(tr("Do you really want to delete?"));
    msgBox.setStandardButtons(QDialogButtonBox::Yes | QDialogButtonBox::No);
    msgBox.setDefaultButton(QDialogButtonBox::No);
    msgBox.setIconPixmap(QApplication::style()->standardIcon(QStyle::SP_MessageBoxQuestion).pixmap(32, 32) );

    int dialogResult = msgBox.exec();

    if (dialogResult == QDialog::Accepted)
    {
        qApp->Settings()->setConfirmItemDelete(not msgBox.isChecked());
    }

    return dialogResult == QDialog::Accepted ? QMessageBox::Yes : QMessageBox::No;
}

//---------------------------------------------------------------------------------------------------------------------
const QStringList VAbstractTool::Colors()
{
    const QStringList colors = QStringList() << ColorBlack          << ColorGreen           << ColorBlue
                                             << ColorDarkRed        << ColorDarkGreen       << ColorDarkBlue
                                             << ColorYellow         << ColorLightSalmon     << ColorGoldenRod
                                             << ColorOrange         << ColorDeepPink        << ColorViolet
                                             << ColorDarkViolet     << ColorMediumSeaGreen  << ColorLime
                                             << ColorDeepSkyBlue    << ColorCornFlowerBlue;
    return colors;
}

//---------------------------------------------------------------------------------------------------------------------
QMap<QString, QString> VAbstractTool::ColorsList()
{
    QMap<QString, QString> map;

    const QStringList colorNames = Colors();
    for (int i = 0; i < colorNames.size(); ++i)
    {
        QString name;
        switch (i)
        {
            case 1: // ColorGreen
                name = tr("Green");
                break;
            case 2: // ColorBlue
                name = tr("Blue");
                break;
            case 3: // ColorDarkRed
                name = tr("Dark Red");
                break;
            case 4: // ColorDarkGreen
                name = tr("Dark Green");
                break;
            case 5: // ColorDarkBlue
                name = tr("Dark Blue");
                break;
            case 6: // ColorYellow
                name = tr("Yellow");
                break;
            case 7: // ColorLightSalmon
                name = tr("Light Salmon");
                break;
            case 8: // ColorGoldenRod
                name = tr("Goldenrod");
                break;
            case 9: // ColorOrange
                name = tr("Orange");
                break;
            case 10: // ColorDeepPink
                name = tr("Deep Pink");
                break;
            case 11: // ColorViolet
                name = tr("Violet");
                break;
            case 12: // ColorDarkViolet
                name = tr("Dark Violet");
                break;
            case 13: // ColorMediumSeaGreen
                name = tr("Medium Sea Green");
                break;
            case 14: // ColorLime
                name = tr("Lime");
                break;
            case 15: // ColorDeepSkyBlue
                name = tr("Deep Sky Blue");
                break;
            case 16: // ColorCornFlowerBlue
                name = tr("Corn Flower Blue");
                break;
            case 0: // ColorBlack
            default:
                name = tr("Black");
                break;
        }

        map.insert(colorNames.at(i), name);
    }
    return map;
}

//---------------------------------------------------------------------------------------------------------------------
QMap<QString, QString> VAbstractTool::supportColorsList()
{
    QMap<QString, QString> map;
    map.insert("gold", tr("Gold"));
    map.insert("forestgreen", tr("Forest Green"));
    map.insert("lawngreen", tr("Lawn Green"));
    map.insert("limegreen", tr("Lime Green"));
    map.insert("greenyellow", tr("Green Yellow"));
    map.insert("sandybrown", tr("Sandy Brown"));
    map.insert("orangered", tr("Orange Red"));
    map.insert("maroon", tr("Maroon"));
    map.insert("pink", tr("Pink"));
    map.insert("hotpink", tr("Hot Pink"));
    map.insert("blueviolet", tr("Blue Violet"));
    map.insert("mediumvioletred", tr("Medium Violet Red"));
    map.insert("indigo", tr("Indigo"));
    map.insert("purple", tr("Purple"));
    map.insert("plum", tr("Plum"));
    map.insert("turquoise", tr("Turquoise"));
    map.insert("mediumturquoise", tr("Medium Turquoise"));
    map.insert("powderblue", tr("Powder Blue"));
    map.insert("lightskyblue", tr("Light Sky Blue"));
    map.insert("navy", tr("Navy"));
    map.insert("magenta", tr("Magenta"));
    return map;
}

//---------------------------------------------------------------------------------------------------------------------
QMap<QString, QString> VAbstractTool::backgroundColorsList()
{
    QMap<QString, QString> map;

    map.insert("darkslategrey", tr("Dark Slate Grey"));
    map.insert("grey", tr("Grey"));
    map.insert("gainsboro", tr("Gainsboro"));
    map.insert("darkseagreen", tr("Dark Sea Green"));
    map.insert("lightgrey", tr("Light Grey"));
    map.insert("darkslategrey", tr("Dark Slate Grey"));
    map.insert("lightsteelblue", tr("Light Steel Blue"));
    map.insert("beige", tr("Beige"));
    map.insert("thistle", tr("Thistle"));
    map.insert("silver", tr("Silver"));
    map.insert("whitesmoke", tr("White Smoke"));
    map.insert("white", tr("White"));
    map.insert("darkgrey", tr("Dark Grey"));
    map.insert("cadetblue", tr("Cadet Blue"));
    map.insert("darkkhaki", tr("Dark Khaki"));
    map.insert("tan", tr("Tan"));

    return map;
}

QPixmap VAbstractTool::createColorIcon(const int w, const int h, const QString &color)
{
    QPixmap pixmap(w, h);
    pixmap.fill(QColor(Qt::black));

    QPainter painter(&pixmap);
    painter.setPen(Qt::black);

    const QRect rectangle = QRect(1, 1, w-2, h-2);

    qDebug() << "createColorIcon - color = " << color;

    if (color == "No Group")
    {
        painter.fillRect(rectangle, QColor(Qt::white));
        painter.drawLine(0, 0, w, h);
        painter.drawLine(0, h, w, 0);
    }
    else if (color == "By Group")
    {
        QFont font = painter.font();
        font.setPixelSize(10);
        painter.setFont(font);
        painter.fillRect(rectangle, QColor(Qt::white));
        painter.drawText(rectangle, Qt::AlignCenter, "GROUP");
    }
    else
    {
        painter.fillRect(rectangle, QColor(color));
    }

    return pixmap;
}

//---------------------------------------------------------------------------------------------------------------------
const QStringList VAbstractTool::fills()
{
    const QStringList fills = QStringList() << FillNone              << FillSolid            << FillDense1
                                            << FillDense2            << FillDense3           << FillDense4
                                            << FillDense5            << FillDense6           << FillDense7
                                            << FillHorizLines        << FillVertLines        << FillCross
                                            << FillBackwardDiagonal  << FillForwardDiagonal  << FilldDiagonalCross;
    return fills;
}

//---------------------------------------------------------------------------------------------------------------------
// cppcheck-suppress unusedFunction
QMap<QString, quint32> VAbstractTool::PointsList() const
{
    const QHash<quint32, QSharedPointer<VGObject> > *objs = data.DataGObjects();
    QMap<QString, quint32> list;
    QHash<quint32, QSharedPointer<VGObject> >::const_iterator i;
    for (i = objs->constBegin(); i != objs->constEnd(); ++i)
    {
        if (i.key() != m_id)
        {
            QSharedPointer<VGObject> obj = i.value();
            if (obj->getType() == GOType::Point && obj->getMode() == Draw::Calculation)
            {
                const QSharedPointer<VPointF> point = data.GeometricObject<VPointF>(i.key());
                list[point->name()] = i.key();
            }
        }
    }
    return list;
}

//---------------------------------------------------------------------------------------------------------------------
void VAbstractTool::setPointNamePosition(quint32 id, const QPointF &pos)
{
    Q_UNUSED(id)
    Q_UNUSED(pos)
}

//---------------------------------------------------------------------------------------------------------------------
void VAbstractTool::setPointNameVisiblity(quint32 id, bool visible)
{
    Q_UNUSED(id)
    Q_UNUSED(visible)
}

//---------------------------------------------------------------------------------------------------------------------
void VAbstractTool::ToolSelectionType(const SelectionType &type)
{
    selectionType = type;
}

//---------------------------------------------------------------------------------------------------------------------
void VAbstractTool::RefreshDataInFile()
{
    // do nothing
}

//---------------------------------------------------------------------------------------------------------------------
void VAbstractTool::ToolCreation(const Source &typeCreation)
{
    if (typeCreation == Source::FromGui)
    {
        AddToFile();
    }
    else
    {
        RefreshDataInFile();
    }
}

//---------------------------------------------------------------------------------------------------------------------
/**
 * @brief AddRecord add record about tool in history.
 * @param id object id in container
 * @param toolType tool type
 * @param doc dom document container
 */
void VAbstractTool::AddRecord(const quint32 id, const Tool &toolType, VAbstractPattern *doc)
{
    QVector<VToolRecord> *history = doc->getHistory();
    VToolRecord record = VToolRecord(id, toolType, doc->getActiveDraftBlockName());
    if (history->contains(record))
    {
        return;
    }

    quint32 cursor = doc->getCursorId();
    if (cursor == NULL_ID)
    {
        history->append(record);
    }
    else
    {
        qint32 index = 0;
        for (qint32 i = 0; i<history->size(); ++i)
        {
            VToolRecord rec = history->at(i);
            if (rec.getId() == cursor)
            {
                index = i;
                break;
            }
        }
        history->insert(index+1, record);
    }
}

//---------------------------------------------------------------------------------------------------------------------
void VAbstractTool::addNodes(VAbstractPattern *doc, QDomElement &domElement, const VPiecePath &path)
{
    if (path.nodeCount() > 0)
    {
        QDomElement nodesElement = doc->createElement(VAbstractPattern::TagNodes);
        for (int i = 0; i < path.nodeCount(); ++i)
        {
            AddNode(doc, nodesElement, path.at(i));
        }
        domElement.appendChild(nodesElement);
    }
}

//---------------------------------------------------------------------------------------------------------------------
void VAbstractTool::addNodes(VAbstractPattern *doc, QDomElement &domElement, const VPiece &piece)
{
    addNodes(doc, domElement, piece.GetPath());
}

//---------------------------------------------------------------------------------------------------------------------
QDomElement VAbstractTool::AddSANode(VAbstractPattern *doc, const QString &tagName, const VPieceNode &node)
{
    QDomElement nod = doc->createElement(tagName);

    doc->SetAttribute(nod, AttrIdObject, node.GetId());

    const Tool type = node.GetTypeTool();
    if (type != Tool::NodePoint)
    {
        doc->SetAttribute(nod, VAbstractPattern::AttrNodeReverse, static_cast<quint8>(node.GetReverse()));
    }
    else
    {
        if (node.GetFormulaSABefore() != currentSeamAllowance)
        {
            doc->SetAttribute(nod, VAbstractPattern::AttrSABefore, node.GetFormulaSABefore());
        }

        if (node.GetFormulaSAAfter() != currentSeamAllowance)
        {
            doc->SetAttribute(nod, VAbstractPattern::AttrSAAfter, node.GetFormulaSAAfter());
        }
    }

    {
        const bool excluded = node.isExcluded();
        if (excluded)
        {
            doc->SetAttribute(nod, VAbstractPattern::AttrNodeExcluded, excluded);
        }
        else
        { // For backward compatebility.
            nod.removeAttribute(VAbstractPattern::AttrNodeExcluded);
        }
    }

    switch (type)
    {
        case (Tool::NodeArc):
            doc->SetAttribute(nod, AttrType, VAbstractPattern::NodeArc);
            break;
        case (Tool::NodeElArc):
            doc->SetAttribute(nod, AttrType, VAbstractPattern::NodeElArc);
            break;
        case (Tool::NodePoint):
            doc->SetAttribute(nod, AttrType, VAbstractPattern::NodePoint);
            break;
        case (Tool::NodeSpline):
            doc->SetAttribute(nod, AttrType, VAbstractPattern::NodeSpline);
            break;
        case (Tool::NodeSplinePath):
            doc->SetAttribute(nod, AttrType, VAbstractPattern::NodeSplinePath);
            break;
        default:
            qWarning() << "May be wrong tool type!!! Ignoring."<<Q_FUNC_INFO;
            break;
    }

    {
        const unsigned char angleType = static_cast<unsigned char>(node.GetAngleType());

        if (angleType > 0)
        {
            doc->SetAttribute(nod, AttrAngle, angleType);
        }
    }

    if (type == Tool::NodePoint)
    {
        if (node.isNotch())
        {
            doc->SetAttribute(nod, VAbstractPattern::AttrNodeIsNotch,         node.isNotch());
            doc->SetAttribute(nod, VAbstractPattern::AttrNodeShowNotch,       node.showNotch());
            doc->SetAttribute(nod, VAbstractPattern::AttrNodeShowSecondNotch, node.showSeamlineNotch());
            doc->SetAttribute(nod, VAbstractPattern::AttrNodeNotchType,    notchTypeToString(node.getNotchType()));
            doc->SetAttribute(nod, VAbstractPattern::AttrNodeNotchSubType, notchSubTypeToString(node.getNotchSubType()));
            doc->SetAttribute(nod, VAbstractPattern::AttrNodeNotchLength,     node.getNotchLength());
            doc->SetAttribute(nod, VAbstractPattern::AttrNodeNotchWidth,      node.getNotchWidth());
            doc->SetAttribute(nod, VAbstractPattern::AttrNodeNotchAngle,      node.getNotchAngle());
            doc->SetAttribute(nod, VAbstractPattern::AttrNodeNotchCount,      node.getNotchCount());
        }

        if (not node.isNotch() && node.getNotchType() == NotchType::Slit
            && node.getNotchSubType() == NotchSubType::Straightforward)
        { // For backward compatebility.
            nod.removeAttribute(VAbstractPattern::AttrNodeIsNotch);
            nod.removeAttribute(VAbstractPattern::AttrNodeNotchType);
            nod.removeAttribute(VAbstractPattern::AttrNodeNotchSubType);
        }
    }
    else
    { // Wrong configuration.
        nod.removeAttribute(VAbstractPattern::AttrNodeIsNotch);
        nod.removeAttribute(VAbstractPattern::AttrNodeNotchType);
        nod.removeAttribute(VAbstractPattern::AttrNodeNotchSubType);
    }

    return nod;
}

//---------------------------------------------------------------------------------------------------------------------
void VAbstractTool::AddNode(VAbstractPattern *doc, QDomElement &domElement, const VPieceNode &node)
{
    domElement.appendChild(AddSANode(doc, VAbstractPattern::TagNode, node));
}

//---------------------------------------------------------------------------------------------------------------------
QVector<VPieceNode> VAbstractTool::PrepareNodes(const VPiecePath &path, VMainGraphicsScene *scene,
                                                VAbstractPattern *doc, VContainer *data)
{
    QVector<VPieceNode> nodes;
    for (int i = 0; i< path.nodeCount(); ++i)
    {
        VPieceNode node = path.at(i);
        const quint32 id = PrepareNode(node, scene, doc, data);
        if (id > NULL_ID)
        {
            node.SetId(id);
            nodes.append(node);
        }
    }
    return nodes;
}

//---------------------------------------------------------------------------------------------------------------------
quint32 VAbstractTool::PrepareNode(const VPieceNode &node, VMainGraphicsScene *scene,
                                   VAbstractPattern *doc, VContainer *data)
{
    SCASSERT(scene != nullptr)
    SCASSERT(doc != nullptr)
    SCASSERT(data != nullptr)

    quint32 id = NULL_ID;
    switch (node.GetTypeTool())
    {
        case (Tool::NodePoint):
            id = CreateNode<VPointF>(data, node.GetId());
            VNodePoint::Create(doc, data, scene, id, node.GetId(), Document::FullParse, Source::FromGui);
            break;
        case (Tool::NodeArc):
            id = CreateNode<VArc>(data, node.GetId());
            VNodeArc::Create(doc, data, id, node.GetId(), Document::FullParse, Source::FromGui);
            break;
        case (Tool::NodeElArc):
            id = CreateNode<VEllipticalArc>(data, node.GetId());
            VNodeEllipticalArc::Create(doc, data, id, node.GetId(), Document::FullParse, Source::FromGui);
            break;
        case (Tool::NodeSpline):
            id = CreateNodeSpline(data, node.GetId());
            VNodeSpline::Create(doc, data, id, node.GetId(), Document::FullParse, Source::FromGui);
            break;
        case (Tool::NodeSplinePath):
            id = CreateNodeSplinePath(data, node.GetId());
            VNodeSplinePath::Create(doc, data, id, node.GetId(), Document::FullParse, Source::FromGui);
            break;
        default:
            qWarning() << "May be wrong tool type!!! Ignoring."<<Q_FUNC_INFO;
            break;
    }
    return id;
}
