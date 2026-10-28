#include <QApplication>
#include <QKeyEvent>
#include <QLineEdit>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QTextEdit>
#include <QTextStream>
#include <QVBoxLayout>
#include <QWidget>

static int failures = 0;

template <typename Base>
class Probe : public Base
{
public:
    using Base::Base;
    bool reached = false;
    bool acceptedOnArrival = false;

protected:
    bool event(QEvent *event) override
    {
        if (event->type() == QEvent::ShortcutOverride || event->type() == QEvent::KeyPress) {
            reached = true;
            acceptedOnArrival = event->isAccepted();
        }
        return Base::event(event);
    }
};

template <typename Widget>
static void expectClaimed(const char *name, Probe<Widget> &receiver, QEvent::Type type, QKeyCombination key,
                          bool claimed)
{
    QKeyEvent event(type, key.key(), key.keyboardModifiers());
    event.ignore();
    receiver.reached = false;
    QCoreApplication::sendEvent(&receiver, &event);
    const bool filterClaimed = !receiver.reached && event.isAccepted();
    const bool untouched = receiver.reached && !receiver.acceptedOnArrival;
    if (claimed ? filterClaimed : untouched)
        return;
    QTextStream(stderr) << name << ": reached the widget: " << receiver.reached
                        << ", accepted on arrival: " << receiver.acceptedOnArrival
                        << ", accepted after: " << event.isAccepted() << ", want claimed " << claimed << "\n";
    ++failures;
}

template <typename Widget>
static void focus(Probe<Widget> &widget)
{
    widget.setFocus();
    QCoreApplication::processEvents();
    if (QApplication::focusWidget() != &widget) {
        QTextStream(stderr) << "cannot focus " << widget.metaObject()->className() << "\n";
        ++failures;
    }
}

int main(int argc, char **argv)
{
    QApplication app(argc, argv);
    const QString text = QStringLiteral("alpha beta gamma");

    QWidget window;
    auto *layout = new QVBoxLayout(&window);
    auto *lineEdit = new Probe<QLineEdit>(text);
    auto *otherLineEdit = new Probe<QLineEdit>(text);
    auto *readOnlyLineEdit = new Probe<QLineEdit>(text);
    auto *plainTextEdit = new Probe<QPlainTextEdit>(text);
    auto *readOnlyPlainTextEdit = new Probe<QPlainTextEdit>(text);
    auto *keyboardSelectablePlainTextEdit = new Probe<QPlainTextEdit>(text);
    auto *textEdit = new Probe<QTextEdit>(text);
    auto *button = new Probe<QPushButton>(QStringLiteral("button"));
    readOnlyLineEdit->setReadOnly(true);
    readOnlyPlainTextEdit->setReadOnly(true);
    keyboardSelectablePlainTextEdit->setReadOnly(true);
    keyboardSelectablePlainTextEdit->setTextInteractionFlags(Qt::TextSelectableByMouse |
                                                             Qt::TextSelectableByKeyboard);
    for (QWidget *widget : std::initializer_list<QWidget *>{lineEdit, otherLineEdit, readOnlyLineEdit, plainTextEdit,
                                                            readOnlyPlainTextEdit, keyboardSelectablePlainTextEdit,
                                                            textEdit, button})
        layout->addWidget(widget);
    readOnlyPlainTextEdit->setFocusPolicy(Qt::StrongFocus);
    button->setFocusPolicy(Qt::StrongFocus);
    window.show();
    window.activateWindow();
    QCoreApplication::processEvents();

    const QKeyCombination altBackspace(Qt::AltModifier, Qt::Key_Backspace);
    const QKeyCombination shiftCtrlEnd(Qt::ShiftModifier | Qt::ControlModifier, Qt::Key_End);
    const QKeyCombination altRight(Qt::AltModifier, Qt::Key_Right);
    const QKeyCombination ctrlRight(Qt::ControlModifier, Qt::Key_Right);
    const QKeyCombination altUp(Qt::AltModifier, Qt::Key_Up);

    focus(*lineEdit);
    expectClaimed("QLineEdit Alt+Backspace", *lineEdit, QEvent::ShortcutOverride, altBackspace, true);
    expectClaimed("QLineEdit Shift+Ctrl+End", *lineEdit, QEvent::ShortcutOverride, shiftCtrlEnd, true);
    expectClaimed("QLineEdit blocked Ctrl+Right", *lineEdit, QEvent::ShortcutOverride, ctrlRight, false);
    expectClaimed("QLineEdit unbound Alt+Up", *lineEdit, QEvent::ShortcutOverride, altUp, false);
    expectClaimed("QLineEdit KeyPress Alt+Backspace", *lineEdit, QEvent::KeyPress, altBackspace, false);
    expectClaimed("unfocused QLineEdit Alt+Backspace", *otherLineEdit, QEvent::ShortcutOverride, altBackspace,
                  false);

    focus(*readOnlyLineEdit);
    expectClaimed("read-only QLineEdit Alt+Backspace", *readOnlyLineEdit, QEvent::ShortcutOverride, altBackspace,
                  false);
    expectClaimed("read-only QLineEdit Alt+Right", *readOnlyLineEdit, QEvent::ShortcutOverride, altRight, true);

    focus(*plainTextEdit);
    expectClaimed("QPlainTextEdit Alt+Backspace", *plainTextEdit, QEvent::ShortcutOverride, altBackspace, true);

    focus(*readOnlyPlainTextEdit);
    expectClaimed("read-only QPlainTextEdit Alt+Right", *readOnlyPlainTextEdit, QEvent::ShortcutOverride, altRight,
                  false);

    focus(*keyboardSelectablePlainTextEdit);
    expectClaimed("keyboard-selectable QPlainTextEdit Alt+Right", *keyboardSelectablePlainTextEdit,
                  QEvent::ShortcutOverride, altRight, true);
    expectClaimed("keyboard-selectable QPlainTextEdit Alt+Backspace", *keyboardSelectablePlainTextEdit,
                  QEvent::ShortcutOverride, altBackspace, false);

    focus(*textEdit);
    expectClaimed("QTextEdit Alt+Backspace", *textEdit, QEvent::ShortcutOverride, altBackspace, true);

    focus(*button);
    expectClaimed("QPushButton Alt+Backspace", *button, QEvent::ShortcutOverride, altBackspace, false);

    return failures == 0 ? 0 : 1;
}
