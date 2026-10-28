#include <QGuiApplication>
#include <QKeyEvent>
#include <QQmlApplicationEngine>
#include <QQuickItem>
#include <QQuickWindow>
#include <QTextStream>

static int failures = 0;

static const char qml[] = R"(
import QtQuick
Window {
    visible: true
    width: 300
    height: 300
    Column {
        TextInput { objectName: "input"; text: "alpha beta gamma" }
        TextInput { objectName: "readOnlyInput"; text: "alpha beta gamma"; readOnly: true }
        TextEdit { objectName: "edit"; text: "alpha beta gamma" }
        TextEdit { objectName: "readOnlyEdit"; text: "alpha beta gamma"; readOnly: true }
        TextEdit {
            objectName: "keyboardSelectableEdit"
            text: "alpha beta gamma"
            readOnly: true
            selectByKeyboard: true
        }
    }
}
)";

static QQuickItem *focus(QQuickWindow *window, const char *name)
{
    auto *item = window->findChild<QQuickItem *>(QLatin1StringView(name));
    if (!item) {
        QTextStream(stderr) << "no item " << name << "\n";
        ++failures;
        return nullptr;
    }
    item->forceActiveFocus();
    QCoreApplication::processEvents();
    if (QGuiApplication::focusObject() != item) {
        QTextStream(stderr) << "cannot focus " << name << "\n";
        ++failures;
        return nullptr;
    }
    return item;
}

static void expectClaimed(const char *name, QQuickItem *item, QKeyCombination key, bool claimed)
{
    if (!item)
        return;
    QKeyEvent event(QEvent::ShortcutOverride, key.key(), key.keyboardModifiers());
    event.ignore();
    QCoreApplication::sendEvent(item, &event);
    if (event.isAccepted() == claimed)
        return;
    QTextStream(stderr) << name << ": accepted " << event.isAccepted() << ", want " << claimed << "\n";
    ++failures;
}

int main(int argc, char **argv)
{
    QGuiApplication app(argc, argv);
    QQmlApplicationEngine engine;
    engine.loadData(qml);
    auto *window = engine.rootObjects().isEmpty() ? nullptr : qobject_cast<QQuickWindow *>(engine.rootObjects()[0]);
    if (!window) {
        QTextStream(stderr) << "cannot load the QML window\n";
        return 1;
    }
    window->requestActivate();
    QCoreApplication::processEvents();

    const QKeyCombination altBackspace(Qt::AltModifier, Qt::Key_Backspace);
    const QKeyCombination altRight(Qt::AltModifier, Qt::Key_Right);
    const QKeyCombination ctrlRight(Qt::ControlModifier, Qt::Key_Right);

    QQuickItem *input = focus(window, "input");
    expectClaimed("TextInput Alt+Backspace", input, altBackspace, true);
    expectClaimed("TextInput blocked Ctrl+Right", input, ctrlRight, false);

    QQuickItem *readOnlyInput = focus(window, "readOnlyInput");
    expectClaimed("read-only TextInput Alt+Right", readOnlyInput, altRight, true);
    expectClaimed("read-only TextInput Alt+Backspace", readOnlyInput, altBackspace, false);

    expectClaimed("TextEdit Alt+Backspace", focus(window, "edit"), altBackspace, true);
    expectClaimed("read-only TextEdit Alt+Right", focus(window, "readOnlyEdit"), altRight, false);
    expectClaimed("keyboard-selectable TextEdit Alt+Right", focus(window, "keyboardSelectableEdit"), altRight, true);

    return failures == 0 ? 0 : 1;
}
