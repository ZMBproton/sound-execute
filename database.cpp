
#include "database.h"

#include <QSqlQuery>
#include <QSqlError>
#include <QStandardPaths>
#include <QDir>
#include <QDirIterator>
#include <QFileInfo>
#include <QDebug>

bool DatabaseManager::initialize()
{
    QString folder = QStandardPaths::writableLocation(
        QStandardPaths::AppDataLocation
        );

    if (!QDir().mkpath(folder)) {
        qWarning() << "Cannot create database folder";
        return false;
    }

    db = QSqlDatabase::addDatabase("QSQLITE", "AudioPlayerDB");
    db.setDatabaseName(folder + "/library.db");

    if (!db.open()) {
        qWarning() << db.lastError().text();
        return false;
    }

    QStringList statements = {
        "PRAGMA foreign_keys = ON",

        "CREATE TABLE IF NOT EXISTS songs ("
        "id INTEGER PRIMARY KEY, "
        "path TEXT NOT NULL UNIQUE, "
        "title TEXT NOT NULL)",

        "CREATE TABLE IF NOT EXISTS playlists ("
        "id INTEGER PRIMARY KEY, "
        "name TEXT NOT NULL UNIQUE)",

        "CREATE TABLE IF NOT EXISTS playlist_tracks ("
        "playlist_id INTEGER NOT NULL "
        "REFERENCES playlists(id) ON DELETE CASCADE, "
        "track_id INTEGER NOT NULL "
        "REFERENCES songs(id) ON DELETE CASCADE, "
        "position INTEGER NOT NULL, "
        "PRIMARY KEY (playlist_id, position))"
    };

    QSqlQuery query(db);

    for (const QString &sql : statements) {
        if (!query.exec(sql)) {
            qWarning() << query.lastError().text();
            return false;
        }
    }

    return true;


}


int DatabaseManager::importFolder(const QString &folder)
{
    if (!db.isOpen())
        return -1;

    QDirIterator it(
        folder,
        {"*.mp3", "*.wav", "*.flac", "*.ogg"},
        QDir::Files,
        QDirIterator::Subdirectories
        );

    QSqlQuery query(db);

    if (!query.prepare(
            "INSERT OR IGNORE INTO songs (path, title) "
            "VALUES (?, ?)"
            )) {
        qWarning() << query.lastError().text();
        return -1;
    }

    int added = 0;

    while (it.hasNext()) {
        QFileInfo info(it.next());

        query.bindValue(0, info.absoluteFilePath());
        query.bindValue(1, info.completeBaseName());

        if (!query.exec()) {
            qWarning() << query.lastError().text();
            return -1;
        }

        if (query.numRowsAffected() > 0)
            ++added;
    }

    return added;
}


QVector<Song> DatabaseManager::getSongs()
{
    QVector<Song> songs;

    QSqlQuery query(db);

    if (!query.exec(
            "SELECT id, path, title FROM songs "
            "ORDER BY title COLLATE NOCASE"
            )) {
        qWarning() << query.lastError().text();
        return songs;
    }

    while (query.next()) {
        Song song;

        song.id = query.value(0).toInt();
        song.path = query.value(1).toString();
        song.title = query.value(2).toString();

        songs.append(song);
    }

    return songs;
}

bool DatabaseManager::createPlaylist(const QString &name)
{
    if (name.trimmed().isEmpty())
        return false;

    QSqlQuery query(db);

    query.prepare(
        "INSERT INTO playlists (name) VALUES (?)"
        );

    query.addBindValue(name.trimmed());

    if (!query.exec()) {
        qWarning() << query.lastError().text();
        return false;
    }

    return true;
}


QVector<Playlist> DatabaseManager::getPlaylists()
{
    QVector<Playlist> playlists;

    QSqlQuery query(db);

    if (!query.exec(
            "SELECT id, name FROM playlists "
            "ORDER BY name COLLATE NOCASE"
            )) {
        qWarning() << query.lastError().text();
        return playlists;
    }

    while (query.next()) {
        Playlist playlist;

        playlist.id = query.value(0).toInt();
        playlist.name = query.value(1).toString();

        playlists.append(playlist);
    }

    return playlists;
}

bool DatabaseManager::addSongToPlaylist(
    int playlistId, int songId)
{
    QSqlQuery query(db);

    query.prepare(
        "INSERT INTO playlist_tracks "
        "(playlist_id, track_id, position) "
        "VALUES (?, ?, "
        "(SELECT COALESCE(MAX(position), 0) + 1 "
        "FROM playlist_tracks WHERE playlist_id = ?))"
        );

    query.addBindValue(playlistId);
    query.addBindValue(songId);
    query.addBindValue(playlistId);

    if (!query.exec()) {
        qWarning() << query.lastError().text();
        return false;
    }

    return true;
}


QVector<Song> DatabaseManager::getPlaylistSongs(
    int playlistId)
{
    QVector<Song> songs;

    QSqlQuery query(db);

    query.prepare(
        "SELECT songs.id, songs.path, songs.title, "
        "playlist_tracks.position "
        "FROM playlist_tracks "
        "JOIN songs ON songs.id = playlist_tracks.track_id "
        "WHERE playlist_tracks.playlist_id = ? "
        "ORDER BY playlist_tracks.position"
        );

    query.addBindValue(playlistId);

    if (!query.exec()) {
        qWarning() << query.lastError().text();
        return songs;
    }

    while (query.next()) {
        Song song;

        song.id = query.value(0).toInt();
        song.path = query.value(1).toString();
        song.title = query.value(2).toString();
        song.position = query.value(3).toInt();

        songs.append(song);
    }

    return songs;
}



bool DatabaseManager::deleteSong(int songId)
{
    QSqlQuery query(db);

    query.prepare("DELETE FROM songs WHERE id = ?");
    query.addBindValue(songId);

    if (!query.exec()) {
        qWarning() << query.lastError().text();
        return false;
    }

    return true;
}

bool DatabaseManager::deletePlaylist(int playlistId)
{
    QSqlQuery query(db);

    query.prepare("DELETE FROM playlists WHERE id = ?");
    query.addBindValue(playlistId);

    if (!query.exec()) {
        qWarning() << query.lastError().text();
        return false;
    }

    return true;
}

bool DatabaseManager::removePlaylistTrack(
    int playlistId, int position)
{
    QSqlQuery query(db);

    query.prepare(
        "DELETE FROM playlist_tracks "
        "WHERE playlist_id = ? AND position = ?"
        );

    query.addBindValue(playlistId);
    query.addBindValue(position);

    if (!query.exec()) {
        qWarning() << query.lastError().text();
        return false;
    }

    return true;
}


bool DatabaseManager::addSong(const QString &filePath)
{
    if (!db.isOpen())
        return false;

    QFileInfo info(filePath);

    if (!info.isFile())
        return false;

    QSqlQuery query(db);

    query.prepare(
        "INSERT OR IGNORE INTO songs (path, title) "
        "VALUES (?, ?)"
        );

    query.addBindValue(info.absoluteFilePath());
    query.addBindValue(info.completeBaseName());

    if (!query.exec()) {
        qWarning() << query.lastError().text();
        return false;
    }

    return true;
}
