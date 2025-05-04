#include "mainwindow.hpp"
#include "ui_mainwindow.h"
#include <QTimer>
#include <QFileDialog>
#include <QMessageBox>
#include <QProcess>
#include <QDateTime>

#include "runtime_client.hpp"
#include "fsmgraphicsitems.hpp"

MainWindow::MainWindow(QWidget* parent)
  : QMainWindow(parent)
  , ui(new Ui::MainWindow)
{
    ui->setupUi(this);
    this->resize(1600, 1000);
    
    for (auto *tbl : { ui->tableInputs,
            ui->tableVariables,
            ui->tableOutputs })
    {
    tbl->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);
    }

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
    
    // Create warning bar
    m_warningBar = new QLabel(this);
    m_warningBar->setFrameStyle(QFrame::Panel | QFrame::Sunken);
    m_warningBar->setStyleSheet("background-color: #FFECB3; color: #9C6500; padding: 8px;");
    m_warningBar->setWordWrap(true);
    m_warningBar->setVisible(false);
    ui->centralSplitter->insertWidget(1, m_warningBar);

    // ui->mainToolBar->hide();
    ui->horizontalSplitter->setSizes({300, 600, 400});
    ui->horizontalSplitter->setStretchFactor(0, 0);  // tabs: fixed
    ui->horizontalSplitter->setStretchFactor(1, 1);  // diagram: stretch
    ui->horizontalSplitter->setStretchFactor(2, 0);  // properties: fixed
    ui->centralSplitter->setStretchFactor(0, 1);  // diagram grows
    ui->centralSplitter->setStretchFactor(1, 0);  // warningBar fixed
    ui->centralSplitter->setStretchFactor(2, 0);  // codeE

    // diagram scene stub
    m_scene = std::make_unique<QGraphicsScene>(this);
    ui->graphicsViewDiagram->setScene(m_scene.get());

    // → wire selection changes in the tree to our new slot
    connect(ui->projectTree,
            &QTreeWidget::itemSelectionChanged,
            this,
            &MainWindow::on_projectTree_itemSelectionChanged);

    ui->actionDisconnect->setEnabled(false);
    m_warningBar->setContentsMargins(1,1,3,3);
    // Populate the project tree on startup
    populateProjectTree();

    connect(ui->addStateButton, &QPushButton::clicked, this, &MainWindow::addState);
    connect(ui->addTransitionButton, &QPushButton::clicked, this, &MainWindow::addTransition);
    connect(ui->addVariableButton, &QPushButton::clicked, this, &MainWindow::addVariable);
    connect(ui->addInputButton, &QPushButton::clicked, this, &MainWindow::addInput);
    connect(ui->addOutputButton, &QPushButton::clicked, this, &MainWindow::addOutput);

    // Ensure the input table is connected for editing
    connect(ui->tableInputs, &QTableWidget::cellChanged, 
            this, &MainWindow::handleInputCellChanged);

    // TODO: this might need some fixing
    // Connect signals for interactive scene
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
                    QTreeWidgetItem* statesRoot = ui->projectTree->topLevelItem(4); // States category (was 3)
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
            QTreeWidgetItem* transRoot = ui->projectTree->topLevelItem(5); // Transitions category (was 4)
            if (!transRoot) return;
            
            // Look for a transition with matching from/to states
            for (int i = 0; i < m_doc.transitions.size(); i++) {
                const auto& transition = m_doc.transitions[i];
                if (transition.from == fromStateId && transition.to == toStateId) {
                    // We need to be careful if there are multiple transitions between the same states
                    // We can check other properties like trigger, guard, etc. if needed
                    if (i < transRoot->childCount()) {
                        ui->projectTree->setCurrentItem(transRoot->child(i));
                        break;
                    }
                }
            }
        }
    });

    // Track when we ever see a first state packet
    m_receivedState = true;

    // Fire once, 1 s after Build&Run, if no state has arrived
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
    

    
    // Zoom & pan: install event filter on the GraphicsView viewport
    ui->graphicsViewDiagram->setTransformationAnchor(
        QGraphicsView::AnchorUnderMouse
    );
    ui->graphicsViewDiagram->viewport()->installEventFilter(this);
}

MainWindow::~MainWindow()
{
    shutdownInterpreterAndChannel();   // safety‑net for “quit” menu etc.
    delete ui;
}

void MainWindow::closeEvent(QCloseEvent *ev)
{
    shutdownInterpreterAndChannel();
    QMainWindow::closeEvent(ev);   // default cleanup
}

// -----------------------------------------------------------------------------
// Core actions (wired up in your ctor but missing implementations)
// -----------------------------------------------------------------------------

void MainWindow::on_actionConnect_triggered()
{
    if (m_runtime) return;
    m_runtime = std::make_unique<RuntimeClient>(
                    "0.0.0.0:45455",
                    "127.0.0.1:45454",
                    this);
    connect(m_runtime.get(), &RuntimeClient::stateReceived,
            this,            &MainWindow::handleStateSnapshot);
            
    // Add this new connection for logging
    connect(m_runtime.get(), &RuntimeClient::logMessage,
            this,            &MainWindow::appendToConsole);
            
    m_runtime->start();

    ui->actionConnect   ->setEnabled(false);
    ui->actionDisconnect->setEnabled(true);
    // ui->buttonInject    ->setEnabled(true);
}

void MainWindow::on_actionDisconnect_triggered()
{
    // 1) tell the runtime client to shut itself down
    if (m_runtime) {
        m_runtime->stop();
        m_runtime.reset();
    }

    // (we no longer kill the interpreter—only the channel)
    ui->actionDisconnect ->setEnabled(false);
    ui->actionConnect    ->setEnabled(true);
    // ui->buttonInject     ->setEnabled(false);
}

void MainWindow::on_actionBuildRun_triggered()
{
    // 1) Block if there are semantic errors
    if (m_warningBar->isVisible()) {
        QMessageBox::warning(this, tr("Cannot Run FSM"),
                             tr("Your FSM JSON has semantic errors:\n%1\n\n"
                                "Please fix them before running.")
                             .arg(m_warningBar->text()));
        return;
    }
    m_warningBar->clear();
    m_warningBar->setVisible(false);

    // 2) Save the FSM JSON
    QString tmp = QDir::temp().filePath("current.fsm.json");
    core_fsm::persistence::saveFile(m_doc, tmp.toStdString(), /*pretty*/true, nullptr);

    // 3) If there’s still an old runtime, ask it to shut down...
    if (m_runtime) {
        m_runtime->shutdown();
        m_runtime.reset();
    }

    // 4) And if there’s an old QProcess, terminate/kill it
    if (m_interpreter) {
        m_interpreter->terminate();
        if (!m_interpreter->waitForFinished(500))
            m_interpreter->kill();
        m_interpreter->deleteLater();
        m_interpreter = nullptr;
    }

    // 5) Launch the fresh interpreter
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
                              tr("Could not start “%1”").arg(exe));
        m_interpreter->deleteLater();
        m_interpreter = nullptr;
        return;
    }

    // 6) Finally, hook up your GUI to it as before
    on_actionConnect_triggered();
    m_receivedState = false;
    m_reconnectTimer->start(1000);
}

bool MainWindow::eventFilter(QObject* obj, QEvent* ev) {
    if (obj == ui->graphicsViewDiagram->viewport()) {
        // Zoom (⇧+wheel) – already in place
        if (ev->type() == QEvent::Wheel) {
            // ...
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

void MainWindow::changeEvent(QEvent* e) {
    QMainWindow::changeEvent(e);
    // Empty implementation to satisfy the linker
}

// Add a new method to write to the console
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
