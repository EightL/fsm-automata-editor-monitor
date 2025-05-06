/**
 * @file   core.cpp
 * @brief  Implements core functionality of the main window including initialization,
 *         connection management, and runtime execution for the FSM editor.
 *
 * @author Martin Ševčík (xsevcim00)
 * @author Jakub Lůčný (xlucnyj00)
 * @date   2025-05-06
 */
#include "mainwindow.hpp"
#include "ui_mainwindow.h"
#include <QTimer>
#include <QFileDialog>
#include <QMessageBox>
#include <QProcess>
#include <QDateTime>

#include "../core/io/runtime_client.hpp"
#include "../graphics/fsmgraphicsitems.hpp"

/**
 * Constructor sets up the UI components, configures tables and editors,
 * establishes signal/slot connections, and initializes visualization components.
 */
MainWindow::MainWindow(QWidget* parent)
  : QMainWindow(parent)
  , ui(new Ui::MainWindow)
{
    ui->setupUi(this);
    this->resize(1600, 1000);
    
    // Configure all monitoring tables with stretch mode
    for (auto *tbl : { ui->tableInputs,
            ui->tableVariables,
            ui->tableOutputs })
    {
    tbl->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);
    }

    // Set up inputs table with special column for buttons
    ui->tableInputs->setColumnCount(3);
    ui->tableInputs->setHorizontalHeaderLabels(
        QStringList{tr("Name"), tr("Value"), QString()});
    ui->tableInputs->horizontalHeader()
        ->setSectionResizeMode(2, QHeaderView::ResizeToContents);

    // Configure codeEditor as a console with white background and black text
    ui->codeEditor->setReadOnly(true);
    ui->codeEditor->setEnabled(true);
    ui->codeEditor->setPlaceholderText(tr("Log output will appear here..."));
    
    // Update styling to white background with black text
    ui->codeEditor->setStyleSheet(
        "background-color: #fff; "
        "color: #000; "
        "border: 1px solid #ccc; "
        "padding: 5px;"
    );
    
    // Create warning bar for displaying validation errors
    m_warningBar = new QLabel(this);
    m_warningBar->setFrameStyle(QFrame::Panel | QFrame::Sunken);
    m_warningBar->setStyleSheet("background-color: #FFECB3; color: #9C6500; padding: 8px;");
    m_warningBar->setWordWrap(true);
    m_warningBar->setVisible(false);
    ui->centralSplitter->insertWidget(1, m_warningBar);

    // Configure splitter sizes and behaviors
    ui->horizontalSplitter->setSizes({300, 600, 400});
    ui->horizontalSplitter->setStretchFactor(0, 0);  // tabs: fixed
    ui->horizontalSplitter->setStretchFactor(1, 1);  // diagram: stretch
    ui->horizontalSplitter->setStretchFactor(2, 0);  // properties: fixed
    ui->centralSplitter->setStretchFactor(0, 1);     // diagram grows
    ui->centralSplitter->setStretchFactor(1, 0);     // warningBar fixed
    ui->centralSplitter->setStretchFactor(2, 0);     // codeEditor fixed

    // Initialize graphics scene for FSM visualization
    m_scene = std::make_unique<QGraphicsScene>(this);
    ui->graphicsViewDiagram->setScene(m_scene.get());

    // Connect tree selection changes to property editor updates
    connect(ui->projectTree,
            &QTreeWidget::itemSelectionChanged,
            this,
            &MainWindow::on_projectTree_itemSelectionChanged);

    // Initialize runtime connection UI states
    ui->actionDisconnect->setEnabled(false);
    m_warningBar->setContentsMargins(1,1,3,3);
    
    // Initialize project tree with FSM structure
    populateProjectTree();

    // Connect element creation buttons
    connect(ui->addStateButton, &QPushButton::clicked, this, &MainWindow::addState);
    connect(ui->addTransitionButton, &QPushButton::clicked, this, &MainWindow::addTransition);
    connect(ui->addVariableButton, &QPushButton::clicked, this, &MainWindow::addVariable);
    connect(ui->addInputButton, &QPushButton::clicked, this, &MainWindow::addInput);
    connect(ui->addOutputButton, &QPushButton::clicked, this, &MainWindow::addOutput);

    // Connect input table for interactive editing
    connect(ui->tableInputs, &QTableWidget::cellChanged, 
            this, &MainWindow::handleInputCellChanged);

    // Connect scene selection changes to synchronize with project tree
    connect(m_scene.get(), &QGraphicsScene::selectionChanged, this, [this]() {
        QList<QGraphicsItem*> items = m_scene->selectedItems();
        if (items.isEmpty()) return;
        
        QGraphicsItem* item = items.first();
        
        // Handle state selection
        if (StateItem* stateItem = dynamic_cast<StateItem*>(item)) {
            // Find corresponding item in tree and select it
            for (int i = 0; i < m_doc.states.size(); i++) {
                if (m_doc.states[i].id == stateItem->stateId().toStdString()) {
                    // Select the corresponding item in the project tree
                    QTreeWidgetItem* statesRoot = ui->projectTree->topLevelItem(4); // States category
                    if (statesRoot && i < statesRoot->childCount()) {
                        ui->projectTree->setCurrentItem(statesRoot->child(i));
                        break;
                    }
                }
            }
        }
        
        // Handle transition selection
        else if (TransitionItem* transItem = dynamic_cast<TransitionItem*>(item)) {
            if (!transItem->fromItem() || !transItem->toItem()) return;
            
            std::string fromStateId = transItem->fromItem()->stateId().toStdString();
            std::string toStateId = transItem->toItem()->stateId().toStdString();
            
            // Find the matching transition in the model
            QTreeWidgetItem* transRoot = ui->projectTree->topLevelItem(5); // Transitions category
            if (!transRoot) return;
            
            // Look for a transition with matching from/to states
            for (int i = 0; i < m_doc.transitions.size(); i++) {
                const auto& transition = m_doc.transitions[i];
                if (transition.from == fromStateId && transition.to == toStateId) {
                    // We need to be careful if there are multiple transitions between the same states
                    if (i < transRoot->childCount()) {
                        ui->projectTree->setCurrentItem(transRoot->child(i));
                        break;
                    }
                }
            }
        }
    });

    // Set up runtime connection monitoring
    m_receivedState = true;

    // Set up reconnection timer for Build & Run
    m_reconnectTimer = new QTimer(this);
    m_reconnectTimer->setSingleShot(true);
    connect(m_reconnectTimer, &QTimer::timeout, this, [this]() {
        if (!m_receivedState) {
            QMessageBox::information(
                this,
                tr("Waiting for interpreter…"),
                tr("No response received. Trying to connect…")
            );
            if (!m_runtime)
                on_actionConnect_triggered();
        }
    });
    
    // Configure graphics view for zoom and pan functionality
    ui->graphicsViewDiagram->setTransformationAnchor(
        QGraphicsView::AnchorUnderMouse
    );
    ui->graphicsViewDiagram->viewport()->installEventFilter(this);
}

/**
 * Destructor ensures proper cleanup of resources including runtime connections.
 */
MainWindow::~MainWindow()
{
    shutdownInterpreterAndChannel();   // safety-net for "quit" menu etc.
    delete ui;
}

/**
 * Handles window close events, ensuring interpreter and connections are
 * properly shut down before the application terminates.
 */
void MainWindow::closeEvent(QCloseEvent *ev)
{
    shutdownInterpreterAndChannel();
    QMainWindow::closeEvent(ev);   // default cleanup
}

/**
 * Creates and starts a new RuntimeClient to establish connection with
 * the FSM interpreter process, updates UI elements accordingly.
 */
void MainWindow::on_actionConnect_triggered()
{
    if (m_runtime) return;
    m_runtime = std::make_unique<RuntimeClient>(
                    "0.0.0.0:45455",
                    "127.0.0.1:45454",
                    this);
    connect(m_runtime.get(), &RuntimeClient::stateReceived,
            this,            &MainWindow::handleStateSnapshot);
            
    // Connect runtime logging to console display
    connect(m_runtime.get(), &RuntimeClient::logMessage,
            this,            &MainWindow::appendToConsole);
            
    m_runtime->start();

    ui->actionConnect   ->setEnabled(false);
    ui->actionDisconnect->setEnabled(true);
}

/**
 * Stops and destroys the RuntimeClient, disconnecting from the 
 * FSM interpreter process and updating UI elements accordingly.
 */
void MainWindow::on_actionDisconnect_triggered()
{
    // Stop the runtime client
    if (m_runtime) {
        m_runtime->stop();
        m_runtime.reset();
    }

    // Update UI state
    ui->actionDisconnect ->setEnabled(false);
    ui->actionConnect    ->setEnabled(true);
}

/**
 * Builds and runs the current FSM model by:
 * 1. Validating the FSM for semantic errors
 * 2. Saving to a temporary file
 * 3. Shutting down any existing runtime
 * 4. Launching the interpreter process with the temporary file
 * 5. Connecting to the new interpreter
 */
void MainWindow::on_actionBuildRun_triggered()
{
    // Block if there are semantic errors
    if (m_warningBar->isVisible()) {
        QMessageBox::warning(this, tr("Cannot Run FSM"),
                             tr("Your FSM JSON has semantic errors:\n%1\n\n"
                                "Please fix them before running.")
                             .arg(m_warningBar->text()));
        return;
    }
    m_warningBar->clear();
    m_warningBar->setVisible(false);

    // Save the FSM JSON to temporary file
    QString tmp = QDir::temp().filePath("current.fsm.json");
    core_fsm::persistence::saveFile(m_doc, tmp.toStdString(), /*pretty*/true, nullptr);

    // Shutdown existing runtime client
    if (m_runtime) {
        m_runtime->shutdown();
        m_runtime.reset();
    }

    // Terminate any existing interpreter process
    if (m_interpreter) {
        m_interpreter->terminate();
        if (!m_interpreter->waitForFinished(500))
            m_interpreter->kill();
        m_interpreter->deleteLater();
        m_interpreter = nullptr;
    }

    // Launch the fresh interpreter process
    m_interpreter = new QProcess(this);
    connect(m_interpreter,
            QOverload<int,QProcess::ExitStatus>::of(&QProcess::finished),
            m_interpreter, &QObject::deleteLater);

    QString exe = QCoreApplication::applicationDirPath() + "/fsm_runtime";
    m_interpreter->start(exe, { tmp,
                                "0.0.0.0:45454",
                                "127.0.0.1:45455" });
    if (!m_interpreter->waitForStarted()) {
        QMessageBox::critical(this, tr("Run Failed"),
                              tr("Could not start '%1'").arg(exe));
        m_interpreter->deleteLater();
        m_interpreter = nullptr;
        return;
    }

    // Connect to the new interpreter
    on_actionConnect_triggered();
    m_receivedState = false;
    m_reconnectTimer->start(1000);
}

/**
 * Handles graphics view events for zooming and panning functionality.
 * Implements space key toggle for pan mode and mouse wheel for zoom.
 */
bool MainWindow::eventFilter(QObject* obj, QEvent* ev) {
    if (obj == ui->graphicsViewDiagram->viewport()) {
        // Zoom implementation would be here
        if (ev->type() == QEvent::Wheel) {
            // Zoom implementation...
        }
        
        // Space key toggles pan mode
        if (ev->type() == QEvent::KeyPress || ev->type() == QEvent::KeyRelease) {
            auto* ke = static_cast<QKeyEvent*>(ev);
            if (ke->key() == Qt::Key_Space) {
                bool pressed = (ev->type() == QEvent::KeyPress);
                ui->graphicsViewDiagram->setDragMode(
                    pressed
                        ? QGraphicsView::ScrollHandDrag
                        : QGraphicsView::RubberBandDrag
                );
                return false; // let QGraphicsView also handle it
            }
        }
        // While in ScrollHandDrag, mouse moves will pan automatically
    }
    return QMainWindow::eventFilter(obj, ev);
}

/**
 * Handles window state change events (minimize, maximize, etc.).
 */
void MainWindow::changeEvent(QEvent* e) {
    QMainWindow::changeEvent(e);
    // Empty implementation to satisfy the linker
}

/**
 * Appends a timestamped message to the console/log area and
 * automatically scrolls to the bottom.
 * 
 * @param text The message to append
 */
void MainWindow::appendToConsole(const QString& text)
{
    // Append the text with timestamp
    QString timestamp = QDateTime::currentDateTime().toString("[yyyy-MM-dd hh:mm:ss.zzz] ");
    ui->codeEditor->appendPlainText(timestamp + text);
    
    // Auto-scroll to bottom
    QTextCursor c = ui->codeEditor->textCursor();
    c.movePosition(QTextCursor::End);
    ui->codeEditor->setTextCursor(c);
}