#include <QtGui/private/qguiapplication_p.h>
#include <QtGui/qpa/qplatformdialoghelper.h>
#include <QtGui/qpa/qplatformmenu.h>
#include <QtGui/qpa/qplatformtheme.h>
#include <QApplication>
#include <QFileInfo>
#include <QIcon>
#include <QPalette>
#include <QStyle>
#include <QTextStream>
#include <QTimer>

static QString describe(const QVariant &v)
{
    if (v.canConvert<QStringList>() && v.metaType().id() == QMetaType::QStringList)
        return v.toStringList().join(QLatin1Char(','));
    if (v.canConvert<QString>())
        return v.toString();
    return QStringLiteral("<%1>").arg(QLatin1String(v.typeName()));
}

static void dump(QTextStream &out)
{
    const QPlatformTheme *t = QGuiApplicationPrivate::platformTheme();

    out << "style " << QApplication::style()->name() << "\n";
    out << "iconTheme " << QIcon::themeName() << "\n";
    out << "colorScheme " << int(t->colorScheme()) << "\n";
    out << "contrast " << int(t->contrastPreference()) << "\n";
    out << "motion " << int(t->motionPreference()) << "\n";

    for (int h = 0; h <= 64; ++h) {
        const QVariant v = t->themeHint(QPlatformTheme::ThemeHint(h));
        if (v.isValid())
            out << "hint " << h << " " << describe(v) << "\n";
    }

    for (int p = 0; p < QPlatformTheme::NPalettes; ++p) {
        const QPalette *pal = t->palette(QPlatformTheme::Palette(p));
        if (!pal)
            continue;
        out << "palette " << p;
        for (int r = 0; r < QPalette::NColorRoles; ++r)
            out << " " << pal->color(QPalette::Active, QPalette::ColorRole(r)).name();
        out << "\n";
    }

    for (int f = 0; f < QPlatformTheme::NFonts; ++f) {
        if (const QFont *font = t->font(QPlatformTheme::Font(f)))
            out << "font " << f << " " << font->toString() << "\n";
    }

    for (int d = 0; d <= QPlatformTheme::MessageDialog; ++d) {
        const auto type = QPlatformTheme::DialogType(d);
        QPlatformDialogHelper *helper = t->createPlatformDialogHelper(type);
        out << "dialog " << d << " native=" << t->usePlatformNativeDialog(type)
            << " helper=" << (helper ? helper->metaObject()->className() : "none") << "\n";
        delete helper;
    }

    QPlatformMenuBar *bar = t->createPlatformMenuBar();
    out << "menubar " << (bar ? bar->metaObject()->className() : "none") << "\n";
    delete bar;

    for (int b = QPlatformDialogHelper::Ok; b <= QPlatformDialogHelper::RestoreDefaults; b <<= 1)
        out << "button " << b << " " << t->standardButtonText(b) << "\n";

    out << "fileIcon " << !t->fileIcon(QFileInfo(QStringLiteral("/etc/hostname"))).isNull() << "\n";

    for (auto key : {QKeySequence::MoveToNextWord, QKeySequence::SelectNextWord, QKeySequence::Copy}) {
        QStringList keys;
        for (const QKeySequence &seq : t->keyBindings(key))
            keys << seq.toString();
        out << "keys " << int(key) << " " << keys.join(QLatin1Char(',')) << "\n";
    }
}

int main(int argc, char **argv)
{
    QApplication app(argc, argv);
    QTimer::singleShot(500, &app, [] {
        QTextStream out(stdout);
        dump(out);
        out.flush();
        QApplication::quit();
    });
    return app.exec();
}
