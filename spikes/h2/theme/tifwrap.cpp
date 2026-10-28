#include <QtGui/private/qguiapplication_p.h>
#include <QtGui/qpa/qplatformintegration.h>
#include <QtGui/qpa/qplatformtheme.h>
#include <QtGui/qpa/qplatformthemefactory_p.h>
#include <QtGui/qpa/qplatformthemeplugin.h>
#include <QKeySequence>
#include <memory>

class TifWrapTheme : public QPlatformTheme
{
public:
    explicit TifWrapTheme(QPlatformTheme *base) : m_base(base) {}

    QList<QKeySequence> keyBindings(QKeySequence::StandardKey key) const override
    {
        switch (key) {
        case QKeySequence::MoveToNextWord:
            return {QKeySequence(Qt::ALT | Qt::Key_Right)};
        case QKeySequence::MoveToPreviousWord:
            return {QKeySequence(Qt::ALT | Qt::Key_Left)};
        default:
            return m_base->keyBindings(key);
        }
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
    std::unique_ptr<QPlatformTheme> m_base;
};

static QPlatformTheme *createBaseTheme(const QString &name)
{
    if (QPlatformTheme *theme = QPlatformThemeFactory::create(name, QString()))
        return theme;
    if (QPlatformTheme *theme = QGuiApplicationPrivate::platform_integration->createPlatformTheme(name))
        return theme;
    return new QPlatformTheme;
}

class TifWrapThemePlugin : public QPlatformThemePlugin
{
    Q_OBJECT
    Q_PLUGIN_METADATA(IID QPlatformThemeFactoryInterface_iid FILE "tifwrap.json")

public:
    QPlatformTheme *create(const QString &key, const QStringList &) override
    {
        if (key.compare(QLatin1String("tifwrap"), Qt::CaseInsensitive) != 0)
            return nullptr;
        const QString base = qEnvironmentVariable("TIF_BASE_THEME", QStringLiteral("gtk3"));
        return new TifWrapTheme(createBaseTheme(base));
    }
};

#include "tifwrap.moc"
