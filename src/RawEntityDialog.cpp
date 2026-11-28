#include "RawEntityDialog.h"
#include "FGDGenerator.h"
#include "FGDHighlighter.h"
#include "FGDImporter.h"
#include <QDialogButtonBox>
#include <QFont>
#include <QSettings>
#include <QVBoxLayout>

RawEntityDialog::RawEntityDialog(QWidget *parent, const FGDEntity &entity)
    : QDialog(parent)
{
    setWindowTitle("Edit as raw FGD: " + entity.className);
    resize(860, 640);
    QVBoxLayout *layout = new QVBoxLayout(this);
    QLabel *hint = new QLabel("Edit the full FGD text of this class. It is parsed again when you press OK. "
                              "If the text has errors they are shown here and nothing is lost.");
    hint->setWordWrap(true);
    layout->addWidget(hint);

    m_edit = new QPlainTextEdit;
    QFont mono("Courier New", 10);
    mono.setStyleHint(QFont::Monospace);
    m_edit->setFont(mono);
    m_edit->setLineWrapMode(QPlainTextEdit::NoWrap);
    m_edit->setStyleSheet("QPlainTextEdit { background: #1e1e1e; color: #d4d4d4; }");
    m_edit->setPlainText(FGDGenerator::generateEntityText(entity).trimmed());
    new FGDHighlighter(m_edit->document());
    layout->addWidget(m_edit, 1);

    m_error = new QLabel;
    m_error->setWordWrap(true);
    m_error->setStyleSheet("QLabel { color: #d04040; }");
    m_error->hide();
    layout->addWidget(m_error);

    QDialogButtonBox *buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel);
    layout->addWidget(buttons);
    connect(buttons, &QDialogButtonBox::accepted, this, &RawEntityDialog::tryAccept);
    connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);

    QSettings settings("FGDBuilder", "FGDBuilder");
    restoreGeometry(settings.value("rawEntityDialogGeometry").toByteArray());
}

bool RawEntityDialog::parseEntity(const QString &text, FGDEntity &out, QString &error) {
    FGDImportReport report;
    QString err;
    FGDProject proj = FGDImporter::importFromString(text, err, &report);
    QStringList problems;
    for (const FGDImportMessage &m : report.messages) {
        if (m.kind == FGDImportMessage::Preserved) continue;
        if (m.line == 0 && m.entity.isEmpty()) continue;
        problems << "Line " + QString::number(m.line) + ": " + m.text;
    }
    if (!proj.rawBlocks.isEmpty() || !proj.includes.isEmpty() || !proj.autoVisGroups.isEmpty())
        problems << "Only one class definition is allowed here, other @ directives were found.";
    if (proj.entities.size() != 1)
        problems << QString("Expected exactly one class definition, found %1.").arg(proj.entities.size());
    if (!problems.isEmpty()) {
        error = problems.join("\n");
        return false;
    }
    out = proj.entities.first();
    return true;
}

void RawEntityDialog::tryAccept() {
    FGDEntity e;
    QString error;
    if (!parseEntity(m_edit->toPlainText(), e, error)) {
        m_error->setText(error);
        m_error->show();
        return;
    }
    m_result = e;
    QSettings settings("FGDBuilder", "FGDBuilder");
    settings.setValue("rawEntityDialogGeometry", saveGeometry());
    accept();
}
