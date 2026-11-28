#pragma once
#include <QDialog>

class QCheckBox;
class QSpinBox;

class GridNavDialog : public QDialog {
    Q_OBJECT
public:
    GridNavDialog(QWidget *parent, bool enabled, int edge, int offsetX, int offsetY, int trace);
    bool isEnabled() const;
    int edgeSize() const;
    int offsetX() const;
    int offsetY() const;
    int traceHeight() const;
private:
    QCheckBox *m_enabled;
    QSpinBox *m_edge;
    QSpinBox *m_offsetX;
    QSpinBox *m_offsetY;
    QSpinBox *m_trace;
};
