#pragma once
#include <QDialog>
#include <QLineEdit>
#include <QList>
#include <QString>
#include <QTreeWidget>
#include "FGDImporter.h"
#include "FGDValidator.h"

struct ProblemRow {
    QString category;
    int line = 0;
    QString entity;
    QString property;
    QString message;
};

class ProblemsDialog : public QDialog {
    Q_OBJECT
public:
    ProblemsDialog(QWidget *parent, const QString &title, const QString &summary,
                   const QList<ProblemRow> &rows, const QString &acceptText = QString());
    static QList<ProblemRow> fromIssues(const QList<FGDIssue> &issues);
    static QList<ProblemRow> fromImportReport(const FGDImportReport &report);
private slots:
    void applyFilter(const QString &text);
    void copyAll();
private:
    QTreeWidget *m_tree;
    QLineEdit *m_filter;
    QList<ProblemRow> m_rows;
    QStringList m_categories;
    void populate(const QString &filter);
};
