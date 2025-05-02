#pragma once

#include <QMainWindow>
#include <memory>
#include <QString>
#include <QTreeWidgetItem>
#include <QTimer>

// #include "ui_mainwindow.h"
#include "runtime_client.hpp"
#include "../persistence.hpp"
#include "fsmgraphicsitems.hpp"

// Forward declarations
class RuntimeClient;
class StateSnapshot;

QT_BEGIN_NAMESPACE
namespace Ui { class MainWindow; }
QT_END_NAMESPACE

class QGraphicsScene;

class MainWindow : public QMainWindow {
    Q_OBJECT

public:
    explicit MainWindow(QWidget* parent = nullptr);
    ~MainWindow() override;

private slots:
    // from step 1…
    void on_actionConnect_triggered();
    void on_actionDisconnect_triggered();
    void on_buttonInject_clicked();
    void on_actionGenerateCode_triggered();
    void on_actionBuildRun_triggered();

    // **new** file I/O slots
    void on_actionOpen_triggered();
    void on_actionSave_triggered();
    void on_actionSaveAs_triggered();
    void on_actionNew_triggered();

    void addState();
    void addTransition();

    // runtime snapshot
    void handleStateSnapshot(const StateSnapshot& snap);

    void on_projectTree_itemSelectionChanged();

private:
    Ui::MainWindow*           ui;
    std::unique_ptr<QGraphicsScene> m_scene;
    std::unique_ptr<RuntimeClient>  m_runtime;
    bool                            m_connected = false;
    QMap<QString,QString> m_accumulatedInputs;
    // **new**: loaded FSM document & path
    core_fsm::persistence::FsmDocument m_doc;
    QString                             m_currentFsmPath;

    // **new**: rebuild the tree
    void populateProjectTree();

    // monitor update
    void updateMonitor(const StateSnapshot& snap);

    void clearPropertyEditor();
    QTimer* m_reconnectTimer{nullptr};
    bool    m_receivedState{false};
    int     m_lastRowIndex{-1};
    // FSM visualization
    void visualizeFsm();
    void clearFsmVisualization();
    void layoutFsmElements();

    // Add these method declarations if they don't exist:
    bool eventFilter(QObject* obj, QEvent* ev) override;
    void changeEvent(QEvent* e) override;
    // Keep track of visualization elements
    QMap<std::string, StateItem*> m_stateItems;
    QList<TransitionItem*> m_transitionItems;
};
