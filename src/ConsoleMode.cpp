#include "ConsoleMode.h"
#include "FGDData.h"
#include "FGDGenerator.h"
#include "FGDImporter.h"
#include "FGDProjectIO.h"
#include "FGDResolver.h"
#include "FGDValidator.h"
#include "Version.h"
#include <QFile>
#include <QFileInfo>
#include <QTextStream>
#include <cstring>

static const int ExitOk = 0;
static const int ExitValidation = 1;
static const int ExitUsage = 2;

bool ConsoleMode::wants(int argc, char **argv) {
    for (int i = 1; i < argc; ++i) {
        if (!strcmp(argv[i], "--export") || !strcmp(argv[i], "--check") || !strcmp(argv[i], "--help")
            || !strcmp(argv[i], "-h") || !strcmp(argv[i], "--version"))
            return true;
    }
    return false;
}

static QString usageText() {
    return
        "FGDBuilder " + appVersion() + "\n"
        "\n"
        "Usage:\n"
        "  fgdbuilder --export <input.fgdbp|input.fgd> <output.fgd> [-v]\n"
        "  fgdbuilder --check <file.fgdbp|file.fgd> [-v]\n"
        "  fgdbuilder --version\n"
        "  fgdbuilder --help\n"
        "\n"
        "--export reads a project or an FGD, validates it, and writes the FGD.\n"
        "--check reads a project or an FGD and prints the import report and validation results.\n"
        "-v also prints informational messages.\n"
        "@include files are searched in the folder of the input file.\n"
        "\n"
        "Exit codes: 0 no errors, 1 validation or syntax errors found, 2 read error or bad arguments.\n";
}

struct Loaded {
    FGDProject project;
    FGDImportReport report;
    bool fromFgd = false;
};

static bool loadInput(const QString &path, Loaded &out, QString &error) {
    QFileInfo fi(path);
    if (!fi.exists() || !fi.isFile()) {
        error = "File not found: " + path;
        return false;
    }
    if (fi.suffix().compare("fgdbp", Qt::CaseInsensitive) == 0) {
        return FGDProjectIO::load(path, out.project, error);
    }
    out.fromFgd = true;
    out.project = FGDImporter::importFromFile(path, error, &out.report);
    return error.isEmpty();
}

static QString where(const QString &file, int line) {
    return line > 0 ? file + ":" + QString::number(line) + ": " : file + ": ";
}

static int report(const Loaded &in, const QString &file, bool verbose, QTextStream &out) {
    int errors = 0, warnings = 0, infos = 0;
    for (const FGDImportMessage &m : in.report.messages) {
        QString level;
        if (m.kind == FGDImportMessage::Warning) { level = "error"; ++errors; }
        else if (m.kind == FGDImportMessage::Skipped && !(m.line == 0 && m.entity.isEmpty())) { level = "warning"; ++warnings; }
        else { level = "info"; ++infos; }
        if (level == "info" && !verbose) continue;
        QString tag = m.kind == FGDImportMessage::Preserved ? "kept as is" : (m.kind == FGDImportMessage::Skipped ? "skipped" : "syntax");
        out << where(file, m.line) << level << ": " << (m.entity.isEmpty() ? QString() : m.entity + ": ")
            << "[" << tag << "] " << m.text << "\n";
    }

    QFileInfo fi(file);
    QString baseDir = fi.absolutePath();
    FGDIncludeResult ext = FGDIncludes::load(in.project.includes, baseDir);
    QList<FGDIssue> issues = FGDValidator::validate(in.project, &ext);
    for (const FGDIssue &i : issues) {
        if (i.level == FGDIssue::Error) ++errors;
        else if (i.level == FGDIssue::Warning) ++warnings;
        else { ++infos; if (!verbose) continue; }
        QString who = i.entity;
        if (!i.property.isEmpty()) who += (who.isEmpty() ? "" : ".") + i.property;
        out << where(file, i.line) << FGDValidator::levelName(i.level) << ": " << (who.isEmpty() ? QString() : who + ": ") << i.message << "\n";
    }
    out << QString("%1: %2 entities, %3 error(s), %4 warning(s), %5 info\n")
        .arg(file).arg(in.project.entities.size()).arg(errors).arg(warnings).arg(infos);
    out.flush();
    return errors;
}

int ConsoleMode::run(const QStringList &args) {
    QTextStream out(stdout);
    QTextStream err(stderr);
    QStringList pos;
    bool verbose = false;
    QString mode;
    for (int i = 1; i < args.size(); ++i) {
        const QString &a = args[i];
        if (a == "-v" || a == "--verbose") verbose = true;
        else if (a == "--help" || a == "-h") { out << usageText(); return ExitOk; }
        else if (a == "--version") { out << "FGDBuilder " << appVersion() << "\n"; return ExitOk; }
        else if (a == "--export" || a == "--check") {
            if (!mode.isEmpty()) { err << "Only one of --export and --check can be used.\n"; return ExitUsage; }
            mode = a;
        }
        else if (a.startsWith("-")) { err << "Unknown option: " << a << "\n" << usageText(); return ExitUsage; }
        else pos << a;
    }
    if (mode == "--check") {
        if (pos.size() != 1) { err << "--check needs exactly one input file.\n" << usageText(); return ExitUsage; }
        Loaded in;
        QString error;
        if (!loadInput(pos[0], in, error)) { err << error << "\n"; return ExitUsage; }
        return report(in, pos[0], verbose, out) > 0 ? ExitValidation : ExitOk;
    }
    if (mode == "--export") {
        if (pos.size() != 2) { err << "--export needs an input file and an output file.\n" << usageText(); return ExitUsage; }
        Loaded in;
        QString error;
        if (!loadInput(pos[0], in, error)) { err << error << "\n"; return ExitUsage; }
        int errors = report(in, pos[0], verbose, out);
        QFile f(pos[1]);
        if (!f.open(QIODevice::WriteOnly | QIODevice::Text)) {
            err << "Cannot write file: " << pos[1] << "\n";
            return ExitUsage;
        }
        f.write(FGDGenerator::generate(in.project).toUtf8());
        f.close();
        out << "Exported: " << pos[1] << "\n";
        out.flush();
        return errors > 0 ? ExitValidation : ExitOk;
    }
    err << usageText();
    return ExitUsage;
}
