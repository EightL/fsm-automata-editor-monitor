#include "fsmgraphicsitems.hpp"
#include <QPainter>
#include <QStyleOptionGraphicsItem>
#include <QtMath>
#include <QTextOption>
#include <QTimer>

StateItem::StateItem(const QString& id, bool isInitial, QGraphicsItem* parent)
    : QGraphicsEllipseItem(-RADIUS, -RADIUS, 2*RADIUS, 2*RADIUS, parent)
    , m_id(id), m_isInitial(isInitial)
    , m_incomingTransitions(), m_outgoingTransitions() // Explicitly initialize the sets
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
    QVariant result = QGraphicsEllipseItem::itemChange(change, value);
    
    if (change == ItemPositionHasChanged && !m_updatingTransitions) {
        // Set the flag to prevent recursion
        m_updatingTransitions = true;
        
        // Use local copies of the sets to avoid modification during iteration
        QSet<TransitionItem*> incomingCopy = m_incomingTransitions;
        QSet<TransitionItem*> outgoingCopy = m_outgoingTransitions;
        
        // Update all connected transitions
        for (auto* transition : incomingCopy) {
            if (transition) {
                transition->updatePosition();
            }
        }
        
        for (auto* transition : outgoingCopy) {
            if (transition) {
                transition->updatePosition();
            }
        }
        
        // Reset the flag
        m_updatingTransitions = false;
    }

    return result;
}

void StateItem::addIncomingTransition(TransitionItem* transition)
{
    if (transition && !m_incomingTransitions.contains(transition)) {
        m_incomingTransitions.insert(transition);
    }
}

void StateItem::addOutgoingTransition(TransitionItem* transition)
{
    if (transition && !m_outgoingTransitions.contains(transition)) {
        m_outgoingTransitions.insert(transition);
    }
}

void StateItem::removeIncomingTransition(TransitionItem* transition)
{
    if (transition && m_incomingTransitions.contains(transition)) {
        m_incomingTransitions.remove(transition);
    }
}

void StateItem::removeOutgoingTransition(TransitionItem* transition)
{
    if (transition && m_outgoingTransitions.contains(transition)) {
        m_outgoingTransitions.remove(transition);
    }
}

StateItem::~StateItem()
{
    // Create a copy of the sets since we'll be modifying them during iteration
    QSet<TransitionItem*> incomingCopy = m_incomingTransitions;
    QSet<TransitionItem*> outgoingCopy = m_outgoingTransitions;
    
    // Clear the sets first to prevent callbacks
    m_incomingTransitions.clear();
    m_outgoingTransitions.clear();
    
    // Now safely notify transitions that we're being destroyed
    // without expecting callbacks
    for (auto* transition : incomingCopy) {
        if (transition) {
            // Directly set null pointer instead of calling remove
            // which would try to modify our now-empty set
            transition->stateDestroyed(this);
        }
    }
    
    for (auto* transition : outgoingCopy) {
        if (transition) {
            // Same direct approach
            transition->stateDestroyed(this);
        }
    }
}

TransitionItem::TransitionItem(StateItem* fromState, StateItem* toState, 
                          const QString& trigger, const QString& guard,
                          const QString& delay, int offsetIndex, QGraphicsItem* parent)
    : QGraphicsPathItem(parent)
    , m_fromState(fromState), m_toState(toState)
    , m_trigger(trigger), m_guard(guard), m_delay(delay)
    , m_offsetIndex(offsetIndex)
{
    m_font = QFont("Arial", 8);
    setPen(QPen(Qt::black, 1.5));
    setFlags(QGraphicsItem::ItemIsSelectable);
    
    // Register with the states - only if both states exist
    if (m_fromState && m_toState) {
        m_fromState->addOutgoingTransition(this);
        m_toState->addIncomingTransition(this);
        
        // DON'T update position here - wait until added to scene
        // The scene change handler will trigger the position update
    }
}

QVariant TransitionItem::itemChange(GraphicsItemChange change, const QVariant &value)
{
    if (change == QGraphicsItem::ItemSceneHasChanged && scene() && !m_isBeingDestroyed) {
        QTimer::singleShot(0, [this]() { 
            // Check if object still exists and not being destroyed
            if (!m_isBeingDestroyed && scene()) {
                this->updatePosition(); 
            }
        });
    }
    
    return QGraphicsPathItem::itemChange(change, value);
}

void TransitionItem::stateDestroyed(StateItem* state)
{
    // Just null out the pointer without trying to deregister
    if (m_fromState == state) {
        m_fromState = nullptr;
    }
    if (m_toState == state) {
        m_toState = nullptr;  
    }
}

TransitionItem::~TransitionItem()
{
    // Set the flag first to prevent any new operations
    m_isBeingDestroyed = true;
    
    // Safely unregister from states if they still exist
    if (m_fromState) {
        // Use try/catch to handle potential invalid pointers
        try {
            m_fromState->removeOutgoingTransition(this);
        } catch (...) {
            // State might be in an invalid state, just ignore
        }
        m_fromState = nullptr;
    }
    
    if (m_toState) {
        try {
            m_toState->removeIncomingTransition(this);
        } catch (...) {
            // State might be in an invalid state, just ignore
        }
        m_toState = nullptr;
    }
}

void TransitionItem::updatePosition()
{
    if (m_isBeingDestroyed) {
        return;
    }
    
    // Check if both states exist before updating position
    if (!m_fromState || !m_toState) {
        return;
    }
    
    // Add scene checks to ensure objects are still in the scene
    if (!scene() || !m_fromState->scene() || !m_toState->scene()) {
        return;
    }
    
    try {
        QPointF fromPos = m_fromState->scenePos();
        QPointF toPos = m_toState->scenePos();
        
        // Calculate the path
        QPainterPath path = createArrowPath(fromPos, toPos);
        setPath(path);
    } catch (...) {
        // Silently handle exceptions from invalid pointers
    }
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
    
    // Only draw the label if both states exist
    if (!m_fromState || !m_toState) {
        return;
    }
    
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
    
    // Calculate the text size dynamically using font metrics
    QFontMetrics fm(m_font);
    QStringList lines = labelText.split('\n');
    int textWidth = 0;
    int textHeight = 0;
    
    // Find the widest line and calculate total height
    for (const QString& line : lines) {
        textWidth = qMax(textWidth, fm.horizontalAdvance(line));
        textHeight += fm.height();
    }
    
    // Add padding around text
    const int padding = 8;
    int boxWidth = qMax(50, textWidth + 2 * padding); // Minimum 50 pixels wide
    int boxHeight = qMax(30, textHeight + 2 * padding); // Minimum 30 pixels high
    
    QPointF labelPos;
    QRectF textRect;
    
    // Check if this is a self-transition (same start and end state)
    if (m_fromState == m_toState) {
        // For self-transitions, position the label below the loop
        QPointF statePos = m_fromState->scenePos();
        
        // Position the label below the lowest point of the loop
        qreal distance = StateItem::RADIUS * 1.5;
        
        // Apply horizontal offset for multiple self-transitions
        const int offsetX = m_offsetIndex * (boxWidth + 10); // Shift based on actual box width
        labelPos = QPointF(statePos.x() + offsetX, statePos.y() + distance);
        textRect = QRectF(labelPos.x() - boxWidth/2, labelPos.y(), boxWidth, boxHeight);
    } else {
        // Regular transition between different states
        QPointF fromPos = m_fromState->scenePos();
        QPointF toPos = m_toState->scenePos();
        QPointF midPoint = (fromPos + toPos) / 2.0;
        
        // Calculate perpendicular vector for offset
        QPointF vec = toPos - fromPos;
        QPointF perpVec(-vec.y(), vec.x());
        
        // Normalize the perpendicular vector
        qreal length = qSqrt(perpVec.x() * perpVec.x() + perpVec.y() * perpVec.y());
        if (!qFuzzyIsNull(length)) {
            perpVec = perpVec / length;
            
            // Offset in perpendicular direction based on index
            int side = (m_offsetIndex % 2 == 0) ? 1 : -1;
            int magnitude = ((m_offsetIndex + 1) / 2) * (boxHeight + 10); // Scale offset by box height
            QPointF offset = perpVec * side * magnitude;
            
            labelPos = midPoint + offset;
        } else {
            labelPos = midPoint;
        }
        
        textRect = QRectF(labelPos.x() - boxWidth/2, labelPos.y() - boxHeight/2, boxWidth, boxHeight);
    }
    
    // Draw text with a small white background for better visibility
    painter->setBrush(QBrush(QColor(255, 255, 255, 180))); // Semi-transparent white
    painter->drawRoundedRect(textRect, 5, 5);
    
    QTextOption textOption;
    textOption.setAlignment(Qt::AlignCenter);
    painter->drawText(textRect, labelText, textOption);
}

QPainterPath TransitionItem::shape() const
{
    // Start with the base arrow path
    QPainterPath resultPath = QGraphicsPathItem::path();
    
    // Only include the text box if both states exist
    if (!m_fromState || !m_toState) {
        return resultPath;
    }
    
    // Build label text
    QString labelText = m_trigger;
    if (!m_guard.isEmpty()) {
        labelText += "\n[" + m_guard + "]";
    }
    if (!m_delay.isEmpty()) {
        labelText += "\n@" + m_delay;
    }
    
    // Calculate the text size dynamically
    QFontMetrics fm(m_font);
    QStringList lines = labelText.split('\n');
    int textWidth = 0;
    int textHeight = 0;
    
    for (const QString& line : lines) {
        textWidth = qMax(textWidth, fm.horizontalAdvance(line));
        textHeight += fm.height();
    }
    
    // Add padding around text
    const int padding = 8;
    int boxWidth = qMax(50, textWidth + 2 * padding);
    int boxHeight = qMax(30, textHeight + 2 * padding);
    
    QPointF labelPos;
    QRectF textRect;
    
    // Check if this is a self-transition (same start and end state)
    if (m_fromState == m_toState) {
        QPointF statePos = m_fromState->scenePos();
        qreal distance = StateItem::RADIUS * 1.5;
        
        // Apply horizontal offset for multiple self-transitions
        const int offsetX = m_offsetIndex * (boxWidth + 10);
        labelPos = QPointF(statePos.x() + offsetX, statePos.y() + distance);
        textRect = QRectF(labelPos.x() - boxWidth/2, labelPos.y(), boxWidth, boxHeight);
    } else {
        // Regular transition between different states
        QPointF fromPos = m_fromState->scenePos();
        QPointF toPos = m_toState->scenePos();
        QPointF midPoint = (fromPos + toPos) / 2.0;
        
        // Calculate perpendicular vector for offset
        QPointF vec = toPos - fromPos;
        QPointF perpVec(-vec.y(), vec.x());
        
        qreal length = qSqrt(perpVec.x() * perpVec.x() + perpVec.y() * perpVec.y());
        if (!qFuzzyIsNull(length)) {
            perpVec = perpVec / length;
            
            int side = (m_offsetIndex % 2 == 0) ? 1 : -1;
            int magnitude = ((m_offsetIndex + 1) / 2) * (boxHeight + 10);
            QPointF offset = perpVec * side * magnitude;
            
            labelPos = midPoint + offset;
        } else {
            labelPos = midPoint;
        }
        
        textRect = QRectF(labelPos.x() - boxWidth/2, labelPos.y() - boxHeight/2, boxWidth, boxHeight);
    }
    
    // Convert text rect to item coordinates and add to path
    textRect = mapFromScene(textRect).boundingRect();
    
    QPainterPath textBoxPath;
    textBoxPath.addRoundedRect(textRect, 5, 5);
    
    resultPath.addPath(textBoxPath);
    
    return resultPath;
}
