
#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <QStringList>
#include "database.h"

class QMediaPlayer;
class QAudioOutput;

QT_BEGIN_NAMESPACE
namespace Ui {
class MainWindow;
}
QT_END_NAMESPACE
class QEvent;
class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    explicit MainWindow(QWidget *parent = nullptr);
    ~MainWindow();

private:
    Ui::MainWindow *ui;

    QMediaPlayer *player;
    QAudioOutput *audioOutput;
    QStringList tracks;
    DatabaseManager database;
    QString currentQueueId;
    int appendToQueue(const QString &path);
    void syncQueueFromWidget();

    int currentTrack = -1;
    void openFile();
    void playTrack(int index);
    void nextTrack();
    void previousTrack();
    void shuffle();

    void importFolder();
    void refreshLibrary();
    void createPlaylist();
    void refreshPlaylists();
    void addToPlaylist();
    void playPlaylist();
    void refreshPlaylistTracks();
    void searchLibrary(const QString &text);

protected:
    bool eventFilter(QObject *watched, QEvent *event) override;
};


#endif // MAINWINDOW_H
