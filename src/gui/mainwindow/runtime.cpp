/**
 * @file   runtime.cpp
 * @brief  Implements runtime monitoring and control functionality for the FSM editor.
 *         Handles state snapshots, updates monitoring displays, and manages the
 *         interpreter process lifecycle.
 *
 * @author Martin Ševčík (xsevcim00)
 * @author Jakub Lůčný (xlucnyj00)
 * @date   2025-05-06
 */
#include <QMessageBox>
#include <QTableWidgetItem>
#include <QPushButton>

#include "../../core/io/runtime_client.hpp"
#include "../graphics/fsmgraphicsitems.hpp"

#include "mainwindow.hpp"
#include "ui_mainwindow.h"

/**
 * Processes state snapshot data received from the FSM interpreter.
 * 
 * Updates the visualization to reflect the current state, highlights active
 * transitions during state changes, and maintains history for transition
 * animations. Updates all monitoring tables with current values.
 * 
 *  
 */
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

/**
 * Updates all monitoring UI components with data from the state snapshot.
 * 
 * Refreshes the current state indicator, highlights the active state in the diagram,
 * and populates the inputs, variables, and outputs tables with current values.
 * For inputs, adds interactive controls to allow sending values to the interpreter.
 */
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

/**
 * Safely terminates the interpreter process and runtime communication.
 * 
 * First signals the interpreter to shut down gracefully through the runtime
 * client, then terminates the interpreter process if it doesn't exit within
 * a timeout period. Called during application exit or when switching projects.
 */
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
        if (!m_interpreter->waitForFinished(3000))  // give it 3 s
            m_interpreter->kill();                  // hard kill
        m_interpreter = nullptr;                    // owned by this
    }
}
