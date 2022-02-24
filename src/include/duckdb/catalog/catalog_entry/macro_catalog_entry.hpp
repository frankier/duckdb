//===----------------------------------------------------------------------===//
//                         DuckDB
//
// duckdb/catalog/catalog_entry/macro_catalog_entry.hpp
//
//
//===----------------------------------------------------------------------===//

#pragma once

#include "duckdb/catalog/catalog_set.hpp"
#include "duckdb/catalog/standard_entry.hpp"
#include "duckdb/parser/parsed_data/create_macro_info.hpp"
#include "duckdb/catalog/catalog_entry/base_macro_catalog_entry.hpp"

namespace duckdb {

//! A macro function in the catalog
class MacroCatalogEntry : public BaseMacroCatalogEntry {
public:
	MacroCatalogEntry(Catalog *catalog, SchemaCatalogEntry *schema, CreateMacroInfo *info);

public:
	//! Serialize the meta information of the MacroCatalogEntry a serializer
	virtual void Serialize(Serializer &serializer);
	//! Deserializes to a CreateMacroInfo
	static unique_ptr<CreateMacroInfo> Deserialize(Deserializer &source);
};
} // namespace duckdb
