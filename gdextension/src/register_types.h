#ifndef DUNE_GDEXTENSION_REGISTER_TYPES_H
#define DUNE_GDEXTENSION_REGISTER_TYPES_H

#include <godot_cpp/core/class_db.hpp>

void initialize_dune_module(godot::ModuleInitializationLevel p_level);
void uninitialize_dune_module(godot::ModuleInitializationLevel p_level);

#endif // DUNE_GDEXTENSION_REGISTER_TYPES_H
