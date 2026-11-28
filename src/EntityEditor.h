#pragma once
#include <QDialog>
#include <QLineEdit>
#include <QComboBox>
#include <QCheckBox>
#include <QLabel>
#include <QListWidget>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QTableWidget>
#include <QTextEdit>
#include "FGDData.h"
#include "FGDResolver.h"

class EntityEditor : public QDialog {
    Q_OBJECT
public:
    explicit EntityEditor(QWidget *parent = nullptr, const FGDEntity &entity = FGDEntity(),
                          const QStringList &knownBases = {}, const FGDClassIndex *index = nullptr);
    FGDEntity getEntity() const;
    void accept() override;
private slots:
    void addKeyvalue();
    void editKeyvalue();
    void duplicateKeyvalue();
    void removeKeyvalue();
    void moveKeyvalue(int delta);
    void addIO();
    void editIO();
    void duplicateIO();
    void removeIO();
    void moveIO(int delta);
    void addBase();
    void removeBase();
    void refreshInherited();
    void filterKeyvalues(const QString &text);
    void filterIOs(const QString &text);
private:
    struct HelperRow {
        QString name;
        bool quoted;
        QCheckBox *check;
        QLineEdit *edit;
    };

    void setupUi();
    void populateFrom(const FGDEntity &entity);
    void refreshKvTable(int select = -1);
    void refreshIoTable(int select = -1);
    void addBasePreset(const QString &name);
    void addIoPreset(const QList<FGDIO> &items);
    void markBaseItem(QListWidgetItem *item);
    QStringList currentBases() const;
    QStringList rawHelperLines() const;
    static QString helperDisplay(const QString &args, bool quoted);
    static QString helperArgs(const QString &text, bool quoted);
    static bool parseHelperLine(const QString &line, FGDHelper &out);

    FGDEntity m_original;
    const FGDClassIndex *m_index = nullptr;
    QComboBox *m_classType;
    QLineEdit *m_className;
    QTextEdit *m_description;
    QLineEdit *m_url;
    QLineEdit *m_color;
    QLineEdit *m_size;
    QCheckBox *m_halfGridSnap;
    QList<HelperRow> m_helperRows;
    QList<QCheckBox *> m_flagHelpers;
    QPlainTextEdit *m_rawHelpers;
    QComboBox *m_baseCombo;
    QListWidget *m_baseList;
    QTableWidget *m_kvTable;
    QTableWidget *m_ioTable;
    QTableWidget *m_inheritedTable;
    QLabel *m_inheritedInfo;
    QLineEdit *m_kvSearch;
    QLineEdit *m_ioSearch;
    QPushButton *m_kvUp;
    QPushButton *m_kvDown;
    QPushButton *m_ioUp;
    QPushButton *m_ioDown;
    QList<FGDKeyvalue> m_keyvalues;
    QList<FGDIO> m_ios;
    QStringList m_knownBases;
};
