#include "AppController.h"

#include <SDL2/SDL.h>

#include <QCoreApplication>
#include <QDateTime>
#include <QDesktopServices>
#include <QDir>
#include <QFile>
#include <QFileDevice>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QProcess>
#include <QProcessEnvironment>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QRegularExpression>
#include <QStandardPaths>
#include <QTextStream>
#include <QUuid>
#include <QUrl>

namespace {

QVariantList listFromJson(const QString &path)
{
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly)) {
        return {};
    }

    QJsonParseError parseError;
    const QJsonDocument document = QJsonDocument::fromJson(file.readAll(), &parseError);
    if (parseError.error != QJsonParseError::NoError || !document.isArray()) {
        return {};
    }
    return document.array().toVariantList();
}

void writeJsonArray(const QString &path, const QVariantList &list)
{
    QFile file(path);
    if (file.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
        file.write(QJsonDocument(QJsonArray::fromVariantList(list)).toJson(QJsonDocument::Indented));
    }
}

QString localPathFromUrl(const QString &source)
{
    const QUrl url(source);
    if (url.isLocalFile()) {
        return url.toLocalFile();
    }
    return source;
}

QString cleanDesktopValue(QString value)
{
    value = value.trimmed();
    if (value.size() >= 2 && ((value.startsWith('"') && value.endsWith('"'))
                              || (value.startsWith('\'') && value.endsWith('\'')))) {
        value = value.mid(1, value.size() - 2);
    }
    value.replace(QStringLiteral("\\n"), QStringLiteral("\n"));
    value.replace(QStringLiteral("\\s"), QStringLiteral(" "));
    value.replace(QStringLiteral("\\t"), QStringLiteral("\t"));
    value.replace(QStringLiteral("\\\\"), QStringLiteral("\\"));
    return value;
}

QStringList splitDesktopExec(const QString &line)
{
    QStringList tokens;
    const QRegularExpression tokenExpression(QStringLiteral(R"(("[^"]*"|'[^']*'|[^\s]+))"));
    QRegularExpressionMatchIterator iterator = tokenExpression.globalMatch(line.trimmed());
    while (iterator.hasNext()) {
        QString token = iterator.next().captured(1).trimmed();
        if (token.size() >= 2 && ((token.startsWith('"') && token.endsWith('"'))
                                  || (token.startsWith('\'') && token.endsWith('\'')))) {
            token = token.mid(1, token.size() - 2);
        }
        token.replace(QStringLiteral("\\\\"), QStringLiteral("\\"));
        tokens.append(token);
    }
    return tokens;
}

bool isSupportedImage(const QString &extension)
{
    return QStringList{QStringLiteral("png"), QStringLiteral("jpg"), QStringLiteral("jpeg"),
                       QStringLiteral("svg"), QStringLiteral("xpm")}
        .contains(extension.toLower());
}

} // namespace

AppController::AppController(QObject *parent)
    : QObject(parent)
{
    m_networkManager = new QNetworkAccessManager(this);
    initializeStorage();
    loadState();

    if (m_settings.value(QStringLiteral("autoStartSteam"), true).toBool()) {
        startSteamIfNeeded();
    }

    m_gamepadTimer.setInterval(16);
    connect(&m_gamepadTimer, &QTimer::timeout, this, &AppController::pollGamepad);
    initializeGamepad();
}

AppController::~AppController()
{
    saveState();
    if (m_gameController) {
        SDL_GameControllerClose(static_cast<SDL_GameController *>(m_gameController));
    }
    SDL_Quit();
}

QString AppController::accent() const { return m_theme.value(QStringLiteral("accent"), QStringLiteral("#14A50F")).toString(); }
QString AppController::background() const { return m_theme.value(QStringLiteral("background"), QStringLiteral("#080A08")).toString(); }
QString AppController::surface() const { return m_theme.value(QStringLiteral("surface"), QStringLiteral("#111411")).toString(); }
QString AppController::textColor() const { return m_theme.value(QStringLiteral("text"), QStringLiteral("#F2F2F2")).toString(); }
QString AppController::secondaryText() const { return m_theme.value(QStringLiteral("secondaryText"), QStringLiteral("#8A918A")).toString(); }
QString AppController::logoPath() const { return m_logoPath; }
QVariantList AppController::games() const { return m_games; }
QVariantList AppController::emulators() const { return m_emulators; }
QVariantList AppController::categories() const { return m_categories; }
bool AppController::fullscreen() const { return m_settings.value(QStringLiteral("fullscreen"), true).toBool(); }
bool AppController::animations() const { return m_settings.value(QStringLiteral("animations"), true).toBool(); }
bool AppController::autoStartSteam() const { return m_settings.value(QStringLiteral("autoStartSteam"), true).toBool(); }
QString AppController::fontScale() const { return m_settings.value(QStringLiteral("fontScale"), QStringLiteral("1.0")).toString(); }
QString AppController::statusMessage() const { return m_statusMessage; }

void AppController::setFullscreen(bool value)
{
    if (fullscreen() == value) {
        return;
    }
    m_settings.insert(QStringLiteral("fullscreen"), value);
    saveState();
    emit fullscreenChanged();
    emit settingsChanged();
}

void AppController::setAnimations(bool value)
{
    if (animations() == value) {
        return;
    }
    m_settings.insert(QStringLiteral("animations"), value);
    saveState();
    emit settingsChanged();
}

void AppController::setAutoStartSteam(bool value)
{
    if (autoStartSteam() == value) {
        return;
    }
    m_settings.insert(QStringLiteral("autoStartSteam"), value);
    saveState();
    emit settingsChanged();
}

QString AppController::configDirectory() const
{
    return QDir(QStandardPaths::writableLocation(QStandardPaths::ConfigLocation))
        .filePath(QStringLiteral("tekno-konsole"));
}

QString AppController::dataDirectory() const
{
    return QDir(QStandardPaths::writableLocation(QStandardPaths::GenericDataLocation))
        .filePath(QStringLiteral("tekno-konsole"));
}

QString AppController::assetDirectory(const QString &assetType) const
{
    return QDir(dataDirectory()).filePath(QStringLiteral("assets/%1").arg(assetType));
}

QString AppController::configFile(const QString &name) const
{
    return QDir(configDirectory()).filePath(name);
}

QString AppController::applicationDirectory() const
{
    return QDir(dataDirectory()).filePath(QStringLiteral("apps"));
}

QString AppController::defaultIconPath() const
{
    return QStringLiteral("qrc:/assets/TeknoKonsole.png");
}

void AppController::initializeStorage()
{
    QDir().mkpath(configDirectory());
    QDir data(dataDirectory());
    data.mkpath(QStringLiteral("assets/logos"));
    data.mkpath(QStringLiteral("assets/icons"));
    data.mkpath(QStringLiteral("assets/covers"));
    data.mkpath(QStringLiteral("assets/banners"));
    data.mkpath(QStringLiteral("apps"));
}

QVariantMap AppController::normalizedGame(const QVariantMap &game) const
{
    QVariantMap normalized = game;
    const QString appId = normalized.value(QStringLiteral("appId")).toString().trimmed();
    const QString launchPath = normalized.value(QStringLiteral("launchPath")).toString().trimmed();
    const QString steamUri = normalized.value(QStringLiteral("steamUri")).toString().trimmed();
    QString sourceType = normalized.value(QStringLiteral("sourceType")).toString().trimmed().toLower();

    if (sourceType.isEmpty()) {
        if (!steamUri.isEmpty() || !appId.isEmpty()) {
            sourceType = QStringLiteral("steam");
        } else if (launchPath.endsWith(QStringLiteral(".desktop"), Qt::CaseInsensitive)) {
            sourceType = QStringLiteral("desktop");
        } else if (launchPath.endsWith(QStringLiteral(".appimage"), Qt::CaseInsensitive)) {
            sourceType = QStringLiteral("appimage");
        } else if (launchPath.endsWith(QStringLiteral(".exe"), Qt::CaseInsensitive)) {
            sourceType = QStringLiteral("wine");
        } else if (!launchPath.isEmpty()) {
            sourceType = QStringLiteral("command");
        }
    }

    if (!normalized.contains(QStringLiteral("name"))) normalized.insert(QStringLiteral("name"), QStringLiteral("Adsız uygulama"));
    if (!normalized.contains(QStringLiteral("appId"))) normalized.insert(QStringLiteral("appId"), QString());
    if (!normalized.contains(QStringLiteral("description"))) normalized.insert(QStringLiteral("description"), QString());
    if (!normalized.contains(QStringLiteral("category"))) normalized.insert(QStringLiteral("category"), QStringLiteral("Tüm Oyunlar"));
    if (!normalized.contains(QStringLiteral("favorite"))) normalized.insert(QStringLiteral("favorite"), false);
    if (!normalized.contains(QStringLiteral("icon")) || normalized.value(QStringLiteral("icon")).toString().trimmed().isEmpty()) normalized.insert(QStringLiteral("icon"), defaultIconPath());
    if (!normalized.contains(QStringLiteral("cover"))) normalized.insert(QStringLiteral("cover"), QString());
    if (!normalized.contains(QStringLiteral("banner"))) normalized.insert(QStringLiteral("banner"), QString());
    if (!normalized.contains(QStringLiteral("displayAsset"))) normalized.insert(QStringLiteral("displayAsset"), QString());
    normalized.insert(QStringLiteral("launchPath"), launchPath);
    normalized.insert(QStringLiteral("steamUri"), steamUri);
    normalized.insert(QStringLiteral("sourceType"), sourceType);
    if (!normalized.contains(QStringLiteral("desktopFile"))) normalized.insert(QStringLiteral("desktopFile"), QString());
    return normalized;
}

void AppController::loadState()
{
    m_theme = {
        {QStringLiteral("accent"), QStringLiteral("#14A50F")},
        {QStringLiteral("background"), QStringLiteral("#080A08")},
        {QStringLiteral("surface"), QStringLiteral("#111411")},
        {QStringLiteral("text"), QStringLiteral("#F2F2F2")},
        {QStringLiteral("secondaryText"), QStringLiteral("#8A918A")}
    };
    m_settings = {
        {QStringLiteral("fullscreen"), true},
        {QStringLiteral("animations"), true},
        {QStringLiteral("autoStartSteam"), true},
        {QStringLiteral("fontScale"), QStringLiteral("1.0")}
    };
    m_categories = {QStringLiteral("Favoriler"), QStringLiteral("Steam Oyunları"),
                    QStringLiteral("Emülatörler"), QStringLiteral("Tüm Oyunlar")};

    const QVariantList loadedGames = listFromJson(configFile(QStringLiteral("games.json")));
    const QVariantList loadedEmulators = listFromJson(configFile(QStringLiteral("emulators.json")));
    const QVariantList loadedCategories = listFromJson(configFile(QStringLiteral("categories.json")));

    for (const QVariant &item : loadedGames) {
        m_games.append(normalizedGame(item.toMap()));
    }
    for (int index = 0; index < m_games.size(); ++index) {
        const QVariantMap game = m_games.at(index).toMap();
        const QString appId = game.value(QStringLiteral("appId")).toString().trimmed();
        if (game.value(QStringLiteral("sourceType")).toString().trimmed().toLower() == QStringLiteral("steam")
            && !appId.isEmpty()) {
            fetchSteamArtwork(index, appId);
        }
    }
    if (!loadedEmulators.isEmpty()) {
        m_emulators = loadedEmulators;
    }
    if (!loadedCategories.isEmpty()) {
        m_categories = loadedCategories;
    }

    QFile themeFile(configFile(QStringLiteral("theme.json")));
    if (themeFile.open(QIODevice::ReadOnly)) {
        const QJsonDocument document = QJsonDocument::fromJson(themeFile.readAll());
        if (document.isObject()) {
            const QVariantMap loadedTheme = document.object().toVariantMap();
            for (auto it = loadedTheme.cbegin(); it != loadedTheme.cend(); ++it) {
                m_theme.insert(it.key(), it.value());
            }
        }
    }

    QFile settingsFile(configFile(QStringLiteral("settings.json")));
    if (settingsFile.open(QIODevice::ReadOnly)) {
        const QJsonDocument document = QJsonDocument::fromJson(settingsFile.readAll());
        if (document.isObject()) {
            const QVariantMap loadedSettings = document.object().toVariantMap();
            for (auto it = loadedSettings.cbegin(); it != loadedSettings.cend(); ++it) {
                m_settings.insert(it.key(), it.value());
            }
        }
    }

    QFile logoFile(configFile(QStringLiteral("logo.json")));
    if (logoFile.open(QIODevice::ReadOnly)) {
        const QJsonDocument document = QJsonDocument::fromJson(logoFile.readAll());
        if (document.isObject()) {
            m_logoPath = document.object().value(QStringLiteral("path")).toString();
        }
    }
    if (m_logoPath.isEmpty()) {
        m_logoPath = defaultIconPath();
    }

    saveState();
}

void AppController::saveState() const
{
    writeJsonArray(configFile(QStringLiteral("games.json")), m_games);
    writeJsonArray(configFile(QStringLiteral("emulators.json")), m_emulators);
    writeJsonArray(configFile(QStringLiteral("categories.json")), m_categories);

    QFile themeFile(configFile(QStringLiteral("theme.json")));
    if (themeFile.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
        themeFile.write(QJsonDocument(QJsonObject::fromVariantMap(m_theme)).toJson(QJsonDocument::Indented));
    }

    QFile settingsFile(configFile(QStringLiteral("settings.json")));
    if (settingsFile.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
        settingsFile.write(QJsonDocument(QJsonObject::fromVariantMap(m_settings)).toJson(QJsonDocument::Indented));
    }

    QFile logoFile(configFile(QStringLiteral("logo.json")));
    if (logoFile.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
        logoFile.write(QJsonDocument(QJsonObject{{QStringLiteral("path"), m_logoPath}})
                           .toJson(QJsonDocument::Indented));
    }
}

void AppController::setStatus(const QString &message)
{
    m_statusMessage = message;
    emit statusChanged();
}

bool AppController::ensureExecutable(const QString &path) const
{
    QFileInfo info(path);
    if (!info.exists() || !info.isFile()) {
        return false;
    }
    if (info.isExecutable()) {
        return true;
    }

    QFile file(path);
    QFileDevice::Permissions permissions = info.permissions();
    permissions |= QFileDevice::ExeOwner | QFileDevice::ExeGroup | QFileDevice::ExeOther;
    return file.setPermissions(permissions) && QFileInfo(path).isExecutable();
}

QString AppController::copyApplication(const QString &sourcePath) const
{
    QFileInfo sourceInfo(sourcePath);
    if (!sourceInfo.exists() || !sourceInfo.isFile()) {
        return {};
    }

    const QString canonicalSource = sourceInfo.canonicalFilePath();
    const QString canonicalData = QFileInfo(applicationDirectory()).canonicalFilePath();
    if (!canonicalSource.isEmpty() && !canonicalData.isEmpty() && canonicalSource.startsWith(canonicalData + QDir::separator())) {
        return sourcePath;
    }

    QDir().mkpath(applicationDirectory());
    const QString suffix = sourceInfo.suffix().toLower();
    const QString targetName = QStringLiteral("%1-%2%3")
                                   .arg(sourceInfo.completeBaseName(), QUuid::createUuid().toString(QUuid::WithoutBraces))
                                   .arg(suffix.isEmpty() ? QString() : QStringLiteral(".") + suffix);
    const QString target = QDir(applicationDirectory()).filePath(targetName);
    if (!QFile::copy(sourcePath, target)) {
        return {};
    }
    return target;
}

QString AppController::copyIconOrDefault(const QString &source, const QString &fallback) const
{
    const QString sourcePath = localPathFromUrl(source);
    QFileInfo info(sourcePath);
    if (!info.exists() || !info.isFile() || !isSupportedImage(info.suffix())) {
        return fallback.isEmpty() ? defaultIconPath() : fallback;
    }

    const QString directory = assetDirectory(QStringLiteral("icons"));
    QDir().mkpath(directory);
    const QString target = QDir(directory).filePath(
        QStringLiteral("%1.%2").arg(QUuid::createUuid().toString(QUuid::WithoutBraces), info.suffix().toLower()));
    if (QFile::copy(sourcePath, target)) {
        return target;
    }
    return fallback.isEmpty() ? defaultIconPath() : fallback;
}

bool AppController::copyAsset(const QString &sourceUrl, const QString &assetType, QString *destinationPath)
{
    if (!QStringList{QStringLiteral("logos"), QStringLiteral("icons"), QStringLiteral("covers"), QStringLiteral("banners")}
             .contains(assetType)) {
        return false;
    }

    const QString sourcePath = localPathFromUrl(sourceUrl);
    QFileInfo sourceInfo(sourcePath);
    if (!sourceInfo.exists() || !sourceInfo.isFile() || !isSupportedImage(sourceInfo.suffix())) {
        return false;
    }

    const QString directory = assetDirectory(assetType);
    QDir().mkpath(directory);
    const QString target = QDir(directory).filePath(
        QStringLiteral("%1.%2").arg(QUuid::createUuid().toString(QUuid::WithoutBraces), sourceInfo.suffix().toLower()));
    if (!QFile::copy(sourcePath, target)) {
        return false;
    }
    if (destinationPath) {
        *destinationPath = target;
    }
    return true;
}

QString AppController::resolveIconPath(const QString &iconValue, const QString &desktopPath) const
{
    if (iconValue.trimmed().isEmpty()) {
        return defaultIconPath();
    }

    const QString raw = localPathFromUrl(iconValue.trimmed());
    QStringList candidates;
    QFileInfo directInfo(raw);
    if (directInfo.isAbsolute()) {
        candidates.append(raw);
    } else if (!desktopPath.isEmpty()) {
        candidates.append(QDir(QFileInfo(desktopPath).absolutePath()).filePath(raw));
    }

    const QStringList roots = {
        QStringLiteral("/usr/share/pixmaps"),
        QStringLiteral("/usr/local/share/pixmaps"),
        QStringLiteral("/usr/share/icons/hicolor/512x512/apps"),
        QStringLiteral("/usr/share/icons/hicolor/256x256/apps"),
        QStringLiteral("/usr/share/icons/hicolor/128x128/apps"),
        QDir(QStandardPaths::writableLocation(QStandardPaths::GenericDataLocation)).filePath(QStringLiteral("icons"))
    };
    for (const QString &root : roots) {
        candidates.append(QDir(root).filePath(raw));
        for (const QString &extension : {QStringLiteral("png"), QStringLiteral("svg"), QStringLiteral("xpm"), QStringLiteral("jpg")}) {
            candidates.append(QDir(root).filePath(raw + QStringLiteral(".") + extension));
        }
    }

    for (const QString &candidate : candidates) {
        QFileInfo info(candidate);
        if (info.exists() && info.isFile() && isSupportedImage(info.suffix())) {
            return candidate;
        }
    }
    return defaultIconPath();
}

QString AppController::resolveAppImageIcon(const QString &appImagePath) const
{
    const QFileInfo appInfo(appImagePath);
    if (!appInfo.exists()) {
        return defaultIconPath();
    }

    const QString base = QDir(appInfo.absolutePath()).filePath(appInfo.completeBaseName());
    for (const QString &extension : {QStringLiteral("png"), QStringLiteral("svg"), QStringLiteral("xpm"), QStringLiteral("jpg")}) {
        const QString candidate = base + QStringLiteral(".") + extension;
        if (QFileInfo::exists(candidate)) {
            return candidate;
        }
    }

    const QString iconDirectory = QDir(appInfo.absolutePath()).filePath(QStringLiteral("icons"));
    for (const QString &extension : {QStringLiteral("png"), QStringLiteral("svg"), QStringLiteral("xpm")}) {
        const QString candidate = QDir(iconDirectory).filePath(appInfo.completeBaseName() + QStringLiteral(".") + extension);
        if (QFileInfo::exists(candidate)) {
            return candidate;
        }
    }
    return defaultIconPath();
}

QVariantMap AppController::readDesktopEntry(const QString &desktopPath, QString *error) const
{
    QFile file(desktopPath);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        if (error) *error = QStringLiteral(".desktop dosyası okunamadı.");
        return {};
    }

    QVariantMap entry;
    bool inDesktopEntry = false;
    const QString contents = QString::fromUtf8(file.readAll());
    const QStringList lines = contents.split(QRegularExpression(QStringLiteral("[\\r\\n]")));
    for (const QString &rawLine : lines) {
        const QString line = rawLine.trimmed();
        if (line.startsWith('[') && line.endsWith(']')) {
            inDesktopEntry = line == QStringLiteral("[Desktop Entry]");
            continue;
        }
        if (!inDesktopEntry || line.isEmpty() || line.startsWith('#') || !line.contains('=')) {
            continue;
        }
        const int separator = line.indexOf('=');
        const QString key = line.left(separator).trimmed();
        if (key.contains('[')) {
            continue;
        }
        entry.insert(key, cleanDesktopValue(line.mid(separator + 1)));
    }

    if (entry.value(QStringLiteral("Type"), QStringLiteral("Application")).toString() != QStringLiteral("Application")) {
        if (error) *error = QStringLiteral("Yalnızca Application türündeki .desktop dosyaları desteklenir.");
        return {};
    }
    if (entry.value(QStringLiteral("Name")).toString().trimmed().isEmpty()
        || entry.value(QStringLiteral("Exec")).toString().trimmed().isEmpty()) {
        if (error) *error = QStringLiteral(".desktop dosyasında Name veya Exec alanı eksik.");
        return {};
    }
    return entry;
}

QStringList AppController::desktopExecArguments(const QString &execLine, const QVariantMap &entry,
                                                const QString &desktopPath) const
{
    const QStringList tokens = splitDesktopExec(execLine);
    if (tokens.isEmpty()) {
        return {};
    }

    QStringList output;
    for (int index = 0; index < tokens.size(); ++index) {
        QString token = tokens.at(index);
        if (index > 0 && token.size() == 2 && token.at(0) == '%'
            && QStringLiteral("fFuUdDicm").contains(token.at(1))) {
            continue;
        }
        token.replace(QStringLiteral("%%"), QStringLiteral("%"));
        token.replace(QStringLiteral("%c"), entry.value(QStringLiteral("Name")).toString());
        token.replace(QStringLiteral("%k"), desktopPath);
        token.replace(QStringLiteral("%d"), QFileInfo(desktopPath).absolutePath());
        token.remove(QRegularExpression(QStringLiteral("%[fFuUim]")));
        if (!token.isEmpty()) {
            output.append(token);
        }
    }
    return output;
}

bool AppController::acquireLaunchGuard(const QString &key)
{
    const QString normalizedKey = key.trimmed();
    if (normalizedKey.isEmpty()) {
        return false;
    }

    const qint64 now = QDateTime::currentMSecsSinceEpoch();
    const auto previous = m_launchCooldowns.constFind(normalizedKey);
    if (previous != m_launchCooldowns.cend() && now - previous.value() < 1500) {
        setStatus(QStringLiteral("Bu kayıt zaten başlatılıyor."));
        return false;
    }
    m_launchCooldowns.insert(normalizedKey, now);
    return true;
}

void AppController::fetchSteamArtwork(int index, const QString &appId)
{
    if (!m_networkManager || index < 0 || index >= m_games.size()
        || !QRegularExpression(QStringLiteral("^[0-9]+$")).match(appId).hasMatch()) {
        return;
    }

    const QVariantMap game = m_games.at(index).toMap();
    const QString cachedPath = game.value(QStringLiteral("steamArtworkPath")).toString().trimmed();
    if (!cachedPath.isEmpty() && QFileInfo::exists(cachedPath)) {
        return;
    }

    const QUrl endpoint(QStringLiteral("https://store.steampowered.com/api/appdetails?appids=%1&l=english").arg(appId));
    QNetworkRequest request(endpoint);
    request.setHeader(QNetworkRequest::UserAgentHeader, QStringLiteral("TeknoKonsole/0.1"));
    QNetworkReply *reply = m_networkManager->get(request);
    connect(reply, &QNetworkReply::finished, this, [this, reply, index, appId]() {
        const QByteArray payload = reply->readAll();
        const bool requestSucceeded = reply->error() == QNetworkReply::NoError;
        reply->deleteLater();

        QString artworkUrl;
        if (requestSucceeded) {
            QJsonParseError parseError;
            const QJsonDocument document = QJsonDocument::fromJson(payload, &parseError);
            const QVariantMap root = parseError.error == QJsonParseError::NoError && document.isObject()
                                         ? document.object().toVariantMap()
                                         : QVariantMap();
            const QVariantMap app = root.value(appId).toMap();
            const QVariantMap data = app.value(QStringLiteral("data")).toMap();
            artworkUrl = data.value(QStringLiteral("header_image")).toString().trimmed();
        }

        if (!artworkUrl.isEmpty()) {
            downloadSteamArtwork(index, appId, QUrl(artworkUrl));
        } else {
            downloadSteamArtwork(index, appId,
                                 QUrl(QStringLiteral("https://cdn.cloudflare.steamstatic.com/steam/apps/%1/header.jpg").arg(appId)));
        }
    });
}

void AppController::downloadSteamArtwork(int index, const QString &appId, const QUrl &url)
{
    if (!m_networkManager || !url.isValid()) {
        return;
    }

    QNetworkRequest request(url);
    request.setHeader(QNetworkRequest::UserAgentHeader, QStringLiteral("TeknoKonsole/0.1"));
    QNetworkReply *reply = m_networkManager->get(request);
    connect(reply, &QNetworkReply::finished, this, [this, reply, index, appId]() {
        const QByteArray imageData = reply->error() == QNetworkReply::NoError ? reply->readAll() : QByteArray();
        reply->deleteLater();
        if (imageData.isEmpty()) {
            return;
        }

        const QString directory = assetDirectory(QStringLiteral("covers"));
        QDir().mkpath(directory);
        const QString path = QDir(directory).filePath(QStringLiteral("steam-%1.jpg").arg(appId));
        QFile imageFile(path);
        if (!imageFile.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
            return;
        }
        imageFile.write(imageData);
        imageFile.close();
        applySteamArtwork(index, appId, path);
    });
}

void AppController::applySteamArtwork(int index, const QString &appId, const QString &path)
{
    if (index < 0 || index >= m_games.size() || !QFileInfo::exists(path)) {
        return;
    }

    QVariantMap game = normalizedGame(m_games.at(index).toMap());
    if (game.value(QStringLiteral("appId")).toString().trimmed() != appId) {
        return;
    }
    if (game.value(QStringLiteral("steamArtworkPath")).toString() == path
        && game.value(QStringLiteral("displayAsset")).toString() == path) {
        return;
    }

    game.insert(QStringLiteral("steamArtworkPath"), path);
    game.insert(QStringLiteral("displayAsset"), path);
    game.insert(QStringLiteral("cover"), path);
    game.insert(QStringLiteral("icon"), path);
    m_games[index] = normalizedGame(game);
    saveState();
    emit contentChanged();
}

bool AppController::launchCommand(const QString &program, const QStringList &arguments, bool)
{
    if (program.trimmed().isEmpty()) {
        setStatus(QStringLiteral("Başlatma komutu boş."));
        return false;
    }

    const QString localProgram = localPathFromUrl(program);
    const bool isAppImage = localProgram.endsWith(QStringLiteral(".AppImage"), Qt::CaseInsensitive);
    if (localProgram.contains(QDir::separator()) && !QFileInfo::exists(localProgram)) {
        setStatus(QStringLiteral("Başlatma dosyası bulunamadı: %1").arg(localProgram));
        return false;
    }
    if (isAppImage && !ensureExecutable(localProgram)) {
        setStatus(QStringLiteral("AppImage çalıştırılabilir değil veya erişilemiyor: %1").arg(localProgram));
        return false;
    }
    const bool isWine = QFileInfo(localProgram).fileName().compare(QStringLiteral("wine"), Qt::CaseInsensitive) == 0;
    if (isWine && !arguments.isEmpty()) {
        const QString exePath = localPathFromUrl(arguments.first());
        if (!QFileInfo::exists(exePath) || !QFileInfo(exePath).isFile()
            || !exePath.endsWith(QStringLiteral(".exe"), Qt::CaseInsensitive)) {
            setStatus(QStringLiteral("Wine EXE dosyası bulunamadı: %1").arg(exePath));
            return false;
        }
    }
    if (!localProgram.contains(QDir::separator()) && QStandardPaths::findExecutable(localProgram).isEmpty()) {
        setStatus(QStringLiteral("Komut bulunamadı: %1").arg(localProgram));
        return false;
    }
    const QString launchKey = QStringLiteral("command:%1:%2").arg(localProgram, arguments.join(QChar(0x1f)));
    if (!acquireLaunchGuard(launchKey)) {
        return false;
    }

    QProcess launcher;
    launcher.setProgram(localProgram);
    launcher.setArguments(arguments);
    if (isAppImage) {
        launcher.setWorkingDirectory(QFileInfo(localProgram).absolutePath());
        QProcessEnvironment environment = QProcessEnvironment::systemEnvironment();
        // AppImage runtime bunu desteklediğinde FUSE gerektirmeden self-extract çalıştırır.
        environment.insert(QStringLiteral("APPIMAGE_EXTRACT_AND_RUN"), QStringLiteral("1"));
        launcher.setProcessEnvironment(environment);
    } else if (isWine && !arguments.isEmpty()) {
        const QString exePath = localPathFromUrl(arguments.first());
        launcher.setWorkingDirectory(QFileInfo(exePath).absolutePath());
        QProcessEnvironment environment = QProcessEnvironment::systemEnvironment();
        environment.insert(QStringLiteral("WINEDEBUG"), QStringLiteral("-all"));
        launcher.setProcessEnvironment(environment);
    }
    suspendGamepad();
    if (!launcher.startDetached()) {
        resumeGamepad();
        setStatus(QStringLiteral("Uygulama başlatılamadı: %1").arg(localProgram));
        return false;
    }
    return true;
}

void AppController::launchGame(int index)
{
    if (index < 0 || index >= m_games.size()) {
        return;
    }

    const QVariantMap game = normalizedGame(m_games.at(index).toMap());
    QString steamUri = game.value(QStringLiteral("steamUri")).toString().trimmed();
    const QString appId = game.value(QStringLiteral("appId")).toString().trimmed();
    if (steamUri.isEmpty() && !appId.isEmpty()) {
        steamUri = QStringLiteral("steam://rungameid/%1").arg(appId);
    }

    if (!steamUri.isEmpty() || game.value(QStringLiteral("sourceType")).toString() == QStringLiteral("steam")) {
        if (steamUri.isEmpty()) {
            setStatus(QStringLiteral("Steam URI veya AppID eksik."));
            return;
        }
        if (QStandardPaths::findExecutable(QStringLiteral("steam")).isEmpty()) {
            setStatus(QStringLiteral("Steam kurulu değil veya PATH içinde bulunamadı."));
            return;
        }
        const QString steamExecutable = QStandardPaths::findExecutable(QStringLiteral("steam"));
        if (!acquireLaunchGuard(QStringLiteral("steam:%1").arg(steamUri))) {
            return;
        }
        suspendGamepad();
        if (!QProcess::startDetached(steamExecutable, {QStringLiteral("-silent"), steamUri})) {
            resumeGamepad();
            setStatus(QStringLiteral("Steam oyunu arka planda başlatılamadı."));
            return;
        }
        setStatus(QStringLiteral("%1 başlatılıyor...").arg(game.value(QStringLiteral("name")).toString()));
        return;
    }

    const QString sourceType = game.value(QStringLiteral("sourceType")).toString().toLower();
    const QString launchPath = game.value(QStringLiteral("launchPath")).toString().trimmed();
    if (sourceType == QStringLiteral("wine") || launchPath.endsWith(QStringLiteral(".exe"), Qt::CaseInsensitive)) {
        const QString exePath = localPathFromUrl(launchPath);
        if (!QFileInfo::exists(exePath) || !QFileInfo(exePath).isFile()) {
            setStatus(QStringLiteral("Windows EXE dosyası bulunamadı: %1").arg(exePath));
            return;
        }
#ifdef Q_OS_WIN
        if (!launchCommand(exePath, {})) {
            return;
        }
        setStatus(QStringLiteral("%1 Windows ile başlatılıyor...").arg(game.value(QStringLiteral("name")).toString()));
#else
        if (QStandardPaths::findExecutable(QStringLiteral("wine")).isEmpty()) {
            setStatus(QStringLiteral("Wine kurulu değil. Windows EXE çalıştırmak için Wine yükleyin."));
            return;
        }
        if (!launchCommand(QStringLiteral("wine"), {exePath})) {
            return;
        }
        setStatus(QStringLiteral("%1 Wine ile başlatılıyor...").arg(game.value(QStringLiteral("name")).toString()));
#endif
        return;
    }
    if (sourceType == QStringLiteral("desktop") || launchPath.endsWith(QStringLiteral(".desktop"), Qt::CaseInsensitive)) {
        QString error;
        const QVariantMap entry = readDesktopEntry(launchPath, &error);
        if (entry.isEmpty()) {
            setStatus(error.isEmpty() ? QStringLiteral(".desktop dosyası kullanılamıyor.") : error);
            return;
        }
        QStringList command = desktopExecArguments(entry.value(QStringLiteral("Exec")).toString(), entry, launchPath);
        if (command.isEmpty()) {
            setStatus(QStringLiteral(".desktop Exec komutu boş veya geçersiz."));
            return;
        }
        const QString program = command.takeFirst();
        if (!launchCommand(program, command)) {
            return;
        }
        setStatus(QStringLiteral("%1 başlatılıyor...").arg(game.value(QStringLiteral("name")).toString()));
        return;
    }

    if (launchPath.isEmpty()) {
        setStatus(QStringLiteral("Bu kayıt için başlatma yolu bulunamadı."));
        return;
    }

    if (launchPath.contains(QStringLiteral("://"))) {
        if (!QDesktopServices::openUrl(QUrl(launchPath))) {
            setStatus(QStringLiteral("URI açılamadı: %1").arg(launchPath));
            return;
        }
    } else {
        const QStringList parts = QProcess::splitCommand(launchPath);
        if (parts.isEmpty() || !launchCommand(parts.first(), parts.mid(1))) {
            return;
        }
    }
    setStatus(QStringLiteral("%1 başlatılıyor...").arg(game.value(QStringLiteral("name")).toString()));
}

void AppController::launchEmulator(int index)
{
    if (index < 0 || index >= m_emulators.size()) {
        return;
    }
    const QVariantMap emulator = m_emulators.at(index).toMap();
    const QString command = emulator.value(QStringLiteral("command")).toString().trimmed();
    const QString type = emulator.value(QStringLiteral("type")).toString().trimmed().toLower();
    const QString desktopFile = emulator.value(QStringLiteral("desktopFile"), command).toString().trimmed();

    if (type == QStringLiteral("desktop") || desktopFile.endsWith(QStringLiteral(".desktop"), Qt::CaseInsensitive)) {
        QString error;
        const QVariantMap entry = readDesktopEntry(desktopFile, &error);
        if (entry.isEmpty()) {
            setStatus(error.isEmpty() ? QStringLiteral("Emülatör .desktop dosyası kullanılamıyor.") : error);
            return;
        }
        QStringList parts = desktopExecArguments(entry.value(QStringLiteral("Exec")).toString(), entry, desktopFile);
        if (parts.isEmpty()) {
            setStatus(QStringLiteral("Emülatör .desktop Exec komutu boş veya geçersiz."));
            return;
        }
        const QString program = parts.takeFirst();
        if (!launchCommand(program, parts)) {
            return;
        }
    } else {
        const QStringList parts = QProcess::splitCommand(command);
        if (parts.isEmpty()) {
            setStatus(QStringLiteral("Emülatör komutu boş."));
            return;
        }
        if (!launchCommand(parts.first(), parts.mid(1))) {
            return;
        }
    }
    setStatus(QStringLiteral("%1 başlatılıyor...").arg(emulator.value(QStringLiteral("name")).toString()));
}

void AppController::addGame(const QString &name, const QString &appId, const QString &description,
                            const QString &category, const QString &launchPath,
                            const QString &steamUri, const QString &iconSource,
                            const QString &sourceType)
{
    if (name.trimmed().isEmpty()) {
        setStatus(QStringLiteral("Oyun veya uygulama adı boş olamaz."));
        return;
    }

    QString storedIcon;
    if (!iconSource.trimmed().isEmpty()) {
        storedIcon = copyIconOrDefault(iconSource);
    }
    if (storedIcon.isEmpty()) {
        storedIcon = defaultIconPath();
    }

    m_games.append(normalizedGame(QVariantMap{
        {QStringLiteral("name"), name.trimmed()},
        {QStringLiteral("appId"), appId.trimmed()},
        {QStringLiteral("description"), description.trimmed()},
        {QStringLiteral("category"), category.trimmed().isEmpty() ? QStringLiteral("Tüm Oyunlar") : category.trimmed()},
        {QStringLiteral("favorite"), false},
        {QStringLiteral("icon"), storedIcon},
        {QStringLiteral("cover"), QString()},
        {QStringLiteral("banner"), QString()},
        {QStringLiteral("displayAsset"), QString()},
        {QStringLiteral("launchPath"), launchPath.trimmed()},
        {QStringLiteral("steamUri"), steamUri.trimmed()},
        {QStringLiteral("sourceType"), sourceType.trimmed()}
    }));
    const int addedIndex = m_games.size() - 1;
    saveState();
    if (sourceType.trimmed().toLower() == QStringLiteral("steam") && !appId.trimmed().isEmpty()) {
        fetchSteamArtwork(addedIndex, appId.trimmed());
    }
    emit contentChanged();
    setStatus(QStringLiteral("Kayıt eklendi: %1").arg(name.trimmed()));
}

void AppController::editGame(int index, const QString &name, const QString &appId,
                             const QString &description, const QString &category,
                             const QString &launchPath, const QString &steamUri,
                             const QString &iconSource, const QString &sourceType)
{
    if (index < 0 || index >= m_games.size() || name.trimmed().isEmpty()) {
        setStatus(QStringLiteral("Kayıt adı boş olamaz."));
        return;
    }

    QVariantMap game = normalizedGame(m_games.at(index).toMap());
    const QString previousAppId = game.value(QStringLiteral("appId")).toString().trimmed();
    game.insert(QStringLiteral("name"), name.trimmed());
    game.insert(QStringLiteral("appId"), appId.trimmed());
    game.insert(QStringLiteral("description"), description.trimmed());
    game.insert(QStringLiteral("category"), category.trimmed().isEmpty() ? QStringLiteral("Tüm Oyunlar") : category.trimmed());
    game.insert(QStringLiteral("launchPath"), launchPath.trimmed());
    game.insert(QStringLiteral("steamUri"), steamUri.trimmed());
    if (!sourceType.trimmed().isEmpty()) {
        game.insert(QStringLiteral("sourceType"), sourceType.trimmed());
    }
    if (!iconSource.trimmed().isEmpty()) {
        game.insert(QStringLiteral("icon"), copyIconOrDefault(iconSource, game.value(QStringLiteral("icon")).toString()));
    }
    if (previousAppId != appId.trimmed()) {
        game.remove(QStringLiteral("steamArtworkPath"));
        game.remove(QStringLiteral("displayAsset"));
        game.remove(QStringLiteral("cover"));
    }
    m_games[index] = normalizedGame(game);
    saveState();
    if (game.value(QStringLiteral("sourceType")).toString().trimmed().toLower() == QStringLiteral("steam")
        && !appId.trimmed().isEmpty()) {
        fetchSteamArtwork(index, appId.trimmed());
    }
    emit contentChanged();
    setStatus(QStringLiteral("Kayıt güncellendi: %1").arg(name.trimmed()));
}

void AppController::removeGame(int index)
{
    if (index < 0 || index >= m_games.size()) {
        return;
    }
    m_games.removeAt(index);
    saveState();
    emit contentChanged();
    setStatus(QStringLiteral("Kayıt silindi."));
}

void AppController::importAppImage(const QString &sourceUrl)
{
    const QString sourcePath = localPathFromUrl(sourceUrl);
    QFileInfo sourceInfo(sourcePath);
    if (!sourceInfo.exists() || !sourceInfo.isFile()) {
        setStatus(QStringLiteral("AppImage dosyası bulunamadı."));
        return;
    }
    if (sourceInfo.suffix().compare(QStringLiteral("AppImage"), Qt::CaseInsensitive) != 0) {
        setStatus(QStringLiteral("Lütfen .AppImage dosyası seçin."));
        return;
    }

    const QString storedPath = copyApplication(sourcePath);
    if (storedPath.isEmpty()) {
        setStatus(QStringLiteral("AppImage kullanıcı verilerine kopyalanamadı."));
        return;
    }
    if (!ensureExecutable(storedPath)) {
        QFile::remove(storedPath);
        setStatus(QStringLiteral("AppImage çalıştırılabilir yapılamadı; dosya eklenmedi."));
        return;
    }

    QString icon = resolveAppImageIcon(sourcePath);
    if (!icon.startsWith(QStringLiteral("qrc:"))) {
        icon = copyIconOrDefault(icon);
    }
    addGame(sourceInfo.completeBaseName(), QString(), QStringLiteral("AppImage uygulaması"),
            QStringLiteral("Tüm Oyunlar"), storedPath, QString(), icon, QStringLiteral("appimage"));
}

void AppController::importWineExe(const QString &sourceUrl)
{
    const QString sourcePath = localPathFromUrl(sourceUrl);
    const QFileInfo sourceInfo(sourcePath);
    if (!sourceInfo.exists() || !sourceInfo.isFile()) {
        setStatus(QStringLiteral("Wine EXE dosyası bulunamadı."));
        return;
    }
    if (sourceInfo.suffix().compare(QStringLiteral("exe"), Qt::CaseInsensitive) != 0) {
        setStatus(QStringLiteral("Lütfen .exe dosyası seçin."));
        return;
    }

    const QString storedPath = copyApplication(sourcePath);
    if (storedPath.isEmpty()) {
        setStatus(QStringLiteral("Wine EXE kullanıcı verilerine kopyalanamadı."));
        return;
    }
    addGame(sourceInfo.completeBaseName(), QString(), QStringLiteral("Windows uygulaması / Wine"),
            QStringLiteral("Tüm Oyunlar"), storedPath, QString(), defaultIconPath(), QStringLiteral("wine"));
}

void AppController::importDesktop(const QString &sourceUrl)
{
    const QString sourcePath = localPathFromUrl(sourceUrl);
    QString error;
    const QVariantMap entry = readDesktopEntry(sourcePath, &error);
    if (entry.isEmpty()) {
        setStatus(error.isEmpty() ? QStringLiteral(".desktop dosyası kullanılamadı.") : error);
        return;
    }

    const QString storedDesktop = copyApplication(sourcePath);
    if (storedDesktop.isEmpty()) {
        setStatus(QStringLiteral(".desktop dosyası kullanıcı verilerine kopyalanamadı."));
        return;
    }

    QString icon = resolveIconPath(entry.value(QStringLiteral("Icon")).toString(), sourcePath);
    if (!icon.startsWith(QStringLiteral("qrc:"))) {
        icon = copyIconOrDefault(icon);
    }
    const QString categories = entry.value(QStringLiteral("Categories"), QStringLiteral("Tüm Oyunlar")).toString()
                                   .split(';', Qt::SkipEmptyParts).value(0, QStringLiteral("Tüm Oyunlar"));
    addGame(entry.value(QStringLiteral("Name")).toString(), QString(),
            entry.value(QStringLiteral("Comment")).toString(), categories,
            storedDesktop, QString(), icon, QStringLiteral("desktop"));
}

void AppController::importAppImageAsEmulator(const QString &sourceUrl)
{
    const QString sourcePath = localPathFromUrl(sourceUrl);
    const QFileInfo sourceInfo(sourcePath);
    if (!sourceInfo.exists() || !sourceInfo.isFile()) {
        setStatus(QStringLiteral("Emülatör AppImage dosyası bulunamadı."));
        return;
    }
    if (sourceInfo.suffix().compare(QStringLiteral("AppImage"), Qt::CaseInsensitive) != 0) {
        setStatus(QStringLiteral("Lütfen .AppImage dosyası seçin."));
        return;
    }

    const QString storedPath = copyApplication(sourcePath);
    if (storedPath.isEmpty() || !ensureExecutable(storedPath)) {
        if (!storedPath.isEmpty()) QFile::remove(storedPath);
        setStatus(QStringLiteral("Emülatör AppImage çalıştırılabilir yapılamadı; dosya eklenmedi."));
        return;
    }

    QString icon = resolveAppImageIcon(sourcePath);
    if (!icon.startsWith(QStringLiteral("qrc:"))) {
        icon = copyIconOrDefault(icon);
    }
    m_emulators.append(QVariantMap{
        {QStringLiteral("name"), sourceInfo.completeBaseName()},
        {QStringLiteral("command"), storedPath},
        {QStringLiteral("type"), QStringLiteral("appimage")},
        {QStringLiteral("icon"), icon.isEmpty() ? defaultIconPath() : icon},
        {QStringLiteral("banner"), QString()}
    });
    saveState();
    emit contentChanged();
    setStatus(QStringLiteral("Emülatör AppImage eklendi: %1").arg(sourceInfo.completeBaseName()));
}

void AppController::importDesktopAsEmulator(const QString &sourceUrl)
{
    const QString sourcePath = localPathFromUrl(sourceUrl);
    QString error;
    const QVariantMap entry = readDesktopEntry(sourcePath, &error);
    if (entry.isEmpty()) {
        setStatus(error.isEmpty() ? QStringLiteral("Emülatör .desktop dosyası kullanılamadı.") : error);
        return;
    }

    const QString storedDesktop = copyApplication(sourcePath);
    if (storedDesktop.isEmpty()) {
        setStatus(QStringLiteral("Emülatör .desktop dosyası kullanıcı verilerine kopyalanamadı."));
        return;
    }
    QString icon = resolveIconPath(entry.value(QStringLiteral("Icon")).toString(), sourcePath);
    if (!icon.startsWith(QStringLiteral("qrc:"))) {
        icon = copyIconOrDefault(icon);
    }
    const QString name = entry.value(QStringLiteral("Name")).toString().trimmed();
    m_emulators.append(QVariantMap{
        {QStringLiteral("name"), name},
        {QStringLiteral("command"), storedDesktop},
        {QStringLiteral("desktopFile"), storedDesktop},
        {QStringLiteral("type"), QStringLiteral("desktop")},
        {QStringLiteral("icon"), icon.isEmpty() ? defaultIconPath() : icon},
        {QStringLiteral("banner"), QString()}
    });
    saveState();
    emit contentChanged();
    setStatus(QStringLiteral("Emülatör masaüstü uygulaması eklendi: %1").arg(name));
}

void AppController::addEmulator(const QString &name, const QString &command)
{
    if (name.trimmed().isEmpty() || command.trimmed().isEmpty()) {
        setStatus(QStringLiteral("Emülatör adı ve komutu boş olamaz."));
        return;
    }
    m_emulators.append(QVariantMap{
        {QStringLiteral("name"), name.trimmed()},
        {QStringLiteral("command"), command.trimmed()},
        {QStringLiteral("type"), QStringLiteral("emulator")},
        {QStringLiteral("icon"), defaultIconPath()},
        {QStringLiteral("banner"), QString()}
    });
    saveState();
    emit contentChanged();
    setStatus(QStringLiteral("Emülatör eklendi: %1").arg(name.trimmed()));
}

void AppController::editEmulator(int index, const QString &name, const QString &command)
{
    if (index < 0 || index >= m_emulators.size() || name.trimmed().isEmpty() || command.trimmed().isEmpty()) {
        return;
    }
    QVariantMap emulator = m_emulators.at(index).toMap();
    emulator.insert(QStringLiteral("name"), name.trimmed());
    emulator.insert(QStringLiteral("command"), command.trimmed());
    m_emulators[index] = emulator;
    saveState();
    emit contentChanged();
}

void AppController::removeEmulator(int index)
{
    if (index < 0 || index >= m_emulators.size()) {
        return;
    }
    m_emulators.removeAt(index);
    saveState();
    emit contentChanged();
    setStatus(QStringLiteral("Emülatör silindi."));
}

void AppController::addCategory(const QString &name)
{
    const QString category = name.trimmed();
    if (category.isEmpty() || m_categories.contains(category)) {
        return;
    }
    m_categories.append(category);
    saveState();
    emit contentChanged();
}

void AppController::removeCategory(int index)
{
    if (index < 0 || index >= m_categories.size() || m_categories.at(index).toString() == QStringLiteral("Tüm Oyunlar")) {
        return;
    }
    m_categories.removeAt(index);
    saveState();
    emit contentChanged();
}

void AppController::setThemeColor(const QString &name, const QString &value)
{
    if (!m_theme.contains(name) || !value.startsWith('#')) {
        return;
    }
    m_theme.insert(name, value);
    saveState();
    emit themeChanged();
}

void AppController::resetTheme()
{
    m_theme = {
        {QStringLiteral("accent"), QStringLiteral("#14A50F")},
        {QStringLiteral("background"), QStringLiteral("#080A08")},
        {QStringLiteral("surface"), QStringLiteral("#111411")},
        {QStringLiteral("text"), QStringLiteral("#F2F2F2")},
        {QStringLiteral("secondaryText"), QStringLiteral("#8A918A")}
    };
    saveState();
    emit themeChanged();
}

void AppController::setLogo(const QString &sourceUrl)
{
    QString destination;
    if (!copyAsset(sourceUrl, QStringLiteral("logos"), &destination)) {
        setStatus(QStringLiteral("Logo dosyası alınamadı. PNG, JPG veya SVG seçin."));
        return;
    }
    m_logoPath = destination;
    saveState();
    emit logoChanged();
    setStatus(QStringLiteral("Logo güncellendi."));
}

QString AppController::importAsset(const QString &sourceUrl, const QString &assetType)
{
    QString destination;
    if (!copyAsset(sourceUrl, assetType, &destination)) {
        setStatus(QStringLiteral("Görsel dosyası alınamadı."));
        return {};
    }
    setStatus(QStringLiteral("Görsel içe aktarıldı."));
    return destination;
}

void AppController::updateMapAsset(QVariantList &list, int index, const QString &assetType, const QString &sourceUrl)
{
    if (index < 0 || index >= list.size()) {
        return;
    }
    const QString property = assetType == QStringLiteral("icons") ? QStringLiteral("icon")
                          : assetType == QStringLiteral("covers") ? QStringLiteral("cover")
                          : QStringLiteral("banner");
    const QString destination = importAsset(sourceUrl, assetType);
    if (destination.isEmpty()) {
        return;
    }
    QVariantMap map = list.at(index).toMap();
    map.insert(property, destination);
    map.insert(QStringLiteral("displayAsset"), destination);
    list[index] = map;
}

void AppController::setGameAsset(int index, const QString &assetType, const QString &sourceUrl)
{
    updateMapAsset(m_games, index, assetType, sourceUrl);
    saveState();
    emit contentChanged();
}

void AppController::setEmulatorAsset(int index, const QString &assetType, const QString &sourceUrl)
{
    updateMapAsset(m_emulators, index, assetType, sourceUrl);
    saveState();
    emit contentChanged();
}

void AppController::initializeGamepad()
{
    if (m_gamepadSuspended || m_gameController) {
        return;
    }

    constexpr Uint32 gamepadSubsystem = SDL_INIT_GAMECONTROLLER | SDL_INIT_JOYSTICK;
    if ((SDL_WasInit(gamepadSubsystem) & gamepadSubsystem) != gamepadSubsystem
        && SDL_Init(gamepadSubsystem) != 0) {
        setStatus(QStringLiteral("Gamepad başlatılamadı: %1").arg(QString::fromUtf8(SDL_GetError())));
        return;
    }

    for (int index = 0; index < SDL_NumJoysticks(); ++index) {
        if (SDL_IsGameController(index)) {
            m_gameController = SDL_GameControllerOpen(index);
            if (m_gameController) {
                setStatus(QStringLiteral("Gamepad bağlandı: %1")
                              .arg(QString::fromUtf8(SDL_GameControllerName(
                                  static_cast<SDL_GameController *>(m_gameController)))));
                break;
            }
        }
    }
    if (m_gameController) {
        m_gamepadTimer.start();
    }
}

void AppController::suspendGamepad()
{
    if (m_gamepadSuspended) {
        return;
    }
    m_gamepadSuspended = true;
    m_gamepadTimer.stop();
    if (m_gameController) {
        SDL_GameControllerClose(static_cast<SDL_GameController *>(m_gameController));
        m_gameController = nullptr;
    }
    SDL_QuitSubSystem(SDL_INIT_GAMECONTROLLER | SDL_INIT_JOYSTICK);
}

void AppController::resumeGamepad()
{
    if (!m_gamepadSuspended) {
        return;
    }
    m_gamepadSuspended = false;
    initializeGamepad();
}

void AppController::startSteamIfNeeded()
{
    const QString steamExecutable = QStandardPaths::findExecutable(QStringLiteral("steam"));
    if (steamExecutable.isEmpty()) {
        return;
    }
    if (QProcess::execute(QStringLiteral("pidof"), {QStringLiteral("steam")}) == 0) {
        return;
    }
    QProcess::startDetached(steamExecutable, {QStringLiteral("-silent")});
}

void AppController::pollGamepad()
{
    if (m_gamepadSuspended || !m_gameController) {
        return;
    }
    SDL_Event event;
    while (SDL_PollEvent(&event)) {
        if (event.type == SDL_CONTROLLERBUTTONDOWN) {
            switch (event.cbutton.button) {
            case SDL_CONTROLLER_BUTTON_DPAD_UP: emit navigate(-1); break;
            case SDL_CONTROLLER_BUTTON_DPAD_DOWN: emit navigate(1); break;
            case SDL_CONTROLLER_BUTTON_DPAD_LEFT: emit navigate(-1); break;
            case SDL_CONTROLLER_BUTTON_DPAD_RIGHT: emit navigate(1); break;
            case SDL_CONTROLLER_BUTTON_A: emit selectPressed(); break;
            case SDL_CONTROLLER_BUTTON_B: emit backPressed(); break;
            case SDL_CONTROLLER_BUTTON_GUIDE: emit homePressed(); break;
            case SDL_CONTROLLER_BUTTON_START: emit menuPressed(); break;
            default: break;
            }
        }
    }
}

void AppController::quit()
{
    QCoreApplication::quit();
}
