/// trace(...)
gml_pragma("global", "global.g_trace = ds_list_create()");
var r = string(argument[0]);
for (var i = 1; i < argument_count; i++) {
    r += " " + string(argument[i]);
}
show_debug_message(r);
ds_list_insert(global.g_trace, 0, r);
ds_list_delete(global.g_trace, 30);
