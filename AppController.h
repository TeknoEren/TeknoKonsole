#pragma once

#include <QVariantList>
#include <QVariantMap>
#include <QObject>
#include <QString>
#include <QHash>
#include <QTimer>
#include <QUrl>

class AppController final : public QObject
{
    Q_OBJECT
    Q_PROPERTY(QString accent READ accent NOTIFY themeChanged)
    Q_PROPERTY(QString background READ background NOTIFY themeChanged)
    Q_PROPERTY(QString surface READ surface NOTIFY themeChanged)
    Q_PROPERTY(QString textColor READ textColor NOTIFY themeChanged)
    Q_PROPERTY(QString secondaryText READ secondaryText NOTIFY themeChanged)
    Q_PROPERTY(QString logoPath READ logoPath NOTIFY logoChanged)
    Q_PROPERTY(QVariantList games READ games NOTIFY contentChanged)
    Q_PROPERTY(QVariantList emulators READ emulators NOTIFY contentChanged)
    Q_PROPERTY(QVariantList categories READ categories NOTIFY contentChanged)
    Q_PROPERTY(bool fullscreen READ fullscreen WRITE setFullscreen NOTIFY fullscreenChanged)
    Q_PROPERTY(bool animations READ animations WRITE setAnimations NOTIFY settingsChanged)
    Q_PROPERTY(bool autoStartSteam READ autoStartSteam WRITE setAutoStartSteam NOTIFY settingsChanged)
    Q_PROPERTY(QString fontScale READ fontScale NOTIFY settingsChanged)
    Q_PROPERTY(QString statusMessage READ statusMessage NOTIFY statusChanged)

public:
    explicit AppController(QObject *parent = nullptr);
    ~AppController() override;

    QString accent() const;
    QString background() const;
    QString surface() const;
    QString textColor() const;
    QString secondaryText() const;
    QString logoPath() const;
    QVariantList games() const;
    QVariantList emulators() const;
    QVariantList categories() const;
    bool fullscreen() const;
    bool animations() const;
    bool autoStartSteam() const;
    QString fontScale() const;
    QString statusMessage() const;

    void setFullscreen(bool value);
    void setAnimations(bool value);
    void setAutoStartSteam(bool value);

    Q_INVOKABLE void launchGame(int index);
    Q_INVOKABLE void launchEmulator(int index);
    Q_INVOKABLE void addGame(const QString &name, const QString &appId, const QString &description,
                             const QString &category = QStringLiteral("Tüm Oyunlar"),
                             const QString &launchPath = QString(),
                             const QString &steamUri = QString(),
                             const QString &iconSource = QString(),
                             const QString &sourceType = QString());
    Q_INVOKABLE void editGame(int index, const QString &name, const QString &appId,
                              const QString &description, const QString &category,
                              const QString &launchPath = QString(),
                              const QString &steamUri = QString(),
                              const QString &iconSource = QString(),
                              const QString &sourceType = QString());
    Q_INVOKABLE void removeGame(int index);
    Q_INVOKABLE void importAppImage(const QString &sourceUrl);
    Q_INVOKABLE void importWineExe(const QString &sourceUrl);
    Q_INVOKABLE void importDesktop(const QString &sourceUrl);
    Q_INVOKABLE void importAppImageAsEmulator(const QString &sourceUrl);
    Q_INVOKABLE void importDesktopAsEmulator(const QString &sourceUrl);
    Q_INVOKABLE void addEmulator(const QString &name, const QString &command);
    Q_INVOKABLE void editEmulator(int index, const QString &name, const QString &command);
    Q_INVOKABLE void removeEmulator(int index);
    Q_INVOKABLE void addCategory(const QString &name);
    Q_INVOKABLE void removeCategory(int index);
    Q_INVOKABLE void setThemeColor(const QString &name, const QString &value);
    Q_INVOKABLE void resetTheme();
    Q_INVOKABLE void setLogo(const QString &sourceUrl);
    Q_INVOKABLE QString importAsset(const QString &sourceUrl, const QString &assetType);
    Q_INVOKABLE void setGameAsset(int index, const QString &assetType, const QString &sourceUrl);
    Q_INVOKABLE void setEmulatorAsset(int index, const QString &assetType, const QString &sourceUrl);
    Q_INVOKABLE void suspendGamepad();
    Q_INVOKABLE void resumeGamepad();
    Q_INVOKABLE void quit();

signals:
    void themeChanged();
    void logoChanged();
    void contentChanged();
    void fullscreenChanged();
    void settingsChanged();
    void statusChanged();
    void navigate(int direction);
    void selectPressed();
    void backPressed();
    void homePressed();
    void menuPressed();

private slots:
    void pollGamepad();

private:
    QString configDirectory() const;
    QString dataDirectory() const;
    QString assetDirectory(const QString &assetType) const;
    QString configFile(const QString &name) const;
    QString applicationDirectory() const;
    QString defaultIconPath() const;
    void initializeStorage();
    void loadState();
    void saveState() const;
    void setStatus(const QString &message);
    bool copyAsset(const QString &sourceUrl, const QString &assetType, QString *destinationPath);
    QString copyIconOrDefault(const QString &source, const QString &fallback = QString()) const;
    QString copyApplication(const QString &sourcePath) const;
    QString resolveIconPath(const QString &iconValue, const QString &desktopPath) const;
    QString resolveAppImageIcon(const QString &appImagePath) const;
    QVariantMap readDesktopEntry(const QString &desktopPath, QString *error) const;
    QStringList desktopExecArguments(const QString &execLine, const QVariantMap &entry,
                                     const QString &desktopPath) const;
    bool ensureExecutable(const QString &path) const;
    bool launchCommand(const QString &program, const QStringList &arguments, bool terminal = false);
    bool acquireLaunchGuard(const QString &key);
    void startSteamIfNeeded();
    void initializeGamepad();
    void fetchSteamArtwork(int index, const QString &appId);
    void downloadSteamArtwork(int index, const QString &appId, const QUrl &url);
    void applySteamArtwork(int index, const QString &appId, const QString &path);
    void updateMapAsset(QVariantList &list, int index, const QString &assetType, const QString &sourceUrl);
    QVariantMap normalizedGame(const QVariantMap &game) const;

    QVariantList m_games;
    QVariantList m_emulators;
    QVariantList m_categories;
    QVariantMap m_theme;
    QVariantMap m_settings;
    QString m_logoPath;
    QString m_statusMessage;
    QTimer m_gamepadTimer;
    QHash<QString, qint64> m_launchCooldowns;
    bool m_gamepadSuspended = false;
    void *m_gameController = nullptr;
    class QNetworkAccessManager *m_networkManager = nullptr;
};
