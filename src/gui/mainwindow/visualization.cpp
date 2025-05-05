#include <QApplication>
#include <QDebug>
#include <QtMath>

#include "../graphics/fsmgraphicsitems.hpp"

#include "mainwindow.hpp"
#include "ui_mainwindow.h"

void MainWindow::visualizeFsm()
{
    // Store existing state positions before clearing
    QMap<std::string, QPointF> statePositions;
    for (auto it = m_stateItems.begin(); it != m_stateItems.end(); ++it) {
        statePositions[it.key()] = it.value()->pos();
    }
    
    // Make sure we don't have any pending item changes when starting
    QApplication::processEvents();
    
    clearFsmVisualization();
    
    // Delay to ensure items are properly cleared
    QApplication::processEvents();
    
    // Track which states are new (need automatic positioning)
    QSet<QString> newStateIds;
    
    // Create states, reusing positions for existing states
    for (const auto& state : m_doc.states) {
        StateItem* item = new StateItem(
            QString::fromStdString(state.id), 
            state.initial
        );
        m_scene->addItem(item);
        m_stateItems[state.id] = item;
        
        // If this state existed before, restore its position
        if (statePositions.contains(state.id)) {
            item->setPos(statePositions[state.id]);
        } else {
            // Mark as new state needing position assignment
            newStateIds.insert(QString::fromStdString(state.id));
        }
    }
    
    // Only lay out new states - but ONLY if there are any new states
    if (!newStateIds.isEmpty()) {
        layoutNewStateElements(newStateIds);
    }
    
    // Process any pending position changes
    QApplication::processEvents();
    
    // Track transitions between each state pair to assign offset indices
    QMap<QPair<std::string, std::string>, int> transitionCounts;
    
    // Now create transitions
    for (const auto& transition : m_doc.transitions) {
        // Skip invalid transitions
        if (!m_stateItems.contains(transition.from) || !m_stateItems.contains(transition.to)) {
            continue;
        }
        
        // Get or create offset index for this state pair
        QPair<std::string, std::string> statePair(transition.from, transition.to);
        int offsetIndex = transitionCounts.value(statePair, 0);
        transitionCounts[statePair] = offsetIndex + 1;
        
        QString delayText;
        if (!transition.delay_ms.is_null()) {
            // Format delay text
            if (transition.delay_ms.is_number_integer()) {
                delayText = QString::number(transition.delay_ms.get<int>()) + "ms";
            } else {
                delayText = QString::fromStdString(transition.delay_ms.dump());
            }
        }
        
        // Verify states again before creating the transition
        StateItem* fromState = m_stateItems.value(transition.from, nullptr);
        StateItem* toState = m_stateItems.value(transition.to, nullptr);
        
        if (fromState && toState && fromState->scene() && toState->scene()) {
            TransitionItem* item = new TransitionItem(
                fromState, toState,
                QString::fromStdString(transition.trigger),
                QString::fromStdString(transition.guard),
                delayText,
                offsetIndex
            );
            
            m_scene->addItem(item);
            m_transitionItems.append(item);
        }
    }
    
    m_scene->setSceneRect(m_scene->itemsBoundingRect().adjusted(-50, -50, 50, 50));
}

void MainWindow::clearFsmVisualization()
{
    // First, remove all transitions from the scene
    for (auto* transition : m_transitionItems) {
        if (transition && transition->scene()) {
            m_scene->removeItem(transition);
            delete transition;
        }
    }
    m_transitionItems.clear();
    
    // Process events to ensure transitions are fully deleted
    QApplication::processEvents();
    
    // Then remove all states
    for (auto& pair : m_stateItems) {
        if (pair && pair->scene()) {
            m_scene->removeItem(pair);
            delete pair;
        }
    }
    m_stateItems.clear();
    
    // Finally clear anything else in the scene
    m_scene->clear();
}

// Add this new method to position only new states
void MainWindow::layoutNewStateElements(const QSet<QString>& newStateIds)
{
    if (newStateIds.isEmpty()) return;
    
    // Calculate bounding rectangle of existing states
    QRectF existingBounds;
    bool hasExisting = false;
    
    for (auto it = m_stateItems.begin(); it != m_stateItems.end(); ++it) {
        // Skip new states when calculating existing bounds
        if (!newStateIds.contains(QString::fromStdString(it.key()))) {
            QPointF pos = it.value()->pos();
            QSizeF size(StateItem::RADIUS * 2, StateItem::RADIUS * 2);
            QRectF itemRect(pos - QPointF(StateItem::RADIUS, StateItem::RADIUS), size);
            
            if (!hasExisting) {
                existingBounds = itemRect;
                hasExisting = true;
            } else {
                existingBounds = existingBounds.united(itemRect);
            }
        }
    }
    
    // Position new states around existing ones
    if (hasExisting) {
        // Place new states evenly around the perimeter of existing states
        qreal perimeter = 2 * (existingBounds.width() + existingBounds.height());
        qreal spacing = perimeter / (newStateIds.size() + 1);
        qreal padding = 100; // Distance from existing states
        
        int i = 0;
        for (const QString& idStr : newStateIds) {
            std::string id = idStr.toStdString();  // Convert back to std::string
            auto* item = m_stateItems[id];
            if (!item) continue;
            
            // Calculate position along perimeter
            qreal pos = spacing * (i + 1);
            QPointF newPos;
            
            if (pos < existingBounds.width()) {
                // Top edge
                newPos = QPointF(
                    existingBounds.left() + pos,
                    existingBounds.top() - padding
                );
            } else if (pos < existingBounds.width() + existingBounds.height()) {
                // Right edge
                newPos = QPointF(
                    existingBounds.right() + padding,
                    existingBounds.top() + (pos - existingBounds.width())
                );
            } else if (pos < 2 * existingBounds.width() + existingBounds.height()) {
                // Bottom edge
                newPos = QPointF(
                    existingBounds.right() - (pos - existingBounds.width() - existingBounds.height()),
                    existingBounds.bottom() + padding
                );
            } else {
                // Left edge
                newPos = QPointF(
                    existingBounds.left() - padding,
                    existingBounds.bottom() - (pos - 2 * existingBounds.width() - existingBounds.height())
                );
            }
            
            item->setPos(newPos);
            i++;
        }
    } else {
        // Place new states in a circle
        const qreal radius = 150.0;
        int i = 0;
        int totalNew = newStateIds.size();
        
        for (const QString& idStr : newStateIds) {
            std::string id = idStr.toStdString();  // Convert back to std::string
            auto* item = m_stateItems[id];
            if (!item) continue;
            
            qreal angle = 2.0 * M_PI * i / totalNew;
            qreal x = radius * std::cos(angle);
            qreal y = radius * std::sin(angle);
            item->setPos(x, y);
            i++;
        }
    }
}

// Add this method for updating state visuals
void MainWindow::updateStateVisual(int stateIndex) {
    const auto& state = m_doc.states[stateIndex];
    std::string stateId = state.id;
    
    // We need to rebuild the visualization when state IDs change
    // since transitions need to be reconnected to the renamed state
    visualizeFsm();
}

// Add this method to update transition visuals
void MainWindow::updateTransitionVisual(int transitionIndex) {
    const auto& transition = m_doc.transitions[transitionIndex];
    const std::string fromId = transition.from;
    const std::string toId = transition.to;
    
    // Check if we need to rebuild due to endpoint changes
    bool endpointChanged = true;
    
    // First, try to find any matching transition by index (not by endpoints)
    // This assumes transitions and transition items are in the same order
    if (transitionIndex < m_transitionItems.size()) {
        auto* item = m_transitionItems[transitionIndex];
        if (item && item->fromItem() && item->toItem()) {
            // If either endpoint changed, rebuild the whole visual
            if (item->fromItem()->stateId().toStdString() != fromId || 
                item->toItem()->stateId().toStdString() != toId) {
                // Endpoints changed, need full rebuild
                visualizeFsm();
                return;
            } else {
                // Same endpoints, just update properties
                endpointChanged = false;
                
                // Update transition properties
                QString delayText;
                if (!transition.delay_ms.is_null()) {
                    if (transition.delay_ms.is_number_integer()) {
                        delayText = QString::number(transition.delay_ms.get<int>()) + "ms";
                    } else {
                        delayText = QString::fromStdString(transition.delay_ms.dump());
                    }
                }
                
                // Update the visual properties
                item->setTrigger(QString::fromStdString(transition.trigger));
                item->setGuard(QString::fromStdString(transition.guard));
                item->setDelay(delayText);
                item->update(); // Force repaint
            }
        }
    }
    
    // If endpoints changed or we couldn't find a matching item, rebuild the visualization
    if (endpointChanged) {
        visualizeFsm();
    }
}
