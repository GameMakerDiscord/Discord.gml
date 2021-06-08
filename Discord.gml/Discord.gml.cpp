/// @author YellowAfterlife

#include "stdafx.h"
#include "discord.h"
#include <map>
#include <queue>
#include <vector>
#include <time.h>

// Debug output macro, { printf(...); printf("\n"); fflush(stdout); }
#define trace(...) { printf("[Discord.gml:%d] ", __LINE__); printf(__VA_ARGS__); printf("\n"); fflush(stdout); }

// Different platforms, different syntax.
#if defined(WIN32)
#define dllx extern "C" __declspec(dllexport)
#define _CRT_SECURE_NO_WARNINGS
#elif defined(GNUC)
#define dllx extern "C" __attribute__ ((visibility("default"))) 
#else
#define dllx extern "C"
#endif

// ID of the async event to perform. (69 = Steam, 70 = Social, etc.)
#define async_event_to_perform 69

using namespace discord;
#define isok(v) ((v) == Result::Ok)
using id_t = std::int64_t;
Core* qCore = nullptr;
/// (auto-exposed to GML this way)
enum class discord_result {
	ok,
	service_unavailable,
	invalid_version,
	lock_failed,
	internal_error,
	invalid_paylaod,
	invalid_command,
	invalid_permissions,
	not_fetched,
	not_found,
	conflict,
	invalid_secret,
	invalid_join_secret,
	no_eligible_activity,
	invalid_invite,
	not_authenticated,
	invalid_access_token,
	application_mismatch,
	invalid_data_url,
	invalid_base64,
	not_filtered,
	lobby_full,
	invalid_lobby_secret,
	invalid_filename,
	invalid_file_size,
	invalid_entitlement,
	not_installed,
	not_running,
	// extension-specific errors follow,
	ext_insufficient_data = 0x10000,
	ext_not_ready = 0x10001,
};
// not a macro to save trouble debugging if return types change
int ri(Result r) {
	return static_cast<int>(r);
}
int ri(discord_result r) {
	return static_cast<int>(r);
}
char* rs(const char* s) {
	static char* tmp = nullptr;
	static size_t size = 0;
	size_t n = strlen(s) + 1;
	if (n > size) {
		size = n;
		tmp = (char*)realloc(tmp, n);
	}
	strcpy(tmp, s);
	return tmp;
}
#define an_ok (static_cast<int>(Result::Ok))
#define is_amiss (qCore == nullptr)
#define proc_amiss if (is_amiss) { return ri(discord_result::ext_not_ready); }

struct async_buffer {
	uint8_t* data;
	uint32_t size;
};
class async_buffer_queue {
	std::queue<async_buffer> queue;
	public:
	async_buffer_queue() {
		//
	}
	void push(uint8_t* data, uint32_t size) {
		uint8_t* copy = new uint8_t[size];
		memcpy(copy, data, size);
		queue.push({ copy, size });
	}
	bool pop(uint8_t* out) {
		if (!queue.empty()) {
			async_buffer q = queue.front();
			memcpy(out, q.data, q.size);
			delete q.data;
			queue.pop();
			return true;
		} else return false;
	}
};

#pragma region GML callbacks
// As per http://help.yoyogames.com/hc/en-us/articles/216755258:
typedef int gml_ds_map;
void(*gml_event_perform_async)(gml_ds_map map, int event_type) = nullptr;
int(*gml_ds_map_create_ext)(int n, ...) = nullptr;
bool(*gml_ds_map_set_double)(gml_ds_map map, char* key, double value) = nullptr;
bool(*gml_ds_map_set_string)(gml_ds_map map, char* key, char* value) = nullptr;
dllx double RegisterCallbacks(char* arg1, char* arg2, char* arg3, char* arg4) {
	gml_event_perform_async = (void(*)(gml_ds_map, int))arg1;
	gml_ds_map_create_ext = (int(*)(int, ...))arg2;
	gml_ds_map_set_double = (bool(*)(gml_ds_map, char*, double))arg3;
	gml_ds_map_set_string = (bool(*)(gml_ds_map, char*, char*))arg4;
	return 0;
}
gml_ds_map gml_ds_map_create() {
	return gml_ds_map_create_ext(0);
}
#pragma endregion

// A wrapper for queuing async events for GML easier.
class async_event {
	private:
	gml_ds_map map;
	public:
	async_event() {
		map = gml_ds_map_create();
	}
	async_event(char* type) {
		map = gml_ds_map_create();
		gml_ds_map_set_string(map, "event_type", type);
	}
	async_event(char* type, Result result) {
		map = gml_ds_map_create();
		gml_ds_map_set_string(map, "event_type", type);
		set_result(result);
	}
	~async_event() {
		//
	}
	/// Dispatches this event and cleans up the map.
	void dispatch() {
		gml_event_perform_async(map, async_event_to_perform);
	}
	bool set(char* key, double value) {
		return gml_ds_map_set_double(map, key, value);
	}
	bool set(char* key, char* value) {
		return gml_ds_map_set_string(map, key, value);
	}
	bool set(char* key, const char* value) {
		// it is known that implementation makes a copy of string instead of copying the address
		return gml_ds_map_set_string(map, key, (char*)value);
	}
	void set_int64(char* key, int64_t value) {
		static char async_event_set_int64[256];
		sprintf(async_event_set_int64, "%s_hi", key);
		set(async_event_set_int64, (int32_t)(value >> 32));
		sprintf(async_event_set_int64, "%s_lo", key);
		set(async_event_set_int64, (uint32_t)(value & 0xFFFFFFFFuL));
	}
	void set_success(bool success) {
		set("success", success);
		set("result", ri(success ? Result::Ok : Result::InternalError));
	}
	void set_result(Result result) {
		set("success", result == Result::Ok);
		set("result", ri(result));
	}
};
dllx double discord_dispatch_event(double map_id) {
	gml_event_perform_async((gml_ds_map)map_id, async_event_to_perform);
	return true;
}

#pragma region Update
dllx double discord_update_raw() {
	if (qCore) {
		qCore->RunCallbacks();
		return true;
	} else return false;
}
#pragma endregion

#pragma region Config
bool discord_config_peer_ids = true;
///
dllx double discord_config_get_use_peer_ids() {
	return discord_config_peer_ids;
}
///
dllx double discord_config_set_use_peer_ids(double enable) {
	discord_config_peer_ids = enable > 0.5;
	return discord_config_peer_ids;
}

bool discord_config_sync_net = false;
///
dllx double discord_config_get_sync_net() {
	return discord_config_sync_net;
}
///
dllx double discord_config_set_sync_net(double enable) {
	discord_config_sync_net = enable > 0.5;
	return discord_config_sync_net;
}
#pragma endregion

#pragma region Users
UserManager* qUsers;
User qSelf{};
UserId qSelfId;
///
dllx char* discord_get_username() {
	if (is_amiss) return "";
	return rs(qSelf.GetUsername());
}
///
dllx char* discord_get_discriminator() {
	if (is_amiss) return "0000";
	return rs(qSelf.GetDiscriminator());
}
dllx double discord_get_user_id_raw(uint64_t* out) {
	if (is_amiss) {
		out[0] = 0;
		return false;
	} else {
		out[0] = qSelf.GetId();
		return true;
	}
}
///
dllx char* discord_get_avatar_url() {
	if (is_amiss) return "";
	return rs(qSelf.GetAvatar());
}
bool discord_ready = false;
dllx double discord_is_ready() {
	return discord_ready;
}
bool discord_readying = false;
dllx double discord_is_readying() {
	return discord_readying;
}

void discord_users_init() {
	qUsers->OnCurrentUserUpdate.Connect([]() {
		qUsers->GetCurrentUser(&qSelf);
		qSelfId = qSelf.GetId();
		async_event e("discord_user_update");
		e.set_int64("user_id", qSelfId);
		e.set("username", qSelf.GetUsername());
		e.set("discriminator", qSelf.GetDiscriminator());
		e.set("avatar_url", qSelf.GetAvatar());
		e.dispatch();
		if (!discord_ready) {
			discord_ready = true;
			discord_readying = false;
			async_event e1("discord_ready", Result::Ok);
			e1.dispatch();
		}
	});
}
#pragma endregion

#pragma region Activities
ActivityManager* qActivities;

void discord_activity_fill_event(Activity const& a, async_event& e) {
	e.set("activity_type", static_cast<int32_t>(a.GetType()));
	e.set("activity_name", a.GetName());
	e.set("activity_details", a.GetDetails());
	e.set("activity_party_id", a.GetParty().GetId());
}

#pragma region Update
Activity* discord_activity_next = nullptr;
#define discord_activity_set(impl) {\
	proc_amiss;\
	if (discord_activity_next == nullptr) return ri(discord_result::ext_insufficient_data);\
	impl;\
	return ri(discord_result::ok);\
}
/// ->result
dllx double discord_activity_register_command(char* command) {
	proc_amiss;
	return ri(qActivities->RegisterCommand(command));
}
/// ->result
dllx double discord_activity_register_steam(double steam_app_id) {
	proc_amiss;
	return ri(qActivities->RegisterSteam((int32_t)steam_app_id));
}
/// ->result
dllx double discord_activity_start() {
	proc_amiss;
	if (discord_activity_next != nullptr) delete discord_activity_next;
	discord_activity_next = new Activity();
	return an_ok;
}
/// ->result
dllx double discord_activity_set_state(char* state) {
	discord_activity_set(discord_activity_next->SetState(state));
}
/// ->result
dllx double discord_activity_set_details(char* details) {
	discord_activity_set(discord_activity_next->SetDetails(details));
}
///
enum class discord_activity_type {
	playing,
	streaming,
	listening,
	watching,
};
/*// Can't do?
dllx double discord_activity_set_type(double type) {
	if (discord_activity_next == nullptr) return false;
	discord_activity_next->SetType(static_cast<ActivityType>(static_cast<int32_t>(type)));
	return true;
}*/
/// ->result
dllx double discord_activity_set_large_image(char* img) {
	discord_activity_set(discord_activity_next->GetAssets().SetLargeImage(img));
}
/// ->result
dllx double discord_activity_set_large_text(char* text) {
	discord_activity_set(discord_activity_next->GetAssets().SetLargeText(text));
}
/// ->result
dllx double discord_activity_set_small_image(char* img) {
	discord_activity_set(discord_activity_next->GetAssets().SetSmallImage(img));
}
/// ->result
dllx double discord_activity_set_small_text(char* text) {
	discord_activity_set(discord_activity_next->GetAssets().SetSmallText(text));
}
/// ->result
dllx double discord_activity_set_start_time(double time) {
	discord_activity_set(discord_activity_next->GetTimestamps().SetStart((int64_t)time));
}
/// ->result
dllx double discord_activity_set_end_time(double time) {
	discord_activity_set(discord_activity_next->GetTimestamps().SetEnd((int64_t)time));
}
/// ->result
dllx double discord_activity_set_party_id(char* id) {
	discord_activity_set(discord_activity_next->GetParty().SetId(id));
}
/// ->result
dllx double discord_activity_set_party_size(double size) {
	discord_activity_set(discord_activity_next->GetParty().GetSize().SetCurrentSize((int32_t)size));
}
/// ->result
dllx double discord_activity_set_party_max_size(double size) {
	discord_activity_set(discord_activity_next->GetParty().GetSize().SetMaxSize((int32_t)size));
}
/// ->result
dllx double discord_activity_set_join_secret(char* secret) {
	discord_activity_set({
		discord_activity_next->GetSecrets().SetJoin(secret);
	});
}
/// ->result
dllx double discord_activity_set_spectate_secret(char* secret) {
	discord_activity_set({
		discord_activity_next->GetSecrets().SetSpectate(secret);
	});
}
/// ->result
dllx double discord_activity_set_match_secret(char* secret) {
	discord_activity_set({
		discord_activity_next->GetSecrets().SetMatch(secret);
	});
}
/// ->result
dllx double discord_activity_update() {
	proc_amiss;
	if (discord_activity_next == nullptr) return ri(discord_result::ext_insufficient_data);
	qActivities->UpdateActivity(*discord_activity_next, [](Result result) {
		async_event e("discord_activity_update", result);
		e.dispatch();
	});
	delete discord_activity_next;
	discord_activity_next = nullptr;
	return an_ok;
}
// end of update
#pragma endregion

///
enum class discord_activity_join_request_reply {
	no,
	yes,
	ignore,
};

///
enum class discord_activity_action_type {
	join = 1,
	spectate,
};

dllx double discord_activity_send_request_reply_raw(id_t* raw, double reply) {
	proc_amiss;
	auto id = raw[0];
	auto re = static_cast<ActivityJoinRequestReply>(static_cast<int32_t>(reply));
	qActivities->SendRequestReply(id, re, [id, re](Result r) {
		async_event e("discord_activity_send_request_reply", r);
		e.set_int64("user_id", id);
		e.set("join_request_reply", static_cast<int32_t>(re));
		e.dispatch();
	});
	return an_ok;
}
dllx double discord_activity_accept_invite_raw(id_t* raw) {
	proc_amiss;
	auto id = raw[0];
	qActivities->AcceptInvite(id, [id](Result r) {
		async_event e("discord_activity_accept_invite", r);
		e.set_int64("user_id", id);
		e.dispatch();
	});
	return an_ok;
}
void discord_activity_init() {
	qActivities->OnActivityJoinRequest.Connect([](User const& user) {
		async_event e("discord_activity_join_request");
		e.set_int64("user_id", user.GetId());
		e.dispatch();
	});
	qActivities->OnActivityInvite.Connect([](ActivityActionType t, User const& user, Activity const& a) {
		async_event e("discord_activity_invite");
		e.set_int64("user_id", user.GetId());
		e.set("activity_action_type", static_cast<int32_t>(t));
		discord_activity_fill_event(a, e);
		e.dispatch();
	});
	qActivities->OnActivityJoin.Connect([](const char* secret) {
		async_event e("discord_activity_join");
		e.set("activity_secret", secret);
		e.dispatch();
	});
	qActivities->OnActivitySpectate.Connect([](const char* secret) {
		async_event e("discord_activity_spectate");
		e.set("spectate_secret", secret);
		e.dispatch();
	});
}
// end of Activities
#pragma endregion

NetworkManager* qNetwork;
char discord_network_route[4096];

#pragma region Lobbies
LobbyManager* qLobbies;
std::vector<LobbyId> discord_lobby_list;
char discord_lobby_metadata_tmp[4096];

#pragma region Access
void discord_lobby_fill_event(const Lobby& lobby, async_event& e) {
	e.set_int64("lobby_id", lobby.GetId());
	e.set_int64("owner_id", lobby.GetOwnerId());
	e.set("lobby_capacity", lobby.GetCapacity());
	e.set("lobby_secret", lobby.GetSecret());
	e.set("lobby_type", static_cast<int>(lobby.GetType()));
}
void discord_lobby_post(const Lobby& lobby) {
	auto lobby_id = lobby.GetId();
	discord_lobby_list.push_back(lobby_id);
	//
	if (discord_config_peer_ids) {
		LobbyMemberTransaction txn;
		if (qLobbies->GetMemberUpdateTransaction(lobby_id, qSelfId, &txn) == Result::Ok) {
			NetworkPeerId peer_id;
			qNetwork->GetPeerId(&peer_id);
			char peer_id_s[21];
			sprintf(peer_id_s, "%lld", peer_id);
			//trace("peer=%s", peer_id_s);
			txn.SetMetadata("network_peer_id", peer_id_s);
			txn.SetMetadata("network_route", discord_network_route);
			qLobbies->UpdateMember(lobby_id, qSelfId, txn, [lobby_id](Result r) {
				if (r != Result::Ok) trace("Failed to update peer ID for lobby %lld, error code %d", lobby_id, static_cast<int>(r));
			});
		}
	}
}

#pragma region Lobby specific
dllx double discord_lobby_get_activity_secret_raw(void* data) {
	uint64_t id = ((uint64_t*)data)[0];
	return ri(qLobbies->GetLobbyActivitySecret(id, (char*)data));
}

dllx double discord_lobby_get_metadata_count_raw(id_t* raw) {
	proc_amiss;
	return ri(qLobbies->LobbyMetadataCount(raw[0], (int32_t*)raw));
}
dllx double discord_lobby_get_metadata_key_raw(id_t* raw, double index) {
	proc_amiss;
	return ri(qLobbies->GetLobbyMetadataKey(raw[0], (int32_t)index, (char*)raw));
}
dllx double discord_lobby_get_metadata_value_raw(id_t* raw, char* key) {
	proc_amiss;
	return ri(qLobbies->GetLobbyMetadataValue(raw[0], key, (char*)raw));
}

dllx double discord_lobby_voice_connect_raw(id_t* raw) {
	proc_amiss;
	auto id = raw[0];
	qLobbies->ConnectVoice(id, [id](Result r) {
		async_event e("discord_lobby_voice_connect", r);
		e.set_int64("lobby_id", id);
		e.dispatch();
	});
	return an_ok;
}
dllx double discord_lobby_voice_disconnect_raw(id_t* raw) {
	proc_amiss;
	auto id = raw[0];
	qLobbies->DisconnectVoice(id, [id](Result r) {
		async_event e("discord_lobby_voice_disconnect", r);
		e.set_int64("lobby_id", id);
		e.dispatch();
	});
	return an_ok;
}
// end of Lobby specific
#pragma endregion

#pragma region Member specific
dllx double discord_lobby_get_owner_user_id_raw(id_t* raw) {
	proc_amiss;
	Lobby lobby;
	auto r = qLobbies->GetLobby(raw[0], &lobby);
	if (r != Result::Ok) return ri(r);
	raw[0] = lobby.GetOwnerId();
	return an_ok;
}

dllx double discord_lobby_get_member_count_raw(id_t* raw) {
	proc_amiss;
	auto id = raw[0];
	return ri(qLobbies->MemberCount(id, (int32_t*)raw));
}
dllx double discord_lobby_get_member_user_id_raw(int64_t* raw, double index) {
	proc_amiss;
	auto id = raw[0];
	return ri(qLobbies->GetMemberUserId(id, (int32_t)index, raw));
}

dllx double discord_lobby_get_member_metadata_count_raw(id_t* raw) {
	proc_amiss;
	return ri(qLobbies->MemberMetadataCount(raw[0], raw[1], (int32_t*)raw));
}
dllx double discord_lobby_get_member_metadata_key_raw(int64_t* raw, double index) {
	proc_amiss;
	auto lobby_id = raw[0];
	auto user_id = raw[1];
	return ri(qLobbies->GetMemberMetadataKey(lobby_id, user_id, (int32_t)index, (char*)raw));
}
dllx double discord_lobby_get_member_metadata_value_raw(int64_t* raw, char* name) {
	proc_amiss;
	auto lobby_id = raw[0];
	auto user_id = raw[1];
	return ri(qLobbies->GetMemberMetadataValue(lobby_id, user_id, name, (char*)raw));
}

dllx double discord_lobby_get_member_username_raw(id_t* raw) {
	proc_amiss;
	User user;
	auto r = ri(qLobbies->GetMemberUser(raw[0], raw[1], &user));
	if (r == an_ok) strcpy((char*)raw, user.GetUsername());
	return r;
}
dllx double discord_lobby_get_member_discriminator_raw(id_t* raw) {
	proc_amiss;
	User user;
	auto r = ri(qLobbies->GetMemberUser(raw[0], raw[1], &user));
	if (r == an_ok) strcpy((char*)raw, user.GetDiscriminator());
	return r;
}
// end of Member specific
#pragma endregion

// end of access
#pragma endregion

#pragma region Creation
///
enum discord_lobby_type {
	discord_lobby_type_private = 1,
	discord_lobby_type_public
};
LobbyTransaction* discord_lobby_create_txn = nullptr;
/// ->result
dllx double discord_lobby_create_start(double type) {
	proc_amiss;
	if (discord_lobby_create_txn != nullptr) {
		delete discord_lobby_create_txn;
		discord_lobby_create_txn = nullptr;
	}
	discord_lobby_create_txn = new LobbyTransaction();
	auto r = qLobbies->GetLobbyCreateTransaction(discord_lobby_create_txn);
	if (r == Result::Ok) {
		discord_lobby_create_txn->SetType(static_cast<LobbyType>(static_cast<int32_t>(type)));
	} else {
		delete discord_lobby_create_txn;
		discord_lobby_create_txn = nullptr;
	}
	return ri(r);
}
/// ->result
dllx double discord_lobby_create_set_metadata(char* key, char* value) {
	proc_amiss;
	if (discord_lobby_create_txn == nullptr) return ri(discord_result::ext_insufficient_data);
	return ri(discord_lobby_create_txn->SetMetadata(key, value));
}
/// ->result
dllx double discord_lobby_create_set_capacity(double cap) {
	proc_amiss;
	if (discord_lobby_create_txn == nullptr) return ri(discord_result::ext_insufficient_data);
	if (cap < 0) cap = 0;
	return ri(discord_lobby_create_txn->SetCapacity((uint32_t)cap));
}
/// ->result
dllx double discord_lobby_create_submit() {
	proc_amiss;
	if (discord_lobby_create_txn == nullptr) return ri(discord_result::ext_insufficient_data);
	qLobbies->CreateLobby(*discord_lobby_create_txn, [](Result r, Lobby const& lobby) {
		if (r == Result::Ok) discord_lobby_post(lobby);
		//
		async_event e("discord_lobby_create", r);
		if (r == Result::Ok) discord_lobby_fill_event(lobby, e);
		e.dispatch();
	});
	delete discord_lobby_create_txn;
	discord_lobby_create_txn = nullptr;
	return an_ok;
}
#pragma endregion

#pragma region Update
// this is very akin to _create_ but without types/methods it's trouble
LobbyTransaction* discord_lobby_update_txn = nullptr;
LobbyId discord_lobby_update_id;
dllx double discord_lobby_update_start_raw(id_t* raw) {
	proc_amiss;
	if (discord_lobby_update_txn != nullptr) {
		delete discord_lobby_update_txn;
		discord_lobby_update_txn = nullptr;
	}
	discord_lobby_update_txn = new LobbyTransaction();
	auto r = qLobbies->GetLobbyUpdateTransaction(raw[0], discord_lobby_update_txn);
	if (r == Result::Ok) {
		discord_lobby_update_id = raw[0];
	} else {
		delete discord_lobby_update_txn;
		discord_lobby_update_txn = nullptr;
	}
	return ri(r);
}
/// ->result
dllx double discord_lobby_update_set_metadata(char* key, char* value) {
	proc_amiss;
	if (discord_lobby_update_txn == nullptr) return ri(discord_result::ext_insufficient_data);
	return ri(discord_lobby_update_txn->SetMetadata(key, value));
}
/// ->result
dllx double discord_lobby_update_set_type(double type) {
	proc_amiss;
	if (discord_lobby_update_txn == nullptr) return ri(discord_result::ext_insufficient_data);
	return ri(discord_lobby_update_txn->SetType(static_cast<LobbyType>(static_cast<int32_t>(type))));
}
/// ->result
dllx double discord_lobby_update_delete_metadata(char* key) {
	proc_amiss;
	if (discord_lobby_update_txn == nullptr) return ri(discord_result::ext_insufficient_data);
	return ri(discord_lobby_update_txn->DeleteMetadata(key));
}
/// ->result
dllx double discord_lobby_update_set_capacity(double cap) {
	proc_amiss;
	if (discord_lobby_update_txn == nullptr) return ri(discord_result::ext_insufficient_data);
	if (cap < 0) cap = 0;
	return ri(discord_lobby_update_txn->SetCapacity((uint32_t)cap));
}
dllx double discord_lobby_update_set_owner_raw(id_t* raw) {
	proc_amiss;
	if (discord_lobby_update_txn == nullptr) return ri(discord_result::ext_insufficient_data);
	return ri(discord_lobby_update_txn->SetOwner(raw[0]));
}
/// ~> async event
dllx double discord_lobby_update_submit() {
	proc_amiss;
	if (discord_lobby_update_txn == nullptr) return ri(discord_result::ext_insufficient_data);
	auto id = discord_lobby_update_id;
	qLobbies->UpdateLobby(id, *discord_lobby_update_txn, [id](Result r) {
		async_event e("discord_lobby_update_submit", r);
		e.set_int64("lobby_id", id);
		e.dispatch();
	});
	delete discord_lobby_update_txn;
	discord_lobby_update_txn = nullptr;
	return an_ok;
}
// end of Update
#pragma endregion

#pragma region Member Update
LobbyId discord_lobby_member_update_lobby_id;
UserId discord_lobby_member_update_user_id;
LobbyMemberTransaction* discord_lobby_member_update_txn = nullptr;
dllx double discord_lobby_member_update_start_raw(id_t* raw) {
	proc_amiss;
	if (discord_lobby_member_update_txn != nullptr) {
		delete discord_lobby_member_update_txn;
		discord_lobby_member_update_txn = nullptr;
	}
	auto txn = new LobbyMemberTransaction();
	discord_lobby_member_update_lobby_id = raw[0];
	discord_lobby_member_update_user_id = raw[1];
	auto r = qLobbies->GetMemberUpdateTransaction(raw[0], raw[1], txn);
	if (r == Result::Ok) {
		discord_lobby_member_update_txn = txn;
	} else {
		delete txn;
	}
	return ri(r);
}
/// ->result
dllx double discord_lobby_member_update_set_metadata(char* key, char* value) {
	proc_amiss;
	if (discord_lobby_member_update_txn == nullptr) return ri(discord_result::ext_insufficient_data);
	return ri(discord_lobby_member_update_txn->SetMetadata(key, value));
}
/// ->result
dllx double discord_lobby_member_update_delete_metadata(char* key) {
	proc_amiss;
	if (discord_lobby_member_update_txn == nullptr) return ri(discord_result::ext_insufficient_data);
	return ri(discord_lobby_member_update_txn->DeleteMetadata(key));
}
/// ~> async event
dllx double discord_lobby_member_update_submit() {
	proc_amiss;
	if (discord_lobby_member_update_txn == nullptr) return ri(discord_result::ext_insufficient_data);
	auto lq = discord_lobby_member_update_lobby_id;
	auto uq = discord_lobby_member_update_user_id;
	qLobbies->UpdateMember(lq, uq, *discord_lobby_member_update_txn, [lq, uq](Result r) {
		async_event e("discord_lobby_member_update_submit");
		e.set_int64("lobby_id", lq);
		e.set_int64("user_id", uq);
		e.dispatch();
	});
	return an_ok;
}
// end of Member Update
#pragma endregion

#pragma region Search
LobbySearchQuery* discord_lobby_search_next = nullptr;
///
dllx double discord_lobby_search_start() {
	proc_amiss;
	if (discord_lobby_search_next != nullptr) {
		delete discord_lobby_search_next;
		discord_lobby_search_next = nullptr;
	}
	discord_lobby_search_next = new LobbySearchQuery();
	auto r = qLobbies->GetSearchQuery(discord_lobby_search_next);
	if (r != Result::Ok) {
		delete discord_lobby_search_next;
		discord_lobby_search_next = nullptr;
	}
	return ri(r);
}
///
dllx double discord_lobby_search_set_limit(double num) {
	proc_amiss;
	if (discord_lobby_search_next == nullptr) return ri(discord_result::ext_insufficient_data);
	return ri(discord_lobby_search_next->Limit(num > 0 ? (uint32_t)num : 0));
}
///
enum class discord_lobby_search_cmp {
	lte = -2,
	lt,
	eq,
	gt,
	gte,
	ne,
};
dllx double discord_lobby_search_set_filter_raw(char* key, char* val, double cmp, double _cast) {
	proc_amiss;
	if (discord_lobby_search_next == nullptr) return ri(discord_result::ext_insufficient_data);
	auto dcmp = static_cast<LobbySearchComparison>(static_cast<int32_t>(cmp));
	auto dcast = static_cast<LobbySearchCast>(static_cast<int32_t>(_cast));
	//trace("%s %s %d %d", key, val, (int32_t)cmp, (int32_t)_cast);
	return ri(discord_lobby_search_next->Filter(key, dcmp, dcast, val));
}
///
dllx double discord_lobby_search_submit() {
	proc_amiss;
	if (discord_lobby_search_next == nullptr) return ri(discord_result::ext_insufficient_data);
	qLobbies->Search(*discord_lobby_search_next, [](Result er) {
		async_event e("discord_lobby_search", er);
		int32_t n = 0;
		if (er == Result::Ok) qLobbies->LobbyCount(&n);
		e.set("lobby_count", n);
		e.dispatch();
	});
	delete discord_lobby_search_next;
	discord_lobby_search_next = nullptr;
	return ri(Result::Ok);
}
///
dllx double discord_lobby_search_get_count() {
	if (is_amiss) return 0;
	int32_t n = 0;
	qLobbies->LobbyCount(&n);
	return n;
}

dllx double discord_lobby_search_get_lobby_id_raw(id_t* raw, double index) {
	proc_amiss;
	return ri(qLobbies->GetLobbyId((int32_t)index, raw));
}
#pragma endregion

#pragma region Connection
dllx double discord_lobby_connect_raw(id_t* lobby_id, char* lobby_secret) {
	proc_amiss;
	qLobbies->ConnectLobby(lobby_id[0], lobby_secret, [lobby_id, lobby_secret](Result r, Lobby lobby) {
		async_event e("discord_lobby_connect", r);
		if (r == Result::Ok) {
			discord_lobby_fill_event(lobby, e);
			discord_lobby_post(lobby);
		} else {
			e.set_int64("lobby_id", lobby_id[0]);
			e.set("lobby_secret", lobby_secret);
		}
		e.dispatch();
	});
	return an_ok;
}

///
dllx double discord_lobby_connect_with_activity_secret(char* activity_secret) {
	proc_amiss;
	qLobbies->ConnectLobbyWithActivitySecret(activity_secret, [](Result r, Lobby lobby) {
		async_event e("discord_lobby_connect_with_activity_secret", r);
		//e.set("activity_secret", activity_secret); // not preserved
		if (r == Result::Ok) {
			discord_lobby_fill_event(lobby, e);
			discord_lobby_post(lobby);
		}
		e.dispatch();
	});
	return an_ok;
}

dllx double discord_lobby_disconnect_raw(id_t* lobby_id) {
	proc_amiss;
	auto id = lobby_id[0];
	qLobbies->DisconnectLobby(id, [id](Result r) {
		async_event e("discord_lobby_disconnect", r);
		e.set_int64("lobby_id", id);
		e.dispatch();
	});
	return an_ok;
}
// end of Connection
#pragma endregion

#pragma region Messages
async_buffer_queue discord_lobby_message_queue;
dllx double discord_lobby_message_next_raw(uint8_t* out) {
	return discord_lobby_message_queue.pop(out);
}
dllx double discord_lobby_send_message_raw(id_t* raw, uint8_t* data, double size) {
	proc_amiss;
	auto id = raw[0];
	qLobbies->SendLobbyMessage(id, data, size, [id](Result r) {
		async_event e("discord_lobby_send_message", r);
		e.set_int64("lobby_id", id);
		e.dispatch();
	});
	return an_ok;
}
#pragma endregion

void discord_lobby_init() {
	qLobbies->OnLobbyUpdate.Connect([](int64_t id) {
		async_event e("discord_lobby_update");
		e.set_int64("lobby_id", id);
		e.dispatch();
	});
	qLobbies->OnLobbyDelete.Connect([](int64_t id, uint32_t reason) {
		std::remove(discord_lobby_list.begin(), discord_lobby_list.end(), id);
		async_event e("discord_lobby_delete");
		e.set_int64("lobby_id", id);
		e.set("reason", reason);
		e.dispatch();
	});
	qLobbies->OnMemberConnect.Connect([](int64_t lobby_id, int64_t user_id) {
		async_event e("discord_lobby_member_connect");
		e.set_int64("lobby_id", lobby_id);
		e.set_int64("user_id", user_id);
		e.dispatch();
	});
	qLobbies->OnMemberUpdate.Connect([](int64_t lobby_id, int64_t user_id) {
		async_event e("discord_lobby_member_update");
		e.set_int64("lobby_id", lobby_id);
		e.set_int64("user_id", user_id);
		e.dispatch();
		//
		if (discord_config_peer_ids) do {
			if (qLobbies->GetMemberMetadataValue(lobby_id, user_id, "network_peer_id", discord_lobby_metadata_tmp) != Result::Ok) break;
			NetworkPeerId peer_id = atoll(discord_lobby_metadata_tmp);
			if (user_id == qSelfId) break;
			if (qLobbies->GetMemberMetadataValue(lobby_id, user_id, "network_route", discord_lobby_metadata_tmp) != Result::Ok) break;
			Result r = qNetwork->UpdatePeer(peer_id, discord_lobby_metadata_tmp);
			if (r != Result::Ok) {
				trace("Failed to update route for user=%lld peer=%lld route=%s - error %d", user_id, peer_id, discord_lobby_metadata_tmp, static_cast<int>(r));
			}
		} while (false);
	});
	qLobbies->OnMemberDisconnect.Connect([](int64_t lobby_id, int64_t user_id) {
		async_event e("discord_lobby_member_disconnect");
		e.set_int64("lobby_id", lobby_id);
		e.set_int64("user_id", user_id);
		e.dispatch();
	});
	qLobbies->OnLobbyMessage.Connect([](int64_t lobby_id, int64_t user_id, uint8_t* data, uint32_t size) {
		async_event e("discord_lobby_message");
		e.set_int64("lobby_id", lobby_id);
		e.set_int64("user_id", user_id);
		discord_lobby_message_queue.push(data, size);
		e.set("size", size);
		e.dispatch();
	});
	qLobbies->OnSpeaking.Connect([](id_t lobby_id, id_t user_id, bool speaking) {
		async_event e("discord_lobby_speaking");
		e.set_int64("lobby_id", lobby_id);
		e.set_int64("user_id", user_id);
		e.set("speaking", speaking);
		e.dispatch();
	});
}
// end of Lobbies
#pragma endregion

#pragma region Network

#pragma region Messages
async_buffer_queue discord_network_message_queue;
dllx double discord_network_message_next_raw(uint8_t* out) {
	return discord_network_message_queue.pop(out);
}

struct discord_network_sync_t {
	NetworkPeerId peer_id;
	NetworkChannelId channel_id;
	uint8_t* data;
	uint32_t size;
};
discord_network_sync_t discord_network_sync_next = { 0, 0, new uint8_t[0], 0 };
std::queue<discord_network_sync_t> discord_network_sync_queue;
///
dllx double discord_network_sync_receive() {
	if (discord_network_sync_queue.empty()) return false;
	delete discord_network_sync_next.data;
	discord_network_sync_next = discord_network_sync_queue.front();
	discord_network_sync_queue.pop();
	return true;
}
dllx double discord_network_sync_get_peer_id_raw(id_t* out) {
	out[0] = discord_network_sync_next.peer_id;
	return true;
}
///
dllx char* discord_network_get_route() {
	return discord_network_route;
}
///
dllx double discord_network_sync_get_channel_id() {
	return discord_network_sync_next.channel_id;
}
///
dllx double discord_network_sync_get_size() {
	return discord_network_sync_next.size;
}
dllx double discord_network_sync_get_data_raw(uint8_t* out) {
	memcpy(out, discord_network_sync_next.data, discord_network_sync_next.size);
	return true;
}
// end of Messages
#pragma endregion

dllx double discord_network_get_peer_id_raw(NetworkPeerId* raw) {
	proc_amiss;
	qNetwork->GetPeerId(raw);
	return an_ok;
}
dllx char* discord_network_get_peer_id_str() {
	if (is_amiss) return "0";
	char peer_id_s[50];
	NetworkPeerId id;
	qNetwork->GetPeerId(&id);
	sprintf(peer_id_s, "%llu %lld", id, id);
	return rs(peer_id_s);
}
dllx double discord_network_open_peer_raw(NetworkPeerId* raw, char* route) {
	proc_amiss;
	return ri(qNetwork->OpenPeer(raw[0], route));
}
dllx double discord_network_open_channel_raw(NetworkPeerId* raw, double channel, double rel) {
	proc_amiss;
	return ri(qNetwork->OpenChannel(raw[0], (NetworkChannelId)channel, rel > 0.5));
}
dllx double discord_network_send_message_raw(NetworkPeerId* peer_id, double channel_id, uint8_t* data, double size) {
	proc_amiss;
	//trace("send=%lld %llu", peer_id, peer_id);
	return ri(qNetwork->SendMessage(peer_id[0], (NetworkChannelId)channel_id, data, size > 0 ? (uint32_t)size : 0));
}
///
dllx double discord_network_flush() {
	proc_amiss;
	return ri(qNetwork->Flush());
}
dllx double discord_lobby_network_flush() {
	proc_amiss;
	return ri(qLobbies->FlushNetwork());
}

void discord_network_init() {
	qNetwork->OnMessage.Connect([](NetworkPeerId peer_id, NetworkChannelId channel, uint8_t* data, uint32_t size) {
		if (discord_config_sync_net) {
			uint8_t* copy = new uint8_t[size];
			memcpy(copy, data, size);
			discord_network_sync_queue.push({ peer_id, channel, copy, size });
		} else {
			async_event e("discord_network_message");
			e.set_int64("peer_id", peer_id);
			e.set("channel", channel);
			discord_network_message_queue.push(data, size);
			e.set("size", size);
			e.dispatch();
		}
	});
	qNetwork->OnRouteUpdate.Connect([](const char* route) {
		strcpy(discord_network_route, route);
		if (discord_config_peer_ids) {
			auto uid = qSelfId;
			for (const auto &id : discord_lobby_list) {
				LobbyMemberTransaction txn;
				if (qLobbies->GetMemberUpdateTransaction(id, uid, &txn) != Result::Ok) continue;
				txn.SetMetadata("network_route", route);
				qLobbies->UpdateMember(id, uid, txn, [id](Result r) {
					if (r != Result::Ok) trace("Failed to update route for lobby %lld, error code %d", id, static_cast<int>(r));
				});
			}
		}
		//
		async_event e("discord_network_route_update");
		e.set("route", route);
		e.dispatch();
	});
}
// end of Networking
#pragma endregion

#pragma region Overlay
#define qOverlay qCore->OverlayManager()
///
dllx double discord_overlay_is_enabled() {
	if (is_amiss) return false;
	bool z;
	qOverlay.IsEnabled(&z);
	return z;
}
///
dllx double discord_overlay_get_locked() {
	if (is_amiss) return false;
	bool z;
	qOverlay.IsLocked(&z);
	return z;
}
///
dllx double discord_overlay_set_locked(double locked) {
	proc_amiss;
	bool z = locked > 0.5;
	qOverlay.SetLocked(z, [z](Result r) {
		async_event e("discord_overlay_set_visible", r);
		e.set("locked", z);
		e.dispatch();
	});
	return an_ok;
}
///
dllx double discord_overlay_open_activity_invite(double activity_action_type) {
	proc_amiss;
	auto t = static_cast<ActivityActionType>(static_cast<int32_t>(activity_action_type));
	qOverlay.OpenActivityInvite(t, [t](Result r) {
		async_event e("discord_overlay_open_activity_invite", r);
		e.set("activity_action_type", static_cast<int32_t>(t));
		e.dispatch();
	});
	return an_ok;
}
void discord_overlay_init() {
	qOverlay.OnToggle.Connect([](bool locked) {
		async_event e("discord_overlay_toggle");
		e.set("locked", locked);
		e.dispatch();
	});
}
// end of Overlay
#pragma endregion

#pragma region Init
dllx double discord_get_time_raw(int64_t* out) {
	out[0] = time(NULL);
	return an_ok;
}
/// (used for revision verification)~
#define discord_gml_revision 1
dllx double discord_ext_revision() {
	return discord_gml_revision;
}
dllx double discord_create_raw(int64_t* raw) {
	auto id = raw[0];
	trace("Binding to ID %lld...", id);
	discord_ready = false;
	auto r = Core::Create(id, raw[1], &qCore);
	if (r != Result::Ok) return ri(r);
	//
	qUsers = &qCore->UserManager();
	discord_users_init();
	qActivities = &qCore->ActivityManager();
	discord_activity_init();
	qLobbies = &qCore->LobbyManager();
	discord_lobby_init();
	qNetwork = &qCore->NetworkManager();
	discord_network_init();
	discord_overlay_init();
	//
	qCore->SetLogHook(LogLevel::Debug, [](discord::LogLevel level, const char* message) {
		switch (level) {
			case LogLevel::Debug: trace("[Debug] %s", message); break;
			case LogLevel::Warn: trace("[Warn] %s", message); break;
			case LogLevel::Error: trace("[Error] %s", message); break;
			case LogLevel::Info: trace("[Info] %s", message); break;
			default: trace("%s", message); break;
		}
		
	});
	//
	discord_readying = true;
	return an_ok;
}
#pragma endregion
