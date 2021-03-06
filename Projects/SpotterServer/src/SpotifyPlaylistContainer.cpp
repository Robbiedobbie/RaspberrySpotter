/*
 * SpotifyPlaylistContainer.cpp
 *
 *  Created on: Dec 24, 2013
 *      Author: rob
 */

#include "SpotifyPlaylistContainer.hpp"
#include "Tasks/PlaylistTask.h"

namespace fambogie {

static void playlist_added(sp_playlistcontainer *pc, sp_playlist *pl,
		int position, void *userdata) {

}

static void playlist_removed(sp_playlistcontainer *pc, sp_playlist *pl,
		int position, void *userdata) {
}

static void playlist_moved(sp_playlistcontainer *pc, sp_playlist *playlist,
		int position, int new_position, void *userdata) {
}

static void container_loaded(sp_playlistcontainer *pc, void *userdata) {
}

SpotifyPlaylistContainer::SpotifyPlaylistContainer(sp_session* session,
		SpotifySession* spotifySession) {
	this->session = session;
	this->spotifySession = spotifySession;
	playlistContainer = sp_session_playlistcontainer(session);

	sp_playlistcontainer_callbacks playlistCallbacks;
	playlistCallbacks.container_loaded = &container_loaded;
	playlistCallbacks.playlist_added = &playlist_added;
	playlistCallbacks.playlist_removed = &playlist_removed;
	playlistCallbacks.playlist_moved = &playlist_moved;

	sp_playlistcontainer_add_callbacks(playlistContainer, &playlistCallbacks,
	nullptr);
}

ClientResponse* SpotifyPlaylistContainer::processTask(PlaylistTask* task) {
	switch (task->getCommand()) {
	case CommandList:
		return listPlaylists(task);
		break;
	case CommandPlayPlaylist:
		return playPlaylist(task);
		break;
	case CommandPlayTrack:
		return playTrack(task);
		break;
	case CommandListTracks:
		return listTracks(task);
		break;
	}
	return nullptr;
}

ClientResponse* SpotifyPlaylistContainer::playPlaylist(PlaylistTask* task) {
	StatusResponse* response = new StatusResponse(false,
			"Playlist Id is out of range!");

	int playlistNum = task->getCommandInfo().playlist;
	if (playlistNum >= 0
			&& playlistNum
					< sp_playlistcontainer_num_playlists(playlistContainer)) {
		sp_playlist* playlist = sp_playlistcontainer_playlist(playlistContainer,
				playlistNum);
		if (sp_playlist_num_tracks(playlist) > 0) {
			sp_track* track = sp_playlist_track(playlist, 0);
			spotifySession->getSpotifyPlayer()->clearPlayQueue();
			spotifySession->getSpotifyPlayer()->playTrack(track);
			for (int i = 1; i < sp_playlist_num_tracks(playlist); i++) {
				spotifySession->getSpotifyPlayer()->addTrackToQueue(
						sp_playlist_track(playlist, i));
			}
			response->setSuccess(true);
			response->setMessage(nullptr);
		} else {
			response->setMessage("Playlist does not have any tracks");
		}
	}

	return response;
}

ClientResponse* SpotifyPlaylistContainer::playTrack(PlaylistTask* task) {
	StatusResponse* response = new StatusResponse(false,
			"Playlist Id is out of range!");

	PlayTrackCommandInfo info = task->getCommandInfo().playTrackCommandInfo;
	if (info.playlistId > -1
			&& info.playlistId
					< sp_playlistcontainer_num_playlists(playlistContainer)) {
		sp_playlist* playlist = sp_playlistcontainer_playlist(playlistContainer,
				info.playlistId);
		if (info.trackId > -1
				&& info.trackId < sp_playlist_num_tracks(playlist)) {
			spotifySession->getSpotifyPlayer()->clearPlayQueue();
			for (int i = 0; i < info.trackId; i++) {
				spotifySession->getSpotifyPlayer()->addTrackToPlayedQueue(
						sp_playlist_track(playlist, i));
			}

			sp_track* track = sp_playlist_track(playlist, info.trackId);
			spotifySession->getSpotifyPlayer()->playTrack(track);
			for (int i = info.trackId + 1; i < sp_playlist_num_tracks(playlist);
					i++) {
				spotifySession->getSpotifyPlayer()->addTrackToQueue(
						sp_playlist_track(playlist, i));
			}
            if(spotifySession->getSpotifyPlayer()->isShuffled()) {
                spotifySession->getSpotifyPlayer()->setShuffle(true);
            }
			response->setSuccess(true);
			response->setMessage(nullptr);
		} else {
			response->setMessage("Track does not exist!");
		}
	}

	return response;
}

ListResponse<PlaylistInfo*>* SpotifyPlaylistContainer::listPlaylists(
		PlaylistTask* task) {
	ListResponse<PlaylistInfo*>* response = new ListResponse<PlaylistInfo*>();
	response->setListType(ListTypePlaylist);
	CommandInfo info = task->getCommandInfo();
	bool sendName = ((info.ListFlags & Name) == Name);
	bool sendNumTracks = ((info.ListFlags & NumTracks) == NumTracks);
	bool sendDescription = ((info.ListFlags & Description) == Description);

	int numPlaylists = sp_playlistcontainer_num_playlists(playlistContainer);
	for (int i = 0; i < numPlaylists; i++) {
		if (sp_playlistcontainer_playlist_type(playlistContainer, i)
				== SP_PLAYLIST_TYPE_PLAYLIST) {
			sp_playlist* playlist = sp_playlistcontainer_playlist(
					playlistContainer, i);
			const char* name = sp_playlist_name(playlist);

			if (!(name[0] == '_' && name[1] == '_')) {
				PlaylistInfo* playlistInfo = new PlaylistInfo();
				playlistInfo->id = i;

				if (sendName) {
					playlistInfo->name = name;
				}
				if (sendNumTracks) {
					playlistInfo->numTracks = sp_playlist_num_tracks(playlist);
				}
				if (sendDescription) {
					playlistInfo->description = sp_playlist_get_description(
							playlist);
				}
				response->addMember(playlistInfo);
			}
		}
	}
	return response;
}

ListResponse<TrackInfo*>* SpotifyPlaylistContainer::listTracks(
		PlaylistTask* task) {
	ListResponse<TrackInfo*>* response = new ListResponse<TrackInfo*>();
	response->setListType(ListTypeSong);
	ListPlaylistTrackInfo info = task->getCommandInfo().listPlaylistTrackInfo;
	int playlistId = info.playlist;
	if (playlistId > -1
			&& playlistId
					< sp_playlistcontainer_num_playlists(playlistContainer)) {
		bool sendName = ((info.trackInfoFlags & TrackInfoName) == TrackInfoName);
		bool sendArtist = ((info.trackInfoFlags & TrackInfoArtists)
				== TrackInfoArtists);
		bool sendAlbum = ((info.trackInfoFlags & TrackInfoAlbum)
				== TrackInfoAlbum);
		bool sendDuration = ((info.trackInfoFlags & TrackInfoDuration)
				== TrackInfoDuration);
		bool sendArtwork = ((info.trackInfoFlags & TrackInfoArtwork)
				== TrackInfoArtwork);

		sp_playlist* playlist = sp_playlistcontainer_playlist(playlistContainer,
				playlistId);
		int numTracks = sp_playlist_num_tracks(playlist);
		for (int i = 0; i < numTracks; i++) {
			TrackInfo* trackInfo = new TrackInfo();
			trackInfo->id = i;

			sp_track* track = sp_playlist_track(playlist, i);

			if (sendName) {
				trackInfo->name = sp_track_name(track);
			}
			if (sendArtist) {
				trackInfo->numArtists = sp_track_num_artists(track);
				trackInfo->artists = new const char*[trackInfo->numArtists];
				for (int j = 0; j < trackInfo->numArtists; j++) {
					trackInfo->artists[j] = sp_artist_name(
							sp_track_artist(track, j));
				}
			}
			if (sendAlbum) {
				trackInfo->album = sp_album_name(sp_track_album(track));
			}
			if (sendDuration) {
				trackInfo->duration = sp_track_duration(track);
			}
			if (sendArtwork) {
				trackInfo->artwork = sp_image_create(session,
						sp_album_cover(sp_track_album(track),
								SP_IMAGE_SIZE_NORMAL));
			}
			response->addMember(trackInfo);
		}

		return response;
	}

	delete response;
	return nullptr;
}

SpotifyPlaylistContainer::~SpotifyPlaylistContainer() {
	sp_playlistcontainer_remove_callbacks(playlistContainer, &playlistCallbacks,
	nullptr);
}

} /* namespace fambogie */
