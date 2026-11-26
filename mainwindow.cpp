
#include "mainwindow.h"
#include "ui_mainwindow.h"

#include <QMediaPlayer>
#include <QAudioOutput>
#include <QPushButton>
#include <QSlider>
#include <QFileDialog>
#include <QFileInfo>
#include <QUrl>
#include <QCheckBox>
#include <QRandomGenerator>
#include <QSqlDatabase>
#include <QSqlQuery>
#include <QSqlError>
#include <QStandardPaths>
#include <QDir>
#include <QDirIterator>
#include <QListWidget>
#include <QInputDialog>
#include <QLineEdit>
#include <QDebug>
#include <QMessageBox>
#include <QMenu>
#include <QAction>
#include <QBoxLayout>
#include <QSizePolicy>
#include <QAbstractItemView>
#include <QUuid>
#include <QEvent>
#include <QTimer>
#include <QStyle>
#include <QSize>
#include <QToolButton>
#include "mpris.h"
#include <QDBusConnection>
#include <QDebug>

static QString formatTime(qint64 milliseconds)
{
    qint64 seconds = milliseconds / 1000;

    return QString("%1:%2")
        .arg(seconds / 60)
        .arg(seconds % 60, 2, 10, QChar('0'));
}

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
    , ui(new Ui::MainWindow)
{
    ui->setupUi(this);

    ui->mainSplitter->setSizes({280, 550, 350});
    auto *mainLayout = qobject_cast<QBoxLayout*>(
        ui->centralwidget->layout()
        );

    if (mainLayout) {
        mainLayout->setStretchFactor(ui->mainSplitter, 1);
    }

    ui->mainSplitter->setSizePolicy(
        QSizePolicy::Expanding,
        QSizePolicy::Expanding
        );

    ui->mainSplitter->setSizes({250, 650, 350});

    if (!database.initialize()) {
        QMessageBox::critical(
            this,
            "Database Error",
            "Could not initialize the music database."
            );
    } else {
        refreshLibrary();
        refreshPlaylists();
    }
    ui->queueList->viewport()->installEventFilter(this);
    player = new QMediaPlayer(this);
    audioOutput = new QAudioOutput(this);

    player->setAudioOutput(audioOutput);
    player->setPitchCompensation(false);


    ui->playButton_2->setText("");
    ui->playButton_2->setIconSize(QSize(24, 24));

    connect(ui->playButton_2, &QPushButton::clicked,
            this, [this]() {

                if (player->playbackState() ==
                    QMediaPlayer::PlayingState) {

                    player->pause();

                } else if (currentTrack >= 0) {

                    player->play();

                } else if (!tracks.isEmpty()) {

                    playTrack(0);
                }
            });

    auto updatePlayIcon = [this](
                              QMediaPlayer::PlaybackState state) {

        if (state == QMediaPlayer::PlayingState) {

            ui->playButton_2->setIcon(
                QIcon(":/icons/pause.svg")
                );

            ui->playButton_2->setToolTip("Pause");

        } else {

            ui->playButton_2->setIcon(
                QIcon(":/icons/play.svg")
                );

            ui->playButton_2->setToolTip("Play");
        }
    };

    connect(player, &QMediaPlayer::playbackStateChanged,
            this, updatePlayIcon);

    updatePlayIcon(player->playbackState());


    connect(player, &QMediaPlayer::playbackStateChanged,
            this, updatePlayIcon);

    updatePlayIcon(player->playbackState());


    connect(ui->searchEdit, &QLineEdit::textChanged,
            this, &MainWindow::searchLibrary);

    connect(ui->libraryList, &QListWidget::itemDoubleClicked,
            this, [this](QListWidgetItem *item) {

                QString path =
                    item->data(Qt::UserRole + 1).toString();

                if (!QFileInfo::exists(path)) {
                    QMessageBox::warning(
                        this,
                        "File Not Found",
                        "This audio file no longer exists."
                        );
                    return;
                }

                int index = tracks.indexOf(path);

                if (index == -1) {
                    index = appendToQueue(path);
                }

                playTrack(index);
            });

    connect(ui->openButton, &QPushButton::clicked,
            this, &MainWindow::openFile);

    ui->volumeSlider->setRange(0, 100);
    ui->volumeSlider->setValue(50);
    audioOutput->setVolume(0.5f);

    connect(ui->volumeSlider, &QSlider::valueChanged,
            this, [this](int value) {
                audioOutput->setVolume(value / 100.0f);
            });


    ui->tempoSlider->setRange(50, 150);
    ui->tempoSlider->setValue(100);

    player->setPlaybackRate(1.0);

    connect(ui->tempoSlider, &QSlider::valueChanged,
            this, [this](int value) {
                player->setPlaybackRate(value / 100.0);
            });


    connect(ui->loopOn, &QToolButton::toggled,
            this, [this](bool enabled) {

        player->setLoops(
            enabled
                ? QMediaPlayer::Infinite
                : QMediaPlayer::Once
        );
    });

    player->setLoops(
        ui->loopOn->isChecked()
            ? QMediaPlayer::Infinite
            : QMediaPlayer::Once
    );

    connect(player, &QMediaPlayer::durationChanged,
           this, [this](qint64 duration) {
               ui->progressSlider->setRange(0, static_cast<int>(duration));
           });

    connect(player, &QMediaPlayer::positionChanged,
            this, [this](qint64 position) {
                if (!ui->progressSlider->isSliderDown()) {
                    ui->progressSlider->setValue(static_cast<int>(position));
                }
            });

    connect(ui->progressSlider, &QSlider::sliderReleased,
            this, [this](){
            player->setPosition(ui->progressSlider->value());
    }
            );

    connect(ui->queueList, &QListWidget::itemDoubleClicked,
            this, [this](QListWidgetItem *item){
        playTrack(ui->queueList->row(item));
    }
            );
    connect(ui->nextButton_2, &QPushButton::clicked,
            this, &MainWindow::nextTrack);
    connect(ui->previousButton_2, &QPushButton::clicked,
            this, &MainWindow::previousTrack);

    connect(player, &QMediaPlayer::mediaStatusChanged,
            this, [this](QMediaPlayer::MediaStatus status) {
                if (status == QMediaPlayer::EndOfMedia) {
                    nextTrack();
                }
            });
    connect(ui->importFolderButton, &QPushButton::clicked,
            this, &MainWindow::importFolder);

    connect(ui->newPlaylistButton, &QPushButton::clicked,
            this, &MainWindow::createPlaylist);

    connect(ui->addToPlaylistButton, &QPushButton::clicked,
            this, &MainWindow::addToPlaylist);

    connect(ui->playPlaylistButton, &QPushButton::clicked,
            this, &MainWindow::playPlaylist);

    connect(player, &QMediaPlayer::positionChanged,
            this, [this](qint64 position) {
                ui->currentTimeLabel->setText(formatTime(position));
            });

    connect(player, &QMediaPlayer::durationChanged,
            this, [this](qint64 duration) {
                ui->durationLabel->setText(formatTime(duration));
            });
    connect(ui->playlistList, &QListWidget::currentItemChanged,
            this, [this](QListWidgetItem *, QListWidgetItem *) {
                refreshPlaylistTracks();
            });

    ui->libraryList->setContextMenuPolicy(
        Qt::CustomContextMenu
        );

    connect(ui->libraryList,
            &QListWidget::customContextMenuRequested,
            this, [this](const QPoint &pos) {

                QListWidgetItem *item = ui->libraryList->itemAt(pos);

                if (!item)
                    return;

                QMenu menu(this);
                QAction *removeAction =
                    menu.addAction("Remove from Library");

                QAction *selected = menu.exec(
                    ui->libraryList->viewport()->mapToGlobal(pos)
                    );

                if (selected == removeAction) {
                    int songId = item->data(Qt::UserRole).toInt();

                    if (database.deleteSong(songId)) {
                        refreshLibrary();
                        refreshPlaylistTracks();
                    } else {
                        QMessageBox::warning(
                            this, "Error", "Could not remove song."
                            );
                    }
                }
            });

    ui->playlistList->setContextMenuPolicy(
        Qt::CustomContextMenu
        );

    connect(ui->playlistList,
            &QListWidget::customContextMenuRequested,
            this, [this](const QPoint &pos) {

                QListWidgetItem *item = ui->playlistList->itemAt(pos);

                if (!item)
                    return;

                QMenu menu(this);
                QAction *deleteAction =
                    menu.addAction("Delete Playlist");

                QAction *selected = menu.exec(
                    ui->playlistList->viewport()->mapToGlobal(pos)
                    );

                if (selected == deleteAction) {

                    auto confirm = QMessageBox::question(
                        this,
                        "Delete Playlist",
                        "Delete playlist \"" + item->text() + "\"?",
                        QMessageBox::Yes | QMessageBox::No
                        );

                    if (confirm != QMessageBox::Yes)
                        return;

                    int playlistId = item->data(Qt::UserRole).toInt();

                    if (database.deletePlaylist(playlistId)) {
                        refreshPlaylists();
                        ui->playlistTracksList->clear();
                    } else {
                        QMessageBox::warning(
                            this, "Error", "Could not delete playlist."
                            );
                    }
                }
            });

    ui->playlistTracksList->setContextMenuPolicy(
        Qt::CustomContextMenu
        );

    connect(ui->playlistTracksList,
            &QListWidget::customContextMenuRequested,
            this, [this](const QPoint &pos) {

                auto *item = ui->playlistTracksList->itemAt(pos);
                auto *playlist = ui->playlistList->currentItem();

                if (!item || !playlist)
                    return;

                QMenu menu(this);
                QAction *removeAction =
                    menu.addAction("Remove from Playlist");

                if (menu.exec(
                        ui->playlistTracksList->viewport()->mapToGlobal(pos)
                        ) != removeAction)
                    return;

                int playlistId =
                    playlist->data(Qt::UserRole).toInt();

                int position =
                    item->data(Qt::UserRole + 1).toInt();

                if (database.removePlaylistTrack(playlistId, position)) {
                    refreshPlaylistTracks();
                } else {
                    QMessageBox::warning(
                        this, "Error",
                        "Could not remove song from playlist."
                        );
                }
            });


    ui->queueList->setContextMenuPolicy(
        Qt::CustomContextMenu
        );

    connect(ui->queueList,
            &QListWidget::customContextMenuRequested,
            this, [this](const QPoint &pos) {

                auto *item = ui->queueList->itemAt(pos);

                if (!item)
                    return;

                QMenu menu(this);
                QAction *removeAction =
                    menu.addAction("Remove from Queue");

                if (menu.exec(
                        ui->queueList->viewport()->mapToGlobal(pos)
                        ) != removeAction)
                    return;

                int index = ui->queueList->row(item);

                tracks.removeAt(index);
                delete ui->queueList->takeItem(index);

                if (index == currentTrack) {
                    player->stop();

                    if (tracks.isEmpty()) {
                        player->setSource(QUrl());
                        currentTrack = -1;
                        ui->fileLabel->clear();
                        ui->progressSlider->setRange(0, 0);
                    } else {
                        if (index >= tracks.size())
                            index = tracks.size() - 1;

                        playTrack(index);
                    }
                }
                else if (index < currentTrack) {                    currentTrack--;
                    ui->queueList->setCurrentRow(currentTrack);
                }
            });

    ui->queueList->setSortingEnabled(false);

    ui->queueList->setSelectionMode(
        QAbstractItemView::SingleSelection
        );

    ui->queueList->setDragEnabled(true);

    ui->queueList->viewport()->setAcceptDrops(true);

    ui->queueList->setDropIndicatorShown(true);

    ui->queueList->setDragDropMode(
        QAbstractItemView::InternalMove
        );

    ui->queueList->setDefaultDropAction(Qt::MoveAction);


#ifdef Q_OS_LINUX

    qInfo() << "[MPRIS] Initializing";

    new MprisRootAdaptor(this);

    new MprisPlayerAdaptor(
        this,
        player,
        ui->playButton_2,
        ui->nextButton_2,
        ui->previousButton_2,
        ui->loopOn,
        ui->shuffleOn,
        ui->volumeSlider,
        ui->tempoSlider,
        ui->queueList
        );

    QDBusConnection bus = QDBusConnection::sessionBus();

    bool serviceOK = bus.registerService(
        "org.mpris.MediaPlayer2.sound_execute"
        );


    if (!serviceOK) {
        qWarning() << "[MPRIS] Service error:"
                   << bus.lastError().message();
    }

    bool objectOK = false;

    if (serviceOK) {
        objectOK = bus.registerObject(
            "/org/mpris/MediaPlayer2",
            this,
            QDBusConnection::ExportAdaptors
            );
    }

    if (serviceOK && !objectOK) {
        qWarning() << "[MPRIS] Object error:"
                   << bus.lastError().message();
    }

#endif

}

MainWindow::~MainWindow()
{
    delete ui;
}


void MainWindow::openFile()
{
    QStringList files = QFileDialog::getOpenFileNames(
        this,
        "Open Audio Files",
        "",
        "Audio Files (*.mp3 *.wav *.flac *.ogg)"
        );

    if (files.isEmpty())
        return;

    bool allImported = true;

    for (const QString &file : files) {

        if (!database.addSong(file)) {
            allImported = false;
        }

        appendToQueue(file);
    }

    refreshLibrary();

    if (currentTrack == -1) {
        playTrack(0);
    }

    if (!allImported) {
        QMessageBox::warning(
            this,
            "Import Error",
            "Some files could not be added to the library."
            );
    }

}




void MainWindow::playTrack(int index)
{
    if (index < 0 || index >= tracks.size())
        return;

    auto *item = ui->queueList->item(index);

    if (!item)
        return;

    currentTrack = index;

    currentQueueId =
        item->data(Qt::UserRole + 1).toString();

    QString file = tracks[index];

    ui->queueList->setCurrentRow(index);
    ui->fileLabel->setText(QFileInfo(file).fileName());

    player->setSource(QUrl::fromLocalFile(file));
    player->play();
}



void MainWindow::nextTrack(){
    if (ui->shuffleOn->isChecked()) {
        shuffle();
    }
    else if (currentTrack + 1 < tracks.size()) {
        playTrack(currentTrack + 1);
    }
}

void MainWindow::previousTrack(){
    if(currentTrack>0){
        playTrack(currentTrack-1);
    } else if (currentTrack == 0){
        player->setPosition(0);
    }
}

void MainWindow::shuffle()
{
    if (tracks.isEmpty())
        return;

    int index;

    do {
        index = QRandomGenerator::global()->bounded(tracks.size());
    } while (index == currentTrack && tracks.size() > 1);

    playTrack(index);
}

void MainWindow::importFolder()
{
    QString folder = QFileDialog::getExistingDirectory(
        this, "Select Music Folder"
        );

    if (folder.isEmpty())
        return;

    int added = database.importFolder(folder);

    if (added < 0) {
        QMessageBox::warning(
            this, "Import Error",
            "Could not import the music folder."
            );
        return;
    }

    refreshLibrary();
}

void MainWindow::createPlaylist()
{
    bool ok;

    QString name = QInputDialog::getText(
                       this,
                       "New Playlist",
                       "Playlist name:",
                       QLineEdit::Normal,
                       "",
                       &ok
                       ).trimmed();

    if (!ok || name.isEmpty())
        return;

    if (!database.createPlaylist(name)) {
        QMessageBox::warning(
            this, "Playlist Error",
            "Could not create playlist. The name may already exist."
            );
        return;
    }

    refreshPlaylists();
}


void MainWindow::refreshPlaylistTracks()
{
    ui->playlistTracksList->clear();

    auto *playlist = ui->playlistList->currentItem();

    if (!playlist)
        return;

    int playlistId =
        playlist->data(Qt::UserRole).toInt();

    QVector<Song> songs =
        database.getPlaylistSongs(playlistId);

    for (const Song &song : songs) {
        auto *item = new QListWidgetItem(
            song.title, ui->playlistTracksList
            );

        item->setData(Qt::UserRole, song.id);
        item->setData(Qt::UserRole + 1, song.position);
    }
}



void MainWindow::refreshLibrary()
{
    ui->libraryList->clear();

    for (const Song &song : database.getSongs()) {

        auto *item = new QListWidgetItem(
            song.title,
            ui->libraryList
            );

        item->setData(Qt::UserRole, song.id);
        item->setData(Qt::UserRole + 1, song.path);
    }

    searchLibrary(ui->searchEdit->text());
}


void MainWindow::addToPlaylist()
{
    auto *song = ui->libraryList->currentItem();
    auto *playlist = ui->playlistList->currentItem();

    if (!song || !playlist)
        return;

    int songId = song->data(Qt::UserRole).toInt();
    int playlistId = playlist->data(Qt::UserRole).toInt();

    if (!database.addSongToPlaylist(playlistId, songId)) {
        QMessageBox::warning(
            this, "Playlist Error",
            "Could not add song to playlist."
            );
    }
    refreshPlaylistTracks();
}

void MainWindow::playPlaylist()
{
    auto *playlist = ui->playlistList->currentItem();

    if (!playlist)
        return;

    int playlistId = playlist->data(Qt::UserRole).toInt();

    QVector<Song> songs =
        database.getPlaylistSongs(playlistId);

    QStringList playlistFiles;

    for (const Song &song : songs) {
        if (QFileInfo::exists(song.path)) {
            playlistFiles.append(song.path);
        }
    }

    player->stop();
    player->setSource(QUrl());


    tracks.clear();
    currentTrack = -1;
    currentQueueId.clear();

    ui->queueList->clear();

    for (const QString &file : playlistFiles) {
        appendToQueue(file);
    }


    if (!tracks.isEmpty()) {
        playTrack(0);
    } else {
        ui->fileLabel->setText("No available tracks");
        ui->progressSlider->setRange(0, 0);
        currentQueueId.clear();
    }

}

void MainWindow::refreshPlaylists()
{
    ui->playlistList->clear();

    QVector<Playlist> playlists = database.getPlaylists();

    for (const Playlist &playlist : playlists) {
        auto *item = new QListWidgetItem(
            playlist.name, ui->playlistList
            );

        item->setData(Qt::UserRole, playlist.id);
    }
}

int MainWindow::appendToQueue(const QString &path)
{
    tracks.append(path);

    auto *item = new QListWidgetItem(
        QFileInfo(path).fileName(),
        ui->queueList
        );

    item->setData(Qt::UserRole, path);

    item->setData(
        Qt::UserRole + 1,
        QUuid::createUuid().toString(QUuid::WithoutBraces)
        );

    return tracks.size() - 1;
}


void MainWindow::syncQueueFromWidget()
{
    QStringList reorderedTracks;
    int newCurrentTrack = -1;

    for (int i = 0; i < ui->queueList->count(); i++) {

        auto *item = ui->queueList->item(i);

        QString path =
            item->data(Qt::UserRole).toString();

        reorderedTracks.append(path);

        QString id =
            item->data(Qt::UserRole + 1).toString();

        if (!currentQueueId.isEmpty() &&
            id == currentQueueId) {
            newCurrentTrack = i;
        }
    }

    tracks = reorderedTracks;
    currentTrack = newCurrentTrack;

    if (currentTrack >= 0) {
        ui->queueList->setCurrentRow(currentTrack);
    }
}


bool MainWindow::eventFilter(QObject *watched, QEvent *event)
{
    if (watched == ui->queueList->viewport() &&
        event->type() == QEvent::Drop) {

        QTimer::singleShot(0, this, [this]() {
            syncQueueFromWidget();
        });
    }

    return QMainWindow::eventFilter(watched, event);
}


void MainWindow::searchLibrary(const QString &text)
{
    QString search = text.trimmed();

    for (int i = 0; i < ui->libraryList->count(); i++) {

        QListWidgetItem *item = ui->libraryList->item(i);

        QString title = item->text();

        QString path =
            item->data(Qt::UserRole + 1).toString();

        QString filename = QFileInfo(path).fileName();

        bool matches =
            title.contains(search, Qt::CaseInsensitive) ||
            filename.contains(search, Qt::CaseInsensitive);

        item->setHidden(!matches);
    }

    QListWidgetItem *selected =
        ui->libraryList->currentItem();

    if (selected && selected->isHidden()) {
        ui->libraryList->setCurrentItem(nullptr);
    }
}
