#pragma once
#include <QDialog>
#include <QLineEdit>
#include <QListWidget>
#include <QStringList>

class ClassPickerDialog : public QDialog {
    Q_OBJECT
public:
    ClassPickerDialog(QWidget *parent, const QString &title, const QString &hint,
                      const QStringList &names, bool multiSelect);
    QStringList selected() const;
private slots:
    void applyFilter(const QString &text);
private:
    QListWidget *m_list;
    QLineEdit *m_filter;
};
