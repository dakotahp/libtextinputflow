#include <QtGui/qpa/qplatformtheme.h>
#include <QtGui/qpa/qplatformthemeplugin.h>
#include <QKeySequence>

class TifSpikeTheme : public QPlatformTheme
{
public:
    QList<QKeySequence> keyBindings(QKeySequence::StandardKey key) const override
    {
        switch (key) {
        case QKeySequence::MoveToNextWord:
            return {QKeySequence(Qt::ALT | Qt::Key_Right)};
        case QKeySequence::MoveToPreviousWord:
            return {QKeySequence(Qt::ALT | Qt::Key_Left)};
        default:
            return QPlatformTheme::keyBindings(key);
        }
    }
};

class TifSpikeThemePlugin : public QPlatformThemePlugin
{
    Q_OBJECT
    Q_PLUGIN_METADATA(IID QPlatformThemeFactoryInterface_iid FILE "tifspike.json")

public:
    QPlatformTheme *create(const QString &key, const QStringList &) override
    {
        return key.compare(QLatin1String("tifspike"), Qt::CaseInsensitive) == 0
            ? new TifSpikeTheme
            : nullptr;
    }
};

#include "tifspike.moc"
