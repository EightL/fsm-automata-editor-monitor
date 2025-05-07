/**
 * @file   file.cpp
 * @brief  Implements file operations for the FSM editor including new, open,
 *         save, and save-as functionality, plus project tree management.
 *
 * @author Martin Ševčík (xsevcim00)
 * @author Jakub Lůčný (xlucnyj00)
 * @date   2025-05-06
 */
#include <QFileDialog>
#include <QMessageBox>
#include <QDateTime>

#include "mainwindow.hpp"
#include "ui_mainwindow.h"

/**
 * Creates a new empty FSM document and resets the UI.
 * 
 * Clears the current document, warning messages, and visualizations,
 * then initializes a new document with default values.
 */
void MainWindow::on_actionNew_triggered()
{
    // Clear any warnings when creating a new FSM
    m_warningBar->clear();
    m_warningBar->setVisible(false);

    // / 1) reset the DTO ----------------------------------------------/
    m_doc = core_fsm::persistence::FsmDocument{};   // fresh, empty document
    m_doc.name = "untitled";                        // a harmless default

    // 2) forget the path so the next Ctrl‑S falls back to Save As… /
    m_currentFsmPath.clear();

    // 3) refresh every view ----------------------------------------/
    populateProjectTree();   // left‑hand tree
    clearFsmVisualization(); // diagram scene
    ui->labelCurrentState->setText(tr("Current State: -"));
}

/**
 * Opens an existing FSM document from a user-selected file.
 * 
 * Displays a file dialog, loads the selected file, and handles any parsing errors.
 * Updates the UI with the loaded FSM contents and displays any warning messages.
 */
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

    // 4) if loadFile() said "hard error", show only bar and bail
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

/**
 * Saves the current FSM document to its existing filepath.
 * 
 * If no filepath is currently set, delegates to on_actionSaveAs_triggered().
 * Handles save errors and displays appropriate error messages.
 */
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

/**
 * Saves the current FSM document to a user-selected filepath.
 * 
 * Displays a file dialog for selecting the save location, then
 * delegates to on_actionSave_triggered() to perform the actual save.
 */
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

/**
 * Rebuilds the project tree with the current FSM document structure.
 * 
 * Clears the existing tree and creates categories for all FSM elements:
 * Project info, inputs, outputs, variables, states, and transitions.
 * Formats each item with descriptive text based on its properties.
 */
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

/**
 * Formats the project name for display in the project tree.
 *
 * @return Formatted string with project name label and value
 */
QString MainWindow::getNameDisplayText() const {
    return tr("Name: %1").arg(QString::fromStdString(m_doc.name.empty() ? "Untitled" : m_doc.name));
}

/**
 * Formats the project comment for display in the project tree.
 */
QString MainWindow::getCommentDisplayText() const {
    return tr("Comment: %1").arg(QString::fromStdString(m_doc.comment.empty() ? "-" : m_doc.comment));
}

/**
 * Updates the display text for the name item in the project tree.
 */
void MainWindow::updateNameTreeItem(QTreeWidgetItem* item) {
    item->setText(0, getNameDisplayText());
}

/**
 * Updates the display text for the comment item in the project tree.
 */
void MainWindow::updateCommentTreeItem(QTreeWidgetItem* item) {
    item->setText(0, getCommentDisplayText());
}
