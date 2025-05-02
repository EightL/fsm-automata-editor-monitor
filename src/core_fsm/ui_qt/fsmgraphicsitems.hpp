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

class StateItem : public QGraphicsEllipseItem {
public:
    StateItem(const QString& id, bool isInitial, QGraphicsItem* parent = nullptr);
    
    QString stateId() const { return m_id; }
    void setInitial(bool isInitial);
    QRectF boundingRect() const override;
    void paint(QPainter* painter, const QStyleOptionGraphicsItem* option, QWidget* widget) override;
    static constexpr qreal RADIUS = 40.0;

private:
    QString m_id;
    bool m_isInitial;
    QFont m_font;
};

class TransitionItem : public QGraphicsPathItem {
public:
    TransitionItem(StateItem* fromState, StateItem* toState, 
                  const QString& trigger, const QString& guard,
                  const QString& delay, QGraphicsItem* parent = nullptr);
                  
    void updatePosition();
    QRectF boundingRect() const override;
    void paint(QPainter* painter, const QStyleOptionGraphicsItem* option, QWidget* widget) override;

private:
    StateItem* m_fromState;
    StateItem* m_toState;
    QString m_trigger;
    QString m_guard;
    QString m_delay;
    QFont m_font;
    QPainterPath createArrowPath(const QPointF& start, const QPointF& end);
};