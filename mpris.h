#pragma once

#include <QAbstractButton>
#include <QCoreApplication>
#include <QDBusAbstractAdaptor>
#include <QDBusConnection>
#include <QDBusMessage>
#include <QDBusObjectPath>
#include <QFileInfo>
#include <QListWidget>
#include <QMediaPlayer>
#include <QSlider>
#include <QUrl>
#include <QVariantMap>
#include <QWidget>

class MprisRootAdaptor final : public QDBusAbstractAdaptor {
    Q_OBJECT
    Q_CLASSINFO("D-Bus Interface", "org.mpris.MediaPlayer2")
    Q_PROPERTY(bool CanQuit READ canQuit)
    Q_PROPERTY(bool CanRaise READ canRaise)
    Q_PROPERTY(bool HasTrackList READ hasTrackList)
    Q_PROPERTY(QString Identity READ identity)
    Q_PROPERTY(QString DesktopEntry READ desktopEntry)
    Q_PROPERTY(QStringList SupportedUriSchemes READ supportedUriSchemes)
    Q_PROPERTY(QStringList SupportedMimeTypes READ supportedMimeTypes)

public:
    explicit MprisRootAdaptor(QWidget *window)
        : QDBusAbstractAdaptor(window), window_(window) {}

    bool canQuit() const { return true; }
    bool canRaise() const { return true; }
    bool hasTrackList() const { return false; }
    QString identity() const { return QStringLiteral("sound.execute();"); }
    QString desktopEntry() const { return {}; }
    QStringList supportedUriSchemes() const { return {}; }
    QStringList supportedMimeTypes() const { return {}; }

public slots:
    void Raise() { window_->showNormal(); window_->raise(); window_->activateWindow(); }
    void Quit() { QCoreApplication::quit(); }

private:
    QWidget *window_;
};

class MprisPlayerAdaptor final : public QDBusAbstractAdaptor {
    Q_OBJECT
    Q_CLASSINFO("D-Bus Interface", "org.mpris.MediaPlayer2.Player")
    Q_PROPERTY(QString PlaybackStatus READ playbackStatus)
    Q_PROPERTY(QString LoopStatus READ loopStatus WRITE setLoopStatus)
    Q_PROPERTY(double Rate READ rate WRITE setRate)
    Q_PROPERTY(bool Shuffle READ shuffle WRITE setShuffle)
    Q_PROPERTY(QVariantMap Metadata READ metadata)
    Q_PROPERTY(double Volume READ volume WRITE setVolume)
    Q_PROPERTY(qlonglong Position READ position)
    Q_PROPERTY(double MinimumRate READ minimumRate)
    Q_PROPERTY(double MaximumRate READ maximumRate)
    Q_PROPERTY(bool CanGoNext READ canGoNext)
    Q_PROPERTY(bool CanGoPrevious READ canGoPrevious)
    Q_PROPERTY(bool CanPlay READ canPlay)
    Q_PROPERTY(bool CanPause READ canPause)
    Q_PROPERTY(bool CanSeek READ canSeek)
    Q_PROPERTY(bool CanControl READ canControl)

public:
    MprisPlayerAdaptor(QObject *owner, QMediaPlayer *player,
                       QAbstractButton *playPauseButton,
                       QAbstractButton *nextButton,
                       QAbstractButton *previousButton,
                       QAbstractButton *loopButton,
                       QAbstractButton *shuffleButton,
                       QSlider *volumeSlider,
                       QSlider *tempoSlider,
                       QListWidget *queue)
        : QDBusAbstractAdaptor(owner), player_(player),
          playPauseButton_(playPauseButton), nextButton_(nextButton),
          previousButton_(previousButton), loopButton_(loopButton),
          shuffleButton_(shuffleButton), volumeSlider_(volumeSlider),
          tempoSlider_(tempoSlider), queue_(queue)
    {
        connect(player_, &QMediaPlayer::playbackStateChanged,
                this, [this] { notifyPropertiesChanged(); });
        connect(player_, &QMediaPlayer::sourceChanged,
                this, [this] { notifyPropertiesChanged(); });
        connect(player_, &QMediaPlayer::durationChanged,
                this, [this] { notifyPropertiesChanged(); });
        connect(loopButton_, &QAbstractButton::toggled,
                this, [this] { notifyPropertiesChanged(); });
        connect(shuffleButton_, &QAbstractButton::toggled,
                this, [this] { notifyPropertiesChanged(); });
        connect(volumeSlider_, &QSlider::valueChanged,
                this, [this] { notifyPropertiesChanged(); });
        connect(tempoSlider_, &QSlider::valueChanged,
                this, [this] { notifyPropertiesChanged(); });
        connect(queue_->model(), &QAbstractItemModel::rowsInserted,
                this, [this] { notifyPropertiesChanged(); });
        connect(queue_->model(), &QAbstractItemModel::rowsRemoved,
                this, [this] { notifyPropertiesChanged(); });
    }

    QString playbackStatus() const {
        switch (player_->playbackState()) {
        case QMediaPlayer::PlayingState: return QStringLiteral("Playing");
        case QMediaPlayer::PausedState:  return QStringLiteral("Paused");
        default:                         return QStringLiteral("Stopped");
        }
    }
    QString loopStatus() const {
        return loopButton_->isChecked() ? QStringLiteral("Track")
                                         : QStringLiteral("None");
    }
    void setLoopStatus(const QString &status) {
        if (status == QStringLiteral("Track") ||
            status == QStringLiteral("None"))
            loopButton_->setChecked(status == QStringLiteral("Track"));
        // Playlist repeat is not implemented in this version.
    }
    double rate() const { return player_->playbackRate(); }
    void setRate(double rateValue) {
        if (rateValue <= 0.0) { Pause(); return; }
        tempoSlider_->setValue(qBound(tempoSlider_->minimum(),
                                     qRound(rateValue * 100.0),
                                     tempoSlider_->maximum()));
    }
    bool shuffle() const { return shuffleButton_->isChecked(); }
    void setShuffle(bool value) { shuffleButton_->setChecked(value); }
    QVariantMap metadata() const {
        if (player_->source().isEmpty()) return {};
        const QString uri = player_->source().toString();
        const QString trackPath = QStringLiteral("/com/sound_execute/Track/")
                                  + QString::number(qHash(uri));
        return {
            {QStringLiteral("mpris:trackid"),
             QVariant::fromValue(QDBusObjectPath(trackPath))},
            {QStringLiteral("mpris:length"),
             QVariant::fromValue(qlonglong(player_->duration()) * 1000)},
            {QStringLiteral("xesam:title"),
             QFileInfo(player_->source().toLocalFile()).completeBaseName()},
            {QStringLiteral("xesam:url"), uri}
        };
    }
    double volume() const { return volumeSlider_->value() / 100.0; }
    void setVolume(double value) {
        volumeSlider_->setValue(qBound(0, qRound(value * 100.0), 100));
    }
    qlonglong position() const { return qlonglong(player_->position()) * 1000; }
    double minimumRate() const { return tempoSlider_->minimum() / 100.0; }
    double maximumRate() const { return tempoSlider_->maximum() / 100.0; }
    bool canGoNext() const { return queue_->count() > 1; }
    bool canGoPrevious() const { return queue_->count() > 1; }
    bool canPlay() const {
        return queue_->count() > 0 || !player_->source().isEmpty();
    }
    bool canPause() const { return !player_->source().isEmpty(); }
    bool canSeek() const { return player_->isSeekable(); }
    bool canControl() const { return true; }

public slots:
    void PlayPause() { if (canPlay()) playPauseButton_->click(); }
    void Play() {
        if (player_->playbackState() == QMediaPlayer::PlayingState) return;
        if (player_->source().isEmpty()) {
            if (canPlay()) playPauseButton_->click();
        } else {
            player_->play();
        }
    }
    void Pause() { player_->pause(); }
    void Stop() { player_->stop(); }
    void Next() { if (canGoNext()) nextButton_->click(); }
    void Previous() { if (canGoPrevious()) previousButton_->click(); }
    void Seek(qlonglong offsetMicroseconds) {
        if (!canSeek()) return;
        const qlonglong target = qBound<qlonglong>(
            0, player_->position() + offsetMicroseconds / 1000,
            player_->duration());
        player_->setPosition(target);
        emit Seeked(target * 1000);
    }
    void SetPosition(const QDBusObjectPath &trackId, qlonglong microseconds) {
        if (!canSeek() || microseconds < 0 ||
            microseconds > qlonglong(player_->duration()) * 1000) return;
        const auto currentTrackId =
            metadata().value(QStringLiteral("mpris:trackid")).value<QDBusObjectPath>();
        if (trackId.path() != currentTrackId.path()) return;
        player_->setPosition(microseconds / 1000);
        emit Seeked(microseconds);
    }
    void OpenUri(const QString &) {}
signals:
    void Seeked(qlonglong positionMicroseconds);

private:
    void notifyPropertiesChanged() {
        QVariantMap changes{
            {QStringLiteral("PlaybackStatus"), playbackStatus()},
            {QStringLiteral("LoopStatus"), loopStatus()},
            {QStringLiteral("Rate"), rate()},
            {QStringLiteral("Shuffle"), shuffle()},
            {QStringLiteral("Metadata"), metadata()},
            {QStringLiteral("Volume"), volume()},
            {QStringLiteral("CanGoNext"), canGoNext()},
            {QStringLiteral("CanGoPrevious"), canGoPrevious()},
            {QStringLiteral("CanPlay"), canPlay()},
            {QStringLiteral("CanPause"), canPause()},
            {QStringLiteral("CanSeek"), canSeek()}
        };
        QDBusMessage signal = QDBusMessage::createSignal(
            QStringLiteral("/org/mpris/MediaPlayer2"),
            QStringLiteral("org.freedesktop.DBus.Properties"),
            QStringLiteral("PropertiesChanged"));
        signal << QStringLiteral("org.mpris.MediaPlayer2.Player")
               << changes << QStringList{};
        QDBusConnection::sessionBus().send(signal);
    }

    QMediaPlayer *player_;
    QAbstractButton *playPauseButton_;
    QAbstractButton *nextButton_;
    QAbstractButton *previousButton_;
    QAbstractButton *loopButton_;
    QAbstractButton *shuffleButton_;
    QSlider *volumeSlider_;
    QSlider *tempoSlider_;
    QListWidget *queue_;
};
