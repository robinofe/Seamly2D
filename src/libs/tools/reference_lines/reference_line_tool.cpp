//---------------------------------------------------------------------------------------------------------------------
//  @file   reference_line_tool.cpp
//  @brief  Non-constructive length reference shown in the draft scene.
//---------------------------------------------------------------------------------------------------------------------

#include "reference_line_tool.h"

#include <QComboBox>
#include <QDialogButtonBox>
#include <QDoubleSpinBox>
#include <QFormLayout>
#include <QGraphicsSceneContextMenuEvent>
#include <QGraphicsSceneMouseEvent>
#include <QLineEdit>
#include <QLineF>
#include <QMenu>
#include <QMessageBox>
#include <QPainter>
#include <QPushButton>
#include <QToolButton>
#include <QHBoxLayout>
#include <stdexcept>

#include "../../ifc/xml/vabstractpattern.h"
#include "../../vpatterndb/calculator.h"
#include "../../vpatterndb/vcontainer.h"
#include "../../vgeometry/vpointf.h"
#include "../../vtools/dialogs/support/edit_formula_dialog.h"
#include "../../vtools/undocommands/addreferenceline.h"
#include "../../vtools/undocommands/deltool.h"
#include "../../vtools/undocommands/savetooloptions.h"
#include "../../ifc/xml/vtoolrecord.h"
#include "../../vwidgets/vmaingraphicsscene.h"
#include "../../vmisc/vabstractapplication.h"
#include "../../vmisc/vcommonsettings.h"

namespace
{
constexpr int referenceLineGraphicsType = QGraphicsItem::UserType + 1000;

class ReferenceLineDialog final : public QDialog
{
    Q_OBJECT

public:
    ReferenceLineDialog(const ReferenceLineData &referenceLine, VAbstractPattern *doc, const VContainer *data,
                        QWidget *parent = nullptr)
        : QDialog(parent)
        , m_doc(doc)
        , m_data(data)
        , m_name(new QLineEdit(referenceLine.name, this))
        , m_formula(new QLineEdit(this))
        , m_anchor(new QComboBox(this))
        , m_anchorPosition(new QDoubleSpinBox(this))
        , m_orientation(new QComboBox(this))
        , m_angle(new QDoubleSpinBox(this))
    {
        setWindowTitle(tr("Reference length"));
        setWindowFlags(windowFlags() & ~Qt::WindowContextHelpButtonHint);

        m_formula->setText(qApp->translateVariables()->FormulaToUser(
            referenceLine.formula, qApp->Settings()->getOsSeparator()));
        auto *formulaRow = new QHBoxLayout;
        formulaRow->addWidget(m_formula);
        auto *formulaButton = new QToolButton(this);
        formulaButton->setText(QStringLiteral("fx"));
        formulaButton->setToolTip(tr("Open formula editor"));
        formulaRow->addWidget(formulaButton);

        m_anchor->addItem(tr("No anchor (freely movable)"), NULL_ID);
        const auto objects = data->DataGObjects();
        QMap<QString, quint32> points;
        for (auto i = objects->constBegin(); i != objects->constEnd(); ++i)
        {
            if (i.value()->getType() == GOType::Point && i.value()->getMode() == Draw::Calculation &&
                i.value()->getIdObject() == NULL_ID)
            {
                points.insert(i.value()->name(), i.key());
            }
        }
        for (auto i = points.constBegin(); i != points.constEnd(); ++i)
        {
            m_anchor->addItem(tr("Point: %1").arg(i.key()), i.value());
        }
        const QVector<VToolRecord> history = *doc->getHistory();
        for (const VToolRecord &record : history)
        {
            if (record.getTypeTool() != Tool::Line)
            {
                continue;
            }
            const QDomElement line = doc->elementById(record.getId(), VAbstractPattern::TagLine);
            try
            {
                const QString first = data->GetGObject(
                    doc->GetParametrUInt(line, AttrFirstPoint, NULL_ID_STR))->name();
                const QString second = data->GetGObject(
                    doc->GetParametrUInt(line, AttrSecondPoint, NULL_ID_STR))->name();
                m_anchor->addItem(tr("Line: %1 - %2").arg(first, second), record.getId());
            }
            catch (...)
            {
            }
        }
        const int anchorIndex = m_anchor->findData(referenceLine.anchorObject);
        m_anchor->setCurrentIndex(anchorIndex >= 0 ? anchorIndex : 0);

        m_anchorPosition->setRange(0.0, 100.0);
        m_anchorPosition->setDecimals(1);
        m_anchorPosition->setSuffix(QStringLiteral(" %"));
        m_anchorPosition->setValue(referenceLine.anchorPosition * 100.0);

        m_orientation->addItem(tr("Free angle"), QStringLiteral("free"));
        m_orientation->addItem(tr("Horizontal"), QStringLiteral("horizontal"));
        m_orientation->addItem(tr("Vertical"), QStringLiteral("vertical"));
        m_orientation->addItem(tr("Along anchored line"), QStringLiteral("anchor"));
        const int orientationIndex = m_orientation->findData(referenceLine.orientation);
        m_orientation->setCurrentIndex(orientationIndex >= 0 ? orientationIndex : 0);

        m_angle->setRange(-360.0, 360.0);
        m_angle->setDecimals(2);
        m_angle->setSuffix(degreeSymbol);
        m_angle->setValue(referenceLine.angle);

        auto *form = new QFormLayout;
        form->addRow(tr("Name:"), m_name);
        form->addRow(tr("Length formula:"), formulaRow);
        form->addRow(tr("Attach to:"), m_anchor);
        form->addRow(tr("Position on line:"), m_anchorPosition);
        form->addRow(tr("Orientation:"), m_orientation);
        form->addRow(tr("Angle:"), m_angle);

        auto *buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);
        form->addRow(buttons);
        setLayout(form);

        connect(formulaButton, &QToolButton::clicked, this, [this]()
        {
            EditFormulaDialog dialog(m_data, NULL_ID, ToolDialog, this);
            dialog.SetFormula(formula());
            dialog.setCheckZero(true);
            dialog.setCheckLessThanZero(true);
            dialog.setPostfix(UnitsToStr(qApp->patternUnit(), true));
            if (dialog.exec() == QDialog::Accepted)
            {
                m_formula->setText(qApp->translateVariables()->FormulaToUser(
                    dialog.GetFormula(), qApp->Settings()->getOsSeparator()));
            }
        });
        connect(buttons, &QDialogButtonBox::accepted, this, [this]()
        {
            try
            {
                Calculator calculator;
                const qreal value = calculator.EvalFormula(m_data->DataVariables(), formula());
                if (!qIsFinite(value) || value <= 0.0)
                {
                    throw std::runtime_error("non-positive length");
                }
                accept();
            }
            catch (...)
            {
                QMessageBox::warning(this, tr("Reference length"),
                                     tr("Enter a valid formula with a result greater than zero."));
            }
        });
        connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);
        auto updateControls = [this]()
        {
            const bool lineAnchor = isLineAnchor(m_anchor->currentData().toUInt());
            m_anchorPosition->setEnabled(lineAnchor);
            const QString orientation = m_orientation->currentData().toString();
            m_angle->setEnabled(orientation == QStringLiteral("free"));
            if (orientation == QStringLiteral("anchor") && !lineAnchor)
            {
                m_orientation->setCurrentIndex(0);
            }
        };
        connect(m_anchor, qOverload<int>(&QComboBox::currentIndexChanged), this,
                [updateControls](int) { updateControls(); });
        connect(m_orientation, qOverload<int>(&QComboBox::currentIndexChanged), this,
                [updateControls](int) { updateControls(); });
        updateControls();
    }

    QString name() const { return m_name->text().trimmed(); }
    QString formula() const
    {
        return qApp->translateVariables()->TryFormulaFromUser(
            m_formula->text(), qApp->Settings()->getOsSeparator());
    }
    quint32 anchorObject() const { return m_anchor->currentData().toUInt(); }
    qreal anchorPosition() const { return m_anchorPosition->value() / 100.0; }
    QString orientation() const { return m_orientation->currentData().toString(); }
    qreal angle() const { return m_angle->value(); }

private:
    bool isLineAnchor(quint32 id) const
    {
        return id != NULL_ID && !m_doc->elementById(id, VAbstractPattern::TagLine).isNull();
    }

    VAbstractPattern *m_doc;
    const VContainer *m_data;
    QLineEdit *m_name;
    QLineEdit *m_formula;
    QComboBox *m_anchor;
    QDoubleSpinBox *m_anchorPosition;
    QComboBox *m_orientation;
    QDoubleSpinBox *m_angle;
};
}

ReferenceLineTool::ReferenceLineTool(VAbstractPattern *doc, VContainer *data, VMainGraphicsScene *scene,
                                     const ReferenceLineData &referenceLine)
    : m_doc(doc)
    , m_data(data)
    , m_scene(scene)
    , m_referenceLine(referenceLine)
{
    setFlag(QGraphicsItem::ItemIsSelectable);
    setAcceptHoverEvents(true);
    updateGeometry();
    m_scene->addItem(this);
    m_doc->addReferenceLine(m_referenceLine.id, this);
}

ReferenceLineTool::~ReferenceLineTool()
{
    m_doc->removeReferenceLine(m_referenceLine.id);
}

QRectF ReferenceLineTool::boundingRect() const
{
    return QRectF(0.0, -28.0, qMax(m_length, 80.0), 56.0).adjusted(-8.0, -4.0, 8.0, 4.0);
}

void ReferenceLineTool::paint(QPainter *painter, const QStyleOptionGraphicsItem *, QWidget *)
{
    painter->save();
    QPen pen(m_formulaValid ? QColor(72, 93, 170) : QColor(190, 45, 45));
    pen.setWidthF(isSelected() ? 2.0 : 1.2);
    pen.setStyle(m_formulaValid ? Qt::DashLine : Qt::DotLine);
    painter->setPen(pen);
    painter->drawLine(QPointF(0.0, 0.0), QPointF(m_length, 0.0));
    painter->drawLine(QPointF(0.0, -6.0), QPointF(0.0, 6.0));
    painter->drawLine(QPointF(m_length, -6.0), QPointF(m_length, 6.0));
    painter->setPen(pen.color());
    painter->drawText(QRectF(0.0, -26.0, qMax(m_length, 80.0), 20.0), Qt::AlignCenter, m_valueText);
    painter->restore();
}

void ReferenceLineTool::evaluate()
{
    try
    {
        Calculator calculator;
        const qreal value = calculator.EvalFormula(m_data->DataVariables(), m_referenceLine.formula);
        m_formulaValid = qIsFinite(value) && value > 0.0;
        m_length = m_formulaValid ? ToPixel(value, *m_data->GetPatternUnit()) : 80.0;
        const QString valueText = qApp->LocaleToString(value) + QLatin1Char(' ') +
                                  UnitsToStr(*m_data->GetPatternUnit(), true);
        m_valueText = m_referenceLine.name.isEmpty() ? valueText
                                                     : QStringLiteral("%1: %2").arg(m_referenceLine.name, valueText);
    }
    catch (...)
    {
        m_formulaValid = false;
        m_length = 80.0;
        m_valueText = m_referenceLine.name.isEmpty() ? tr("Invalid formula")
                                                     : tr("%1: invalid formula").arg(m_referenceLine.name);
    }
}

void ReferenceLineTool::updateGeometry()
{
    prepareGeometryChange();
    evaluate();

    QPointF origin(m_referenceLine.xPos, m_referenceLine.yPos);
    qreal anchoredLineAngle = m_referenceLine.angle;
    if (m_referenceLine.anchorObject != NULL_ID)
    {
        try
        {
            const QDomElement line = m_doc->elementById(m_referenceLine.anchorObject, VAbstractPattern::TagLine);
            if (!line.isNull())
            {
                const QPointF first = static_cast<QPointF>(*m_data->GeometricObject<VPointF>(
                    m_doc->GetParametrUInt(line, AttrFirstPoint, NULL_ID_STR)));
                const QPointF second = static_cast<QPointF>(*m_data->GeometricObject<VPointF>(
                    m_doc->GetParametrUInt(line, AttrSecondPoint, NULL_ID_STR)));
                origin = first + (second - first) * m_referenceLine.anchorPosition;
                anchoredLineAngle = QLineF(first, second).angle();
            }
            else
            {
                origin = static_cast<QPointF>(*m_data->GeometricObject<VPointF>(m_referenceLine.anchorObject));
            }
        }
        catch (...)
        {
            m_referenceLine.anchorObject = NULL_ID;
        }
    }
    setPos(origin);
    qreal displayAngle = m_referenceLine.angle;
    if (m_referenceLine.orientation == QStringLiteral("horizontal"))
    {
        displayAngle = 0.0;
    }
    else if (m_referenceLine.orientation == QStringLiteral("vertical"))
    {
        displayAngle = 90.0;
    }
    else if (m_referenceLine.orientation == QStringLiteral("anchor"))
    {
        displayAngle = anchoredLineAngle;
    }
    setRotation(displayAngle);
    setVisible(m_referenceLine.visible);
    setFlag(QGraphicsItem::ItemIsMovable, m_referenceLine.anchorObject == NULL_ID);
    setZValue(1000.0);
    update();
}

void ReferenceLineTool::addToFile()
{
    QDomElement element = m_doc->createElement(VAbstractPattern::TagReferenceLine);
    saveOptions(element);
    auto *command = new AddReferenceLine(element, m_doc);
    connect(command, &AddReferenceLine::NeedFullParsing, m_doc, &VAbstractPattern::NeedFullParsing);
    qApp->getUndoStack()->push(command);
}

void ReferenceLineTool::updateReferenceLine(const ReferenceLineData &referenceLine)
{
    m_referenceLine = referenceLine;
    updateGeometry();
}

ReferenceLineData ReferenceLineTool::referenceLine() const
{
    return m_referenceLine;
}

bool ReferenceLineTool::configure()
{
    return editReferenceLine();
}

void ReferenceLineTool::saveOptions(QDomElement &element) const
{
    m_doc->SetAttribute(element, VDomDocument::AttrId, m_referenceLine.id);
    m_doc->SetAttribute(element, VAbstractPattern::AttrName, m_referenceLine.name);
    m_doc->SetAttribute(element, VAbstractPattern::VariableFormula, m_referenceLine.formula);
    m_doc->SetAttribute(element, VAbstractPattern::AttrXPos, m_referenceLine.xPos);
    m_doc->SetAttribute(element, VAbstractPattern::AttrYPos, m_referenceLine.yPos);
    m_doc->SetAttribute(element, VAbstractPattern::AttrRotation, m_referenceLine.angle);
    m_doc->SetAttribute(element, VAbstractPattern::AttrAnchorObject, m_referenceLine.anchorObject);
    m_doc->SetAttribute(element, VAbstractPattern::AttrAnchorPosition, m_referenceLine.anchorPosition);
    m_doc->SetAttribute(element, VAbstractPattern::AttrOrientation, m_referenceLine.orientation);
    m_doc->SetAttribute(element, VAbstractPattern::AttrVisible, m_referenceLine.visible);
}

void ReferenceLineTool::saveChanges()
{
    QDomElement oldElement = m_doc->elementById(m_referenceLine.id, VAbstractPattern::TagReferenceLine);
    if (oldElement.isNull())
    {
        return;
    }
    QDomElement newElement = oldElement.cloneNode().toElement();
    saveOptions(newElement);
    auto *command = new SaveToolOptions(oldElement, newElement, m_doc, m_referenceLine.id);
    connect(command, &SaveToolOptions::NeedLiteParsing, m_doc, &VAbstractPattern::LiteParseTree);
    connect(command, &SaveToolOptions::NeedFullParsing, m_doc, &VAbstractPattern::NeedFullParsing);
    qApp->getUndoStack()->push(command);
}

bool ReferenceLineTool::editReferenceLine()
{
    ReferenceLineDialog dialog(m_referenceLine, m_doc, m_data, qApp->getMainWindow());
    if (dialog.exec() != QDialog::Accepted)
    {
        return false;
    }
    m_referenceLine.name = dialog.name();
    m_referenceLine.formula = dialog.formula();
    m_referenceLine.anchorObject = dialog.anchorObject();
    m_referenceLine.anchorPosition = dialog.anchorPosition();
    m_referenceLine.orientation = dialog.orientation();
    m_referenceLine.angle = dialog.angle();
    if (m_referenceLine.anchorObject == NULL_ID)
    {
        m_referenceLine.xPos = pos().x();
        m_referenceLine.yPos = pos().y();
    }
    updateGeometry();
    saveChanges();
    return true;
}

void ReferenceLineTool::deleteReferenceLine()
{
    if (QMessageBox::question(qApp->getMainWindow(), tr("Delete reference length"),
                              tr("Delete this reference length?")) != QMessageBox::Yes)
    {
        return;
    }
    auto *command = new DelTool(m_doc, m_referenceLine.id);
    connect(command, &DelTool::NeedFullParsing, m_doc, &VAbstractPattern::NeedFullParsing);
    qApp->getUndoStack()->push(command);
}

void ReferenceLineTool::contextMenuEvent(QGraphicsSceneContextMenuEvent *event)
{
    QMenu menu;
    QAction *editAction = menu.addAction(tr("Edit reference length..."));
    QAction *deleteAction = menu.addAction(tr("Delete reference length"));
    QAction *selected = menu.exec(event->screenPos());
    if (selected == editAction)
    {
        editReferenceLine();
    }
    else if (selected == deleteAction)
    {
        deleteReferenceLine();
    }
}

void ReferenceLineTool::mouseDoubleClickEvent(QGraphicsSceneMouseEvent *event)
{
    editReferenceLine();
    event->accept();
}

void ReferenceLineTool::mouseReleaseEvent(QGraphicsSceneMouseEvent *event)
{
    QGraphicsObject::mouseReleaseEvent(event);
    if (m_referenceLine.anchorObject == NULL_ID && event->button() == Qt::LeftButton)
    {
        const QPointF newPosition = pos();
        if (!qFuzzyCompare(m_referenceLine.xPos, newPosition.x()) ||
            !qFuzzyCompare(m_referenceLine.yPos, newPosition.y()))
        {
            m_referenceLine.xPos = newPosition.x();
            m_referenceLine.yPos = newPosition.y();
            saveChanges();
        }
    }
}

#include "reference_line_tool.moc"
