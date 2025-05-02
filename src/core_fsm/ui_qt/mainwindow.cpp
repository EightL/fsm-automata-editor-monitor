#include "mainwindow.hpp"
#include "ui_mainwindow.h"

#include <QFileDialog>
#include <QMessageBox>
#include <QTableWidgetItem>
#include <QProcess>
#include <QLineEdit>
#include <QCheckBox>
#include <QPlainTextEdit>
#include <QComboBox>
#include <iostream>
#include <cmath>

#include <QDialog>
#include <QFormLayout>
#include <QVBoxLayout>
#include <QDialogButtonBox>
#include <QInputDialog>

#include "runtime_client.hpp"
#include "fsmgraphicsitems.hpp"

MainWindow::MainWindow(QWidget* parent)
  : QMainWindow(parent)
  , ui(new Ui::MainWindow)
{
    ui->setupUi(this);

    // diagram scene stub
    m_scene = std::make_unique<QGraphicsScene>(this);
    ui->graphicsViewDiagram->setScene(m_scene.get());

    // wire up core actions
    connect(ui->actionConnect,    &QAction::triggered, this, &MainWindow::on_actionConnect_triggered);
    connect(ui->actionDisconnect, &QAction::triggered, this, &MainWindow::on_actionDisconnect_triggered);
    connect(ui->buttonInject,     &QPushButton::clicked, this, &MainWindow::on_buttonInject_clicked);
    connect(ui->actionGenerateCode, &QAction::triggered, this, &MainWindow::on_actionGenerateCode_triggered);
    connect(ui->actionBuildRun,     &QAction::triggered, this, &MainWindow::on_actionBuildRun_triggered);

    // wire up file menu
    connect(ui->actionOpen,  &QAction::triggered, this, &MainWindow::on_actionOpen_triggered);
    connect(ui->actionSave,  &QAction::triggered, this, &MainWindow::on_actionSave_triggered);
    connect(ui->actionSaveAs,&QAction::triggered, this, &MainWindow::on_actionSaveAs_triggered);
    connect(ui->actionNew, &QAction::triggered, this, &MainWindow::on_actionNew_triggered);

    // → wire selection changes in the tree to our new slot
    connect(ui->projectTree,
            &QTreeWidget::itemSelectionChanged,
            this,
            &MainWindow::on_projectTree_itemSelectionChanged);

    ui->actionDisconnect->setEnabled(false);
    
    // Populate the project tree on startup
    populateProjectTree();

    connect(ui->addStateButton, &QPushButton::clicked, this, &MainWindow::addState);
    connect(ui->addTransitionButton, &QPushButton::clicked, this, &MainWindow::addTransition);

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
                    QTreeWidgetItem* statesRoot = ui->projectTree->topLevelItem(2); // States category
                    ui->projectTree->setCurrentItem(statesRoot->child(i));
                    break;
                }
            }
        }
        
        // Handle transition selection - similar approach
    });
}

MainWindow::~MainWindow() {
    if (m_runtime) {
        m_runtime.reset();
    }
    delete ui;
}

// ————— File → Open —————
void MainWindow::on_actionOpen_triggered() {
    QString path = QFileDialog::getOpenFileName(
        this,
        tr("Open FSM…"),
        QString(),
        tr("FSM JSON Files (*.json)")
    );
    if (path.isEmpty()) return;

    core_fsm::persistence::FsmDocument doc;
    std::string err;
    if (!core_fsm::persistence::loadFile(path.toStdString(), doc, &err)) {
        QMessageBox::critical(this,
                              tr("Error Loading FSM"),
                              tr("Failed to load “%1”:\n%2")
                                .arg(path, QString::fromStdString(err)));
        return;
    }

    m_doc = std::move(doc);
    m_currentFsmPath = path;
    populateProjectTree();
    visualizeFsm();
}

// ————— File → Save —————
void MainWindow::on_actionSave_triggered() {
    if (m_currentFsmPath.isEmpty()) {
        return on_actionSaveAs_triggered();
    }

    std::string err;
    if (!core_fsm::persistence::saveFile(m_doc,
          m_currentFsmPath.toStdString(),
          /*pretty*/true, &err))
    {
        QMessageBox::critical(this,
                              tr("Error Saving FSM"),
                              tr("Failed to save “%1”:\n%2")
                                .arg(m_currentFsmPath, QString::fromStdString(err)));
    }
}

// ————— File → Save As… —————
void MainWindow::on_actionSaveAs_triggered() {
    QString path = QFileDialog::getSaveFileName(
        this,
        tr("Save FSM As…"),
        QString(),
        tr("FSM JSON Files (*.json)")
    );
    if (path.isEmpty()) return;

    m_currentFsmPath = path;
    on_actionSave_triggered();
}

// ————— rebuild the left-hand tree —————
void MainWindow::populateProjectTree() {
    ui->projectTree->clear();

    // Inputs
    auto *inRoot = new QTreeWidgetItem({ tr("Inputs") });
    for (auto const& in : m_doc.inputs) {
        inRoot->addChild(new QTreeWidgetItem({ QString::fromStdString(in) }));
    }
    ui->projectTree->addTopLevelItem(inRoot);

    // Outputs
    auto *outRoot = new QTreeWidgetItem({ tr("Outputs") });
    for (auto const& out : m_doc.outputs) {
        outRoot->addChild(new QTreeWidgetItem({ QString::fromStdString(out) }));
    }
    ui->projectTree->addTopLevelItem(outRoot);

    // Variables
    auto *varRoot = new QTreeWidgetItem({ tr("Variables") });
    for (auto const& v : m_doc.variables) {
        QString label = QString::fromStdString(v.name)
                      + " = "
                      + QString::fromStdString(v.init.dump());
        varRoot->addChild(new QTreeWidgetItem({ label }));
    }
    ui->projectTree->addTopLevelItem(varRoot);

    // States
    auto *stRoot = new QTreeWidgetItem({ tr("States") });
    for (auto const& s : m_doc.states) {
        QString label = QString::fromStdString(s.id)
                      + (s.initial ? tr(" (initial)") : QString());
        stRoot->addChild(new QTreeWidgetItem({ label }));
    }
    ui->projectTree->addTopLevelItem(stRoot);

    // Transitions
    auto *trRoot = new QTreeWidgetItem({ tr("Transitions") });
    for (auto const& t : m_doc.transitions) {
        QString label = QString::fromStdString(t.from)
                      + " → "
                      + QString::fromStdString(t.to)
                      + "  [" + QString::fromStdString(t.guard) + "]";
        if (!t.delay_ms.is_null()) {
            if (t.delay_ms.is_number_integer()) {
                // true numeric delay
                label += tr(" @ %1ms")
                         .arg(QString::number(t.delay_ms.get<int>()));
            }
            else {
                // fallback: show whatever JSON the user gave (string, array, etc.)
                label += " @ " + QString::fromStdString(t.delay_ms.dump());
            }
        }
        trRoot->addChild(new QTreeWidgetItem({ label }));
    }
    ui->projectTree->addTopLevelItem(trRoot);

    ui->projectTree->expandAll();
}

void MainWindow::on_actionNew_triggered()
{
    // 0) (optional) ask the user whether to save the current work
    //    if (!maybeSaveChanges()) return; /

    // / 1) reset the DTO ----------------------------------------------/
    m_doc = core_fsm::persistence::FsmDocument{};   // fresh, empty document
    m_doc.name = "untitled";                        // a harmless default

    // 2) forget the path so the next Ctrl‑S falls back to Save As… /
    m_currentFsmPath.clear();

    // 3) refresh every view ----------------------------------------/
    populateProjectTree();   // left‑hand tree
    clearFsmVisualization(); // diagram scene
    ui->labelCurrentState->setText(tr("Current State: -"));
    ui->tableLastValues ->setRowCount(0);
}

// … the rest of your slots (connect/inject/generate/build/run) unchanged …

void MainWindow::handleStateSnapshot(const StateSnapshot& snap) {
    updateMonitor(snap);
}

void MainWindow::updateMonitor(const StateSnapshot& snap) {
    ui->labelCurrentState->setText(
        tr("Current State: %1").arg(snap.state)
    );

    // Highlight current state in the visualization
    std::string currentState = snap.state.toStdString();
    for (const auto& stateId : m_stateItems.keys()) {
        StateItem* item = m_stateItems[stateId];
    
        if (stateId == currentState) {
            item->setBrush(QBrush(Qt::lightGray));
        } else {
            item->setBrush(QBrush(Qt::white));
        }
    }
    
    ui->tableLastValues->clearContents();

    int rows = snap.inputs.size() + snap.vars.size() + snap.outputs.size();
    ui->tableLastValues->setRowCount(rows);
    int row = 0;
    auto fill = [&](auto const& map){
        for (auto it = map.constBegin(); it != map.constEnd(); ++it) {
            ui->tableLastValues->setItem(row, 0,
                new QTableWidgetItem(it.key()));
            ui->tableLastValues->setItem(row, 1,
                new QTableWidgetItem(it.value()));
            ++row;
        }
    };
    fill(snap.inputs);
    fill(snap.vars);
    fill(snap.outputs);
}

void MainWindow::clearPropertyEditor() {
    // remove every row from the QFormLayout
    while (ui->formProperties->rowCount() > 0) {
        ui->formProperties->removeRow(0);
    }
}

void MainWindow::on_projectTree_itemSelectionChanged() {
    clearPropertyEditor();

    auto items = ui->projectTree->selectedItems();
    if (items.isEmpty()) return;

    auto *it = items.first();
    auto *parent = it->parent();
    if (!parent) return;  // top-level ("Inputs", "States", …), nothing to edit

    const QString category = parent->text(0);
    const int index = parent->indexOfChild(it);

    // — Inputs / Outputs: just a single name field
    if (category == tr("Inputs") || category == tr("Outputs")) {
        auto *edit = new QLineEdit(it->text(0));
        ui->formProperties->addRow(tr("Name:"), edit);

        connect(edit, &QLineEdit::editingFinished, this, [this, index, it, edit, category]() {
            const QString newName = edit->text().trimmed();
            if (newName.isEmpty()) return;
            it->setText(0, newName);
            if (category == tr("Inputs"))
                m_doc.inputs[index] = newName.toStdString();
            else
                m_doc.outputs[index] = newName.toStdString();
        });
        return;
    }

    // — Variables: name + initial value
    if (category == tr("Variables")) {
        // name
        auto *nameEdit = new QLineEdit(QString::fromStdString(m_doc.variables[index].name));
        ui->formProperties->addRow(tr("Name:"), nameEdit);
        connect(nameEdit, &QLineEdit::editingFinished, this, [this, index, it, nameEdit]() {
            QString newName = nameEdit->text().trimmed();
            if (newName.isEmpty()) return;
            m_doc.variables[index].name = newName.toStdString();
            it->setText(0, QString::fromStdString(m_doc.variables[index].name) 
                          + " = " 
                          + QString::fromStdString(m_doc.variables[index].init.dump()));
        });

        // init
        auto *initEdit = new QLineEdit(QString::fromStdString(m_doc.variables[index].init.dump()));
        ui->formProperties->addRow(tr("Initial:"), initEdit);
        connect(initEdit, &QLineEdit::editingFinished, this, [this, index, it, initEdit]() {
            m_doc.variables[index].init = nlohmann::json::parse(initEdit->text().toStdString(), nullptr, false);
            it->setText(0, QString::fromStdString(m_doc.variables[index].name) 
                          + " = " 
                          + QString::fromStdString(m_doc.variables[index].init.dump()));
        });
        return;
    }

    // — States: id, is-initial, onEnter
    if (category == tr("States")) {
        // ID
        auto *idEdit = new QLineEdit(QString::fromStdString(m_doc.states[index].id));
        ui->formProperties->addRow(tr("ID:"), idEdit);
        connect(idEdit, &QLineEdit::editingFinished, this, [this, index, it, idEdit]() {
            QString newId = idEdit->text().trimmed();
            if (newId.isEmpty()) return;
            m_doc.states[index].id = newId.toStdString();
            it->setText(0, QString::fromStdString(m_doc.states[index].id) 
                        + (m_doc.states[index].initial ? tr(" (initial)") : QString()));
        });

        // Initial flag
        auto *chk = new QCheckBox;
        chk->setChecked(m_doc.states[index].initial);
        ui->formProperties->addRow(tr("Initial:"), chk);
        connect(chk, &QCheckBox::toggled, this, [this, index, it, chk](bool on) {
            m_doc.states[index].initial = on;
            it->setText(0, QString::fromStdString(m_doc.states[index].id) 
                        + (on ? tr(" (initial)") : QString()));
        });

        // onEnter action
        auto *actionEdit = new QPlainTextEdit(QString::fromStdString(m_doc.states[index].onEnter));
        actionEdit->setPlaceholderText(tr("C/C++ snippet…"));
        ui->formProperties->addRow(tr("On Enter:"), actionEdit);
        connect(actionEdit, &QPlainTextEdit::textChanged, this, [this, index, actionEdit]() {
            m_doc.states[index].onEnter = actionEdit->toPlainText().toStdString();
        });

        // After updating state properties
        if (m_stateItems.contains(m_doc.states[index].id)) {
            m_stateItems[m_doc.states[index].id]->setInitial(m_doc.states[index].initial);
            // Only update the graphics if needed
        }

        return;
    }

    // — Transitions: trigger, guard, delay, from, to
    if (category == tr("Transitions")) {
        auto &trn = m_doc.transitions[index];

        // trigger (input event)
        auto *trigEdit = new QLineEdit(QString::fromStdString(trn.trigger));
        ui->formProperties->addRow(tr("Trigger:"), trigEdit);
        connect(trigEdit, &QLineEdit::editingFinished, this, [this, index, trigEdit]() {
            QString newTrigger = trigEdit->text().trimmed();
            m_doc.transitions[index].trigger = newTrigger.toStdString();
        });

        // guard
        auto *guardEdit = new QLineEdit(QString::fromStdString(trn.guard));
        ui->formProperties->addRow(tr("Guard:"), guardEdit);
        connect(guardEdit, &QLineEdit::editingFinished, this, [this, index, guardEdit]() {
            m_doc.transitions[index].guard = guardEdit->text().toStdString();
        });

        // delay (as JSON)
        auto *delayEdit = new QLineEdit(QString::fromStdString(trn.delay_ms.dump()));
        ui->formProperties->addRow(tr("Delay (ms or var):"), delayEdit);
        connect(delayEdit, &QLineEdit::editingFinished, this, [this, index, delayEdit]() {
            m_doc.transitions[index].delay_ms = nlohmann::json::parse(delayEdit->text().toStdString(), nullptr, false);
        });

        // from/to comboboxes
        QStringList states;
        for (auto const& s : m_doc.states)
            states << QString::fromStdString(s.id);

        auto *fromBox = new QComboBox;
        fromBox->addItems(states);
        fromBox->setCurrentText(QString::fromStdString(trn.from));
        ui->formProperties->addRow(tr("From:"), fromBox);
        connect(fromBox, &QComboBox::currentTextChanged, this, [this, index](const QString &txt){
            m_doc.transitions[index].from = txt.toStdString();
        });

        auto *toBox = new QComboBox;
        toBox->addItems(states);
        toBox->setCurrentText(QString::fromStdString(trn.to));
        ui->formProperties->addRow(tr("To:"), toBox);
        connect(toBox, &QComboBox::currentTextChanged, this, [this, index](const QString &txt){
            m_doc.transitions[index].to = txt.toStdString();
        });

        // After updating transition properties
        for (auto* item : m_transitionItems) {
            item->updatePosition();
        }

        return;
    }
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
    m_runtime->start();

    ui->actionConnect   ->setEnabled(false);
    ui->actionDisconnect->setEnabled(true);
    ui->buttonInject    ->setEnabled(true);
}

void MainWindow::on_actionDisconnect_triggered()
{
    if (!m_runtime) return;
    m_runtime->shutdown();           // ← new public method
    m_runtime.reset();

    ui->actionDisconnect->setEnabled(false);
    ui->actionConnect   ->setEnabled(true);
    ui->buttonInject    ->setEnabled(false);
    ui->labelCurrentState->setText(tr("Current State: -"));
    ui->tableLastValues ->setRowCount(0);
}

void MainWindow::on_buttonInject_clicked()
{
    if (!m_runtime) return;
    const QString name  = ui->lineEditInputName ->text().trimmed();
    const QString value = ui->lineEditInputValue->text();
    if (name.isEmpty()) return;
    m_runtime->inject(name, value);
    ui->lineEditInputValue->clear();
}


void MainWindow::on_actionGenerateCode_triggered()
{
    // 1) dump current FSM to a temp JSON
    QString tmpJson = QDir::temp().filePath("current.fsm.json");
    core_fsm::persistence::saveFile(m_doc,
        tmpJson.toStdString(), true, nullptr);

    // 2) ensure the CMake‐generated folder exists
    QString genDir = QString::fromUtf8((GENERATED_DIR));
    QDir().mkpath(genDir);

    // 3) pick up the codegen program
    QString program = QString::fromUtf8((CODEGEN_EXE));

    // 4) sanity-check it
    QFileInfo fi(program);
    if (!fi.exists() || !(fi.permissions() & QFileDevice::ExeUser)) {
        QMessageBox::critical(this,
            tr("Generation Error"),
            tr("Codegen not found or not executable:\n%1").arg(program));
        return;
    }

    // 5) spawn it
    QProcess proc(this);
    proc.setProgram(program);
    proc.setArguments({ tmpJson, genDir,
                        /* if you still need templates from source dir: */
                        QStringLiteral(SRC_TEMPLATE_DIR) 
                      });

    proc.start();
    printf("Codegen: %s %s\n", program.toStdString().c_str(),
           genDir.toStdString().c_str());
    printf("argumetns: %s\n", proc.arguments().join(" ").toStdString().c_str());
    if (!proc.waitForStarted(2000)) {
        QMessageBox::critical(this,
            tr("Generation Error"),
            tr("Could not start %1:\n%2")
              .arg(program, proc.errorString()));
        return;
    }
    if (!proc.waitForFinished()) {
        QString err = QString::fromUtf8(proc.readAllStandardError()).trimmed();
        QMessageBox::critical(this,
            tr("Generation Failed"),
            tr("Codegen failed:\n%1").arg(err.isEmpty()
                                        ? proc.errorString()
                                        : err));
        return;
    }

    QMessageBox::information(this,
        tr("Done"),
        tr("Generated sources in %1").arg(genDir));
}


void MainWindow::on_actionBuildRun_triggered()
{
    // Save JSON again, then launch the existing interpreter
    QString tmp = QDir::temp().filePath("current.fsm.json");
    core_fsm::persistence::saveFile(m_doc,
                                    tmp.toStdString(),
                                    /*pretty*/true,
                                    nullptr);

    auto *proc = new QProcess(this);
    connect(proc,
            QOverload<int,QProcess::ExitStatus>::of(&QProcess::finished),
            proc, &QObject::deleteLater);

    // assume fsm_runtime is next to your UI executable
    QString exe = QCoreApplication::applicationDirPath() + "/fsm_runtime";
    proc->start(exe, { tmp,
                       "0.0.0.0:45454",    // interpreter bind
                       "127.0.0.1:45455"   // GUI listens here
                     });

    if (!proc->waitForStarted()) {
        QMessageBox::critical(this,
            tr("Run Failed"),
            tr("Could not start “%1”").arg(exe));
        delete proc;
        return;
    }

    // auto‐connect to fresh instance
    on_actionConnect_triggered();
}

void MainWindow::clearFsmVisualization()
{
    m_scene->clear();
    m_stateItems.clear();
    m_transitionItems.clear();
}

void MainWindow::visualizeFsm()
{
    clearFsmVisualization();
    
    // Create state items
    for (const auto& state : m_doc.states) {
        StateItem* item = new StateItem(
            QString::fromStdString(state.id), 
            state.initial
        );
        m_scene->addItem(item);
        m_stateItems[state.id] = item;
    }
    
    // Layout states in a reasonable arrangement
    layoutFsmElements();
    
    // Create transition items
    for (const auto& transition : m_doc.transitions) {
        // Skip transitions with invalid states
        if (!m_stateItems.contains(transition.from) || !m_stateItems.contains(transition.to)) {
            continue;
        }
        
        QString delayText;
        if (!transition.delay_ms.is_null()) {
            if (transition.delay_ms.is_number_integer()) {
                delayText = QString::number(transition.delay_ms.get<int>()) + "ms";
            } else {
                delayText = QString::fromStdString(transition.delay_ms.dump());
            }
        }
        
        TransitionItem* item = new TransitionItem(
            m_stateItems[transition.from],
            m_stateItems[transition.to],
            QString::fromStdString(transition.trigger),
            QString::fromStdString(transition.guard),
            delayText
        );
        m_scene->addItem(item);
        m_transitionItems.append(item);
    }
    
    // Set scene rect to contain all items with some padding
    m_scene->setSceneRect(m_scene->itemsBoundingRect().adjusted(-50, -50, 50, 50));
}

void MainWindow::layoutFsmElements()
{
    // Simple circular layout
    const int states = m_stateItems.size();
    if (states == 0) return;
    
    // For a single state, place it in the center
    if (states == 1) {
        m_stateItems.first()->setPos(0, 0);
        return;
    }
    
    // For multiple states, arrange in a circle
    const qreal radius = 150.0;
    int i = 0;
    
    for (auto* item : m_stateItems) {
        qreal angle = 2.0 * M_PI * i / states;
        qreal x = radius * std::cos(angle);
        qreal y = radius * std::sin(angle);
        item->setPos(x, y);
        i++;
    }
}

void MainWindow::addState()
{
    // Create a custom dialog with the same fields as the property editor
    QDialog dialog(this);
    dialog.setWindowTitle(tr("Add New State"));
    QVBoxLayout* layout = new QVBoxLayout(&dialog);
    
    // ID field
    QFormLayout* form = new QFormLayout();
    QLineEdit* idEdit = new QLineEdit();
    form->addRow(tr("ID:"), idEdit);
    
    // Initial state checkbox
    QCheckBox* initialCheck = new QCheckBox();
    initialCheck->setChecked(m_doc.states.empty()); // Default to initial if first state
    form->addRow(tr("Initial:"), initialCheck);
    
    // onEnter script
    QLabel* scriptLabel = new QLabel(tr("On Enter:"));
    QPlainTextEdit* scriptEdit = new QPlainTextEdit();
    scriptEdit->setPlaceholderText(tr("C/C++ snippet..."));
    scriptEdit->setMinimumHeight(100);
    
    // Add to layout
    layout->addLayout(form);
    layout->addWidget(scriptLabel);
    layout->addWidget(scriptEdit);
    
    // Dialog buttons
    QDialogButtonBox* buttons = new QDialogButtonBox(
        QDialogButtonBox::Ok | QDialogButtonBox::Cancel);
    connect(buttons, &QDialogButtonBox::accepted, &dialog, &QDialog::accept);
    connect(buttons, &QDialogButtonBox::rejected, &dialog, &QDialog::reject);
    layout->addWidget(buttons);
    
    // Set focus to ID field
    idEdit->setFocus();
    
    // Show the dialog
    if (dialog.exec() != QDialog::Accepted) {
        return;
    }
    
    // Get values from the dialog
    QString stateId = idEdit->text().trimmed();
    if (stateId.isEmpty()) {
        QMessageBox::warning(this, tr("Invalid State"), tr("State ID cannot be empty."));
        return;
    }
    
    // Check for unique ID
    std::string newId = stateId.toStdString();
    if (std::any_of(m_doc.states.begin(), m_doc.states.end(), 
                   [&newId](const auto& state) { return state.id == newId; })) {
        QMessageBox::warning(this, tr("Invalid State"), 
                            tr("A state with this ID already exists."));
        return;
    }
    
    // Create the new state
    core_fsm::persistence::StateDesc newState;
    newState.id = newId;
    newState.initial = initialCheck->isChecked();
    newState.onEnter = scriptEdit->toPlainText().toStdString();
    
    // If this is set as initial, clear other initial states
    if (newState.initial) {
        for (auto& state : m_doc.states) {
            state.initial = false;
        }
    }
    
    // Add the state
    m_doc.states.push_back(newState);
    
    // Update UI
    populateProjectTree();
    visualizeFsm();
    
    // Select the new state in the tree
    QTreeWidgetItem* statesRoot = ui->projectTree->topLevelItem(2); // States category
    if (statesRoot && statesRoot->childCount() > 0) {
        ui->projectTree->setCurrentItem(statesRoot->child(statesRoot->childCount() - 1));
    }
}

void MainWindow::addTransition()
{
    if (m_doc.states.empty()) {
        QMessageBox::warning(this, tr("Cannot Add Transition"), 
                           tr("You need at least one state to create a transition."));
        return;
    }
    
    // Create a custom dialog
    QDialog dialog(this);
    dialog.setWindowTitle(tr("Add New Transition"));
    QFormLayout* form = new QFormLayout(&dialog);
    
    // Get list of state IDs for combos
    QStringList stateIds;
    for (const auto& state : m_doc.states) {
        stateIds << QString::fromStdString(state.id);
    }
    
    // From state selection
    QComboBox* fromCombo = new QComboBox();
    fromCombo->addItems(stateIds);
    form->addRow(tr("From:"), fromCombo);
    
    // To state selection
    QComboBox* toCombo = new QComboBox();
    toCombo->addItems(stateIds);
    if (stateIds.size() > 1) {
        toCombo->setCurrentIndex(1); // Select second state by default if available
    }
    form->addRow(tr("To:"), toCombo);
    
    // Trigger field
    QLineEdit* triggerEdit = new QLineEdit();
    form->addRow(tr("Trigger:"), triggerEdit);
    
    // Guard condition
    QLineEdit* guardEdit = new QLineEdit();
    guardEdit->setPlaceholderText(tr("C/C++ condition expression..."));
    form->addRow(tr("Guard:"), guardEdit);
    
    // Delay field
    QLineEdit* delayEdit = new QLineEdit("0");
    form->addRow(tr("Delay (ms or var):"), delayEdit);
    
    // Dialog buttons
    QDialogButtonBox* buttons = new QDialogButtonBox(
        QDialogButtonBox::Ok | QDialogButtonBox::Cancel);
    connect(buttons, &QDialogButtonBox::accepted, &dialog, &QDialog::accept);
    connect(buttons, &QDialogButtonBox::rejected, &dialog, &QDialog::reject);
    form->addRow(buttons);
    
    // Show the dialog
    if (dialog.exec() != QDialog::Accepted) {
        return;
    }
    
    // Create new transition from dialog values
    core_fsm::persistence::TransitionDesc newTransition;
    newTransition.from = fromCombo->currentText().toStdString();
    newTransition.to = toCombo->currentText().toStdString();
    newTransition.trigger = triggerEdit->text().toStdString();
    newTransition.guard = guardEdit->text().toStdString();
    
    // Parse delay - handle both numeric and variable references
    QString delayText = delayEdit->text().trimmed();
    try {
        newTransition.delay_ms = nlohmann::json::parse(delayText.toStdString());
    } catch (...) {
        // If not valid JSON, try to convert to integer
        bool ok;
        int delay = delayText.toInt(&ok);
        if (ok) {
            newTransition.delay_ms = delay;
        } else {
            // Treat as a variable reference (string)
            newTransition.delay_ms = delayText.toStdString();
        }
    }
    
    // Add the transition
    m_doc.transitions.push_back(newTransition);
    
    // Update UI
    populateProjectTree();
    visualizeFsm();
    
    // Select the new transition in the tree
    QTreeWidgetItem* transitionsRoot = ui->projectTree->topLevelItem(4); // Transitions category
    if (transitionsRoot && transitionsRoot->childCount() > 0) {
        ui->projectTree->setCurrentItem(transitionsRoot->child(transitionsRoot->childCount() - 1));
    }
}
