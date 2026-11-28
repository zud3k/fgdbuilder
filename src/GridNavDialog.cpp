#include "GridNavDialog.h"
#include <QCheckBox>
#include <QDialogButtonBox>
#include <QFormLayout>
#include <QLabel>
#include <QSpinBox>
#include <QVBoxLayout>

GridNavDialog::GridNavDialog(QWidget *parent, bool enabled, int edge, int offsetX, int offsetY, int trace)
    : QDialog(parent)
{
    setWindowTitle("@gridnav Settings");
    QVBoxLayout *layout = new QVBoxLayout(this);
    m_enabled = new QCheckBox("Include @gridnav in output");
    m_enabled->setChecked(enabled);
    m_enabled->setToolTip("Enables building and saving the grid navigation file (Hammer 4, Dota 2).\nDefaults from dota.fgd: 64, 32, 32, 3500.");
    layout->addWidget(m_enabled);
    QFormLayout *form = new QFormLayout;
    m_edge = new QSpinBox;
    m_edge->setRange(1, 1000000);
    m_edge->setValue(edge);
    m_offsetX = new QSpinBox;
    m_offsetX->setRange(-1000000, 1000000);
    m_offsetX->setValue(offsetX);
    m_offsetY = new QSpinBox;
    m_offsetY->setRange(-1000000, 1000000);
    m_offsetY->setValue(offsetY);
    m_trace = new QSpinBox;
    m_trace->setRange(0, 1000000);
    m_trace->setValue(trace);
    form->addRow("Edge size:", m_edge);
    form->addRow("Offset X:", m_offsetX);
    form->addRow("Offset Y:", m_offsetY);
    form->addRow("Trace height:", m_trace);
    layout->addLayout(form);
    QDialogButtonBox *buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel);
    layout->addWidget(buttons);
    connect(buttons, &QDialogButtonBox::accepted, this, &QDialog::accept);
    connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);
}

bool GridNavDialog::isEnabled() const { return m_enabled->isChecked(); }
int GridNavDialog::edgeSize() const { return m_edge->value(); }
int GridNavDialog::offsetX() const { return m_offsetX->value(); }
int GridNavDialog::offsetY() const { return m_offsetY->value(); }
int GridNavDialog::traceHeight() const { return m_trace->value(); }
