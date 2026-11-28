#include "FGDImporter.h"
#include <QFile>
#include <QSet>

static bool isDelimiter(QChar c) {
    switch (c.unicode()) {
        case '@': case '[': case ']': case '(': case ')': case '{': case '}':
        case '=': case ',': case ':': case '+': case '"':
            return true;
        default:
            return false;
    }
}

static bool looksNumeric(const QString &w) {
    int i = 0;
    int n = w.length();
    if (i < n && w[i] == '-') ++i;
    int digits = 0;
    while (i < n && w[i].isDigit()) { ++i; ++digits; }
    if (digits == 0) return false;
    if (i < n && w[i] == '.') {
        ++i;
        while (i < n && w[i].isDigit()) ++i;
    }
    return i == n;
}

static const QSet<QString> &guiHelperNames() {
    static const QSet<QString> names = {
        "base", "color", "size", "studio", "studioprop", "iconsprite", "sphere", "line",
        "vecline", "axis", "wirebox", "origin", "lightprop", "decal", "overlay", "sprite",
        "sweptplayerhull", "instance", "animator", "keyframe", "worldtext", "halfgridsnap"
    };
    return names;
}

QString FGDImporter::normalizeArgs(const QString &raw) {
    QString out;
    bool inQuote = false;
    bool pendingSpace = false;
    for (int i = 0; i < raw.length(); ++i) {
        QChar c = raw[i];
        if (inQuote) {
            out += c;
            if (c == '"') inQuote = false;
            continue;
        }
        if (c.isSpace()) {
            pendingSpace = !out.isEmpty();
            continue;
        }
        if (pendingSpace) { out += ' '; pendingSpace = false; }
        out += c;
        if (c == '"') inQuote = true;
    }
    return out;
}

void FGDImporter::tokenize() {
    int i = 0;
    int len = m_src.length();
    int line = 1;
    m_tokens.clear();
    auto push = [&](Token::Type t, int s, int e, int ln, const QString &v) {
        Token tok;
        tok.type = t;
        tok.value = v;
        tok.start = s;
        tok.end = e;
        tok.line = ln;
        m_tokens.append(tok);
    };
    while (i < len) {
        QChar c = m_src[i];
        if (c == '\n') { ++line; ++i; continue; }
        if (c.isSpace()) { ++i; continue; }
        if (c == '/' && i + 1 < len && m_src[i + 1] == '/') {
            ++m_commentLines;
            while (i < len && m_src[i] != '\n') ++i;
            continue;
        }
        Token::Type single = Token::Eof;
        switch (c.unicode()) {
            case '@': single = Token::At; break;
            case '[': single = Token::LBracket; break;
            case ']': single = Token::RBracket; break;
            case '(': single = Token::LParen; break;
            case ')': single = Token::RParen; break;
            case '{': single = Token::LBrace; break;
            case '}': single = Token::RBrace; break;
            case '=': single = Token::Equals; break;
            case ',': single = Token::Comma; break;
            case ':': single = Token::Colon; break;
            case '+': single = Token::Plus; break;
            default: break;
        }
        if (single != Token::Eof) {
            push(single, i, i + 1, line, QString(c));
            ++i;
            continue;
        }
        if (c == '"') {
            int s = i;
            int startLine = line;
            ++i;
            int vs = i;
            while (i < len && m_src[i] != '"') {
                if (m_src[i] == '\n') ++line;
                ++i;
            }
            QString v = m_src.mid(vs, i - vs);
            bool closed = i < len;
            if (closed) ++i;
            push(Token::String, s, i, startLine, v);
            if (!closed) note(FGDImportMessage::Warning, startLine, "Unterminated string, the rest of the file was read as its content.");
            continue;
        }
        int s = i;
        while (i < len && !m_src[i].isSpace() && !isDelimiter(m_src[i])
               && !(m_src[i] == '/' && i + 1 < len && m_src[i + 1] == '/')) ++i;
        if (i == s) { ++i; continue; }
        QString w = m_src.mid(s, i - s);
        push(looksNumeric(w) ? Token::Number : Token::Word, s, i, line, w);
    }
    push(Token::Eof, len, len, line, QString());
}

const FGDImporter::Token &FGDImporter::peek(int offset) const {
    int idx = m_pos + offset;
    if (idx >= m_tokens.size()) return m_tokens.last();
    return m_tokens[idx];
}

bool FGDImporter::is(Token::Type t, int offset) const {
    return peek(offset).type == t;
}

bool FGDImporter::isWord(const QString &lower, int offset) const {
    return peek(offset).type == Token::Word && peek(offset).value.toLower() == lower;
}

FGDImporter::Token FGDImporter::consume() {
    if (m_pos >= m_tokens.size()) return m_tokens.last();
    Token t = m_tokens[m_pos];
    if (t.type != Token::Eof) ++m_pos;
    return t;
}

bool FGDImporter::atEnd() const {
    return peek().type == Token::Eof;
}

QString FGDImporter::rawText(int fromTok, int toTok) const {
    if (fromTok >= toTok || fromTok < 0 || toTok > m_tokens.size()) return QString();
    int s = m_tokens[fromTok].start;
    int e = m_tokens[toTok - 1].end;
    return m_src.mid(s, e - s);
}

QString FGDImporter::excerpt(int fromTok, int toTok) const {
    QString t = normalizeArgs(rawText(fromTok, toTok));
    if (t.length() > 80) t = t.left(77) + "...";
    return t;
}

void FGDImporter::note(FGDImportMessage::Kind kind, int line, const QString &text) {
    if (m_report) m_report->add(kind, line, m_entity, text);
}

bool FGDImporter::readParenArgs(QString &args, int &firstInner, int &lastInner) {
    args.clear();
    firstInner = lastInner = m_pos;
    if (!is(Token::LParen)) return false;
    int open = m_pos;
    consume();
    int depth = 1;
    firstInner = m_pos;
    while (!atEnd() && !is(Token::At)) {
        if (is(Token::LParen)) ++depth;
        else if (is(Token::RParen)) {
            --depth;
            if (depth == 0) break;
        }
        consume();
    }
    lastInner = m_pos;
    if (is(Token::RParen)) {
        consume();
        args = normalizeArgs(rawText(firstInner, lastInner));
        return true;
    }
    note(FGDImportMessage::Warning, m_tokens[open].line, "Missing ')' for the '(' on this line.");
    args = normalizeArgs(rawText(firstInner, lastInner));
    return false;
}

bool FGDImporter::startsMember() const {
    if (!is(Token::Word)) return false;
    if (is(Token::LParen, 1)) return true;
    QString lw = peek().value.toLower();
    if ((lw == "input" || lw == "output") && is(Token::Word, 1)) return true;
    if (lw == "key" && is(Token::Word, 1) && is(Token::LParen, 2)) return true;
    return false;
}

void FGDImporter::skipUnexpected(const QString &where) {
    int s = m_pos;
    int line = peek().line;
    consume();
    while (!atEnd() && !is(Token::RBracket) && !is(Token::At) && !startsMember()) consume();
    note(FGDImportMessage::Skipped, line, where + ": skipped unrecognized text: " + excerpt(s, m_pos));
}

int FGDImporter::readInts(QList<int> &out) {
    out.clear();
    if (!is(Token::LParen)) return 0;
    consume();
    while (!atEnd() && !is(Token::RParen) && !is(Token::At)) {
        if (is(Token::Number)) out << consume().value.toInt();
        else if (is(Token::Comma)) consume();
        else {
            note(FGDImportMessage::Skipped, peek().line, "Unexpected '" + peek().value + "' in numeric arguments.");
            consume();
        }
    }
    if (is(Token::RParen)) consume();
    return out.size();
}

QString FGDImporter::parseDescription() {
    QString result;
    while (is(Token::String)) {
        result += consume().value;
        if (is(Token::Plus)) {
            consume();
            if (!is(Token::String))
                note(FGDImportMessage::Warning, peek().line, "'+' is not followed by a string.");
        } else {
            break;
        }
    }
    return result;
}

void FGDImporter::parseHelper(FGDEntity &e) {
    Token nameTok = consume();
    QString lname = nameTok.value.toLower();
    if (!is(Token::LParen)) {
        if (lname == "halfgridsnap") {
            e.halfGridSnap = true;
            return;
        }
        FGDHelper h;
        h.name = nameTok.value;
        h.parens = false;
        e.helpers.append(h);
        if (!guiHelperNames().contains(lname))
            note(FGDImportMessage::Preserved, nameTok.line, "Helper '" + nameTok.value + "' kept as raw text.");
        return;
    }
    QString args;
    int first = 0, last = 0;
    readParenArgs(args, first, last);
    if (lname == "base") {
        for (int i = first; i < last; ++i) {
            const Token &t = m_tokens[i];
            if (t.type == Token::Word || t.type == Token::Number) e.bases << t.value;
            else if (t.type == Token::Comma) continue;
            else note(FGDImportMessage::Skipped, t.line, "Unexpected '" + t.value + "' inside base().");
        }
        return;
    }
    if (lname == "color") { e.color = args; return; }
    if (lname == "size")  { e.size = args; return; }
    FGDHelper h;
    h.name = nameTok.value;
    h.args = args;
    h.parens = true;
    e.helpers.append(h);
    if (!guiHelperNames().contains(lname))
        note(FGDImportMessage::Preserved, nameTok.line, "Helper '" + nameTok.value + "(" + args + ")' kept as raw text.");
}

bool FGDImporter::parseClassHeader(FGDEntity &e) {
    while (true) {
        if (atEnd() || is(Token::At)) {
            note(FGDImportMessage::Warning, e.line, "Class header is not terminated by '=', definition dropped.");
            return false;
        }
        if (is(Token::Equals)) break;
        if (is(Token::LBracket)) {
            note(FGDImportMessage::Warning, peek().line, "Class header has no '=' and class name, definition dropped.");
            return false;
        }
        if (is(Token::Word)) {
            parseHelper(e);
        } else {
            int s = m_pos;
            int line = peek().line;
            consume();
            note(FGDImportMessage::Skipped, line, "Skipped unrecognized text in class header: " + excerpt(s, m_pos));
        }
    }
    consume();
    if (is(Token::Word) || is(Token::Number) || is(Token::String)) {
        e.className = consume().value;
    } else {
        note(FGDImportMessage::Warning, peek().line, "Class name is missing after '=', definition dropped.");
        return false;
    }
    m_entity = e.className;
    if (is(Token::Colon)) {
        consume();
        if (is(Token::String)) e.description = parseDescription();
        if (is(Token::Colon)) {
            consume();
            if (is(Token::String)) e.url = consume().value;
            else note(FGDImportMessage::Warning, peek().line, "Expected a URL string after the second ':'.");
        }
    }
    return true;
}

bool FGDImporter::parseIO(FGDIO &io) {
    Token kw = consume();
    io.line = kw.line;
    io.isInput = (kw.value.toLower() == "input");
    if (!is(Token::Word)) return false;
    io.name = consume().value;
    if (!is(Token::LParen)) {
        note(FGDImportMessage::Warning, kw.line, "I/O '" + io.name + "' has no type in parentheses.");
        return true;
    }
    consume();
    QString type;
    while (!atEnd() && !is(Token::RParen) && !is(Token::At)) type += consume().value;
    if (is(Token::RParen)) consume();
    io.type = type.toLower();
    if (is(Token::Colon)) {
        consume();
        io.description = parseDescription();
    }
    return true;
}

void FGDImporter::parseChoiceItems(FGDKeyvalue &kv) {
    while (!atEnd() && !is(Token::RBracket)) {
        if (is(Token::At)) {
            note(FGDImportMessage::Warning, peek().line, "Choices list of '" + kv.name + "' is not closed with ']'.");
            return;
        }
        if (!(is(Token::Number) || is(Token::String) || is(Token::Word))) {
            int s = m_pos;
            int line = peek().line;
            consume();
            note(FGDImportMessage::Skipped, line, "Skipped unrecognized text in choices of '" + kv.name + "': " + excerpt(s, m_pos));
            continue;
        }
        QString value = consume().value;
        QString label;
        QString desc;
        if (is(Token::Colon)) {
            consume();
            if (is(Token::String) || is(Token::Number) || is(Token::Word)) label = consume().value;
            if (is(Token::Colon)) {
                consume();
                desc = parseDescription();
            }
        }
        kv.choiceItems.append({value, label});
        kv.choiceDescriptions.append({value, desc});
    }
    if (is(Token::RBracket)) consume();
    else note(FGDImportMessage::Warning, peek().line, "Choices list of '" + kv.name + "' is not closed with ']'.");
}

void FGDImporter::parseFlagItems(FGDKeyvalue &kv) {
    while (!atEnd() && !is(Token::RBracket)) {
        if (is(Token::At)) {
            note(FGDImportMessage::Warning, peek().line, "Flags list of '" + kv.name + "' is not closed with ']'.");
            return;
        }
        if (!(is(Token::Number) || is(Token::String) || is(Token::Word))) {
            int s = m_pos;
            int line = peek().line;
            consume();
            note(FGDImportMessage::Skipped, line, "Skipped unrecognized text in flags of '" + kv.name + "': " + excerpt(s, m_pos));
            continue;
        }
        QString bit = consume().value;
        QString label;
        int def = 0;
        QString desc;
        if (is(Token::Colon)) {
            consume();
            if (is(Token::String) || is(Token::Number) || is(Token::Word)) label = consume().value;
            if (is(Token::Colon)) {
                consume();
                if (is(Token::Number) || is(Token::Word) || is(Token::String)) def = consume().value.toInt();
                if (is(Token::Colon)) {
                    consume();
                    desc = parseDescription();
                }
            }
        }
        kv.flagItems.append({bit, {label, def}});
        kv.flagDescriptions.append(desc);
    }
    if (is(Token::RBracket)) consume();
    else note(FGDImportMessage::Warning, peek().line, "Flags list of '" + kv.name + "' is not closed with ']'.");
}

bool FGDImporter::parseKeyvalue(FGDKeyvalue &kv) {
    kv.line = peek().line;
    kv.name = consume().value;
    if (!is(Token::LParen)) return false;
    consume();
    QString type;
    while (!atEnd() && !is(Token::RParen) && !is(Token::At)) type += consume().value;
    if (is(Token::RParen)) consume();
    kv.type = type.toLower();
    bool listType = (kv.type == "choices" || kv.type == "flags");

    while (true) {
        if (isWord("readonly")) { kv.readOnly = true; consume(); continue; }
        if (isWord("report"))   { kv.report = true;   consume(); continue; }
        if (is(Token::LBracket) && !listType) {
            int s = m_pos;
            int depth = 0;
            while (!atEnd() && !is(Token::At)) {
                if (is(Token::LBracket)) ++depth;
                else if (is(Token::RBracket)) { --depth; if (depth == 0) { consume(); break; } }
                consume();
            }
            QString raw = normalizeArgs(rawText(s, m_pos));
            kv.extra = raw;
            note(FGDImportMessage::Preserved, m_tokens[s].line, "Keyvalue '" + kv.name + "': bracketed block " + raw + " kept as raw text.");
            continue;
        }
        break;
    }

    QString fields[3];
    int nFields = 0;
    while (nFields < 3 && is(Token::Colon)) {
        consume();
        bool isDescField = (nFields == 2) || (kv.type == "flags" && nFields == 1);
        if (is(Token::Colon)) { ++nFields; continue; }
        if (isDescField) {
            if (is(Token::String)) fields[nFields] = parseDescription();
        } else if (is(Token::String)) {
            fields[nFields] = consume().value;
        } else if ((is(Token::Number) || is(Token::Word)) && !(is(Token::Word) && is(Token::LParen, 1))) {
            fields[nFields] = consume().value;
        }
        ++nFields;
    }
    kv.displayName = fields[0];
    if (kv.type == "flags") {
        kv.description = fields[1];
    } else {
        kv.defaultValue = fields[1];
        kv.description = fields[2];
    }
    if (is(Token::Colon)) {
        int s = m_pos;
        int line = peek().line;
        while (is(Token::Colon) || is(Token::String) || is(Token::Number) || is(Token::Plus)) consume();
        note(FGDImportMessage::Skipped, line, "Keyvalue '" + kv.name + "': extra fields after the description were skipped: " + excerpt(s, m_pos));
    }

    if (listType) {
        if (is(Token::Equals)) consume();
        if (is(Token::LBracket)) {
            consume();
            if (kv.type == "choices") parseChoiceItems(kv);
            else parseFlagItems(kv);
        } else {
            note(FGDImportMessage::Warning, kv.line, "Keyvalue '" + kv.name + "' of type " + kv.type + " has no [ ... ] list.");
        }
    }
    return true;
}

void FGDImporter::parseBody(FGDEntity &e) {
    consume();
    while (!atEnd() && !is(Token::RBracket)) {
        if (is(Token::At)) {
            note(FGDImportMessage::Warning, peek().line, "Body of '" + e.className + "' is not closed with ']' before the next @ directive.");
            return;
        }
        if (is(Token::Word)) {
            QString lw = peek().value.toLower();
            if ((lw == "input" || lw == "output") && is(Token::Word, 1)) {
                FGDIO io;
                if (parseIO(io)) e.ios.append(io);
                continue;
            }
            bool keyKw = (lw == "key" && is(Token::Word, 1) && is(Token::LParen, 2));
            if (keyKw) consume();
            if (is(Token::LParen, 1)) {
                FGDKeyvalue kv;
                kv.keyKeyword = keyKw;
                if (parseKeyvalue(kv)) e.keyvalues.append(kv);
                continue;
            }
        }
        skipUnexpected("Body of '" + e.className + "'");
    }
    if (is(Token::RBracket)) consume();
    else note(FGDImportMessage::Warning, peek().line, "Body of '" + e.className + "' is not closed with ']' at the end of the file.");
}

void FGDImporter::parseClass(EntityClassType type, int line) {
    FGDEntity e;
    e.classType = type;
    e.line = line;
    m_entity.clear();
    if (!parseClassHeader(e)) {
        int s = m_pos;
        while (!atEnd() && !is(Token::At)) consume();
        if (m_pos > s)
            note(FGDImportMessage::Skipped, m_tokens[s].line, "Skipped the rest of the dropped definition: " + excerpt(s, m_pos));
        m_entity.clear();
        return;
    }
    if (is(Token::LBracket)) {
        parseBody(e);
    } else {
        note(FGDImportMessage::Warning, line, "Class has no body '[ ... ]'.");
    }
    m_proj.entities.append(e);
    m_entity.clear();
}

FGDAutoVisGroupChild FGDImporter::parseAutoVisChild() {
    FGDAutoVisGroupChild child;
    child.name = consume().value;
    if (is(Token::LBracket)) consume();
    while (!atEnd() && !is(Token::RBracket) && !is(Token::At)) {
        if (is(Token::String)) {
            if (is(Token::LBracket, 1)) child.children.append(parseAutoVisChild());
            else child.entities << consume().value;
        } else {
            int s = m_pos;
            int line = peek().line;
            consume();
            note(FGDImportMessage::Skipped, line, "@AutoVisGroup: skipped unrecognized text: " + excerpt(s, m_pos));
        }
    }
    if (is(Token::RBracket)) consume();
    return child;
}

void FGDImporter::parseAutoVisGroup() {
    if (is(Token::Equals)) consume();
    FGDAutoVisGroup avg;
    if (is(Token::String)) avg.parent = consume().value;
    if (is(Token::LBracket)) consume();
    while (!atEnd() && !is(Token::RBracket) && !is(Token::At)) {
        if (is(Token::String)) {
            avg.children.append(parseAutoVisChild());
        } else {
            int s = m_pos;
            int line = peek().line;
            consume();
            note(FGDImportMessage::Skipped, line, "@AutoVisGroup: skipped unrecognized text: " + excerpt(s, m_pos));
        }
    }
    if (is(Token::RBracket)) consume();
    m_proj.autoVisGroups.append(avg);
}

void FGDImporter::parseUnknownDirective(int atTok, const QString &name) {
    int depth = 0;
    while (!atEnd()) {
        if (is(Token::At) && depth == 0) break;
        if (is(Token::LBracket) || is(Token::LBrace)) {
            ++depth;
        } else if (is(Token::RBracket) || is(Token::RBrace)) {
            bool square = is(Token::RBracket);
            --depth;
            if (depth <= 0) {
                depth = 0;
                if (square) { consume(); break; }
            }
        }
        consume();
    }
    QString raw = rawText(atTok, m_pos).trimmed();
    m_proj.rawBlocks << raw;
    note(FGDImportMessage::Preserved, m_tokens[atTok].line, "Unknown directive @" + name + " kept as raw text: " + excerpt(atTok, m_pos));
}

void FGDImporter::parseDirective() {
    int atTok = m_pos;
    int line = peek().line;
    consume();
    if (!is(Token::Word)) {
        note(FGDImportMessage::Skipped, line, "Stray '@' without a directive name.");
        return;
    }
    QString name = consume().value;
    QString kw = name.toLower();
    m_entity.clear();

    EntityClassType ct;
    if (classTypeFromName(kw, ct)) {
        parseClass(ct, line);
        return;
    }
    if (kw == "version") {
        QList<int> v;
        if (readInts(v) >= 1) m_proj.fgdVersion = v[0];
        else note(FGDImportMessage::Warning, line, "@version has no number.");
    } else if (kw == "include") {
        if (is(Token::String)) m_proj.includes << consume().value;
        else note(FGDImportMessage::Warning, line, "@include is not followed by a quoted file name.");
    } else if (kw == "mapsize") {
        QList<int> v;
        if (readInts(v) >= 2) {
            m_proj.mapsizeMin = v[0];
            m_proj.mapsizeMax = v[1];
            m_proj.useMapsSize = true;
        } else {
            note(FGDImportMessage::Warning, line, "@mapsize needs two numbers, directive skipped.");
        }
    } else if (kw == "gridnav") {
        QList<int> v;
        if (readInts(v) >= 4) {
            m_proj.useGridNav = true;
            m_proj.gridNavEdgeSize = v[0];
            m_proj.gridNavOffsetX = v[1];
            m_proj.gridNavOffsetY = v[2];
            m_proj.gridNavTraceHeight = v[3];
        } else {
            note(FGDImportMessage::Warning, line, "@gridnav needs four numbers, directive skipped.");
        }
    } else if (kw == "materialexclusion") {
        if (is(Token::LBracket)) consume();
        while (!atEnd() && !is(Token::RBracket) && !is(Token::At)) {
            if (is(Token::String)) {
                m_proj.materialExclusions << consume().value;
            } else {
                int s = m_pos;
                int tl = peek().line;
                consume();
                note(FGDImportMessage::Skipped, tl, "@MaterialExclusion: skipped unrecognized text: " + excerpt(s, m_pos));
            }
        }
        if (is(Token::RBracket)) consume();
    } else if (kw == "autovisgroup") {
        parseAutoVisGroup();
    } else {
        parseUnknownDirective(atTok, name);
    }
}

FGDProject FGDImporter::parse() {
    m_proj = FGDProject();
    m_pos = 0;
    while (!atEnd()) {
        if (is(Token::At)) {
            parseDirective();
            continue;
        }
        int s = m_pos;
        int line = peek().line;
        while (!atEnd() && !is(Token::At)) consume();
        note(FGDImportMessage::Skipped, line, "Skipped unrecognized text outside any definition: " + excerpt(s, m_pos));
    }
    if (m_commentLines > 0 && m_report)
        m_report->add(FGDImportMessage::Skipped, 0, QString(), QString::number(m_commentLines) + " comment lines (//) are not stored in the project and will not be exported.");
    return m_proj;
}

FGDProject FGDImporter::importFromString(const QString &text, QString &errorOut, FGDImportReport *report) {
    FGDImporter importer;
    importer.m_src = text;
    if (importer.m_src.startsWith(QChar(0xFEFF))) importer.m_src.remove(0, 1);
    importer.m_src.replace("\r\n", "\n");
    importer.m_src.replace('\r', '\n');
    importer.m_report = report;
    errorOut.clear();
    importer.tokenize();
    return importer.parse();
}

FGDProject FGDImporter::importFromFile(const QString &path, QString &errorOut, FGDImportReport *report) {
    QFile f(path);
    if (!f.open(QIODevice::ReadOnly)) {
        errorOut = "Cannot open file: " + path;
        return {};
    }
    QByteArray data = f.readAll();
    f.close();
    return importFromString(QString::fromUtf8(data), errorOut, report);
}
