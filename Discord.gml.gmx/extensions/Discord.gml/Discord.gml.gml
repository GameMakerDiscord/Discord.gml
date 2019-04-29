#define discord_init
/// ()~

#define discord_cleanup
/// ()~

#define discord_create
/// (client_id:int64 in a string, flags = 0x0)->ok?
//!#import "global"
//#global discord_last_result

// initialize misc things:
var ii = array_create(0);
ii[0] = "lobby_id";
ii[1] = "owner_id";
ii[2] = "user_id";
ii[3] = "peer_id";
var i = array_length_1d(ii);
var ih = array_create(i);
var il = array_create(i);
while (--i >= 0) {
    ih[i] = ii[i] + "_hi";
    il[i] = ii[i] + "_lo";
}
global.g_discord_async_int64_id = ii;
global.g_discord_async_int64_hi = ih;
global.g_discord_async_int64_lo = il;

// verify that version is correct:
var rev = discord_ext_revision();
if (rev == 0) {
    show_debug_message("Discord.gml: Extension failed to load!");
} else if (rev != discord_gml_revision) {
    show_debug_message("Discord.gml: Version mismatch!");
} else {
    show_debug_message("Discord.gml: rev " + string(rev));
}

//
var _id = argument[0];
var _flags; if (argument_count > 1) _flags = argument[1]; else _flags = 0;
if (!is_string(_id)) get_string("Consider the following:", "https://bugs.yoyogames.com/view.php?id=30024");
var b/*:Buffer*/ = buffer_create(4096, buffer_grow, 1);
var ii = int64(_id);
global.g_discord_gml_buf = b;
global.g_discord_msg_buf = buffer_create(1024, buffer_grow, 1);
//
buffer_write(b, buffer_u64, ii); // https://bugs.yoyogames.com/view.php?id=30023
buffer_write(b, buffer_u64, _flags);
return discord_create_raw(buffer_get_address(b));

#define discord_get_time
/// ()->timestamp:int64
var b/*:Buffer*/ = global.g_discord_gml_buf;
return discord_glue_read_value(discord_get_time_raw(buffer_get_address(b)), b, buffer_u64);

#define discord_update
/// () : run once per frame!
discord_update_raw();

#define discord_async_event
/// (map = async_load) : preprocesses event data
var e; if (argument_count > 0) e = argument[0]; else e = async_load;
var t = e[?"event_type"];
if (string_copy(t, 1, 8) != "discord_") return false;
var ii = global.g_discord_async_int64_id;
var ih = global.g_discord_async_int64_hi;
var il = global.g_discord_async_int64_lo;
var i = array_length_1d(ii);
while (--i >= 0) {
    var h = e[?ih[i]];
    if (h != undefined) {
        e[?ii[i]] = (h << 32) | e[?il[i]];
        ds_map_delete(e, ih[i]);
        ds_map_delete(e, il[i]);
    }
}
if (t == "discord_lobby_message" && e[?"buffer"] == undefined) {
    var b/*:Buffer*/ = global.g_discord_msg_buf;
    i = e[?"size"];
    if (buffer_get_size(b) < i) buffer_resize(b, i);
    if (discord_lobby_message_next_raw(buffer_get_address(b))) {
        e[?"buffer"] = b;
    } else e[?"buffer"] = -1;
}
if (t == "discord_network_message" && e[?"buffer"] == undefined) {
    var b/*:Buffer*/ = global.g_discord_msg_buf;
    i = e[?"size"];
    if (buffer_get_size(b) < i) buffer_resize(b, i);
    if (discord_network_message_next_raw(buffer_get_address(b))) {
        e[?"buffer"] = b;
    } else e[?"buffer"] = -1;
}
return true;

#define discord_get_user_id
/// ()->user_id
var b/*:Buffer*/ = global.g_discord_gml_buf;
if (discord_get_user_id_raw(buffer_get_address(b))) {
    buffer_seek(b, 0, 0);
    return buffer_read(b, buffer_u64);
} else return 0;

//{ glue

#define discord_glue_int64_buf
/// (...:int64)->buffer~
var b/*:Buffer*/ = global.g_discord_gml_buf;
buffer_seek(b, 0, 0);
for (var i = 0; i < argument_count; i++) {
    var a = argument[i];
    if (is_int32(a) || is_real(a) || is_int64(a)) {
        buffer_write(b, buffer_u64, a);
    } else show_error("expected an int64, got `" + string(a) + "` (" + typeof(a) + ")", 1);
}
return b;

#define discord_glue_int64_ptr
/// (...:int64)->ptr~
var b/*:Buffer*/ = global.g_discord_gml_buf;
buffer_seek(b, 0, 0);
for (var i = 0; i < argument_count; i++) {
    var a = argument[i];
    if (is_int32(a) || is_real(a) || is_int64(a)) {
        buffer_write(b, buffer_u64, a);
    } else show_error("expected an int64, got `" + string(a) + "` (" + typeof(a) + ")", 1);
}
return buffer_get_address(b);

#define discord_glue_read_value
/// (result, buffer, type)->?value~
discord_last_result = argument0;
if (argument0 == discord_result_ok) {
    var b/*:Buffer*/ = argument1;
    buffer_seek(b, 0, 0);
    return buffer_read(b, argument2);
} else return undefined;

//} glue
//{ lobby search

#define discord_lobby_search_set_filter
/// (key:string, value:string|number, cmp:discord_lobby_search_cmp)
var k = argument0, v = argument1, c = argument2;
var dc; if (is_string(v) || is_ptr(v)) {
    dc = 1;
} else {
    dc = 2;
    if (is_real(v) && (v * 1000000) % 1 != 0) {
        v = string_format(v, 0, 15);
        var d = string_pos(".", v);
        if (d) {
            var i = string_byte_length(v);
            while (i > d) {
                if (string_byte_at(v, i) != ord("0")) {
                    v = string_copy(v, 1, i);
                    break;
                } else i -= 1;
            }
            if (i <= d) v = string_copy(v, 1, d - 1);
        }
    } else v = string(v);
}
return discord_lobby_search_set_filter_raw(k, v, c, dc);

#define discord_lobby_search_get_lobby_id
/// (index:int)->lobby_id||undefined
var b/*:Buffer*/ = global.g_discord_gml_buf;
return discord_glue_read_value(discord_lobby_search_get_lobby_id_raw(buffer_get_address(b), argument0), b, buffer_u64);

//} lobby search
//{ lobby flow

#define discord_activity_send_request_reply
/// (user_id, activity_request_reply)
return discord_activity_send_request_reply_raw(discord_glue_int64_ptr(argument0), argument1);

#define discord_activity_accept_invite
/// (user_id)
return discord_activity_send_request_reply_raw(discord_glue_int64_ptr(argument0));

#define discord_lobby_connect
/// (lobby_id, lobby_secret)
return discord_lobby_connect_raw(discord_glue_int64_ptr(argument0), argument1);

#define discord_lobby_disconnect
/// (lobby_id)
return discord_lobby_disconnect_raw(discord_glue_int64_ptr(argument0));

#define discord_lobby_voice_connect
/// (lobby_id) ~> async event
return discord_lobby_voice_connect_raw(discord_glue_int64_ptr(argument0));

#define discord_lobby_voice_disconnect
/// (lobby_id) ~> async event
return discord_lobby_voice_disconnect_raw(discord_glue_int64_ptr(argument0));

//} lobby flow
//{ lobby-specific
#define discord_lobby_get_activity_secret
/// (lobby_id)->?secret:string
var b/*:Buffer*/ = discord_glue_int64_buf(argument0);
return discord_glue_read_value(discord_lobby_get_activity_secret_raw(buffer_get_address(b)), b, buffer_string);

#define discord_lobby_get_metadata_count
/// (lobby_id)->?count
var b/*:Buffer*/ = discord_glue_int64_buf(argument0);
return discord_glue_read_value(discord_lobby_get_metadata_count_raw(buffer_get_address(b)), b, buffer_s32);

#define discord_lobby_get_metadata_key
/// (lobby_id, key_index:int)->?key:string
var b/*:Buffer*/ = discord_glue_int64_buf(argument0);
return discord_glue_read_value(discord_lobby_get_metadata_key_raw(buffer_get_address(b), argument1), b, buffer_string);

#define discord_lobby_get_metadata_value
/// (lobby_id, key:string)->?value:string
var b/*:Buffer*/ = discord_glue_int64_buf(argument0);
return discord_glue_read_value(discord_lobby_get_metadata_value_raw(buffer_get_address(b), argument1), b, buffer_string);

#define discord_lobby_update_start
/// (lobby_id)->result
return discord_lobby_update_start_raw(discord_glue_int64_ptr(argument0));

#define discord_lobby_update_set_owner
// (user_id)->result
return discord_lobby_update_set_owner_raw(discord_glue_int64_ptr(argument0));

#define discord_lobby_member_update_start
/// (lobby_id, user_id)->result
return discord_lobby_member_update_start_raw(discord_glue_int64_ptr(argument0, argument1));

#define discord_lobby_send_message
/// (lobby_id, buffer, size = buffer.tell) ~> async event
var lobby_id = argument[0], buf/*:Buffer*/ = argument[1];
var size; if (argument_count > 2) size = argument[2]; else size = buffer_tell(buf);
return discord_lobby_send_message_raw(discord_glue_int64_ptr(lobby_id), buffer_get_address(buf), size);

//} lobby-specific
//{ member-specific 

#define discord_lobby_get_owner_user_id
/// (lobby_id)->?user_id
var b/*:Buffer*/ = discord_glue_int64_buf(argument0);
return discord_glue_read_value(discord_lobby_get_owner_user_id_raw(buffer_get_address(b)), b, buffer_u64);

#define discord_lobby_get_member_count
/// (lobby_id)->?count
var b/*:Buffer*/ = discord_glue_int64_buf(argument0);
return discord_glue_read_value(discord_lobby_get_member_count_raw(buffer_get_address(b)), b, buffer_s32);

#define discord_lobby_get_member_user_id
/// (lobby_id, member_index:int)->?user_id
var b/*:Buffer*/ = discord_glue_int64_buf(argument0);
return discord_glue_read_value(discord_lobby_get_member_user_id_raw(buffer_get_address(b), argument1), b, buffer_u64);

#define discord_lobby_get_member_metadata_count
/// (lobby_id, user_id)->?count
var b/*:Buffer*/ = discord_glue_int64_buf(argument0);
return discord_glue_read_value(discord_lobby_get_member_metadata_count_raw(buffer_get_address(b)), b, buffer_s32);

#define discord_lobby_get_member_metadata_key
/// (lobby_id, user_id, key_index:int)->?key:string
var b/*:Buffer*/ = discord_glue_int64_buf(argument0, argument1);
return discord_glue_read_value(discord_lobby_get_member_metadata_key_raw(buffer_get_address(b), argument2), b, buffer_string);

#define discord_lobby_get_member_metadata_value
/// (lobby_id, user_id, key:string)->?value:string
var b/*:Buffer*/ = discord_glue_int64_buf(argument0, argument1);
return discord_glue_read_value(discord_lobby_get_member_metadata_value_raw(buffer_get_address(b), argument2), b, buffer_string);

#define discord_lobby_get_member_username
/// (lobby_id, user_id)->?username:string
var b/*:Buffer*/ = discord_glue_int64_buf(argument0, argument1);
return discord_glue_read_value(discord_lobby_get_member_username_raw(buffer_get_address(b)), b, buffer_string);

#define discord_lobby_get_member_discriminator
/// (lobby_id, user_id)->?discriminator:string
var b/*:Buffer*/ = discord_glue_int64_buf(argument0, argument1);
return discord_glue_read_value(discord_lobby_get_member_discriminator_raw(buffer_get_address(b)), b, buffer_string);

//} member-specific
//{ networking

#define discord_network_get_peer_id
/// ()->peer_id
var b/*:Buffer*/ = global.g_discord_gml_buf;
return discord_glue_read_value(discord_network_get_peer_id_raw(buffer_get_address(b)), b, buffer_u64);

#define discord_network_open_peer
/// (peer_id, route)->result
return discord_network_open_peer_raw(discord_glue_int64_ptr(argument0), argument1);

#define discord_network_open_channel
/// (peer_id, channel_id:byte, reliable:bool)->result
return discord_network_open_channel_raw(discord_glue_int64_ptr(argument0), argument1, argument2);

#define discord_network_sync_get_peer_id
/// ()->peer_id
var b/*:Buffer*/ = global.g_discord_gml_buf;
if (discord_network_sync_get_peer_id_raw(buffer_get_address(b))) {
    buffer_seek(b, 0, 0);
    return buffer_read(b, buffer_u64);
} else return 0;

#define discord_network_sync_get_data
/// (target_buffer)
var b/*:Buffer*/ = argument0;
var n = discord_network_sync_get_size();
if (buffer_get_size(b) < n) buffer_resize(b, n);
return discord_network_sync_get_data_raw(buffer_get_address(b));

#define discord_network_send_message
/// (peer_id, channel_id:byte, buffer, size = buffer.tell)->result
var peer_id = argument[0], channel_id = argument[1], buffer/*:Buffer*/ = argument[2];
var size; if (argument_count > 3) size = argument[3]; else size = buffer_tell(buffer);;
var b/*:Buffer*/ = global.g_discord_gml_buf;
buffer_seek(b, 0, 0);
buffer_write(b, buffer_u64, peer_id);
return discord_network_send_message_raw(buffer_get_address(b), channel_id, buffer_get_address(buffer), size);

//} networking