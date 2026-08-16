//---------------------------------------------------------------------------------------------------------------------
//  @file   reference_line_tool.h
//  @brief  Non-constructive length reference shown in the draft scene.
//---------------------------------------------------------------------------------------------------------------------

#ifndef REFERENCE_LINE_TOOL_H
#define REFERENCE_LINE_TOOL_H

#include <QGraphicsObject>

#include "../../vmisc/def.h"

class QDomElement;
class VAbstractPattern;
class VContainer;
class VMainGraphicsScene;

class ReferenceLineTool final : public QGraphicsObject
{
    Q_OBJECT

public:
    ReferenceLineTool(VAbstractPattern *doc, VContainer *data, VMainGraphicsScene *scene,
                      const ReferenceLineData &referenceLine);
    ~ReferenceLineTool() override;

    QRectF boundingRect() const override;
    void paint(QPainter *painter, const QStyleOptionGraphicsItem *option, QWidget *widget = nullptr) override;

    void addToFile();
    void updateReferenceLine(const ReferenceLineData &referenceLine);
    ReferenceLineData referenceLine() const;
    bool configure();

protected:
    void contextMenuEvent(QGraphicsSceneContextMenuEvent *event) override;
    void mouseDoubleClickEvent(QGraphicsSceneMouseEvent *event) override;
    void mouseReleaseEvent(QGraphicsSceneMouseEvent *event) override;

private:
    VAbstractPattern *m_doc;
    VContainer *m_data;
    VMainGraphicsScene *m_scene;
    ReferenceLineData m_referenceLine;
    qreal m_length{0.0};
    QString m_valueText;
    bool m_formulaValid{false};

    void evaluate();
    void updateGeometry();
    bool editReferenceLine();
    void saveOptions(QDomElement &element) const;
    void saveChanges();
    void deleteReferenceLine();
};

#endif // REFERENCE_LINE_TOOL_H
