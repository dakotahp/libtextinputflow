#include <QtGui/qpa/qplatformtheme.h>
#include <QGuiApplication>
#include <QKeySequence>
#include <QSet>
#include <QTextStream>

static int failures = 0;

static QSet<QKeySequence> keySet(const QList<QKeySequence> &sequences)
{
    return QSet<QKeySequence>(sequences.begin(), sequences.end());
}

static QString describe(const QList<QKeySequence> &sequences)
{
    QStringList names;
    for (const QKeySequence &sequence : sequences)
        names << sequence.toString();
    return names.join(QLatin1Char(','));
}

static void expectKeys(const char *name, QKeySequence::StandardKey key, const QList<QKeySequence> &expected)
{
    const QList<QKeySequence> actual = QKeySequence::keyBindings(key);
    if (keySet(actual) == keySet(expected) && actual.size() == expected.size())
        return;
    QTextStream(stderr) << name << ": got [" << describe(actual) << "], want [" << describe(expected) << "]\n";
    ++failures;
}

int main(int argc, char **argv)
{
    QGuiApplication app(argc, argv);
    const QPlatformTheme plain;

    expectKeys("MoveToNextWord", QKeySequence::MoveToNextWord, {QKeySequence(Qt::ALT | Qt::Key_Right)});
    expectKeys("MoveToPreviousWord", QKeySequence::MoveToPreviousWord,
               {QKeySequence(Qt::CTRL | Qt::Key_Left), QKeySequence(Qt::ALT | Qt::Key_Left)});
    expectKeys("SelectNextWord", QKeySequence::SelectNextWord,
               {QKeySequence(Qt::SHIFT | Qt::CTRL | Qt::Key_Right), QKeySequence(Qt::SHIFT | Qt::ALT | Qt::Key_Right)});
    expectKeys("MoveToNextChar", QKeySequence::MoveToNextChar, {QKeySequence(Qt::Key_Right)});
    expectKeys("SelectEndOfDocument", QKeySequence::SelectEndOfDocument,
               {QKeySequence(Qt::SHIFT | Qt::CTRL | Qt::Key_End)});
    expectKeys("DeleteStartOfWord", QKeySequence::DeleteStartOfWord,
               {QKeySequence(Qt::CTRL | Qt::Key_Backspace), QKeySequence(Qt::ALT | Qt::Key_Backspace)});
    expectKeys("SelectAll", QKeySequence::SelectAll, {QKeySequence(Qt::CTRL | Qt::Key_A)});
    expectKeys("Copy", QKeySequence::Copy, plain.keyBindings(QKeySequence::Copy));
    expectKeys("Back", QKeySequence::Back, plain.keyBindings(QKeySequence::Back));
    expectKeys("Forward", QKeySequence::Forward, plain.keyBindings(QKeySequence::Forward));

    return failures == 0 ? 0 : 1;
}
