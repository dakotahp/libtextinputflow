#include "textinputflow.h"

#include <QtGui/private/qguiapplication_p.h>
#include <QtGui/qpa/qplatformintegration.h>
#include <QtGui/qpa/qplatformtheme.h>
#include <QtGui/qpa/qplatformthemefactory_p.h>
#include <QtGui/qpa/qplatformthemeplugin.h>
#include <QGuiApplication>
#include <QKeyEvent>
#include <QKeySequence>
#include <QVariant>
#include <memory>
#include <optional>
#include <vector>

static const QLatin1StringView pluginKey("textinputflow");

struct StandardKeyCommand
{
    QKeySequence::StandardKey key;
    tif_command command;
};

static const StandardKeyCommand standardKeyCommands[] = {
    {QKeySequence::MoveToPreviousChar, {TIF_ACTION_MOVE, TIF_UNIT_CHARACTER, TIF_DIRECTION_BACKWARD, false}},
    {QKeySequence::MoveToNextChar, {TIF_ACTION_MOVE, TIF_UNIT_CHARACTER, TIF_DIRECTION_FORWARD, false}},
    {QKeySequence::MoveToPreviousWord, {TIF_ACTION_MOVE, TIF_UNIT_WORD, TIF_DIRECTION_BACKWARD, false}},
    {QKeySequence::MoveToNextWord, {TIF_ACTION_MOVE, TIF_UNIT_WORD, TIF_DIRECTION_FORWARD, false}},
    {QKeySequence::MoveToStartOfLine, {TIF_ACTION_MOVE, TIF_UNIT_LINE_ENDS, TIF_DIRECTION_BACKWARD, false}},
    {QKeySequence::MoveToEndOfLine, {TIF_ACTION_MOVE, TIF_UNIT_LINE_ENDS, TIF_DIRECTION_FORWARD, false}},
    {QKeySequence::MoveToStartOfDocument, {TIF_ACTION_MOVE, TIF_UNIT_DOCUMENT_ENDS, TIF_DIRECTION_BACKWARD, false}},
    {QKeySequence::MoveToEndOfDocument, {TIF_ACTION_MOVE, TIF_UNIT_DOCUMENT_ENDS, TIF_DIRECTION_FORWARD, false}},
    {QKeySequence::SelectPreviousChar, {TIF_ACTION_MOVE, TIF_UNIT_CHARACTER, TIF_DIRECTION_BACKWARD, true}},
    {QKeySequence::SelectNextChar, {TIF_ACTION_MOVE, TIF_UNIT_CHARACTER, TIF_DIRECTION_FORWARD, true}},
    {QKeySequence::SelectPreviousWord, {TIF_ACTION_MOVE, TIF_UNIT_WORD, TIF_DIRECTION_BACKWARD, true}},
    {QKeySequence::SelectNextWord, {TIF_ACTION_MOVE, TIF_UNIT_WORD, TIF_DIRECTION_FORWARD, true}},
    {QKeySequence::SelectStartOfLine, {TIF_ACTION_MOVE, TIF_UNIT_LINE_ENDS, TIF_DIRECTION_BACKWARD, true}},
    {QKeySequence::SelectEndOfLine, {TIF_ACTION_MOVE, TIF_UNIT_LINE_ENDS, TIF_DIRECTION_FORWARD, true}},
    {QKeySequence::SelectStartOfDocument, {TIF_ACTION_MOVE, TIF_UNIT_DOCUMENT_ENDS, TIF_DIRECTION_BACKWARD, true}},
    {QKeySequence::SelectEndOfDocument, {TIF_ACTION_MOVE, TIF_UNIT_DOCUMENT_ENDS, TIF_DIRECTION_FORWARD, true}},
    {QKeySequence::Backspace, {TIF_ACTION_DELETE, TIF_UNIT_CHARACTER, TIF_DIRECTION_BACKWARD, false}},
    {QKeySequence::Delete, {TIF_ACTION_DELETE, TIF_UNIT_CHARACTER, TIF_DIRECTION_FORWARD, false}},
    {QKeySequence::DeleteStartOfWord, {TIF_ACTION_DELETE, TIF_UNIT_WORD, TIF_DIRECTION_BACKWARD, false}},
    {QKeySequence::DeleteEndOfWord, {TIF_ACTION_DELETE, TIF_UNIT_WORD, TIF_DIRECTION_FORWARD, false}},
    {QKeySequence::SelectAll, {TIF_ACTION_SELECT_ALL, TIF_UNIT_NONE, TIF_DIRECTION_NONE, false}},
};

struct NamedKey
{
    uint32_t keysym;
    Qt::Key key;
};

static const NamedKey namedKeys[] = {
    {0xff51u, Qt::Key_Left},   {0xff52u, Qt::Key_Up},       {0xff53u, Qt::Key_Right},
    {0xff54u, Qt::Key_Down},   {0xff50u, Qt::Key_Home},     {0xff57u, Qt::Key_End},
    {0xff55u, Qt::Key_PageUp}, {0xff56u, Qt::Key_PageDown}, {0xff08u, Qt::Key_Backspace},
    {0xffffu, Qt::Key_Delete},
};

static const tif_command *commandFor(QKeySequence::StandardKey key)
{
    for (const StandardKeyCommand &entry : standardKeyCommands) {
        if (entry.key == key)
            return &entry.command;
    }
    return nullptr;
}

static std::optional<QKeyCombination> toCombination(tif_key key)
{
    Qt::KeyboardModifiers mods;
    if (key.mods & TIF_MOD_SHIFT)
        mods |= Qt::ShiftModifier;
    if (key.mods & TIF_MOD_CONTROL)
        mods |= Qt::ControlModifier;
    if (key.mods & TIF_MOD_ALT)
        mods |= Qt::AltModifier;
    if (key.mods & TIF_MOD_SUPER)
        mods |= Qt::MetaModifier;

    if (key.keysym >= 'a' && key.keysym <= 'z')
        return QKeyCombination(mods, Qt::Key(Qt::Key_A + int(key.keysym - 'a')));
    for (const NamedKey &named : namedKeys) {
        if (named.keysym == key.keysym)
            return QKeyCombination(mods, named.key);
    }
    return std::nullopt;
}

static std::optional<tif_key> toTifKey(const QKeyEvent *event)
{
    const Qt::KeyboardModifiers modifiers = event->modifiers();
    uint32_t mods = 0;
    if (modifiers & Qt::ShiftModifier)
        mods |= TIF_MOD_SHIFT;
    if (modifiers & Qt::ControlModifier)
        mods |= TIF_MOD_CONTROL;
    if (modifiers & Qt::AltModifier)
        mods |= TIF_MOD_ALT;
    if (modifiers & Qt::MetaModifier)
        mods |= TIF_MOD_SUPER;

    const int key = event->key();
    if (key >= Qt::Key_A && key <= Qt::Key_Z)
        return tif_key{uint32_t('a' + (key - Qt::Key_A)), mods};
    for (const NamedKey &named : namedKeys) {
        if (named.key == key)
            return tif_key{named.keysym, mods};
    }
    return std::nullopt;
}

static const char *const textWidgetClasses[] = {
    "QLineEdit", "QTextEdit", "QPlainTextEdit", "QQuickTextInput", "QQuickTextEdit",
};

static bool isTextWidget(const QObject *object)
{
    for (const char *className : textWidgetClasses) {
        if (object->inherits(className))
            return true;
    }
    return false;
}

static bool canRun(const QObject *textWidget, const tif_command &command)
{
    const bool editable = !textWidget->property("readOnly").toBool();
    if (command.action == TIF_ACTION_DELETE)
        return editable;

    const QVariant flags = textWidget->property("textInteractionFlags");
    if (flags.isValid())
        return flags.value<Qt::TextInteractionFlags>() & (Qt::TextSelectableByKeyboard | Qt::TextEditable);
    const QVariant selectByKeyboard = textWidget->property("selectByKeyboard");
    if (selectByKeyboard.isValid())
        return editable || selectByKeyboard.toBool();
    return true;
}

class ShortcutOverrideFilter : public QObject
{
public:
    explicit ShortcutOverrideFilter(tif_ctx *ctx) : m_ctx(ctx) {}

    bool eventFilter(QObject *receiver, QEvent *event) override
    {
        if (event->type() != QEvent::ShortcutOverride || receiver != QGuiApplication::focusObject() ||
            !isTextWidget(receiver))
            return false;

        const std::optional<tif_key> key = toTifKey(static_cast<QKeyEvent *>(event));
        if (!key)
            return false;
        tif_refresh(m_ctx);
        tif_command command;
        if (tif_lookup(m_ctx, *key, &command) != TIF_LOOKUP_COMMAND || !canRun(receiver, command))
            return false;
        // The Qt Quick text items ignore a ShortcutOverride they do not claim themselves, so the
        // receiver must not see the event after it is accepted here.
        event->accept();
        return true;
    }

private:
    tif_ctx *m_ctx;
};

class TextInputFlowTheme : public QPlatformTheme
{
public:
    explicit TextInputFlowTheme(QPlatformTheme *base) : m_base(base), m_ctx(tif_new()) { installShortcutFilter(); }
    ~TextInputFlowTheme() override
    {
        m_filter.reset();
        tif_free(m_ctx);
    }

    QList<QKeySequence> keyBindings(QKeySequence::StandardKey key) const override
    {
        installShortcutFilter();
        tif_refresh(m_ctx);

        const tif_command *command = commandFor(key);
        if (!command)
            return m_base->keyBindings(key);

        QList<QKeyCombination> libraryKeys;
        tif_binding binding;
        for (size_t i = 0; tif_binding_at(m_ctx, i, &binding); ++i) {
            if (const std::optional<QKeyCombination> combination = toCombination(binding.key))
                libraryKeys.append(*combination);
        }

        QList<QKeySequence> result;
        for (const QKeySequence &sequence : m_base->keyBindings(key)) {
            if (sequence.count() != 1 || !libraryKeys.contains(sequence[0]))
                result.append(sequence);
        }

        std::vector<tif_key> keys(tif_keys_for_command(m_ctx, command, nullptr, 0));
        tif_keys_for_command(m_ctx, command, keys.data(), keys.size());
        for (const tif_key &libraryKey : keys) {
            const std::optional<QKeyCombination> combination = toCombination(libraryKey);
            if (combination && !result.contains(QKeySequence(*combination)))
                result.append(QKeySequence(*combination));
        }
        return result;
    }

    QPlatformMenuItem *createPlatformMenuItem() const override { return m_base->createPlatformMenuItem(); }
    QPlatformMenu *createPlatformMenu() const override { return m_base->createPlatformMenu(); }
    QPlatformMenuBar *createPlatformMenuBar() const override { return m_base->createPlatformMenuBar(); }
    void showPlatformMenuBar() override { m_base->showPlatformMenuBar(); }
    bool usePlatformNativeDialog(DialogType type) const override { return m_base->usePlatformNativeDialog(type); }
    QPlatformDialogHelper *createPlatformDialogHelper(DialogType type) const override
    {
        return m_base->createPlatformDialogHelper(type);
    }
#ifndef QT_NO_SYSTEMTRAYICON
    QPlatformSystemTrayIcon *createPlatformSystemTrayIcon() const override
    {
        return m_base->createPlatformSystemTrayIcon();
    }
#endif
    Qt::ColorScheme colorScheme() const override { return m_base->colorScheme(); }
    const QPalette *palette(Palette type) const override { return m_base->palette(type); }
    const QFont *font(Font type) const override { return m_base->font(type); }
    QVariant themeHint(ThemeHint hint) const override { return m_base->themeHint(hint); }
    QPixmap standardPixmap(StandardPixmap sp, const QSizeF &size) const override
    {
        return m_base->standardPixmap(sp, size);
    }
    QIcon fileIcon(const QFileInfo &info, IconOptions options) const override { return m_base->fileIcon(info, options); }
    QIconEngine *createIconEngine(const QString &name) const override { return m_base->createIconEngine(name); }
    QString standardButtonText(int button) const override { return m_base->standardButtonText(button); }
    QKeySequence standardButtonShortcut(int button) const override { return m_base->standardButtonShortcut(button); }
    void requestColorScheme(Qt::ColorScheme scheme) override { m_base->requestColorScheme(scheme); }
    Qt::ContrastPreference contrastPreference() const override { return m_base->contrastPreference(); }
    Qt::MotionPreference motionPreference() const override { return m_base->motionPreference(); }

private:
    // Qt sends the first ShortcutOverride before it asks for any key bindings, so waiting for
    // keyBindings() would let the first key press reach app shortcuts.
    void installShortcutFilter() const
    {
        if (m_filter || !QCoreApplication::instance())
            return;
        m_filter = std::make_unique<ShortcutOverrideFilter>(m_ctx);
        QCoreApplication::instance()->installEventFilter(m_filter.get());
    }

    std::unique_ptr<QPlatformTheme> m_base;
    tif_ctx *m_ctx;
    mutable std::unique_ptr<ShortcutOverrideFilter> m_filter;
};

static QPlatformTheme *createBaseTheme(const QString &name)
{
    if (name.compare(pluginKey, Qt::CaseInsensitive) != 0) {
        if (QPlatformTheme *theme = QPlatformThemeFactory::create(name, QString()))
            return theme;
        if (QPlatformTheme *theme = QGuiApplicationPrivate::platform_integration->createPlatformTheme(name))
            return theme;
    }
    qWarning("textinputflow: cannot create the Qt theme \"%s\", using Qt's plain theme", qUtf8Printable(name));
    return new QPlatformTheme;
}

class TextInputFlowThemePlugin : public QPlatformThemePlugin
{
    Q_OBJECT
    Q_PLUGIN_METADATA(IID QPlatformThemeFactoryInterface_iid FILE "textinputflow.json")

public:
    QPlatformTheme *create(const QString &key, const QStringList &) override
    {
        if (key.compare(pluginKey, Qt::CaseInsensitive) != 0)
            return nullptr;
        QString base = qEnvironmentVariable("TEXTINPUTFLOW_QT_BASE_THEME");
        if (base.isEmpty())
            base = QStringLiteral("gtk3");
        return new TextInputFlowTheme(createBaseTheme(base));
    }
};

#include "textinputflowtheme.moc"
