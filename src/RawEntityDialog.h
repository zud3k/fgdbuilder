#pragma once
#include <QDialog>
#include <QLabel>
#include <QPlainTextEdit>
#include "FGDData.h"

class RawEntityDialog : public QDialog {
    Q_OBJECT
public:
    RawEntityDialog(QWidget *parent, const FGDEntity &entity);
    FGDEntity getEntity() const { return m_result; }
    static bool parseEntity(const QString &text, FGDEntity &out, QString &error);
private slots:
    void tryAccept();
private:
    QPlainTextEdit *m_edit;
    QLabel *m_error;
    FGDEntity m_result;
};
