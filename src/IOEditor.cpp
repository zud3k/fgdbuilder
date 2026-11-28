#include "IOEditor.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QFormLayout>
#include <QGroupBox>
#include <QPushButton>
#include <QDialogButtonBox>

IOEditor::IOEditor(QWidget *parent, const FGDIO &io)
    : QDialog(parent)
{
    setWindowTitle("Input / Output Editor");
    setMinimumWidth(400);
    setupUi();
    m_line = io.line;
    m_input->setChecked(io.isInput);
    m_output->setChecked(!io.isInput);
    m_name->setText(io.name);
    if (!io.type.isEmpty()) m_type->setCurrentText(io.type);
    m_description->setText(io.description);
}

void IOEditor::setupUi() {
    QVBoxLayout *mainLayout = new QVBoxLayout(this);
    QGroupBox *dirGroup = new QGroupBox("Direction");
    QHBoxLayout *dirLayout = new QHBoxLayout(dirGroup);
    m_input = new QRadioButton("Input");
    m_output = new QRadioButton("Output");
    m_input->setChecked(true);
    dirLayout->addWidget(m_input);
    dirLayout->addWidget(m_output);
    dirLayout->addStretch();
    mainLayout->addWidget(dirGroup);
    QFormLayout *form = new QFormLayout;
    m_name = new QLineEdit;
    m_type = new QComboBox;
    m_type->setEditable(true);
    m_type->setInsertPolicy(QComboBox::NoInsert);
    m_type->addItems({"void","string","integer","float","bool","ehandle","target_destination","color255","vector"});
    m_type->setToolTip("Value type passed by this input or output.\nUse bool here, boolean is only for keyvalues (the Garry's Mod FGD uses boolean for I/O by mistake).\nYou can type a type that is not in the list, it is written to the FGD as is.");
    m_description = new QLineEdit;
    form->addRow("Name:", m_name);
    form->addRow("Type:", m_type);
    form->addRow("Description:", m_description);
    mainLayout->addLayout(form);
    QDialogButtonBox *buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel);
    mainLayout->addWidget(buttons);
    connect(buttons, &QDialogButtonBox::accepted, this, &QDialog::accept);
    connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);
}

FGDIO IOEditor::getIO() const {
    FGDIO io;
    io.isInput = m_input->isChecked();
    io.name = m_name->text().trimmed();
    io.type = m_type->currentText().trimmed();
    io.line = m_line;
    io.description = m_description->text();
    return io;
}
