#include "vcsv_resource_loader.h"

#include <godot_cpp/classes/file_access.hpp>
#include <godot_cpp/core/class_db.hpp>
#include <godot_cpp/variant/packed_string_array.hpp>

#include "vcsv_data_table.h"
#include "vcsv_parse_options.h"

namespace godot {

VCSVResourceLoader::VCSVResourceLoader() {
}

Variant VCSVResourceLoader::_load(const String &p_path, const String &p_original_path, bool p_use_sub_threads,
		int32_t p_cache_mode) const {
	// Parse the raw file exactly like the editor import: default options
	// (comma delimiter, 1 header row, utf8) already match the .import params.
	Ref<VCSVDataTable> table = VCSVDataTable::from_file(p_path, Ref<VCSVParseOptions>(), String());
	if (table.is_null()) {
		return Variant();
	}
	// Take a strong reference on the Resource base so the returned Variant owns
	// the table (mirrors vml_resource_router's Ref<Resource> -> Variant pattern).
	Ref<Resource> res = Object::cast_to<Resource>(table.ptr());
	return res;
}

bool VCSVResourceLoader::_recognize_path(const String &p_path, const StringName &p_type) const {
	const String ext = p_path.get_extension().to_lower();
	return ext == "csv" || ext == "tsv";
}

bool VCSVResourceLoader::_handles_type(const StringName &p_type) const {
	return p_type == StringName("VCSVDataTable");
}

PackedStringArray VCSVResourceLoader::_get_recognized_extensions() const {
	PackedStringArray a;
	a.push_back("csv");
	a.push_back("tsv");
	return a;
}

bool VCSVResourceLoader::_exists(const String &p_path) const {
	return FileAccess::file_exists(p_path);
}

String VCSVResourceLoader::_get_resource_type(const String &p_path) const {
	return "VCSVDataTable";
}

void VCSVResourceLoader::_bind_methods() {
}

} // namespace godot
