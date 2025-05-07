/**
 * @file   mainwindow.hpp
 * @brief  Defines the main application window for the FSM editor and visualizer.
 *         Provides an integrated environment for creating, editing, visualizing,
 *         and running finite state machines.
 *
 * The MainWindow class integrates multiple components:
 * - Project explorer tree for FSM elements management
 * - Graphical FSM editor with interactive state/transition visualization
 * - Property editor for modifying FSM element attributes
 * - Runtime monitor for observing FSM execution
 * - File I/O capabilities for loading/saving FSM definitions
 *
 * @author Martin Ševčík (xsevcim00)
 * @author Jakub Lůčný (xlucnyj00)
 * @date   2025-05-06
 */
#pragma once

#include <QMainWindow>
#include <memory>
#include <QString>
#include <QTreeWidgetItem>
#include <QTimer>
#include <QLabel>
// #include "ui_mainwindow.h"
#include "../../core/io/runtime_client.hpp"
#include "../../core/persistence.hpp"
#include "../graphics/fsmgraphicsitems.hpp"

// Forward declarations
class RuntimeClient;
class StateSnapshot;

QT_BEGIN_NAMESPACE
namespace Ui { class MainWindow; }
QT_END_NAMESPACE

class QGraphicsScene;

/**
 * @brief Main application window providing FSM editing, visualization and runtime monitoring.
 *
 * Manages the complete FSM lifecycle including creation, editing, persistence,
 * visualization, and runtime execution. Combines multiple UI components including
 * a project tree, graphical editor, property panel, and monitoring tables.
 */
class MainWindow : public QMainWindow {
    Q_OBJECT

public:
    /**
     * @brief Constructs the main window and initializes all UI components.
     * @param parent Parent widget
     */
    explicit MainWindow(QWidget* parent = nullptr);
    
    /**
     * @brief Destroys the main window and cleans up resources.
     */
    ~MainWindow() override;

    /**
     * @brief Appends a message to the console/log area with timestamp.
     * @param text The message to append
     */
    void appendToConsole(const QString& text);

private slots:
    /**
     * @brief Establishes connection to the FSM runtime.
     */
    void on_actionConnect_triggered();
    
    /**
     * @brief Disconnects from the FSM runtime.
     */
    void on_actionDisconnect_triggered();
    
    /**
     * @brief Builds and runs the current FSM definition.
     * 
     * Saves the current FSM to a temporary file and launches the interpreter,
     * then connects to it for monitoring state changes.
     */
    void on_actionBuildRun_triggered();

    /**
     * @brief Creates a new empty FSM definition.
     */
    void on_actionNew_triggered();
    
    /**
     * @brief Opens an existing FSM definition from file.
     */
    void on_actionOpen_triggered();
    
    /**
     * @brief Saves the current FSM definition to its existing filepath.
     * 
     * If no filepath is set, delegates to on_actionSaveAs_triggered().
     */
    void on_actionSave_triggered();
    
    /**
     * @brief Saves the current FSM definition to a user-selected filepath.
     */
    void on_actionSaveAs_triggered();
    
    /**
     * @brief Shows dialog to create and add a new state to the FSM.
     */
    void addState();
    
    /**
     * @brief Shows dialog to create and add a new transition to the FSM.
     */
    void addTransition();
    
    /**
     * @brief Shows dialog to create and add a new variable to the FSM.
     */
    void addVariable();
    
    /**
     * @brief Shows dialog to create and add a new input to the FSM.
     */
    void addInput();
    
    /**
     * @brief Shows dialog to create and add a new output to the FSM.
     */
    void addOutput();

    /**
     * @brief Deletes the selected FSM element after confirmation.
     * 
     * @param category Category of the item ("States", "Transitions", etc.)
     * @param index Index of the item within its category
     */
    void deleteSelectedItem(const QString& category, int index);
    
    /**
     * @brief Handles window close event to ensure proper cleanup.
     * @param ev Close event
     */
    void closeEvent(QCloseEvent *ev) override;
    
    /**
     * @brief Gracefully shuts down the runtime interpreter and communication channel.
     */
    void shutdownInterpreterAndChannel();
    
    /**
     * @brief Processes a state snapshot received from the runtime.
     * 
     * Updates the monitoring tables, highlights the current state in the visualization,
     * and shows active transitions.
     * 
     * @param snap The state snapshot containing current state, variables, inputs and outputs
     */
    void handleStateSnapshot(const StateSnapshot& snap);
    
    /**
     * @brief Handles when a user edits an input value in the monitoring table.
     * @param row Row index of the changed cell
     * @param column Column index of the changed cell
     */
    void handleInputCellChanged(int row, int column);
    
    /**
     * @brief Updates the property editor when tree selection changes.
     */
    void on_projectTree_itemSelectionChanged();
    
    /**
     * @brief Handles when a user edits a variable value in the monitoring table.
     * @param row Row index of the changed cell
     * @param column Column index of the changed cell
     */
    void handleVariableCellChanged(int row, int column);

private:
    StateSnapshot m_lastSnapshot;                    ///< Most recent state snapshot from runtime
    Ui::MainWindow* ui;                              ///< Generated UI components
    std::unique_ptr<QGraphicsScene> m_scene;         ///< Scene for FSM visualization
    std::unique_ptr<RuntimeClient> m_runtime;        ///< Client for FSM runtime communication
    bool m_connected = false;                        ///< Connection state flag
    QMap<QString,QString> m_accumulatedInputs;       ///< Input values collected between snapshots
    core_fsm::persistence::FsmDocument m_doc;        ///< Current FSM document model
    QString m_currentFsmPath;                        ///< Path to the currently loaded FSM file
    QProcess* m_interpreter = nullptr;               ///< External FSM runtime interpreter process
    QLabel* m_warningBar{nullptr};                   ///< Bar for displaying warnings and errors
    bool m_receivedFirstSnapshot = false;            ///< Whether first runtime state was received
    std::string m_prevStateId;                       ///< Previous state ID for transition highlighting
    
    /**
     * @brief Updates project tree with current FSM document contents.
     */
    void populateProjectTree();
    
    /**
     * @brief Updates monitoring tables with latest state snapshot.
     * @param snap The state snapshot to display
     */
    void updateMonitor(const StateSnapshot& snap);

    /**
     * @brief Clears all fields in the property editor.
     */
    void clearPropertyEditor();
    
    QTimer* m_reconnectTimer{nullptr};               ///< Timer for connection retry
    bool m_receivedState{false};                     ///< Whether any state was received
    int m_lastRowIndex{-1};                          ///< Last selected row index
    
    /**
     * @brief Creates or updates visualization elements for the current FSM.
     * 
     * Constructs state and transition graphics items and positions them
     * in the graphics scene according to the FSM document.
     */
    void visualizeFsm();
    
    /**
     * @brief Removes all visualization elements from the graphics scene.
     */
    void clearFsmVisualization();
    
    /**
     * @brief Arranges newly created state items in the visualization.
     * 
     * Places new states in a visually appealing way, either in a circle
     * or around existing states.
     * 
     * @param newStateIds Set of IDs of states that need positioning
     */
    void layoutNewStateElements(const QSet<QString>& newStateIds);

    /**
     * @brief Handles events for implementing zoom and pan in the graphics view.
     * @param obj Object that received the event
     * @param ev The event
     * @return true if event was handled, false otherwise
     */
    bool eventFilter(QObject* obj, QEvent* ev) override;
    
    /**
     * @brief Handles window state change events.
     * @param e The event
     */
    void changeEvent(QEvent* e) override;
    
    QMap<std::string, StateItem*> m_stateItems;      ///< Maps state IDs to graphical items
    QList<TransitionItem*> m_transitionItems;        ///< List of all transition graphical items

    /**
     * @brief Updates visual representation of a transition after property changes.
     * @param transitionIndex Index of the transition to update
     */
    void updateTransitionVisual(int transitionIndex);
    
    /**
     * @brief Updates visual representation of a state after property changes.
     * @param stateIndex Index of the state to update
     */
    void updateStateVisual(int stateIndex);
    
    /**
     * @brief Updates tree item text for a variable after its properties change.
     * @param index Index of the variable in the document
     * @param item Tree widget item to update
     */
    void updateVariableTreeItem(int index, QTreeWidgetItem* item);

    /**
     * @brief Gets formatted display text for the project name.
     * @return Formatted name string
     */
    QString getNameDisplayText() const;
    
    /**
     * @brief Gets formatted display text for the project comment.
     * @return Formatted comment string
     */
    QString getCommentDisplayText() const; 
    
    /**
     * @brief Updates tree item text for project name.
     * @param item Tree widget item to update
     */
    void updateNameTreeItem(QTreeWidgetItem* item);
    
    /**
     * @brief Updates tree item text for project comment.
     * @param item Tree widget item to update
     */
    void updateCommentTreeItem(QTreeWidgetItem* item);
};
