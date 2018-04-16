// This file is part of Heimer.
// Copyright (C) 2018 Jussi Lind <jussi.lind@iki.fi>
//
// Heimer is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, either Config::Editor 3 of the License, or
// (at your option) any later Config::Editor.
// Heimer is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with Heimer. If not, see <http://www.gnu.org/licenses/>.

#include "mainwindow.hpp"

#include "config.hpp"
#include "aboutdlg.hpp"
#include "mediator.hpp"
#include "mindmapdata.hpp"
#include "mclogger.hh"

#include <QAction>
#include <QApplication>
#include <QDateTime>
#include <QDesktopWidget>
#include <QFileDialog>
#include <QGraphicsLineItem>
#include <QMenu>
#include <QMenuBar>
#include <QMessageBox>
#include <QScreen>
#include <QSettings>
#include <QStandardPaths>
#include <QTimer>
#include <QToolBar>
#include <QHBoxLayout>
#include <QVBoxLayout>

#include <cassert>

MainWindow * MainWindow::m_instance = nullptr;

static const QString FILE_EXTENSION(Config::FILE_EXTENSION);

MainWindow::MainWindow(QString mindMapFile)
: m_aboutDlg(new AboutDlg(this))
, m_argMindMapFile(mindMapFile)
, m_mediator(new Mediator(*this))
{
    if (!m_instance)
    {
        m_instance = this;
    }
    else
    {
        qFatal("MainWindow already instantiated!");
    }

    setWindowIcon(QIcon(":/heimer-editor.png"));

    init();

    if (!m_argMindMapFile.isEmpty())
    {
        QTimer::singleShot(0, this, SLOT(openArgMindMap()));
    }
}

void MainWindow::addRedoAction(QMenu & menu, std::function<void ()> handler)
{
    m_redoAction = new QAction(tr("Redo"), this);
    m_redoAction->setShortcut(QKeySequence("Ctrl+Shift+Z"));
    menu.addAction(m_redoAction);
    connect(m_redoAction, &QAction::triggered, handler);
    m_redoAction->setEnabled(false);
}

void MainWindow::addUndoAction(QMenu & menu, std::function<void ()> handler)
{
    m_undoAction = new QAction(tr("Undo"), this);
    m_undoAction->setShortcut(QKeySequence("Ctrl+Z"));
    menu.addAction(m_undoAction);
    connect(m_undoAction, &QAction::triggered, handler);
    m_undoAction->setEnabled(false);
}

void MainWindow::createEditMenu()
{
    const auto editMenu = menuBar()->addMenu(tr("&Edit"));

    const auto undoHandler = [this] () {
        m_mediator->undo();
        setupMindMapAfterUndoOrRedo();
    };

    addUndoAction(*editMenu, undoHandler);

    const auto redoHandler = [this] () {
        m_mediator->redo();
        setupMindMapAfterUndoOrRedo();
    };

    addRedoAction(*editMenu, redoHandler);
}

void MainWindow::createFileMenu()
{
    const auto fileMenu = menuBar()->addMenu(tr("&File"));

    // Add "new"-action
    const auto newAct = new QAction(tr("&New..."), this);
    newAct->setShortcut(QKeySequence("Ctrl+N"));
    fileMenu->addAction(newAct);
    connect(newAct, SIGNAL(triggered()), this, SLOT(initializeNewMindMap()));

    // Add "open"-action
    const auto openAct = new QAction(tr("&Open..."), this);
    openAct->setShortcut(QKeySequence("Ctrl+O"));
    fileMenu->addAction(openAct);
    connect(openAct, SIGNAL(triggered()), this, SLOT(openMindMap()));

    // Add "save"-action
    m_saveAction = new QAction(tr("&Save"), this);
    m_saveAction->setShortcut(QKeySequence("Ctrl+S"));
    fileMenu->addAction(m_saveAction);
    connect(m_saveAction, SIGNAL(triggered()), this, SLOT(saveMindMap()));
    m_saveAction->setEnabled(false);

    // Add "save as"-action
    m_saveAsAction = new QAction(tr("&Save as..."), this);
    m_saveAsAction->setShortcut(QKeySequence("Ctrl+Shift+S"));
    fileMenu->addAction(m_saveAsAction);
    connect(m_saveAsAction, SIGNAL(triggered()), this, SLOT(saveMindMapAs()));
    m_saveAsAction->setEnabled(false);

    // Add "quit"-action
    const auto quitAct = new QAction(tr("&Quit"), this);
    quitAct->setShortcut(QKeySequence("Ctrl+W"));
    fileMenu->addAction(quitAct);
    connect(quitAct, SIGNAL(triggered()), this, SLOT(close()));
}

void MainWindow::createHelpMenu()
{
    const auto helpMenu = menuBar()->addMenu(tr("&Help"));

    // Add "about"-action
    const auto aboutAct = new QAction(tr("&About"), this);
    helpMenu->addAction(aboutAct);
    connect(aboutAct, SIGNAL(triggered()), this, SLOT(showAboutDlg()));

    // Add "about Qt"-action
    const auto aboutQtAct = new QAction(tr("About &Qt"), this);
    helpMenu->addAction(aboutQtAct);
    connect(aboutQtAct, SIGNAL(triggered()), this, SLOT(showAboutQtDlg()));
}

QString MainWindow::getFileDialogFileText() const
{
    return tr("Heimer Files") + " (*" + FILE_EXTENSION + ")";
}

void MainWindow::init()
{
    setTitle(tr("New file"));

    QSettings settings;

    // Detect screen dimensions
    QScreen *screen = QGuiApplication::primaryScreen();
    QRect screenGeometry = screen->geometry();
    const int height = screenGeometry.height();
    const int width = screenGeometry.width();

    // Read dialog size data
    settings.beginGroup(m_settingsGroup);
    const float defaultScale = 0.8;
    resize(settings.value("size", QSize(width, height) * defaultScale).toSize());
    settings.endGroup();

    // Try to center the window.
    move(width / 2 - this->width() / 2, height / 2 - this->height() / 2);

    m_mediator->showHelloText();

    populateMenuBar();
}

void MainWindow::setTitle(QString openFileName)
{
    setWindowTitle(
        QString(Config::APPLICATION_NAME) + " " + Config::APPLICATION_VERSION + " - " +
        openFileName);
}

MainWindow * MainWindow::instance()
{
    return MainWindow::m_instance;
}

void MainWindow::closeEvent(QCloseEvent * event)
{
    // Open settings file
    QSettings settings;

    // Save window size
    settings.beginGroup(m_settingsGroup);
    settings.setValue("size", size());
    settings.endGroup();

    event->accept();
}

void MainWindow::populateMenuBar()
{
    createFileMenu();

    createEditMenu();

    createHelpMenu();
}

void MainWindow::openArgMindMap()
{
    doOpenMindMap(m_argMindMapFile);
}

void MainWindow::openMindMap()
{
    MCLogger().info() << "Open file";

    const auto path = loadRecentPath();
    const auto fileName = QFileDialog::getOpenFileName(this, tr("Open File"), path, getFileDialogFileText());
    if (!fileName.isEmpty())
    {
        doOpenMindMap(fileName);
    }
}

void MainWindow::disableUndoAndRedo()
{
    m_undoAction->setEnabled(false);
    m_redoAction->setEnabled(false);
}

void MainWindow::enableUndo(bool enable)
{
    m_undoAction->setEnabled(enable);
}

QString MainWindow::loadRecentPath() const
{
    QSettings settings;
    settings.beginGroup(m_settingsGroup);
    const auto path = settings.value("recentPath", QStandardPaths::writableLocation(QStandardPaths::HomeLocation)).toString();
    settings.endGroup();
    return path;
}

void MainWindow::resizeEvent(QResizeEvent * event)
{
    if (!m_mediator->hasNodes())
    {
        m_mediator->center();
    }

    QMainWindow::resizeEvent(event);
}

void MainWindow::showAboutDlg()
{
    m_aboutDlg->exec();
}

void MainWindow::showAboutQtDlg()
{
    QMessageBox::aboutQt(this, tr("About Qt"));
}

void MainWindow::doOpenMindMap(QString fileName)
{
    MCLogger().info() << "Opening '" << fileName.toStdString();

    if (m_mediator->openMindMap(fileName))
    {
        disableUndoAndRedo();

        setTitle(fileName);

        saveRecentPath(fileName);

        setSaveActionStatesOnNewMindMap();

        successLog();
    }
}

void MainWindow::saveRecentPath(QString fileName)
{
    QSettings settings;
    settings.beginGroup(m_settingsGroup);
    settings.setValue("recentPath", fileName);
    settings.endGroup();
}

void MainWindow::setupMindMapAfterUndoOrRedo()
{
    m_saveAction->setEnabled(true);
    m_undoAction->setEnabled(m_mediator->isUndoable());
    m_redoAction->setEnabled(m_mediator->isRedoable());

    setSaveActionStatesOnNewMindMap();

    m_mediator->setupMindMapAfterUndoOrRedo();
}

void MainWindow::saveMindMap()
{
    MCLogger().info() << "Save..";

    if (m_mediator->isSaved())
    {
        if (!m_mediator->saveMindMap())
        {
            const auto msg = QString(tr("Failed to save file."));
            MCLogger().error() << msg.toStdString();
            showMessageBox(msg);
            return;
        }
        successLog();
    }
    else
    {
        saveMindMapAs();
    }
}

void MainWindow::saveMindMapAs()
{
    MCLogger().info() << "Save as..";

    QString fileName = QFileDialog::getSaveFileName(this,
        tr("Save File As"),
        QStandardPaths::writableLocation(QStandardPaths::HomeLocation),
        getFileDialogFileText());

    if (fileName.isEmpty())
    {
        return;
    }

    if (!fileName.endsWith(FILE_EXTENSION))
    {
        fileName += FILE_EXTENSION;
    }

    if (m_mediator->saveMindMapAs(fileName))
    {
        const auto msg = QString(tr("File '")) + fileName + tr("' saved.");
        MCLogger().info() << msg.toStdString();
        setTitle(fileName);
        successLog();
    }
    else
    {
        const auto msg = QString(tr("Failed to save file as '") + fileName + "'.");
        MCLogger().error() << msg.toStdString();
        showMessageBox(msg);
    }
}

void MainWindow::showErrorDialog(QString message)
{
    QMessageBox::critical(this,
         Config::APPLICATION_NAME,
         message,
         "");
}

void MainWindow::showMessageBox(QString message)
{
    QMessageBox msgBox;
    msgBox.setText(message);
    msgBox.exec();
}

void MainWindow::initializeNewMindMap()
{
    MCLogger().info() << "New file";

    m_mediator->initializeNewMindMap();

    disableUndoAndRedo();

    setSaveActionStatesOnNewMindMap();

    setTitle(tr("New file"));
}

void MainWindow::setSaveActionStatesOnNewMindMap()
{
    m_saveAction->setEnabled(false);
    m_saveAsAction->setEnabled(true);
}

void MainWindow::successLog()
{
    MCLogger().info() << "Huge success!";
}

MainWindow::~MainWindow()
{
    delete m_mediator;
}
