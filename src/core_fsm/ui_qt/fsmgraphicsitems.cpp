#include "fsmgraphicsitems.hpp"
#include <QPainter>
#include <QStyleOptionGraphicsItem>
#include <QtMath>
#include <QTextOption>

StateItem::StateItem(const QString& id, bool isInitial, QGraphicsItem* parent)
    : QGraphicsEllipseItem(-RADIUS, -RADIUS, 2*RADIUS, 2*RADIUS, parent)
    , m_id(id), m_isInitial(isInitial)
{
    m_font = QFont("Arial", 10);
    setBrush(QBrush(Qt::white));
    setPen(QPen(Qt::black, 2));
    
    // Set flags to enable position notifications
    setFlags(QGraphicsItem::ItemIsSelectable | 
             QGraphicsItem::ItemIsMovable | 
             QGraphicsItem::ItemSendsGeometryChanges);
}

void StateItem::setInitial(bool isInitial)
{
    m_isInitial = isInitial;
    update();
}

QRectF StateItem::boundingRect() const
{
    QRectF baseRect = QGraphicsEllipseItem::boundingRect();
    // Make slightly larger to account for the initial state marking
    return baseRect.adjusted(-5, -5, 5, 5);
}

void StateItem::paint(QPainter* painter, const QStyleOptionGraphicsItem* option, QWidget* widget)
{
    QGraphicsEllipseItem::paint(painter, option, widget);
    
    // Draw the state name
    painter->setFont(m_font);
    painter->drawText(boundingRect(), Qt::AlignCenter, m_id);
    
    // If initial state, draw a double circle
    if (m_isInitial) {
        painter->setPen(QPen(Qt::black, 2));
        painter->setBrush(Qt::NoBrush);
        painter->drawEllipse(-RADIUS-5, -RADIUS-5, 2*(RADIUS+5), 2*(RADIUS+5));
    }
}

QVariant StateItem::itemChange(GraphicsItemChange change, const QVariant &value)
{
    // If the position changed
    if (change == ItemPositionChange || change == ItemPositionHasChanged) {
        // Update all connected transitions
        for (auto* transition : m_incomingTransitions) {
            transition->updatePosition();
        }
        for (auto* transition : m_outgoingTransitions) {
            transition->updatePosition();
        }
    }
    
    // Call the parent implementation to handle other changes
    return QGraphicsEllipseItem::itemChange(change, value);
}

void StateItem::addIncomingTransition(TransitionItem* transition)
{
    m_incomingTransitions.insert(transition);
}

void StateItem::addOutgoingTransition(TransitionItem* transition)
{
    m_outgoingTransitions.insert(transition);
}

void StateItem::removeIncomingTransition(TransitionItem* transition)
{
    m_incomingTransitions.remove(transition);
}

void StateItem::removeOutgoingTransition(TransitionItem* transition)
{
    m_outgoingTransitions.remove(transition);
}

TransitionItem::TransitionItem(StateItem* fromState, StateItem* toState, 
                          const QString& trigger, const QString& guard,
                          const QString& delay, QGraphicsItem* parent)
    : QGraphicsPathItem(parent)
    , m_fromState(fromState), m_toState(toState)
    , m_trigger(trigger), m_guard(guard), m_delay(delay)
{
    m_font = QFont("Arial", 8);
    setPen(QPen(Qt::black, 1.5));
    setFlags(QGraphicsItem::ItemIsSelectable);
    
    // Register with the states
    if (m_fromState) {
        m_fromState->addOutgoingTransition(this);
    }
    if (m_toState) {
        m_toState->addIncomingTransition(this);
    }
    
    updatePosition();
}

TransitionItem::~TransitionItem()
{
    // Unregister from the states
    if (m_fromState) {
        m_fromState->removeOutgoingTransition(this);
    }
    if (m_toState) {
        m_toState->removeIncomingTransition(this);
    }
}

void TransitionItem::updatePosition()
{
    QPointF fromPos = m_fromState->scenePos();
    QPointF toPos = m_toState->scenePos();
    
    // Calculate the path
    QPainterPath path = createArrowPath(fromPos, toPos);
    setPath(path);
}

QPainterPath TransitionItem::createArrowPath(const QPointF& start, const QPointF& end)
{
    QPainterPath path;
    
    // Calculate the vector from start to end
    QPointF delta = end - start;
    qreal length = qSqrt(delta.x() * delta.x() + delta.y() * delta.y());
    
    if (qFuzzyCompare(length, 0.0)) {
        // Self-transition (loop)
        
        // Define the starting point on the state circle (lower-left quadrant)
        const qreal startAngle = 140.0 * (M_PI/180.0); // 225 degrees in radians (south-west)
        QPointF startPoint(
            start.x() + StateItem::RADIUS * qCos(startAngle),
            start.y() + StateItem::RADIUS * qSin(startAngle)
        );
        
        // Define the ending point on the state circle (bottom quadrant)
        const qreal endAngle = 35.0 * (M_PI/180.0); // 315 degrees in radians (south-east)
        QPointF endPoint(
            start.x() + StateItem::RADIUS * qCos(endAngle),
            start.y() + StateItem::RADIUS * qSin(endAngle)
        );
        
        // Define control points for a bezier curve
        qreal distance = StateItem::RADIUS * 2; // control point distance from center
        QPointF controlPoint1(
            start.x() - distance/2,
            start.y() + distance
        );
        QPointF controlPoint2(
            start.x() + distance/2,
            start.y() + distance
        );
        
        // Create a cubic bezier curve as the path
        path.moveTo(startPoint);
        path.cubicTo(controlPoint1, controlPoint2, endPoint);
        
        // Calculate the direction at the end point for the arrow
        QPointF direction = endPoint - controlPoint2;
        qreal dirLength = qSqrt(direction.x() * direction.x() + direction.y() * direction.y());
        direction = direction / dirLength;
        QPointF perpDirection(-direction.y(), direction.x());
        
        // Draw arrow head
        const qreal arrowSize = 8.0;
        QPointF arrowP1 = endPoint - direction * arrowSize + perpDirection * arrowSize/2;
        QPointF arrowP2 = endPoint - direction * arrowSize - perpDirection * arrowSize/2;
        
        path.moveTo(arrowP1);
        path.lineTo(endPoint);
        path.lineTo(arrowP2);
    }
    else {
        // Regular transition between different states - existing code
        QPointF unit = delta / length;
        QPointF perpUnit(-unit.y(), unit.x());
        
        // Calculate actual start/end points on the edge of the states
        QPointF actualStart = start + unit * StateItem::RADIUS;
        QPointF actualEnd = end - unit * StateItem::RADIUS;
        
        // Draw the line
        path.moveTo(actualStart);
        path.lineTo(actualEnd);
        
        // Draw arrow head
        const qreal arrowSize = 10.0;
        QPointF arrowP1 = actualEnd - unit * arrowSize + perpUnit * arrowSize/2;
        QPointF arrowP2 = actualEnd - unit * arrowSize - perpUnit * arrowSize/2;
        
        path.moveTo(arrowP1);
        path.lineTo(actualEnd);
        path.lineTo(arrowP2);
    }
    
    return path;
}

QRectF TransitionItem::boundingRect() const
{
    // Add extra space for the label
    return QGraphicsPathItem::boundingRect().adjusted(-30, -30, 30, 30);
}

void TransitionItem::paint(QPainter* painter, const QStyleOptionGraphicsItem* option, QWidget* widget)
{
    QGraphicsPathItem::paint(painter, option, widget);
    
    // Draw the label
    painter->setFont(m_font);
    
    // Build label text
    QString labelText = m_trigger;
    if (!m_guard.isEmpty()) {
        labelText += "\n[" + m_guard + "]";
    }
    if (!m_delay.isEmpty()) {
        labelText += "\n@" + m_delay;
    }
    
    // Find midpoint of the path for label position
    QPointF midPoint = (m_fromState->scenePos() + m_toState->scenePos()) / 2.0;
    
    // Draw text with a small white background for better visibility
    QRectF textRect(midPoint.x() - 40, midPoint.y() - 20, 80, 40);
    painter->setBrush(QBrush(QColor(255, 255, 255, 180))); // Semi-transparent white
    painter->drawRoundedRect(textRect, 5, 5);
    
    QTextOption textOption;
    textOption.setAlignment(Qt::AlignCenter);
    painter->drawText(textRect, labelText, textOption);
}
