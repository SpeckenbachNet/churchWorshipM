#include "mainwin.h"
#include "./ui_mainwin.h"
#include "toolbarm.h"

MainWin::MainWin(QWidget *parent)
    : QMainWindow(parent),
    ui(new Ui::MainWin)
{
    ui->setupUi(this);
    initializeForm();

    initializeConnections();
}

// ====== Initializing / Settings  ======

void MainWin::initializeForm() {
    // --- Create Buttons and objects ---
    ui->toolBarFrame->addButton("settingsBtn","",":icons/settings");
    m_settingsBtn = ui->toolBarFrame->findChild<QToolButton*>("settingsBtn");
    m_settingsBtn->setEnabled(false);

    ui->toolBarFrame->addSpacer();  // Spacer for align buttons right

    ui->toolBarFrame->addButton("helpBtn","",":icons/help");
    m_clearBtn = ui->toolBarFrame->findChild<QToolButton*>("helpBtn");
    m_clearBtn->setEnabled(false);
}

void MainWin::initializeConnections() {
    connect(ui->splitter, &QSplitter::splitterMoved, this, [this](int pos, int index) {
        //updateListIconSize();
    });
}

// ====== Events ======

void MainWin::resizeEvent(QResizeEvent *event) {
    QMainWindow::resizeEvent(event);
}

MainWin::~MainWin()
{
    delete ui;
}
