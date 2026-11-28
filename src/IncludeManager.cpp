#include "IncludeManager.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QPushButton>
#include <QListWidget>
#include <QDialogButtonBox>
#include <QLabel>
#include <QFileDialog>
#include <QFileInfo>
#include <QSettings>

IncludeManager::IncludeManager(QWidget *parent, const QStringList &includes, const QString &baseDir)
    : QDialog(parent), m_baseDir(baseDir)
{
    setWindowTitle("Manage @include Files");
    setMinimumWidth(380);
    QVBoxLayout *layout = new QVBoxLayout(this);
    layout->addWidget(new QLabel("FGD files to include (e.g. base.fgd). They are looked up in the project folder or the folder of the imported FGD:"));
    m_list = new QListWidget;
    for (auto &inc : includes) m_list->addItem(inc);
    layout->addWidget(m_list);
    m_input = new QLineEdit;
    m_input->setPlaceholderText("filename.fgd");
    QHBoxLayout *row = new QHBoxLayout;
    QPushButton *addBtn = new QPushButton("Add");
    QPushButton *removeBtn = new QPushButton("Remove");
    QPushButton *browseBtn = new QPushButton("Browse...");
    browseBtn->setToolTip("Pick an FGD file. Its name is added if it is in the project folder, otherwise its full path.");
    row->addWidget(m_input);
    row->addWidget(browseBtn);
    row->addWidget(addBtn);
    row->addWidget(removeBtn);
    layout->addLayout(row);
    QDialogButtonBox *buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel);
    layout->addWidget(buttons);
    connect(browseBtn, &QPushButton::clicked, this, [this]{
        QSettings settings("FGDBuilder", "FGDBuilder");
        QString start = m_baseDir.isEmpty() ? settings.value("lastDir/fgd").toString() : m_baseDir;
        QString path = QFileDialog::getOpenFileName(this, "Select FGD to include", start, "Forge Game Data (*.fgd);;All Files (*)");
        if (path.isEmpty()) return;
        QFileInfo fi(path);
        QString value = (!m_baseDir.isEmpty() && fi.absolutePath() == QFileInfo(m_baseDir).absoluteFilePath()) ? fi.fileName() : path;
        m_list->addItem(value);
    });
    connect(addBtn, &QPushButton::clicked, this, &IncludeManager::addInclude);
    connect(removeBtn, &QPushButton::clicked, this, &IncludeManager::removeInclude);
    connect(buttons, &QDialogButtonBox::accepted, this, &QDialog::accept);
    connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);
}

void IncludeManager::addInclude() {
    QString s = m_input->text().trimmed();
    if (!s.isEmpty()) { m_list->addItem(s); m_input->clear(); }
}

void IncludeManager::removeInclude() {
    int row = m_list->currentRow();
    if (row >= 0) delete m_list->takeItem(row);
}

QStringList IncludeManager::getIncludes() const {
    QStringList result;
    for (int i = 0; i < m_list->count(); ++i) result << m_list->item(i)->text();
    return result;
}
