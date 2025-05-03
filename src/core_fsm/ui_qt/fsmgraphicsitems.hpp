#pragma once

#include <QGraphicsItem>
#include <QGraphicsEllipseItem>
#include <QGraphicsLineItem>
#include <QGraphicsPathItem>
#include <QPainterPath>
#include <QString>
#include <QFont>
#include <QBrush>
#include <QPen>
#include <QSet>

class TransitionItem; // Forward declaration

class StateItem : public QGraphicsEllipseItem {
public:
    StateItem(const QString& id, bool isInitial, QGraphicsItem* parent = nullptr);
    ~StateItem();

    QString stateId() const { return m_id; }
    void setStateId(const QString& id) { m_id = id; }
    void setInitial(bool isInitial);
    QRectF boundingRect() const override;
    void paint(QPainter* painter, const QStyleOptionGraphicsItem* option, QWidget* widget) override;
    static constexpr qreal RADIUS = 40.0;

    // Add this method to notify when position changes
    QVariant itemChange(GraphicsItemChange change, const QVariant &value) override;
    
    // Add these methods to manage connections with transitions
    void addIncomingTransition(TransitionItem* transition);
    void addOutgoingTransition(TransitionItem* transition);
    void removeIncomingTransition(TransitionItem* transition);
    void removeOutgoingTransition(TransitionItem* transition);

private:
    QString m_id;
    bool m_isInitial;
    QFont m_font;

    // Add these to track connected transitions
    QSet<TransitionItem*> m_incomingTransitions;
    QSet<TransitionItem*> m_outgoingTransitions;

    // Add a flag to prevent recursive updates
    bool m_updatingTransitions = false;
};

class TransitionItem : public QGraphicsPathItem {
public:
    StateItem* fromItem() const { return m_fromState; }
    StateItem*   toItem() const { return m_toState; }
    TransitionItem(StateItem* fromState, StateItem* toState,
                  const QString& trigger, const QString& guard,
                  const QString& delay, int offsetIndex = 0, 
                  QGraphicsItem* parent = nullptr);
    // Add this method to handle state destruction
    void stateDestroyed(StateItem* state);

    void updatePosition();
    QRectF boundingRect() const override;
    void paint(QPainter* painter, const QStyleOptionGraphicsItem* option, QWidget* widget) override;

    // Add destructor declaration:
    ~TransitionItem();

    // Add itemChange method to handle scene changes
    QVariant itemChange(GraphicsItemChange change, const QVariant &value) override;

    // Add these methods to the TransitionItem class declaration
    void setTrigger(const QString& trigger) { m_trigger = trigger; }
    void setGuard(const QString& guard) { m_guard = guard; }
    void setDelay(const QString& delay) { m_delay = delay; }

    // Add shape method to the TransitionItem class declaration
    QPainterPath shape() const override;

    void setOffsetIndex(int index) { m_offsetIndex = index; }

private:
    StateItem* m_fromState;
    StateItem* m_toState;
    QString m_trigger;
    QString m_guard;
    QString m_delay;
    QFont m_font;
    QPainterPath createArrowPath(const QPointF& start, const QPointF& end);

    // Add this field
    bool m_isBeingDestroyed = false;

    int m_offsetIndex = 0; // Used to offset multiple transitions between same states
};