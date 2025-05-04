#include "mainwindow.hpp"
#include "ui_mainwindow.h"
#include <QTimer>
#include <QColor>
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
#include <QDebug>

#include <QDialog>
#include <QFormLayout>
#include <QVBoxLayout>
#include <QDialogButtonBox>
#include <QInputDialog>
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


void MainWindow::handleInputCellChanged(int row, int column)
{
    if (column != 1) return;           // only the value column

    QString name  = ui->tableInputs->item(row, 0)->text();
    QString value = ui->tableInputs->item(row, 1)->text();

    // send as an inject
    if (m_runtime) {
        nlohmann::json j = {
            {"type",  "inject"},
            {"name",  name.toStdString()},
            {"value", value.toStdString()}
        };
        m_runtime->sendCustomMessage(j.dump());
        
        // Update our local snapshot to reflect the change immediately
        m_lastSnapshot.inputs[name] = value;
    }
}

/* ------------------------------------------------------------------
 *  A user edited the “Variables” table
 * ----------------------------------------------------------------*/
void MainWindow::handleVariableCellChanged(int row, int column)
{
    if (column != 1) return;          // only the value column matters

    QString name  = ui->tableVariables->item(row, 0)->text();
    QString value = ui->tableVariables->item(row, 1)->text();

    bool isVariable = std::any_of(m_doc.variables.begin(), m_doc.variables.end(),
                                  [&name](const auto& v){
                                      return QString::fromStdString(v.name) == name;
                                  });

    if (m_runtime) {
        nlohmann::json j = {
            {"type",  isVariable ? "setVar" : "inject"},
            {"name",  name.toStdString()},
            {"value", value.toStdString()}
        };
        m_runtime->sendCustomMessage(j.dump());
    }
}

// ————— File → Open —————
void MainWindow::on_actionOpen_triggered() {
    // 1) ask for file
    QString path = QFileDialog::getOpenFileName(
        this,
        tr("Open FSM…"),
        QString(),
        tr("FSM JSON Files (*.json)")
    );
    if (path.isEmpty()) return;

    // 2) clear previous warning (we'll re‐set it below if needed)
    m_warningBar->clear();
    m_warningBar->setVisible(false);

    // 3) try to load, capturing any err/warning
    core_fsm::persistence::FsmDocument doc;
    std::string err;
    bool ok = core_fsm::persistence::loadFile(path.toStdString(), doc, &err);

    // 4) if loadFile() said “hard error”, show only bar and bail
    if (!ok) {
        m_warningBar->setText(QString::fromStdString(err));
        m_warningBar->setVisible(true);
        return;
    }

    // 5) At this point we have a valid doc (maybe with a warning in err)
    m_doc = std::move(doc);
    m_currentFsmPath = path;
    populateProjectTree();

    // 6) ALWAYS render the graph
    visualizeFsm();

    // 7) Now if err is non‐empty, show it as a warning bar
    if (!err.empty()) {
        m_warningBar->setText(QString::fromStdString(err));
        m_warningBar->setVisible(true);
    }
    else {
        // no warning → keep it hidden
        m_warningBar->clear();
        m_warningBar->setVisible(false);
    }
}
// ————— File → Save —————
void MainWindow::on_actionSave_triggered() {
    // Clear any previous warnings
    m_warningBar->clear();
    m_warningBar->setVisible(false);
    
    if (m_currentFsmPath.isEmpty()) {
        return on_actionSaveAs_triggered();
    }

    std::string err;
    if (!core_fsm::persistence::saveFile(m_doc,
          m_currentFsmPath.toStdString(),
          /*pretty*/true, &err))
    {
        m_warningBar->setText(tr("Failed to save \"%1\": %2")
                              .arg(m_currentFsmPath, QString::fromStdString(err)));
        m_warningBar->setVisible(true);
        
        QMessageBox::critical(this,
                              tr("Error Saving FSM"),
                              tr("Failed to save \"%1\":\n%2")
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

    // Project Info (new section for name and comment)
    auto *infoRoot = new QTreeWidgetItem({ tr("Project Info") });
    QTreeWidgetItem* nameItem = new QTreeWidgetItem({ getNameDisplayText() });
    QTreeWidgetItem* commentItem = new QTreeWidgetItem({ getCommentDisplayText() });
    infoRoot->addChild(nameItem);
    infoRoot->addChild(commentItem);
    ui->projectTree->addTopLevelItem(infoRoot);

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
                      + QString::fromStdString(v.init.dump())
                      + " (" + QString::fromStdString(v.type) + ")";
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
    // Clear any warnings when creating a new FSM
    m_warningBar->clear();
    m_warningBar->setVisible(false);

    // / 1) reset the DTO ----------------------------------------------/
    m_doc = core_fsm::persistence::FsmDocument{};   // fresh, empty document
    m_doc.name = "untitled";                        // a harmless default

    // 2) forget the path so the next Ctrl‑S falls back to Save As… /
    m_currentFsmPath.clear();

    // 3) refresh every view ----------------------------------------/
    populateProjectTree();   // left‑hand tree
    clearFsmVisualization(); // diagram scene
    ui->labelCurrentState->setText(tr("Current State: -"));
}

void MainWindow::handleStateSnapshot(const StateSnapshot& snap)
{
    if (!m_receivedState) {
        m_receivedState = true;
        m_reconnectTimer->stop();   // optional – prevents any late pop‑up
    }
    // first-ever snapshot: just record state, no arrow yet
    if (!m_receivedFirstSnapshot) {
        m_receivedFirstSnapshot = true;
        m_prevStateId = snap.state.toStdString();
    }
    else {
        std::string from = m_prevStateId;
        std::string to   = snap.state.toStdString();

        // find which transition in the doc matches
        for (int i = 0; i < m_doc.transitions.size(); ++i) {
            const auto &t = m_doc.transitions[i];
            if (t.from == from && t.to == to) {
                // flash the corresponding TransitionItem
                if (i < m_transitionItems.size()) {
                    m_transitionItems[i]->highlight();
                }
                break;
            }
        }
        m_prevStateId = to;
    }
    m_lastSnapshot = snap;
    // now update the tables/diagram as before
    updateMonitor(snap);
}

void MainWindow::shutdownInterpreterAndChannel()
{
    // Gracefully ask the runtime logic to stop
    if (m_runtime) {
        m_runtime->shutdown();   // tells the interpreter to exit
        m_runtime->stop();       // joins the worker thread
        m_runtime.reset();
    }

    // Then stop the external process
    if (m_interpreter) {
        m_interpreter->terminate();                 // SIGTERM / WM_CLOSE
        if (!m_interpreter->waitForFinished(3000))  // give it 3 s
            m_interpreter->kill();                  // hard kill
        m_interpreter = nullptr;                    // owned by this
    }
}

void MainWindow::closeEvent(QCloseEvent *ev)
{
    shutdownInterpreterAndChannel();
    QMainWindow::closeEvent(ev);   // default cleanup
}

void MainWindow::updateMonitor(const StateSnapshot& snap)
{
    // 1) state label + highlight (unchanged)
    ui->labelCurrentState->setText(tr("Current State: %1").arg(snap.state));
    std::string current = snap.state.toStdString();
    for (auto id : m_stateItems.keys())
        m_stateItems[id]->setBrush(id == current ? QBrush(Qt::lightGray)
                                                 : QBrush(Qt::white));

    // ------------------------------------------------------------------
    //  INPUTS  (name=read-only, value=editable, + Send button)
    // ------------------------------------------------------------------
    ui->tableInputs->blockSignals(true);
    ui->tableInputs->clearContents();
    
    // Use all defined inputs from the document instead of just those in snapshot
    ui->tableInputs->setRowCount(m_doc.inputs.size());
    
    int row = 0;
    for (const auto& inputName : m_doc.inputs) {
        QString name = QString::fromStdString(inputName);
        
        // name cell
        auto *n = new QTableWidgetItem(name);
        n->setFlags(n->flags() & ~Qt::ItemIsEditable);
        ui->tableInputs->setItem(row, 0, n);

        // value cell - use value from snapshot if available
        QString value = snap.inputs.value(name, "");
        auto *v = new QTableWidgetItem(value);
        v->setFlags(v->flags() | Qt::ItemIsEditable);
        ui->tableInputs->setItem(row, 1, v);

        // Send button
        auto *btn = new QPushButton(tr("Send"));
        ui->tableInputs->setCellWidget(row, 2, btn);

        // wire the button to send whatever's in "value"
        connect(btn, &QPushButton::clicked, this, [this, row]() {
            QString name  = ui->tableInputs->item(row, 0)->text();
            QString value = ui->tableInputs->item(row, 1)->text();
            if (m_runtime) {
                nlohmann::json j = {
                    {"type",  "inject"},
                    {"name",  name.toStdString()},
                    {"value", value.toStdString()}
                };
                m_runtime->sendCustomMessage(j.dump());
            }
        });
        
        row++;
    }
    ui->tableInputs->blockSignals(false);

    // ------------------------------------------------------------------
    //  VARIABLES  (completely read-only, no special coloring)
    // ------------------------------------------------------------------
    ui->tableVariables->blockSignals(true);
    ui->tableVariables->clearContents();
    ui->tableVariables->setRowCount(snap.vars.size());
    row = 0;
    for (auto it = snap.vars.cbegin(); it != snap.vars.cend(); ++it, ++row) {
        auto *n = new QTableWidgetItem(it.key());
        auto *v = new QTableWidgetItem(it.value());
        n->setFlags(n->flags() & ~Qt::ItemIsEditable);
        v->setFlags(v->flags() & ~Qt::ItemIsEditable);
        ui->tableVariables->setItem(row,0,n);
        ui->tableVariables->setItem(row,1,v);
    }
    ui->tableVariables->blockSignals(false);

    // ------------------------------------------------------------------
    //  OUTPUTS  (read-only, show all defined outputs)
    // ------------------------------------------------------------------
    ui->tableOutputs->blockSignals(true);
    ui->tableOutputs->clearContents();
    ui->tableOutputs->setRowCount(m_doc.outputs.size());
    
    row = 0;
    for (const auto& outputName : m_doc.outputs) {
        QString name = QString::fromStdString(outputName);
        
        // name cell
        auto *n = new QTableWidgetItem(name);
        n->setFlags(n->flags() & ~Qt::ItemIsEditable);
        ui->tableOutputs->setItem(row, 0, n);

        // value cell - use value from snapshot if available
        QString value = snap.outputs.value(name, "");
        auto *v = new QTableWidgetItem(value);
        v->setFlags(v->flags() & ~Qt::ItemIsEditable);
        ui->tableOutputs->setItem(row, 1, v);
        
        row++;
    }
    ui->tableOutputs->blockSignals(false);
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

    // Add delete button at the bottom of form for all items except Project Info
    QPushButton* deleteButton = nullptr;
    if (category != tr("Project Info")) {
        deleteButton = new QPushButton(tr("Delete"));
        deleteButton->setIcon(QIcon::fromTheme("edit-delete"));
        deleteButton->setStyleSheet("background-color: #ffaaaa; font-weight: bold;");
        
        connect(deleteButton, &QPushButton::clicked, this, [this, category, index]() {
            deleteSelectedItem(category, index);
        });
    }

    // Handle Project Info items
    if (category == tr("Project Info")) {
        if (index == 0) {  // Name
            // Extract just the name value from the display string
            QString currentName = QString::fromStdString(m_doc.name);
            
            auto *nameEdit = new QLineEdit(currentName);
            ui->formProperties->addRow(tr("Project Name:"), nameEdit);
            connect(nameEdit, &QLineEdit::editingFinished, this, [this, it, nameEdit]() {
                QString newName = nameEdit->text().trimmed();
                m_doc.name = newName.toStdString();
                updateNameTreeItem(it);
            });
        } else if (index == 1) {  // Comment
            // Use a text area for comment editing
            QString currentComment = QString::fromStdString(m_doc.comment);
            
            auto *commentEdit = new QPlainTextEdit(currentComment);
            commentEdit->setMinimumHeight(100);
            ui->formProperties->addRow(tr("Project Comment:"), commentEdit);
            connect(commentEdit, &QPlainTextEdit::textChanged, this, [this, it, commentEdit]() {
                QString newComment = commentEdit->toPlainText();
                m_doc.comment = newComment.toStdString();
                updateCommentTreeItem(it);
            });
        }
        return;
    }

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

        // Add delete button at the end
        if (deleteButton) {
            ui->formProperties->addRow("", deleteButton);
        }
        return;
    }

    // — Variables: name + type + initial value
    if (category == tr("Variables")) {
        // name
        auto *nameEdit = new QLineEdit(QString::fromStdString(m_doc.variables[index].name));
        ui->formProperties->addRow(tr("Name:"), nameEdit);
        connect(nameEdit, &QLineEdit::editingFinished, this, [this, index, it, nameEdit]() {
            QString newName = nameEdit->text().trimmed();
            if (newName.isEmpty()) return;
            m_doc.variables[index].name = newName.toStdString();
            updateVariableTreeItem(index, it);
        });

        // type selection
        QComboBox* typeCombo = new QComboBox();
        typeCombo->addItems(QStringList() << "int" << "float" << "string");
        
        // Set current type from the variable
        QString currentType = QString::fromStdString(m_doc.variables[index].type);
        if (currentType.isEmpty()) {
            // Default to int if no type is set
            typeCombo->setCurrentIndex(0);
            m_doc.variables[index].type = "int"; // Initialize type if not set
        } else if (currentType == "float") {
            typeCombo->setCurrentIndex(1);
        } else if (currentType == "string") {
            typeCombo->setCurrentIndex(2);
        } else {
            typeCombo->setCurrentIndex(0); // Default to int for any unrecognized types
        }
        
        ui->formProperties->addRow(tr("Type:"), typeCombo);
        
        connect(typeCombo, QOverload<int>::of(&QComboBox::currentIndexChanged), 
            this, [this, index, it, typeCombo](int) {
                m_doc.variables[index].type = typeCombo->currentText().toStdString();
                updateVariableTreeItem(index, it);
        });

        // init
        auto *initEdit = new QLineEdit(QString::fromStdString(m_doc.variables[index].init.dump()));
        ui->formProperties->addRow(tr("Initial:"), initEdit);
        connect(initEdit, &QLineEdit::editingFinished, this, [this, index, it, initEdit]() {
            QString valueText = initEdit->text().trimmed();
            try {
                if (valueText.isEmpty()) {
                    m_doc.variables[index].init = 0;  // Default to 0 for empty values
                } else {
                    m_doc.variables[index].init = nlohmann::json::parse(valueText.toStdString(), nullptr, false);
                }
            } catch (...) {
                if (valueText.isEmpty()) {
                    m_doc.variables[index].init = 0;  // Default to 0 for empty values
                } else {
                    m_doc.variables[index].init = valueText.toStdString();
                }
            }
            updateVariableTreeItem(index, it);
        });

        // Add delete button at the end
        if (deleteButton) {
            ui->formProperties->addRow("", deleteButton);
        }
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
            
            // Remember the old ID before changing it
            std::string oldId = m_doc.states[index].id;
            std::string newIdStr = newId.toStdString();
            
            // Skip if ID hasn't changed
            if (oldId == newIdStr) return;
            
            // Change state ID in the model
            m_doc.states[index].id = newIdStr;
            
            // Update tree item text
            it->setText(0, newId + (m_doc.states[index].initial ? tr(" (initial)") : QString()));
            
            // Update all transitions that reference this state
            for (auto& transition : m_doc.transitions) {
                if (transition.from == oldId) {
                    transition.from = newIdStr;
                }
                if (transition.to == oldId) {
                    transition.to = newIdStr;
                }
            }
            
            // Refresh transitions in the tree
            populateProjectTree();
            
            // Update visualizations
            updateStateVisual(index);
        });

        // Initial flag
        auto *chk = new QCheckBox;
        chk->setChecked(m_doc.states[index].initial);
        ui->formProperties->addRow(tr("Initial:"), chk);
        connect(chk, &QCheckBox::toggled, this, [this, index, it, chk](bool on) {
            m_doc.states[index].initial = on;
            it->setText(0, QString::fromStdString(m_doc.states[index].id) 
                        + (on ? tr(" (initial)") : QString()));
            updateStateVisual(index);
        });

        // onEnter action - use a separate editor in the property panel now
        auto *actionEdit = new QPlainTextEdit(QString::fromStdString(m_doc.states[index].onEnter));
        actionEdit->setPlaceholderText(tr("C/C++ snippet…"));
        ui->formProperties->addRow(tr("On Enter:"), actionEdit);
        connect(actionEdit, &QPlainTextEdit::textChanged, this, [this, index, actionEdit]() {
            m_doc.states[index].onEnter = actionEdit->toPlainText().toStdString();
        });

        // After updating state properties
        if (m_stateItems.contains(m_doc.states[index].id)) {
            m_stateItems[m_doc.states[index].id]->setInitial(m_doc.states[index].initial);
        }
        
        // Add delete button at the end
        if (deleteButton) {
            ui->formProperties->addRow("", deleteButton);
        }
        return;
    }

    // — Transitions: trigger, guard, delay, from, to
    if (category == tr("Transitions")) {
        auto &trn = m_doc.transitions[index];

        // Helper function to update the tree item text
        auto updateTransitionLabel = [this, it, index]() {
            const auto& t = m_doc.transitions[index];
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
                    // fallback: show whatever JSON the user gave
                    label += " @ " + QString::fromStdString(t.delay_ms.dump());
                }
            }
            it->setText(0, label);
            
            // Update visual transition representation
            updateTransitionVisual(index);
        };

        // trigger (input event)
        auto *trigEdit = new QLineEdit(QString::fromStdString(trn.trigger));
        ui->formProperties->addRow(tr("Trigger:"), trigEdit);
        connect(trigEdit, &QLineEdit::editingFinished, this, [this, index, trigEdit, updateTransitionLabel]() {
            QString newTrigger = trigEdit->text().trimmed();
            m_doc.transitions[index].trigger = newTrigger.toStdString();
            updateTransitionLabel();
        });

        // guard
        auto *guardEdit = new QLineEdit(QString::fromStdString(trn.guard));
        ui->formProperties->addRow(tr("Guard:"), guardEdit);
        connect(guardEdit, &QLineEdit::editingFinished, this, [this, index, guardEdit, updateTransitionLabel]() {
            m_doc.transitions[index].guard = guardEdit->text().toStdString();
            updateTransitionLabel();
        });

        // delay (as JSON)
        QLineEdit* delayEdit = new QLineEdit();  // Empty by default (will be treated as null)
        delayEdit->setPlaceholderText(tr("Leave empty for null"));
        
        // Set the current delay value if it exists
        if (!m_doc.transitions[index].delay_ms.is_null()) {
            if (m_doc.transitions[index].delay_ms.is_number_integer()) {
                // If it's a number, show just the number
                delayEdit->setText(QString::number(m_doc.transitions[index].delay_ms.get<int>()));
            } else {
                // For other types (strings, etc), show the raw JSON
                delayEdit->setText(QString::fromStdString(m_doc.transitions[index].delay_ms.dump()));
            }
        }
        
        ui->formProperties->addRow(tr("Delay (ms or var):"), delayEdit);
        connect(delayEdit, &QLineEdit::editingFinished, this, [this, index, delayEdit, updateTransitionLabel]() {
            QString delayText = delayEdit->text().trimmed();
            if (delayText.isEmpty()) {
                // Set to null if empty
                m_doc.transitions[index].delay_ms = nullptr;
            } else {
                try {
                    m_doc.transitions[index].delay_ms = nlohmann::json::parse(delayText.toStdString());
                } catch (...) {
                    // If not valid JSON, try to convert to integer
                    bool ok;
                    int delay = delayText.toInt(&ok);
                    if (ok) {
                        m_doc.transitions[index].delay_ms = delay;
                    } else {
                        // Treat as a variable reference (string)
                        m_doc.transitions[index].delay_ms = delayText.toStdString();
                    }
                }
            }
            updateTransitionLabel();
        });

        // from/to comboboxes
        QStringList states;
        for (auto const& s : m_doc.states)
            states << QString::fromStdString(s.id);

        auto *fromBox = new QComboBox;
        fromBox->addItems(states);
        fromBox->setCurrentText(QString::fromStdString(trn.from));
        ui->formProperties->addRow(tr("From:"), fromBox);
        connect(fromBox, &QComboBox::currentTextChanged, this, [this, index, updateTransitionLabel](const QString &txt){
            m_doc.transitions[index].from = txt.toStdString();
            updateTransitionLabel();
        });

        auto *toBox = new QComboBox;
        toBox->addItems(states);
        toBox->setCurrentText(QString::fromStdString(trn.to));
        ui->formProperties->addRow(tr("To:"), toBox);
        connect(toBox, &QComboBox::currentTextChanged, this, [this, index, updateTransitionLabel](const QString &txt){
            m_doc.transitions[index].to = txt.toStdString();
            updateTransitionLabel();
        });

        // After updating transition properties
        for (auto* item : m_transitionItems) {
            item->updatePosition();
        }

        // Add delete button at the end
        if (deleteButton) {
            ui->formProperties->addRow("", deleteButton);
        }
        return;
    }
}

void MainWindow::deleteSelectedItem(const QString& category, int index) {
    // Confirm deletion
    QMessageBox::StandardButton reply = QMessageBox::question(
        this, 
        tr("Confirm Deletion"),
        tr("Are you sure you want to delete this %1?").arg(category.toLower()),
        QMessageBox::Yes | QMessageBox::No
    );
    
    if (reply != QMessageBox::Yes) {
        return;
    }
    
    // Handle deletion based on item type
    if (category == tr("Inputs")) {
        // Delete input
        if (index >= 0 && index < m_doc.inputs.size()) {
            m_doc.inputs.erase(m_doc.inputs.begin() + index);
        }
    }
    else if (category == tr("Outputs")) {
        // Delete output
        if (index >= 0 && index < m_doc.outputs.size()) {
            m_doc.outputs.erase(m_doc.outputs.begin() + index);
        }
    }
    else if (category == tr("Variables")) {
        // Delete variable
        if (index >= 0 && index < m_doc.variables.size()) {
            m_doc.variables.erase(m_doc.variables.begin() + index);
        }
    }
    else if (category == tr("States")) {
        // Delete state and all associated transitions
        if (index >= 0 && index < m_doc.states.size()) {
            std::string stateId = m_doc.states[index].id;
            
            // First remove all transitions connected to this state
            auto it = m_doc.transitions.begin();
            while (it != m_doc.transitions.end()) {
                if (it->from == stateId || it->to == stateId) {
                    it = m_doc.transitions.erase(it);
                } else {
                    ++it;
                }
            }
            
            // Then remove the state
            m_doc.states.erase(m_doc.states.begin() + index);
            
            // Update visualization
            visualizeFsm();
        }
    }
    else if (category == tr("Transitions")) {
        // Delete transition
        if (index >= 0 && index < m_doc.transitions.size()) {
            m_doc.transitions.erase(m_doc.transitions.begin() + index);
            
            // Update visualization
            visualizeFsm();
        }
    }
    
    // Update UI
    populateProjectTree();
    clearPropertyEditor();
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
    QSet<QString> newStateIds;  // Changed from std::string to QString
    
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
            newStateIds.insert(QString::fromStdString(state.id));  // Convert to QString
        }
    }
    
    // Only lay out new states
    layoutNewStateElements(newStateIds);
    
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

// Add this new method to position only new states
void MainWindow::layoutNewStateElements(const QSet<QString>& newStateIds)
{
    if (newStateIds.isEmpty()) return;
    
    // If all states are new, use the regular circular layout
    if (newStateIds.size() == m_stateItems.size()) {
        layoutFsmElements();
        return;
    }
    
    // Calculate bounding rectangle of existing states
    QRectF existingBounds;
    bool hasExisting = false;
    
    for (auto it = m_stateItems.begin(); it != m_stateItems.end(); ++it) {
        if (!newStateIds.contains(QString::fromStdString(it.key()))) {  // Convert key to QString
            if (!hasExisting) {
                existingBounds = QRectF(it.value()->pos(), QSizeF(0, 0));
                hasExisting = true;
            } else {
                existingBounds = existingBounds.united(QRectF(it.value()->pos(), QSizeF(0, 0)));
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
    QLineEdit* delayEdit = new QLineEdit();  // Empty by default (will be treated as null)
    delayEdit->setPlaceholderText(tr("Leave empty for null"));
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
    if (delayText.isEmpty()) {
        // Set to null if empty
        newTransition.delay_ms = nullptr;
    } else {
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

void MainWindow::addVariable()
{
    // Create dialog
    QDialog dialog(this);
    dialog.setWindowTitle(tr("Add New Variable"));
    QFormLayout* form = new QFormLayout(&dialog);
    
    // Name field
    QLineEdit* nameEdit = new QLineEdit();
    form->addRow(tr("Name:"), nameEdit);
    
    // Type selection combo box
    QComboBox* typeCombo = new QComboBox();
    typeCombo->addItems(QStringList() << "int" << "float" << "string");
    typeCombo->setCurrentIndex(0); // Default to int
    form->addRow(tr("Type:"), typeCombo);
    
    // Initial value field (JSON)
    QLineEdit* initEdit = new QLineEdit("0");  // Default to 0
    initEdit->setPlaceholderText(tr("JSON value (number, string, array, etc.)"));
    form->addRow(tr("Initial value:"), initEdit);
    
    // Dialog buttons
    QDialogButtonBox* buttons = new QDialogButtonBox(
        QDialogButtonBox::Ok | QDialogButtonBox::Cancel);
    connect(buttons, &QDialogButtonBox::accepted, &dialog, &QDialog::accept);
    connect(buttons, &QDialogButtonBox::rejected, &dialog, &QDialog::reject);
    form->addRow(buttons);
    
    // Set focus to name field
    nameEdit->setFocus();
    
    // Show the dialog
    if (dialog.exec() != QDialog::Accepted) {
        return;
    }
    
    // Get values from the dialog
    QString name = nameEdit->text().trimmed();
    if (name.isEmpty()) {
        QMessageBox::warning(this, tr("Invalid Variable"), tr("Variable name cannot be empty."));
        return;
    }
    
    // Check for unique name
    std::string newName = name.toStdString();
    if (std::any_of(m_doc.variables.begin(), m_doc.variables.end(), 
                   [&newName](const auto& var) { return var.name == newName; })) {
        QMessageBox::warning(this, tr("Invalid Variable"), 
                            tr("A variable with this name already exists."));
        return;
    }
    
    // Create the new variable
    core_fsm::persistence::VariableDesc newVar;
    newVar.name = newName;
    newVar.type = typeCombo->currentText().toStdString(); // Set the type
    
    // Parse initial value
    try {
        QString initValue = initEdit->text().trimmed();
        if (initValue.isEmpty()) {
            newVar.init = 0;  // Default to 0 for empty values
        } else {
            newVar.init = nlohmann::json::parse(initValue.toStdString(), nullptr, false);
        }
    } catch (...) {
        // If parsing fails, use the raw text as a string value
        QString initValue = initEdit->text().trimmed();
        if (initValue.isEmpty()) {
            newVar.init = 0;  // Default to 0 for empty values
        } else {
            newVar.init = initValue.toStdString();
        }
    }
    
    // Add the variable
    m_doc.variables.push_back(newVar);
    
    // Update UI
    populateProjectTree();
    
    // Select the new variable in the tree
    QTreeWidgetItem* varRoot = ui->projectTree->topLevelItem(2); // Variables category
    if (varRoot && varRoot->childCount() > 0) {
        ui->projectTree->setCurrentItem(varRoot->child(varRoot->childCount() - 1));
    }
}

void MainWindow::addInput()
{
    // Create dialog
    QDialog dialog(this);
    dialog.setWindowTitle(tr("Add New Input"));
    QFormLayout* form = new QFormLayout(&dialog);
    
    // Name field
    QLineEdit* nameEdit = new QLineEdit();
    form->addRow(tr("Name:"), nameEdit);
    
    // Dialog buttons
    QDialogButtonBox* buttons = new QDialogButtonBox(
        QDialogButtonBox::Ok | QDialogButtonBox::Cancel);
    connect(buttons, &QDialogButtonBox::accepted, &dialog, &QDialog::accept);
    connect(buttons, &QDialogButtonBox::rejected, &dialog, &QDialog::reject);
    form->addRow(buttons);
    
    // Set focus to name field
    nameEdit->setFocus();
    
    // Show the dialog
    if (dialog.exec() != QDialog::Accepted) {
        return;
    }
    
    // Get values from the dialog
    QString name = nameEdit->text().trimmed();
    if (name.isEmpty()) {
        QMessageBox::warning(this, tr("Invalid Input"), tr("Input name cannot be empty."));
        return;
    }
    
    // Check for unique name
    std::string newName = name.toStdString();
    if (std::find(m_doc.inputs.begin(), m_doc.inputs.end(), newName) != m_doc.inputs.end()) {
        QMessageBox::warning(this, tr("Invalid Input"), 
                            tr("An input with this name already exists."));
        return;
    }
    
    // Add the input
    m_doc.inputs.push_back(newName);
    
    // Update UI
    populateProjectTree();
    
    // Select the new input in the tree
    QTreeWidgetItem* inputRoot = ui->projectTree->topLevelItem(0); // Inputs category
    if (inputRoot && inputRoot->childCount() > 0) {
        ui->projectTree->setCurrentItem(inputRoot->child(inputRoot->childCount() - 1));
    }
}

void MainWindow::addOutput()
{
    // Create dialog
    QDialog dialog(this);
    dialog.setWindowTitle(tr("Add New Output"));
    QFormLayout* form = new QFormLayout(&dialog);
    
    // Name field
    QLineEdit* nameEdit = new QLineEdit();
    form->addRow(tr("Name:"), nameEdit);
    
    // Dialog buttons
    QDialogButtonBox* buttons = new QDialogButtonBox(
        QDialogButtonBox::Ok | QDialogButtonBox::Cancel);
    connect(buttons, &QDialogButtonBox::accepted, &dialog, &QDialog::accept);
    connect(buttons, &QDialogButtonBox::rejected, &dialog, &QDialog::reject);
    form->addRow(buttons);
    
    // Set focus to name field
    nameEdit->setFocus();
    
    // Show the dialog
    if (dialog.exec() != QDialog::Accepted) {
        return;
    }
    
    // Get values from the dialog
    QString name = nameEdit->text().trimmed();
    if (name.isEmpty()) {
        QMessageBox::warning(this, tr("Invalid Output"), tr("Output name cannot be empty."));
        return;
    }
    
    // Check for unique name
    std::string newName = name.toStdString();
    if (std::find(m_doc.outputs.begin(), m_doc.outputs.end(), newName) != m_doc.outputs.end()) {
        QMessageBox::warning(this, tr("Invalid Output"), 
                            tr("An output with this name already exists."));
        return;
    }
    
    // Add the output
    m_doc.outputs.push_back(newName);
    
    // Update UI
    populateProjectTree();
    
    // Select the new output in the tree
    QTreeWidgetItem* outputRoot = ui->projectTree->topLevelItem(1); // Outputs category
    if (outputRoot && outputRoot->childCount() > 0) {
        ui->projectTree->setCurrentItem(outputRoot->child(outputRoot->childCount() - 1));
    }
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

// Add this method for updating state visuals
void MainWindow::updateStateVisual(int stateIndex) {
    const auto& state = m_doc.states[stateIndex];
    std::string stateId = state.id;
    
    // We need to rebuild the visualization when state IDs change
    // since transitions need to be reconnected to the renamed state
    visualizeFsm();
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

// Add this helper function to your MainWindow class
void MainWindow::updateVariableTreeItem(int index, QTreeWidgetItem* item) {
    const auto& var = m_doc.variables[index];
    QString displayText = QString::fromStdString(var.name) + 
                         " = " + 
                         QString::fromStdString(var.init.dump()) + 
                         " (" + QString::fromStdString(var.type) + ")";
    item->setText(0, displayText);
}

QString MainWindow::getNameDisplayText() const {
    return tr("Name: %1").arg(QString::fromStdString(m_doc.name.empty() ? "Untitled" : m_doc.name));
}

QString MainWindow::getCommentDisplayText() const {
    return tr("Comment: %1").arg(QString::fromStdString(m_doc.comment.empty() ? "-" : m_doc.comment));
}

void MainWindow::updateNameTreeItem(QTreeWidgetItem* item) {
    item->setText(0, getNameDisplayText());
}

void MainWindow::updateCommentTreeItem(QTreeWidgetItem* item) {
    item->setText(0, getCommentDisplayText());
}
