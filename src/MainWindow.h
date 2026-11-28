#pragma once
#include <QMainWindow>
#include <QListWidget>
#include <QTextEdit>
#include <QLabel>
#include <QSplitter>
#include <QLineEdit>
#include <QCheckBox>
#include <QUndoStack>
#include <QMenu>
#include "FGDData.h"
#include "FGDResolver.h"
#include "UndoCommands.h"

class MainWindow : public QMainWindow {
    Q_OBJECT
public:
    explicit MainWindow(QWidget *parent = nullptr);
protected:
    void closeEvent(QCloseEvent *event) override;
private slots:
    void newProject();
    void openProject();
    void importFGD();
    void saveProject();
    void saveProjectAs();
    void exportFGD();
    void addEntity();
    void editEntity();
    void editEntityRaw();
    void removeEntity();
    void duplicateEntity();
    void addBaseToSelected();
    void moveEntityUp();
    void moveEntityDown();
    void sortEntitiesAZ();
    void onEntitySelectionChanged();
    void editIncludes();
    void reloadIncludes();
    void editMaterialExclusions();
    void editAutoVisGroups();
    void editMapSize();
    void editGridNav();
    void editHeaderComment();
    void editFGDVersion();
    void checkProblems();
    void showAbout();
    void updatePreview();
    void onSearchTextChanged(const QString &text);
    void openRecentFile(const QString &path);
private:
    void setupUi();
    void setupMenuBar();
    void setupToolBar();
    void refreshEntityList();
    void setWindowModified_(bool modified);
    bool confirmDiscard();
    void loadFromFile(const QString &path);
    void saveToFile(const QString &path);
    void addRecentFile(const QString &path);
    void updateRecentFilesMenu();
    QString defaultExportPath() const;
    int realIndexFromRow(int row) const;
    QList<int> selectedRealIndexes() const;
    void selectRealIndexes(const QList<int> &indexes);
    UndoContext undoContext();
    void commitEntityEdit(int realIdx, const FGDEntity &edited, const QString &text);
    void commitSettings(const FGDProjectSettings &after, const QString &text);
    bool matchesSearch(const FGDEntity &e, const QString &filter, bool namesOnly) const;
    void ensureExternals();
    void forceReloadExternals();
    FGDClassIndex buildIndex();
    QStringList knownBaseNames(const FGDClassIndex &index) const;
    QString uniqueClassName(const QString &wanted) const;
    QString startDir(const QString &key) const;
    void rememberDir(const QString &key, const QString &path);
    QString projectDir() const;
    void updateDiscord(const QString &fileLabel);
    QString currentFileLabel() const;

    QListWidget *m_entityList;
    QTextEdit *m_preview;
    QLabel *m_statusLabel;
    QLineEdit *m_searchEdit;
    QCheckBox *m_namesOnly;
    FGDProject m_project;
    QString m_filePath;
    QString m_baseDir;
    FGDIncludeResult m_externals;
    QString m_externalsKey;
    QUndoStack *m_undoStack;
    QMenu *m_recentMenu;
    QStringList m_recentFiles;
    QSplitter *m_splitter = nullptr;
    bool m_discordEnabled = true;
    bool m_discordShowFile = true;
    static const int MaxRecentFiles = 10;
};
