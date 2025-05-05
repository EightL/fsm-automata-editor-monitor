#include <QFileDialog>
#include <QMessageBox>
#include <QLineEdit>
#include <QCheckBox>
#include <QPlainTextEdit>
#include <QComboBox>

#include <QDialog>
#include <QFormLayout>
#include <QVBoxLayout>
#include <QDialogButtonBox>
#include <QLabel>

#include "mainwindow.hpp"
#include "ui_mainwindow.h"

void MainWindow::addState()
{
    // Create a custom dialog with the same fields as the property editor
    QDialog dialog(this);
    dialog.setWindowTitle(tr("Add New State"));
    QVBoxLayout* layout = new QVBoxLayout(&dialog);
    
    // ID field
    QFormLayout* form = new QFormLayout();
    QLineEdit* idEdit = new QLineEdit();
    form->addRow(tr("Name:"), idEdit);
    
    // Initial state checkbox
    QCheckBox* initialCheck = new QCheckBox();
    initialCheck->setChecked(m_doc.states.empty()); // Default to initial if first state
    form->addRow(tr("Initial:"), initialCheck);
    
    // onEnter script
    QLabel* scriptLabel = new QLabel(tr("On Enter:"));
    QPlainTextEdit* scriptEdit = new QPlainTextEdit();
    scriptEdit->setPlaceholderText(tr("JS expression"));
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
    triggerEdit->setPlaceholderText(tr("Name of input"));
    form->addRow(tr("Trigger:"), triggerEdit);
    
    // Guard condition
    QLineEdit* guardEdit = new QLineEdit();
    guardEdit->setPlaceholderText(tr("JS condition expression..."));
    form->addRow(tr("Guard:"), guardEdit);
    
    // Delay field
    QLineEdit* delayEdit = new QLineEdit();  // Empty by default (will be treated as null)
    delayEdit->setPlaceholderText(tr("Time in ms, \"variable\" or leave empty"));
    form->addRow(tr("Delay:"), delayEdit);
    
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
