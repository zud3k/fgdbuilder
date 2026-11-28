#pragma once
#include "FGDData.h"
#include <QString>
#include <QList>
#include <QVector>

struct FGDImportMessage {
    enum Kind { Preserved, Skipped, Warning };
    Kind kind = Warning;
    int line = 0;
    QString entity;
    QString text;
};

struct FGDImportReport {
    QList<FGDImportMessage> messages;
    void add(FGDImportMessage::Kind kind, int line, const QString &entity, const QString &text) {
        FGDImportMessage m;
        m.kind = kind;
        m.line = line;
        m.entity = entity;
        m.text = text;
        messages.append(m);
    }
    int count(FGDImportMessage::Kind kind) const {
        int n = 0;
        for (const auto &m : messages) if (m.kind == kind) ++n;
        return n;
    }
    bool isEmpty() const { return messages.isEmpty(); }
};

class FGDImporter {
public:
    static FGDProject importFromFile(const QString &path, QString &errorOut, FGDImportReport *report = nullptr);
    static FGDProject importFromString(const QString &text, QString &errorOut, FGDImportReport *report = nullptr);
    static QString normalizeArgs(const QString &raw);
private:
    struct Token {
        enum Type { Word, String, Number, LBracket, RBracket, LParen, RParen, LBrace, RBrace, Equals, Comma, Colon, Plus, At, Eof } type = Eof;
        QString value;
        int start = 0;
        int end = 0;
        int line = 0;
    };

    QString m_src;
    QVector<Token> m_tokens;
    int m_pos = 0;
    int m_commentLines = 0;
    FGDImportReport *m_report = nullptr;
    QString m_entity;
    FGDProject m_proj;

    void tokenize();
    const Token &peek(int offset = 0) const;
    bool is(Token::Type t, int offset = 0) const;
    bool isWord(const QString &lower, int offset = 0) const;
    Token consume();
    bool atEnd() const;
    QString excerpt(int fromTok, int toTok) const;
    QString rawText(int fromTok, int toTok) const;
    void note(FGDImportMessage::Kind kind, int line, const QString &text);

    bool readParenArgs(QString &args, int &firstInner, int &lastInner);
    void skipUnexpected(const QString &where);
    bool startsMember() const;
    int readInts(QList<int> &out);

    void parseDirective();
    void parseClass(EntityClassType type, int line);
    bool parseClassHeader(FGDEntity &e);
    void parseHelper(FGDEntity &e);
    void parseBody(FGDEntity &e);
    bool parseKeyvalue(FGDKeyvalue &kv);
    bool parseIO(FGDIO &io);
    void parseChoiceItems(FGDKeyvalue &kv);
    void parseFlagItems(FGDKeyvalue &kv);
    QString parseDescription();
    void parseAutoVisGroup();
    FGDAutoVisGroupChild parseAutoVisChild();
    void parseUnknownDirective(int atTok, const QString &name);

    FGDProject parse();
};
