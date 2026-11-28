#include "DiscordRPC.h"
#include "MainWindow.h"
#include "EntityEditor.h"
#include "IncludeManager.h"
#include "BaseClassManager.h"
#include "AutoVisGroupEditor.h"
#include "MapSizeDialog.h"
#include "GridNavDialog.h"
#include "ProblemsDialog.h"
#include "RawEntityDialog.h"
#include "ClassPickerDialog.h"
#include "FGDGenerator.h"
#include "FGDImporter.h"
#include "FGDProjectIO.h"
#include "FGDValidator.h"
#include "FGDHighlighter.h"
#include "Version.h"
#include <QMenuBar>
#include <QToolBar>
#include <QAction>
#include <QSplitter>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QWidget>
#include <QPushButton>
#include <QFileDialog>
#include <QMessageBox>
#include <QInputDialog>
#include <QStatusBar>
#include <QApplication>
#include <QFile>
#include <QFont>
#include <QIcon>
#include <QTextEdit>
#include <QLabel>
#include <QFileInfo>
#include <QDir>
#include <QSettings>
#include <QCloseEvent>
#include <QUndoStack>
#include <QUndoCommand>
#include <QSet>
#include <QRegularExpression>
#include <algorithm>
#include <functional>

static bool renameInVisChild(FGDAutoVisGroupChild &c, const QString &from, const QString &to) {
    bool changed = false;
    for (QString &e : c.entities)
        if (e == from) { e = to; changed = true; }
    for (FGDAutoVisGroupChild &sub : c.children)
        if (renameInVisChild(sub, from, to)) changed = true;
    return changed;
}

MainWindow::MainWindow(QWidget *parent) : QMainWindow(parent) {
    setWindowTitle("FGDBuilder");
    setWindowIcon(QIcon(":/icons/app.png"));
    setMinimumSize(900, 600);
    m_undoStack = new QUndoStack(this);
    m_undoStack->setUndoLimit(200);

    QSettings settings("FGDBuilder", "FGDBuilder");
    m_discordEnabled = settings.value("discord/enabled", true).toBool();
    m_discordShowFile = settings.value("discord/showFileName", true).toBool();

    setupUi();
    setupMenuBar();
    setupToolBar();
    statusBar()->addWidget(m_statusLabel = new QLabel("Ready"));

    m_recentFiles = settings.value("recentFiles").toStringList();
    updateRecentFilesMenu();
    restoreGeometry(settings.value("mainWindowGeometry").toByteArray());
    restoreState(settings.value("mainWindowState").toByteArray());
    if (m_splitter) {
        QByteArray sp = settings.value("mainSplitter").toByteArray();
        if (!sp.isEmpty()) m_splitter->restoreState(sp);
    }

    connect(m_undoStack, &QUndoStack::cleanChanged, this, [this](bool clean) {
        QString title = "FGDBuilder";
        if (!m_filePath.isEmpty()) title += " - " + QFileInfo(m_filePath).fileName();
        if (!clean) title += " *";
        setWindowTitle(title);
    });
    updateDiscord("No file open");
}

void MainWindow::closeEvent(QCloseEvent *event) {
    if (!confirmDiscard()) {
        event->ignore();
        return;
    }
    QSettings settings("FGDBuilder", "FGDBuilder");
    settings.setValue("recentFiles", m_recentFiles);
    settings.setValue("mainWindowGeometry", saveGeometry());
    settings.setValue("mainWindowState", saveState());
    if (m_splitter) settings.setValue("mainSplitter", m_splitter->saveState());
    event->accept();
}

QString MainWindow::projectDir() const {
    if (!m_filePath.isEmpty()) return QFileInfo(m_filePath).absolutePath();
    return m_baseDir;
}

QString MainWindow::startDir(const QString &key) const {
    QSettings settings("FGDBuilder", "FGDBuilder");
    QString saved = settings.value("lastDir/" + key).toString();
    if (!saved.isEmpty() && QDir(saved).exists()) return saved;
    return projectDir();
}

void MainWindow::rememberDir(const QString &key, const QString &path) {
    QSettings settings("FGDBuilder", "FGDBuilder");
    settings.setValue("lastDir/" + key, QFileInfo(path).absolutePath());
}

UndoContext MainWindow::undoContext() {
    UndoContext c;
    c.project = &m_project;
    c.changed = [this]{ ensureExternals(); refreshEntityList(); };
    return c;
}

void MainWindow::ensureExternals() {
    QString key = m_project.includes.join("\n") + "|" + m_baseDir;
    if (key == m_externalsKey) return;
    forceReloadExternals();
}

void MainWindow::forceReloadExternals() {
    m_externalsKey = m_project.includes.join("\n") + "|" + m_baseDir;
    if (m_project.includes.isEmpty()) {
        m_externals = FGDIncludeResult();
        return;
    }
    m_externals = FGDIncludes::load(m_project.includes, m_baseDir);
    if (m_statusLabel) {
        QString msg = QString("Included %1 classes from %2 file(s)").arg(m_externals.classes.size()).arg(m_externals.loadedFiles.size());
        if (!m_externals.warnings.isEmpty()) msg += QString(", %1 warning(s), see Project > Check for Problems").arg(m_externals.warnings.size());
        m_statusLabel->setText(msg);
    }
}

FGDClassIndex MainWindow::buildIndex() {
    ensureExternals();
    FGDClassIndex index;
    index.build(m_project.entities, &m_externals);
    return index;
}

QStringList MainWindow::knownBaseNames(const FGDClassIndex &index) const {
    return index.classNames(true);
}

void MainWindow::setupUi() {
    QWidget *central = new QWidget;
    setCentralWidget(central);
    QHBoxLayout *mainLayout = new QHBoxLayout(central);
    mainLayout->setContentsMargins(4, 4, 4, 4);

    QSplitter *splitter = new QSplitter(Qt::Horizontal);
    m_splitter = splitter;

    QWidget *leftPanel = new QWidget;
    QVBoxLayout *leftLayout = new QVBoxLayout(leftPanel);
    leftLayout->setContentsMargins(0,0,0,0);
    leftLayout->addWidget(new QLabel("Entities:"));

    m_searchEdit = new QLineEdit;
    m_searchEdit->setPlaceholderText("Search entities, properties, descriptions...");
    m_searchEdit->setClearButtonEnabled(true);
    leftLayout->addWidget(m_searchEdit);

    m_namesOnly = new QCheckBox("Class names only");
    m_namesOnly->setToolTip("When checked, the search looks only at class names.\nWhen unchecked, it also looks at descriptions, keyvalue names and descriptions, and inputs and outputs.");
    leftLayout->addWidget(m_namesOnly);

    m_entityList = new QListWidget;
    m_entityList->setMinimumWidth(220);
    m_entityList->setSelectionMode(QAbstractItemView::ExtendedSelection);
    leftLayout->addWidget(m_entityList);

    QHBoxLayout *entityBtns = new QHBoxLayout;
    QPushButton *addBtn = new QPushButton("Add");
    QPushButton *editBtn = new QPushButton("Edit");
    QPushButton *removeBtn = new QPushButton("Remove");
    QPushButton *dupBtn = new QPushButton("Duplicate");
    entityBtns->addWidget(addBtn);
    entityBtns->addWidget(editBtn);
    entityBtns->addWidget(removeBtn);
    entityBtns->addWidget(dupBtn);
    leftLayout->addLayout(entityBtns);

    QHBoxLayout *moveRow = new QHBoxLayout;
    QPushButton *upBtn = new QPushButton("Move Up");
    QPushButton *downBtn = new QPushButton("Move Down");
    QPushButton *baseBtn = new QPushButton("Add Base...");
    baseBtn->setToolTip("Add one or more base classes to all selected entities at once.");
    moveRow->addWidget(upBtn);
    moveRow->addWidget(downBtn);
    moveRow->addWidget(baseBtn);
    moveRow->addStretch();
    leftLayout->addLayout(moveRow);

    splitter->addWidget(leftPanel);

    QWidget *rightPanel = new QWidget;
    QVBoxLayout *rightLayout = new QVBoxLayout(rightPanel);
    rightLayout->setContentsMargins(0,0,0,0);
    rightLayout->addWidget(new QLabel("FGD Preview:"));
    m_preview = new QTextEdit;
    m_preview->setReadOnly(true);
    QFont monoFont("Courier New", 9);
    monoFont.setStyleHint(QFont::Monospace);
    m_preview->setFont(monoFont);
    m_preview->setStyleSheet("QTextEdit { background: #1e1e1e; color: #d4d4d4; }");
    new FGDHighlighter(m_preview->document());
    rightLayout->addWidget(m_preview);

    splitter->addWidget(rightPanel);
    splitter->setStretchFactor(0, 0);
    splitter->setStretchFactor(1, 1);
    splitter->setSizes({240, 600});

    mainLayout->addWidget(splitter);

    connect(addBtn, &QPushButton::clicked, this, &MainWindow::addEntity);
    connect(editBtn, &QPushButton::clicked, this, &MainWindow::editEntity);
    connect(removeBtn, &QPushButton::clicked, this, &MainWindow::removeEntity);
    connect(dupBtn, &QPushButton::clicked, this, &MainWindow::duplicateEntity);
    connect(upBtn, &QPushButton::clicked, this, &MainWindow::moveEntityUp);
    connect(downBtn, &QPushButton::clicked, this, &MainWindow::moveEntityDown);
    connect(baseBtn, &QPushButton::clicked, this, &MainWindow::addBaseToSelected);
    connect(m_entityList, &QListWidget::itemSelectionChanged, this, &MainWindow::onEntitySelectionChanged);
    connect(m_entityList, &QListWidget::itemDoubleClicked, this, &MainWindow::editEntity);
    connect(m_searchEdit, &QLineEdit::textChanged, this, &MainWindow::onSearchTextChanged);
    connect(m_namesOnly, &QCheckBox::toggled, this, [this]{ refreshEntityList(); });
}

static QAction* makeAction(const QString &text, QObject *receiver, std::function<void()> slot, const QKeySequence &key = {}) {
    QAction *a = new QAction(text);
    if (!key.isEmpty()) a->setShortcut(key);
    QObject::connect(a, &QAction::triggered, receiver, slot);
    return a;
}

void MainWindow::setupMenuBar() {
    QMenu *fileMenu = menuBar()->addMenu("&File");
    fileMenu->addAction(makeAction("&New", this, [this]{ newProject(); }, QKeySequence::New));
    fileMenu->addAction(makeAction("&Open...", this, [this]{ openProject(); }, QKeySequence::Open));
    fileMenu->addAction(makeAction("&Import FGD...", this, [this]{ importFGD(); }, QKeySequence("Ctrl+I")));
    fileMenu->addSeparator();
    fileMenu->addAction(makeAction("&Save", this, [this]{ saveProject(); }, QKeySequence::Save));
    fileMenu->addAction(makeAction("Save &As...", this, [this]{ saveProjectAs(); }, QKeySequence::SaveAs));
    fileMenu->addSeparator();
    fileMenu->addAction(makeAction("&Export FGD...", this, [this]{ exportFGD(); }, QKeySequence("Ctrl+E")));
    fileMenu->addSeparator();
    m_recentMenu = fileMenu->addMenu("Recent Files");
    fileMenu->addSeparator();
    fileMenu->addAction(makeAction("E&xit", qApp, []{ qApp->quit(); }, QKeySequence::Quit));

    QMenu *editMenu = menuBar()->addMenu("&Edit");
    editMenu->addAction(m_undoStack->createUndoAction(this, "&Undo"));
    editMenu->actions().last()->setShortcut(QKeySequence::Undo);
    editMenu->addAction(m_undoStack->createRedoAction(this, "&Redo"));
    editMenu->actions().last()->setShortcut(QKeySequence::Redo);
    editMenu->addSeparator();
    editMenu->addAction(makeAction("&Add Entity", this, [this]{ addEntity(); }, QKeySequence("Ctrl+N")));
    editMenu->addAction(makeAction("&Edit Entity", this, [this]{ editEntity(); }, QKeySequence("F2")));
    editMenu->addAction(makeAction("Edit as &Raw FGD...", this, [this]{ editEntityRaw(); }, QKeySequence("Ctrl+R")));
    editMenu->addAction(makeAction("&Remove Entity", this, [this]{ removeEntity(); }, QKeySequence::Delete));
    editMenu->addAction(makeAction("D&uplicate Entity", this, [this]{ duplicateEntity(); }, QKeySequence("Ctrl+D")));
    editMenu->addAction(makeAction("Add &Base to Selected...", this, [this]{ addBaseToSelected(); }, QKeySequence("Ctrl+B")));
    editMenu->addSeparator();
    editMenu->addAction(makeAction("Move &Up", this, [this]{ moveEntityUp(); }, QKeySequence("Ctrl+Up")));
    editMenu->addAction(makeAction("Move &Down", this, [this]{ moveEntityDown(); }, QKeySequence("Ctrl+Down")));
    editMenu->addSeparator();
    editMenu->addAction(makeAction("Sort A-Z", this, [this]{ sortEntitiesAZ(); }));

    QMenu *projectMenu = menuBar()->addMenu("&Project");
    projectMenu->addAction(makeAction("&Header Comment...", this, [this]{ editHeaderComment(); }));
    projectMenu->addAction(makeAction("&@include Files...", this, [this]{ editIncludes(); }));
    projectMenu->addAction(makeAction("&Reload @include Files", this, [this]{ reloadIncludes(); }, QKeySequence("F5")));
    projectMenu->addAction(makeAction("&@mapsize...", this, [this]{ editMapSize(); }));
    projectMenu->addAction(makeAction("@&gridnav...", this, [this]{ editGridNav(); }));
    projectMenu->addAction(makeAction("@&MaterialExclusion...", this, [this]{ editMaterialExclusions(); }));
    projectMenu->addAction(makeAction("@&AutoVisGroup...", this, [this]{ editAutoVisGroups(); }));
    projectMenu->addAction(makeAction("@&version...", this, [this]{ editFGDVersion(); }));
    projectMenu->addSeparator();
    projectMenu->addAction(makeAction("&Check for Problems...", this, [this]{ checkProblems(); }, QKeySequence("Alt+P")));

    QMenu *settingsMenu = menuBar()->addMenu("&Settings");
    QMenu *discordMenu = settingsMenu->addMenu("&Discord");
    QAction *enableAct = discordMenu->addAction("Show Discord &status");
    enableAct->setCheckable(true);
    enableAct->setChecked(m_discordEnabled);
    QAction *fileAct = discordMenu->addAction("Show &file name in Discord");
    fileAct->setCheckable(true);
    fileAct->setChecked(m_discordShowFile);
    fileAct->setEnabled(m_discordEnabled);
    connect(enableAct, &QAction::toggled, this, [this, fileAct](bool on) {
        m_discordEnabled = on;
        fileAct->setEnabled(on);
        QSettings settings("FGDBuilder", "FGDBuilder");
        settings.setValue("discord/enabled", on);
        updateDiscord(currentFileLabel());
    });
    connect(fileAct, &QAction::toggled, this, [this](bool on) {
        m_discordShowFile = on;
        QSettings settings("FGDBuilder", "FGDBuilder");
        settings.setValue("discord/showFileName", on);
        updateDiscord(currentFileLabel());
    });

    QMenu *helpMenu = menuBar()->addMenu("&Help");
    helpMenu->addAction(makeAction("&About FGDBuilder", this, [this]{ showAbout(); }));
}

void MainWindow::setupToolBar() {
    QToolBar *tb = addToolBar("Main");
    tb->setObjectName("MainToolBar");
    tb->setMovable(false);
    tb->addAction(QIcon(":/icons/new.png"), "New", this, &MainWindow::newProject);
    tb->addAction(QIcon(":/icons/open.png"), "Open", this, &MainWindow::openProject);
    tb->addAction(QIcon(":/icons/save.png"), "Save", this, &MainWindow::saveProject);
    tb->addAction(QIcon(":/icons/export.png"), "Export FGD", this, &MainWindow::exportFGD);
    tb->addAction(QIcon(":/icons/add.png"), "Add Entity", this, &MainWindow::addEntity);
    tb->addAction(QIcon(":/icons/delete.png"), "Remove Entity", this, &MainWindow::removeEntity);
    tb->addAction(QIcon(":/icons/edit.png"), "Edit Entity", this, &MainWindow::editEntity);
}

QString MainWindow::currentFileLabel() const {
    return m_filePath.isEmpty() ? QString("Untitled") : QFileInfo(m_filePath).fileName();
}

void MainWindow::updateDiscord(const QString &fileLabel) {
    DiscordRPC::instance().setEnabled(m_discordEnabled);
    QString state = m_discordShowFile ? fileLabel : QString("Editing an FGD");
    DiscordRPC::instance().setActivity(state, appVersionLabel());
}

bool MainWindow::matchesSearch(const FGDEntity &e, const QString &filter, bool namesOnly) const {
    if (filter.isEmpty()) return true;
    QString label = (classTypeToString(e.classType) + " " + e.className).toLower();
    if (label.contains(filter)) return true;
    if (namesOnly) return false;
    if (e.description.toLower().contains(filter)) return true;
    for (const FGDKeyvalue &kv : e.keyvalues) {
        if (kv.name.toLower().contains(filter) || kv.displayName.toLower().contains(filter)
            || kv.description.toLower().contains(filter)) return true;
        for (const auto &c : kv.choiceItems)
            if (c.second.toLower().contains(filter)) return true;
        for (const auto &f : kv.flagItems)
            if (f.second.first.toLower().contains(filter)) return true;
    }
    for (const FGDIO &io : e.ios)
        if (io.name.toLower().contains(filter) || io.description.toLower().contains(filter)) return true;
    return false;
}

void MainWindow::refreshEntityList() {
    QString filter = m_searchEdit ? m_searchEdit->text().trimmed().toLower() : QString();
    bool namesOnly = m_namesOnly && m_namesOnly->isChecked();
    QSet<int> previous;
    for (QListWidgetItem *it : m_entityList->selectedItems())
        previous.insert(it->data(Qt::UserRole).toInt());
    int prevCurrent = m_entityList->currentItem() ? m_entityList->currentItem()->data(Qt::UserRole).toInt() : -1;

    m_entityList->blockSignals(true);
    m_entityList->clear();
    QListWidgetItem *currentItem = nullptr;
    for (int i = 0; i < m_project.entities.size(); ++i) {
        const FGDEntity &e = m_project.entities[i];
        if (!matchesSearch(e, filter, namesOnly)) continue;
        QString label = classTypeToString(e.classType) + " " + e.className;
        QListWidgetItem *item = new QListWidgetItem(label);
        item->setData(Qt::UserRole, i);
        m_entityList->addItem(item);
        if (previous.contains(i)) item->setSelected(true);
        if (i == prevCurrent) currentItem = item;
    }
    if (currentItem) m_entityList->setCurrentItem(currentItem, QItemSelectionModel::NoUpdate);
    m_entityList->blockSignals(false);
    updatePreview();
}

int MainWindow::realIndexFromRow(int row) const {
    if (row < 0 || row >= m_entityList->count()) return -1;
    return m_entityList->item(row)->data(Qt::UserRole).toInt();
}

QList<int> MainWindow::selectedRealIndexes() const {
    QList<int> result;
    for (QListWidgetItem *it : m_entityList->selectedItems())
        result << it->data(Qt::UserRole).toInt();
    std::sort(result.begin(), result.end());
    return result;
}

void MainWindow::selectRealIndexes(const QList<int> &indexes) {
    m_entityList->blockSignals(true);
    m_entityList->clearSelection();
    QListWidgetItem *last = nullptr;
    for (int i = 0; i < m_entityList->count(); ++i) {
        QListWidgetItem *it = m_entityList->item(i);
        if (indexes.contains(it->data(Qt::UserRole).toInt())) {
            it->setSelected(true);
            last = it;
        }
    }
    if (last) m_entityList->setCurrentItem(last, QItemSelectionModel::NoUpdate);
    m_entityList->blockSignals(false);
    updatePreview();
}

void MainWindow::onSearchTextChanged(const QString &) {
    refreshEntityList();
}

void MainWindow::setWindowModified_(bool modified) {
    QString title = "FGDBuilder";
    if (!m_filePath.isEmpty()) title += " - " + QFileInfo(m_filePath).fileName();
    if (modified) title += " *";
    setWindowTitle(title);
}

bool MainWindow::confirmDiscard() {
    if (m_undoStack->isClean()) return true;
    auto r = QMessageBox::question(this, "Unsaved Changes",
        "You have unsaved changes. Discard them?",
        QMessageBox::Yes | QMessageBox::No);
    return r == QMessageBox::Yes;
}

void MainWindow::newProject() {
    if (!confirmDiscard()) return;
    m_project = FGDProject();
    m_filePath.clear();
    m_baseDir.clear();
    m_undoStack->clear();
    forceReloadExternals();
    refreshEntityList();
    setWindowModified_(false);
    m_statusLabel->setText("New project created.");
    updateDiscord("New project");
}

void MainWindow::loadFromFile(const QString &path) {
    FGDProject proj;
    QString err;
    if (!FGDProjectIO::load(path, proj, err)) {
        QMessageBox::warning(this, "Error", err);
        return;
    }
    m_project = proj;
    m_filePath = path;
    m_baseDir = QFileInfo(path).absolutePath();
    m_undoStack->clear();
    m_undoStack->setClean();
    rememberDir("project", path);
    addRecentFile(path);
    forceReloadExternals();
    refreshEntityList();
    setWindowModified_(false);
    if (m_externals.warnings.isEmpty()) m_statusLabel->setText("Loaded: " + path);
    updateDiscord(QFileInfo(path).fileName());
}

void MainWindow::saveToFile(const QString &path) {
    QString err;
    if (!FGDProjectIO::save(path, m_project, err)) {
        QMessageBox::warning(this, "Error", err);
        return;
    }
    m_filePath = path;
    m_baseDir = QFileInfo(path).absolutePath();
    m_undoStack->setClean();
    rememberDir("project", path);
    addRecentFile(path);
    setWindowModified_(false);
    m_statusLabel->setText("Saved: " + path);
    updateDiscord(QFileInfo(path).fileName());
}

void MainWindow::openProject() {
    if (!confirmDiscard()) return;
    QString path = QFileDialog::getOpenFileName(this, "Open Project", startDir("project"), "FGDBuilder Project (*.fgdbp);;All Files (*)");
    if (!path.isEmpty()) loadFromFile(path);
}

void MainWindow::importFGD() {
    QString path = QFileDialog::getOpenFileName(this, "Import FGD", startDir("fgd"), "Forge Game Data (*.fgd);;All Files (*)");
    if (path.isEmpty()) return;
    rememberDir("fgd", path);
    QString err;
    FGDImportReport report;
    FGDProject imported = FGDImporter::importFromFile(path, err, &report);
    if (!err.isEmpty()) {
        QMessageBox::warning(this, "Import Error", err);
        return;
    }
    QString msg = QString("Imported %1 entities from:\n%2\n\n%3 kept as raw text, %4 skipped, %5 warnings.\n\nReplace current project or merge into it?")
        .arg(imported.entities.size()).arg(path)
        .arg(report.count(FGDImportMessage::Preserved))
        .arg(report.count(FGDImportMessage::Skipped))
        .arg(report.count(FGDImportMessage::Warning));
    QMessageBox dlg(this);
    dlg.setWindowTitle("Import FGD");
    dlg.setText(msg);
    QPushButton *replaceBtn = dlg.addButton("Replace", QMessageBox::AcceptRole);
    QPushButton *mergeBtn   = dlg.addButton("Merge",   QMessageBox::AcceptRole);
    dlg.addButton("Cancel", QMessageBox::RejectRole);
    dlg.exec();
    bool imported_ok = false;
    if (dlg.clickedButton() == replaceBtn) {
        if (!confirmDiscard()) return;
        m_project = imported;
        m_filePath.clear();
        m_baseDir = QFileInfo(path).absolutePath();
        m_undoStack->clear();
        forceReloadExternals();
        refreshEntityList();
        m_undoStack->resetClean();
        setWindowModified_(true);
        m_statusLabel->setText("Imported (replace): " + path);
        updateDiscord("Imported: " + QFileInfo(path).fileName());
        imported_ok = true;
    } else if (dlg.clickedButton() == mergeBtn) {
        if (m_baseDir.isEmpty()) m_baseDir = QFileInfo(path).absolutePath();
        int at = m_project.entities.size();
        QUndoCommand *macro = new QUndoCommand("Import FGD (merge)");
        new EntityInsertCommand(undoContext(), at, imported.entities, "Import entities", macro);
        FGDProjectSettings before = projectSettings(m_project);
        FGDProjectSettings after = before;
        for (const QString &inc : imported.includes)
            if (!after.includes.contains(inc)) after.includes << inc;
        after.rawBlocks << imported.rawBlocks;
        if (!(before == after)) new SettingsCommand(undoContext(), before, after, "Import settings", macro);
        m_undoStack->push(macro);
        m_statusLabel->setText("Imported (merge): " + path);
        imported_ok = true;
    }
    if (imported_ok && !report.isEmpty()) {
        ProblemsDialog rd(this, "Import Report",
            QString("%1 entities were imported from %2. Nothing is dropped silently: the lists below show what was kept as raw text, what was skipped, and which warnings the parser raised.")
                .arg(imported.entities.size()).arg(QFileInfo(path).fileName()),
            ProblemsDialog::fromImportReport(report));
        rd.exec();
    }
}

void MainWindow::saveProject() {
    if (m_filePath.isEmpty()) { saveProjectAs(); return; }
    saveToFile(m_filePath);
}

void MainWindow::saveProjectAs() {
    QString path = QFileDialog::getSaveFileName(this, "Save Project As", startDir("project"), "FGDBuilder Project (*.fgdbp);;All Files (*)");
    if (!path.isEmpty()) saveToFile(path);
}

QString MainWindow::defaultExportPath() const {
    if (!m_filePath.isEmpty()) {
        QFileInfo fi(m_filePath);
        return fi.absoluteDir().filePath(fi.baseName() + ".fgd");
    }
    QString dir = startDir("export");
    return dir.isEmpty() ? QString() : QDir(dir).filePath("output.fgd");
}

void MainWindow::exportFGD() {
    ensureExternals();
    QList<FGDIssue> all = FGDValidator::validate(m_project, &m_externals);
    QList<FGDIssue> serious;
    for (const FGDIssue &i : all)
        if (i.level != FGDIssue::Info) serious << i;
    if (!serious.isEmpty()) {
        ProblemsDialog dlg(this, "Validation Warnings",
            QString("%1 error(s) and %2 warning(s) were found. Export anyway?")
                .arg(FGDValidator::count(all, FGDIssue::Error)).arg(FGDValidator::count(all, FGDIssue::Warning)),
            ProblemsDialog::fromIssues(serious), "Export anyway");
        if (dlg.exec() != QDialog::Accepted) return;
    }
    QString path = QFileDialog::getSaveFileName(this, "Export FGD", defaultExportPath(), "Forge Game Data (*.fgd);;All Files (*)");
    if (path.isEmpty()) return;
    rememberDir("export", path);
    QString content = FGDGenerator::generate(m_project);
    QFile f(path);
    if (!f.open(QIODevice::WriteOnly | QIODevice::Text)) {
        QMessageBox::warning(this, "Error", "Cannot write file: " + path);
        return;
    }
    f.write(content.toUtf8());
    f.close();
    m_statusLabel->setText("Exported: " + path);
    QMessageBox::information(this, "Export Successful", "FGD exported to:\n" + path);
}

void MainWindow::checkProblems() {
    ensureExternals();
    QList<FGDIssue> all = FGDValidator::validate(m_project, &m_externals);
    QString summary = QString("%1 error(s), %2 warning(s), %3 information message(s).")
        .arg(FGDValidator::count(all, FGDIssue::Error))
        .arg(FGDValidator::count(all, FGDIssue::Warning))
        .arg(FGDValidator::count(all, FGDIssue::Info));
    ProblemsDialog dlg(this, "Check for Problems", summary, ProblemsDialog::fromIssues(all));
    dlg.exec();
}

QString MainWindow::uniqueClassName(const QString &wanted) const {
    QSet<QString> names;
    for (const FGDEntity &e : m_project.entities) names.insert(e.className);
    if (!names.contains(wanted)) return wanted;
    int n = 2;
    QString candidate = wanted;
    while (names.contains(candidate)) candidate = wanted + QString::number(n++);
    return candidate;
}

void MainWindow::addEntity() {
    FGDClassIndex index = buildIndex();
    EntityEditor dlg(this, FGDEntity(), knownBaseNames(index), &index);
    if (dlg.exec() != QDialog::Accepted) return;
    FGDEntity e = dlg.getEntity();
    if (e.className.isEmpty()) { QMessageBox::warning(this, "Warning", "Entity class name cannot be empty."); return; }
    int at = m_project.entities.size();
    m_undoStack->push(new EntityInsertCommand(undoContext(), at, {e}, "Add Entity"));
    selectRealIndexes({at});
}

void MainWindow::commitEntityEdit(int realIdx, const FGDEntity &edited, const QString &text) {
    const FGDEntity old = m_project.entities[realIdx];
    if (old == edited) return;
    QList<int> idxs{realIdx};
    QList<FGDEntity> before{old}, after{edited};
    FGDProjectSettings settingsBefore = projectSettings(m_project);
    FGDProjectSettings settingsAfter = settingsBefore;
    if (!old.className.isEmpty() && old.className != edited.className && old.classType != EntityClassType::ExtendClass) {
        for (int i = 0; i < m_project.entities.size(); ++i) {
            if (i == realIdx || !m_project.entities[i].bases.contains(old.className)) continue;
            FGDEntity copy = m_project.entities[i];
            copy.bases.replaceInStrings(QRegularExpression("^" + QRegularExpression::escape(old.className) + "$"), edited.className);
            idxs << i;
            before << m_project.entities[i];
            after << copy;
        }
        for (FGDAutoVisGroup &g : settingsAfter.autoVisGroups)
            for (FGDAutoVisGroupChild &c : g.children)
                renameInVisChild(c, old.className, edited.className);
    }
    bool settingsChanged = !(settingsBefore == settingsAfter);
    if (!settingsChanged && idxs.size() == 1) {
        m_undoStack->push(new EntityReplaceCommand(undoContext(), idxs, before, after, text));
        return;
    }
    QUndoCommand *macro = new QUndoCommand(text);
    new EntityReplaceCommand(undoContext(), idxs, before, after, text, macro);
    if (settingsChanged) new SettingsCommand(undoContext(), settingsBefore, settingsAfter, text, macro);
    m_undoStack->push(macro);
    QString note = QString("Renamed '%1' to '%2'").arg(old.className, edited.className);
    if (idxs.size() > 1) note += QString(", updated base() in %1 other class(es)").arg(idxs.size() - 1);
    if (settingsChanged) note += ", updated @AutoVisGroup";
    m_statusLabel->setText(note + ".");
}

void MainWindow::editEntity() {
    int realIdx = realIndexFromRow(m_entityList->currentRow());
    if (realIdx < 0) return;
    FGDClassIndex index = buildIndex();
    EntityEditor dlg(this, m_project.entities[realIdx], knownBaseNames(index), &index);
    if (dlg.exec() != QDialog::Accepted) return;
    FGDEntity e = dlg.getEntity();
    if (e.className.isEmpty()) { QMessageBox::warning(this, "Warning", "Entity class name cannot be empty."); return; }
    commitEntityEdit(realIdx, e, "Edit Entity");
    selectRealIndexes({realIdx});
}

void MainWindow::editEntityRaw() {
    int realIdx = realIndexFromRow(m_entityList->currentRow());
    if (realIdx < 0) return;
    RawEntityDialog dlg(this, m_project.entities[realIdx]);
    if (dlg.exec() != QDialog::Accepted) return;
    FGDEntity e = dlg.getEntity();
    if (e.className.isEmpty()) { QMessageBox::warning(this, "Warning", "Entity class name cannot be empty."); return; }
    commitEntityEdit(realIdx, e, "Edit Entity (raw)");
    selectRealIndexes({realIdx});
}

void MainWindow::removeEntity() {
    QList<int> sel = selectedRealIndexes();
    if (sel.isEmpty()) return;
    QString question = sel.size() == 1
        ? "Remove entity '" + m_project.entities[sel[0]].className + "'?"
        : QString("Remove %1 selected entities?").arg(sel.size());
    auto r = QMessageBox::question(this, "Remove Entity", question, QMessageBox::Yes | QMessageBox::No);
    if (r != QMessageBox::Yes) return;
    QList<FGDEntity> items;
    for (int i : sel) items << m_project.entities[i];
    m_undoStack->push(new EntityRemoveCommand(undoContext(), sel, items, sel.size() == 1 ? "Remove Entity" : "Remove Entities"));
}

void MainWindow::duplicateEntity() {
    QList<int> sel = selectedRealIndexes();
    if (sel.isEmpty()) return;
    QUndoCommand *macro = new QUndoCommand(sel.size() == 1 ? "Duplicate Entity" : "Duplicate Entities");
    QSet<QString> taken;
    for (const FGDEntity &e : m_project.entities) taken.insert(e.className);
    QList<int> newIndexes;
    for (int k = sel.size() - 1; k >= 0; --k) {
        FGDEntity copy = m_project.entities[sel[k]];
        QString wanted = copy.className + "_copy";
        QString name = wanted;
        int n = 2;
        while (taken.contains(name)) name = wanted + QString::number(n++);
        taken.insert(name);
        copy.className = name;
        new EntityInsertCommand(undoContext(), sel[k] + 1, {copy}, "Duplicate", macro);
    }
    for (int k = 0; k < sel.size(); ++k) newIndexes << sel[k] + k + 1;
    m_undoStack->push(macro);
    selectRealIndexes(newIndexes);
}

void MainWindow::addBaseToSelected() {
    QList<int> sel = selectedRealIndexes();
    if (sel.isEmpty()) {
        QMessageBox::information(this, "Add Base", "Select one or more entities first.");
        return;
    }
    FGDClassIndex index = buildIndex();
    ClassPickerDialog picker(this, "Add Base to " + QString::number(sel.size()) + " Entities",
        "Select the base classes to add to every selected entity (Ctrl and Shift select several).",
        knownBaseNames(index), true);
    if (picker.exec() != QDialog::Accepted) return;
    QStringList chosen = picker.selected();
    if (chosen.isEmpty()) return;
    QList<int> idxs;
    QList<FGDEntity> before, after;
    for (int i : sel) {
        FGDEntity copy = m_project.entities[i];
        for (const QString &b : chosen)
            if (!copy.bases.contains(b) && b != copy.className) copy.bases << b;
        if (copy == m_project.entities[i]) continue;
        idxs << i;
        before << m_project.entities[i];
        after << copy;
    }
    if (idxs.isEmpty()) {
        m_statusLabel->setText("All selected entities already have these bases.");
        return;
    }
    m_undoStack->push(new EntityReplaceCommand(undoContext(), idxs, before, after, "Add Base to Entities"));
    m_statusLabel->setText(QString("Added %1 base class(es) to %2 entities.").arg(chosen.size()).arg(idxs.size()));
}

void MainWindow::moveEntityUp() {
    int realIdx = realIndexFromRow(m_entityList->currentRow());
    if (realIdx <= 0) return;
    m_undoStack->push(new EntityMoveCommand(undoContext(), realIdx, realIdx - 1, "Move Entity Up"));
    selectRealIndexes({realIdx - 1});
}

void MainWindow::moveEntityDown() {
    int realIdx = realIndexFromRow(m_entityList->currentRow());
    if (realIdx < 0 || realIdx >= m_project.entities.size() - 1) return;
    m_undoStack->push(new EntityMoveCommand(undoContext(), realIdx, realIdx + 1, "Move Entity Down"));
    selectRealIndexes({realIdx + 1});
}

void MainWindow::sortEntitiesAZ() {
    QVector<int> order = FGDGenerator::sortPermutation(m_project.entities);
    bool identity = true;
    for (int i = 0; i < order.size(); ++i)
        if (order[i] != i) { identity = false; break; }
    if (identity) {
        m_statusLabel->setText("Entities are already sorted.");
        return;
    }
    m_undoStack->push(new EntityPermuteCommand(undoContext(), order, "Sort Entities A-Z"));
}

void MainWindow::onEntitySelectionChanged() {
    updatePreview();
    updateDiscord(currentFileLabel());
}

void MainWindow::updatePreview() {
    QList<int> sel = selectedRealIndexes();
    if (!sel.isEmpty()) {
        FGDProject part;
        for (int i : sel) part.entities.append(m_project.entities[i]);
        m_preview->setPlainText(FGDGenerator::generate(part).trimmed());
    } else {
        m_preview->setPlainText(FGDGenerator::generate(m_project));
    }
}

void MainWindow::commitSettings(const FGDProjectSettings &after, const QString &text) {
    FGDProjectSettings before = projectSettings(m_project);
    if (before == after) return;
    m_undoStack->push(new SettingsCommand(undoContext(), before, after, text));
}

void MainWindow::editIncludes() {
    IncludeManager dlg(this, m_project.includes, projectDir());
    if (dlg.exec() == QDialog::Accepted) {
        FGDProjectSettings s = projectSettings(m_project);
        s.includes = dlg.getIncludes();
        commitSettings(s, "Edit @include Files");
    }
}

void MainWindow::reloadIncludes() {
    forceReloadExternals();
    m_statusLabel->setText(QString("Reloaded @include files: %1 classes from %2 file(s), %3 warning(s).")
        .arg(m_externals.classes.size()).arg(m_externals.loadedFiles.size()).arg(m_externals.warnings.size()));
}

void MainWindow::editMaterialExclusions() {
    BaseClassManager dlg(this, m_project.materialExclusions);
    if (dlg.exec() == QDialog::Accepted) {
        FGDProjectSettings s = projectSettings(m_project);
        s.materialExclusions = dlg.getExclusions();
        commitSettings(s, "Edit @MaterialExclusion");
    }
}

void MainWindow::editAutoVisGroups() {
    FGDClassIndex index = buildIndex();
    AutoVisGroupEditor dlg(this, m_project.autoVisGroups, index.entityNames());
    if (dlg.exec() == QDialog::Accepted) {
        FGDProjectSettings s = projectSettings(m_project);
        s.autoVisGroups = dlg.getGroups();
        commitSettings(s, "Edit @AutoVisGroup");
    }
}

void MainWindow::editMapSize() {
    MapSizeDialog dlg(this, m_project.useMapsSize, m_project.mapsizeMin, m_project.mapsizeMax);
    if (dlg.exec() == QDialog::Accepted) {
        FGDProjectSettings s = projectSettings(m_project);
        s.useMapsSize = dlg.isEnabled();
        s.mapsizeMin = dlg.getMin();
        s.mapsizeMax = dlg.getMax();
        commitSettings(s, "Edit @mapsize");
    }
}

void MainWindow::editGridNav() {
    GridNavDialog dlg(this, m_project.useGridNav, m_project.gridNavEdgeSize, m_project.gridNavOffsetX,
                      m_project.gridNavOffsetY, m_project.gridNavTraceHeight);
    if (dlg.exec() == QDialog::Accepted) {
        FGDProjectSettings s = projectSettings(m_project);
        s.useGridNav = dlg.isEnabled();
        s.gridNavEdgeSize = dlg.edgeSize();
        s.gridNavOffsetX = dlg.offsetX();
        s.gridNavOffsetY = dlg.offsetY();
        s.gridNavTraceHeight = dlg.traceHeight();
        commitSettings(s, "Edit @gridnav");
    }
}

void MainWindow::editHeaderComment() {
    bool ok;
    QString text = QInputDialog::getMultiLineText(this, "Header Comment",
        "Comment lines at the top of the FGD file:", m_project.headerComment, &ok);
    if (ok) {
        FGDProjectSettings s = projectSettings(m_project);
        s.headerComment = text;
        commitSettings(s, "Edit Header Comment");
    }
}

void MainWindow::editFGDVersion() {
    bool ok;
    int v = QInputDialog::getInt(this, "@version",
        "FGD version number (0 = omit @version from output):",
        m_project.fgdVersion, 0, 9999, 1, &ok);
    if (ok) {
        FGDProjectSettings s = projectSettings(m_project);
        s.fgdVersion = v;
        commitSettings(s, "Edit @version");
    }
}

void MainWindow::showAbout() {
    QMessageBox::about(this, "About FGDBuilder",
        "<b>" + appVersionLabel() + "</b><br>"
        "Build date: " __DATE__ "<br><br>"
        "A tool for creating Forge Game Data (.fgd) files.<br><br>"
        "Supports @PointClass, @SolidClass, @NPCClass,<br>"
        "@BaseClass, @FilterClass, @KeyFrameClass, @MoveClass, @ExtendClass,<br>"
        "@include, @mapsize, @gridnav, @MaterialExclusion, @AutoVisGroup.<br><br>"
        "FGD files define entities for use in Valve's Hammer editor.<br><br>"
        "<a href='https://github.com/zud3k/fgdbuilder'>https://github.com/zud3k/fgdbuilder</a>");
}

void MainWindow::addRecentFile(const QString &path) {
    m_recentFiles.removeAll(path);
    m_recentFiles.prepend(path);
    while (m_recentFiles.size() > MaxRecentFiles) m_recentFiles.removeLast();
    updateRecentFilesMenu();
}

void MainWindow::updateRecentFilesMenu() {
    if (!m_recentMenu) return;
    m_recentMenu->clear();
    if (m_recentFiles.isEmpty()) {
        m_recentMenu->addAction("(empty)")->setEnabled(false);
        return;
    }
    for (const QString &path : m_recentFiles) {
        QFileInfo fi(path);
        QString label = fi.fileName() + "  [" + fi.absolutePath() + "]";
        QAction *a = m_recentMenu->addAction(label);
        a->setToolTip(path);
        connect(a, &QAction::triggered, this, [this, path]{ openRecentFile(path); });
    }
}

void MainWindow::openRecentFile(const QString &path) {
    if (!confirmDiscard()) return;
    if (!QFile::exists(path)) {
        QMessageBox::warning(this, "File Not Found", "File not found:\n" + path);
        m_recentFiles.removeAll(path);
        updateRecentFilesMenu();
        return;
    }
    loadFromFile(path);
}
