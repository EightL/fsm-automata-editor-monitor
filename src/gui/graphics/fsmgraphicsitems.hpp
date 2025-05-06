/**
 * @file   fsmgraphicsitems.hpp
 * @brief  Defines the graphical representation classes for FSM visualization.
 *         Provides StateItem and TransitionItem for drawing states and transitions
 *         in the graphical editor.
 *
 * @author Martin Ševčík (xsevcim00)
 * @author Jakub Lůčný (xlucnyj00)
 * @date   2025-05-06
 */
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
#include <QTimer>

class TransitionItem; // Forward declaration

/**
 * @brief Graphical representation of an FSM state.
 *
 * Visualizes states as circular items with labels and provides 
 * visual distinction for initial states. Manages connection references
 * to incoming and outgoing transitions.
 */
class StateItem : public QGraphicsEllipseItem {
public:
    /**
     * @brief Construct a new State Item.
     * @param id        Text identifier for this state
     * @param isInitial Whether this is the initial state
     * @param parent    Parent graphics item in the scene hierarchy
     */
    StateItem(const QString& id, bool isInitial, QGraphicsItem* parent = nullptr);
    
    /**
     * @brief Destructor - cleans up transitions connected to this state.
     */
    ~StateItem();

    /**
     * @brief Get the state identifier.
     * @return The state's textual ID
     */
    QString stateId() const { return m_id; }
    
    /**
     * @brief Set the state identifier.
     * @param id New ID for this state
     */
    void setStateId(const QString& id) { m_id = id; }
    
    /**
     * @brief Change the initial state status.
     * @param isInitial Whether this state should be marked as initial
     */
    void setInitial(bool isInitial);
    
    /**
     * @brief Get the bounding rectangle for this item.
     * @return Rectangle containing the entire visual representation
     */
    QRectF boundingRect() const override;
    
    /**
     * @brief Draw the state visualization.
     * @param painter The painter to use for drawing
     * @param option  Style options for the item
     * @param widget  Widget being painted on
     */
    void paint(QPainter* painter, const QStyleOptionGraphicsItem* option, QWidget* widget) override;
    
    /**
     * @brief Standard radius for state circles.
     */
    static constexpr qreal RADIUS = 40.0;

    /**
     * @brief Handle changes to the item's state.
     * Used to update connected transitions when position changes.
     * 
     * @param change The type of change
     * @param value  The new value
     * @return QVariant with the processed change value
     */
    QVariant itemChange(GraphicsItemChange change, const QVariant &value) override;
    
    /**
     * @brief Register an incoming transition with this state.
     * @param transition The transition to register
     */
    void addIncomingTransition(TransitionItem* transition);
    
    /**
     * @brief Register an outgoing transition with this state.
     * @param transition The transition to register
     */
    void addOutgoingTransition(TransitionItem* transition);
    
    /**
     * @brief Remove an incoming transition reference.
     * @param transition The transition to unregister
     */
    void removeIncomingTransition(TransitionItem* transition);
    
    /**
     * @brief Remove an outgoing transition reference.
     * @param transition The transition to unregister
     */
    void removeOutgoingTransition(TransitionItem* transition);

private:
    QString m_id;                              ///< State identifier
    bool m_isInitial;                          ///< Whether this is the initial state
    QFont m_font;                              ///< Font for rendering text
    QSet<TransitionItem*> m_incomingTransitions; ///< Transitions entering this state
    QSet<TransitionItem*> m_outgoingTransitions; ///< Transitions leaving this state
    bool m_updatingTransitions = false;        ///< Flag to prevent recursive updates
};

/**
 * @brief Graphical representation of a transition between states.
 *
 * Visualizes transitions as arrows with optional labels for triggers,
 * guards, and delays. Automatically updates position when connected
 * states move and handles proper cleanup when states are deleted.
 */
class TransitionItem : public QGraphicsPathItem {
public:
    /**
     * @brief Get the source state of this transition.
     * @return Pointer to the state from which this transition originates
     */
    StateItem* fromItem() const { return m_fromState; }
    
    /**
     * @brief Get the target state of this transition.
     * @return Pointer to the state to which this transition leads
     */
    StateItem* toItem() const { return m_toState; }
    
    /**
     * @brief Construct a new Transition Item.
     * @param fromState   Source state
     * @param toState     Target state
     * @param trigger     Input trigger name
     * @param guard       Guard expression
     * @param delay       Delay specification
     * @param offsetIndex Index for offsetting parallel transitions
     * @param parent      Parent graphics item
     */
    TransitionItem(StateItem* fromState, StateItem* toState,
                  const QString& trigger, const QString& guard,
                  const QString& delay, int offsetIndex = 0, 
                  QGraphicsItem* parent = nullptr);
    
    /**
     * @brief Handle state destruction to prevent dangling pointers.
     * @param state The state being destroyed
     */
    void stateDestroyed(StateItem* state);

    /**
     * @brief Update the transition's path based on connected states' positions.
     */
    void updatePosition();
    
    /**
     * @brief Get the bounding rectangle for this transition.
     * @return Rectangle containing the entire visual representation
     */
    QRectF boundingRect() const override;
    
    /**
     * @brief Draw the transition visualization.
     * @param painter The painter to use for drawing
     * @param option  Style options for the item
     * @param widget  Widget being painted on
     */
    void paint(QPainter* painter, const QStyleOptionGraphicsItem* option, QWidget* widget) override;

    /**
     * @brief Destructor - cleans up and notifies connected states.
     */
    ~TransitionItem();

    /**
     * @brief Handle changes to the item's state.
     * @param change The type of change
     * @param value  The new value
     * @return QVariant with the processed change value
     */
    QVariant itemChange(GraphicsItemChange change, const QVariant &value) override;

    /**
     * @brief Set the input trigger for this transition.
     * @param trigger New trigger expression
     */
    void setTrigger(const QString& trigger) { m_trigger = trigger; }
    
    /**
     * @brief Set the guard condition for this transition.
     * @param guard New guard expression
     */
    void setGuard(const QString& guard) { m_guard = guard; }
    
    /**
     * @brief Set the delay specification for this transition.
     * @param delay New delay value or variable name
     */
    void setDelay(const QString& delay) { m_delay = delay; }

    /**
     * @brief Temporarily highlight this transition.
     * 
     * Makes the transition stand out visually for a brief period.
     * Used to indicate active transitions during simulation.
     */
    void highlight() {
        // Make a fat green pen
        QPen highlightPen = m_normalPen;
        highlightPen.setColor(Qt::green);
        highlightPen.setWidth(m_normalPen.width() + 2);
        setPen(highlightPen);
        
        // Revert after 250ms
        QTimer::singleShot(250, [this]() {
            setPen(m_normalPen);
        });
    }

    /**
     * @brief Define the shape for hit detection and selection.
     * @return Path representing the clickable area of the transition
     */
    QPainterPath shape() const override;

    /**
     * @brief Set the offset index for parallel transitions.
     * @param index New offset index value
     */
    void setOffsetIndex(int index) { m_offsetIndex = index; }

private:
    QPen m_normalPen;           ///< Standard pen for drawing
    StateItem* m_fromState;     ///< Source state
    StateItem* m_toState;       ///< Target state
    QString m_trigger;          ///< Input trigger expression
    QString m_guard;            ///< Guard condition expression
    QString m_delay;            ///< Delay specification
    QFont m_font;               ///< Font for rendering text
    bool m_isBeingDestroyed = false;  ///< Flag to prevent recursive destruction
    int m_offsetIndex = 0;      ///< Offset index for parallel transitions

    /**
     * @brief Create an arrow path between two points.
     * @param start Starting point of the arrow
     * @param end   Ending point of the arrow
     * @return Path representing an arrow from start to end
     */
    QPainterPath createArrowPath(const QPointF& start, const QPointF& end);
};