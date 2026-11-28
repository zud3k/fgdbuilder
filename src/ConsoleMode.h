#pragma once
#include <QStringList>

class ConsoleMode {
public:
    static bool wants(int argc, char **argv);
    static int run(const QStringList &args);
};
