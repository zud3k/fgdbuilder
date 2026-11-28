#include "ProblemsDialog.h"
#include <QApplication>
#include <QClipboard>
#include <QDialogButtonBox>
#include <QHeaderView>
#include <QLabel>
#include <QPushButton>
#include <QVBoxLayout>

ProblemsDialog::ProblemsDialog(QWidget *parent, const QString &title, const QString &summary,
                               const QList<ProblemRow> &rows, const QString &acceptText)
    : QDialog(parent), m_rows(rows)
{
    setWindowTitle(title);
    resize(900, 520);
    QVBoxLayout *layout = new QVBoxLayout(this);
    QLabel *label = new QLabel(summary);
    label->setWordWrap(true);
    layout->addWidget(label);

    m_filter = new QLineEdit;
    m_filter->setPlaceholderText("Filter by entity, property or message...");
    layout->addWidget(m_filter);

    m_tree = new QTreeWidget;
    m_tree->setHeaderLabels({"Line", "Entity", "Property", "Message"});
    m_tree->setRootIsDecorated(true);
    m_tree->setAlternatingRowColors(true);
    m_tree->header()->setStretchLastSection(true);
    m_tree->setColumnWidth(0, 60);
    m_tree->setColumnWidth(1, 180);
    m_tree->setColumnWidth(2, 150);
    layout->addWidget(m_tree, 1);

    for (const ProblemRow &r : m_rows)
        if (!m_categories.contains(r.category)) m_categories << r.category;

    QDialogButtonBox *buttons = new QDialogButtonBox;
    QPushButton *copyBtn = buttons->addButton("Copy to clipboard", QDialogButtonBox::ActionRole);
    if (acceptText.isEmpty()) {
        buttons->addButton(QDialogButtonBox::Close);
        connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);
    } else {
        buttons->addButton(acceptText, QDialogButtonBox::AcceptRole);
        buttons->addButton(QDialogButtonBox::Cancel);
        connect(buttons, &QDialogButtonBox::accepted, this, &QDialog::accept);
        connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);
    }
    connect(copyBtn, &QPushButton::clicked, this, &ProblemsDialog::copyAll);
    layout->addWidget(buttons);

    connect(m_filter, &QLineEdit::textChanged, this, &ProblemsDialog::applyFilter);
    populate(QString());
}

void ProblemsDialog::populate(const QString &filter) {
    m_tree->clear();
    QString f = filter.trimmed().toLower();
    for (const QString &cat : m_categories) {
        QTreeWidgetItem *top = new QTreeWidgetItem;
        int count = 0;
        for (const ProblemRow &r : m_rows) {
            if (r.category != cat) continue;
            if (!f.isEmpty() && !(r.entity.toLower().contains(f) || r.property.toLower().contains(f) || r.message.toLower().contains(f))) continue;
            QTreeWidgetItem *item = new QTreeWidgetItem;
            item->setText(0, r.line > 0 ? QString::number(r.line) : QString());
            item->setText(1, r.entity);
            item->setText(2, r.property);
            item->setText(3, r.message);
            item->setToolTip(3, r.message);
            top->addChild(item);
            ++count;
        }
        if (count == 0) { delete top; continue; }
        top->setText(0, cat + " (" + QString::number(count) + ")");
        QFont fnt = top->font(0);
        fnt.setBold(true);
        top->setFont(0, fnt);
        m_tree->addTopLevelItem(top);
        top->setFirstColumnSpanned(true);
        top->setExpanded(true);
    }
}

void ProblemsDialog::applyFilter(const QString &text) {
    populate(text);
}

void ProblemsDialog::copyAll() {
    QStringList lines;
    for (const ProblemRow &r : m_rows) {
        QString who = r.entity;
        if (!r.property.isEmpty()) who += "." + r.property;
        lines << r.category + (r.line > 0 ? " line " + QString::number(r.line) : QString()) + ": " + (who.isEmpty() ? QString() : who + ": ") + r.message;
    }
    QApplication::clipboard()->setText(lines.join("\n"));
}

QList<ProblemRow> ProblemsDialog::fromIssues(const QList<FGDIssue> &issues) {
    QList<ProblemRow> rows;
    static const FGDIssue::Level order[] = { FGDIssue::Error, FGDIssue::Warning, FGDIssue::Info };
    for (FGDIssue::Level lv : order) {
        for (const FGDIssue &i : issues) {
            if (i.level != lv) continue;
            ProblemRow r;
            r.category = lv == FGDIssue::Error ? "Errors" : (lv == FGDIssue::Warning ? "Warnings" : "Information");
            r.line = i.line;
            r.entity = i.entity;
            r.property = i.property;
            r.message = i.message;
            rows << r;
        }
    }
    return rows;
}

QList<ProblemRow> ProblemsDialog::fromImportReport(const FGDImportReport &report) {
    QList<ProblemRow> rows;
    static const FGDImportMessage::Kind order[] = { FGDImportMessage::Preserved, FGDImportMessage::Skipped, FGDImportMessage::Warning };
    for (FGDImportMessage::Kind k : order) {
        for (const FGDImportMessage &m : report.messages) {
            if (m.kind != k) continue;
            ProblemRow r;
            r.category = k == FGDImportMessage::Preserved ? "Kept as is" : (k == FGDImportMessage::Skipped ? "Skipped" : "Warnings");
            r.line = m.line;
            r.entity = m.entity;
            r.message = m.text;
            rows << r;
        }
    }
    return rows;
}
