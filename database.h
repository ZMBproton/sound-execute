#ifndef DATABASE_H
#define DATABASE_H

#include <QSqlDatabase>
#include <QString>
#include <QVector>

struct Song {
    int id;
    QString path;
    QString title;
    int position = -1;
};

struct Playlist {
    int id;
    QString name;
};

class DatabaseManager
{
public:
    bool initialize();

    int importFolder(const QString &folder);
    QVector<Song> getSongs();

    bool createPlaylist(const QString &name);
    QVector<Playlist> getPlaylists();
    bool addSongToPlaylist(int playlistId, int songId);
    QVector<Song> getPlaylistSongs(int playlistId);
    bool deleteSong(int songId);
    bool deletePlaylist(int playlistId);
    bool removePlaylistTrack(int playlistId, int position);
    bool addSong(const QString &filePath);

private:
    QSqlDatabase db;
};

#endif // DATABASE_H
