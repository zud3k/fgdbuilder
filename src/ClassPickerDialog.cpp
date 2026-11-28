#include "ClassPickerDialog.h"
#include <QDialogButtonBox>
#include <QLabel>
#include <QVBoxLayout>

ClassPickerDialog::ClassPickerDialog(QWidget *parent, const QString &title, const QString &hint,
                                     const QStringList &names, bool multiSelect)
    : QDialog(parent)
{
    setWindowTitle(title);
    setMinimumSize(380, 460);
    QVBoxLayout *layout = new QVBoxLayout(this);
    QLabel *label = new QLabel(hint);
    label->setWordWrap(true);
    layout->addWidget(label);
    m_filter = new QLineEdit;
    m_filter->setPlaceholderText("Filter...");
    layout->addWidget(m_filter);
    m_list = new QListWidget;
    m_list->setSelectionMode(multiSelect ? QAbstractItemView::ExtendedSelection : QAbstractItemView::SingleSelection);
    m_list->addItems(names);
    layout->addWidget(m_list, 1);
    QDialogButtonBox *buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel);
    layout->addWidget(buttons);
    connect(m_filter, &QLineEdit::textChanged, this, &ClassPickerDialog::applyFilter);
    connect(m_list, &QListWidget::itemDoubleClicked, this, [this, multiSelect](QListWidgetItem *) {
        if (!multiSelect) accept();
    });
    connect(buttons, &QDialogButtonBox::accepted, this, &QDialog::accept);
    connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);
}

void ClassPickerDialog::applyFilter(const QString &text) {
    QString f = text.trimmed().toLower();
    for (int i = 0; i < m_list->count(); ++i)
        m_list->item(i)->setHidden(!f.isEmpty() && !m_list->item(i)->text().toLower().contains(f));
}

QStringList ClassPickerDialog::selected() const {
    QStringList result;
    for (QListWidgetItem *it : m_list->selectedItems()) result << it->text();
    return result;
}
