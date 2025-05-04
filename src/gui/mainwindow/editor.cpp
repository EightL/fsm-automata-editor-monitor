#include <QFileDialog>
#include <QMessageBox>
#include <QTableWidgetItem>
#include <QLineEdit>
#include <QCheckBox>
#include <QPlainTextEdit>
#include <QComboBox>
#include <iostream>

#include <QDialog>
#include <QFormLayout>
#include <QVBoxLayout>
#include <QDialogButtonBox>
#include <QInputDialog>

#include "../../core/io/runtime_client.hpp"
#include "../graphics/fsmgraphicsitems.hpp"

#include "mainwindow.hpp"
#include "ui_mainwindow.h"

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

// Add this helper function to your MainWindow class
void MainWindow::updateVariableTreeItem(int index, QTreeWidgetItem* item) {
    const auto& var = m_doc.variables[index];
    QString displayText = QString::fromStdString(var.name) + 
                         " = " + 
                         QString::fromStdString(var.init.dump()) + 
                         " (" + QString::fromStdString(var.type) + ")";
    item->setText(0, displayText);
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
