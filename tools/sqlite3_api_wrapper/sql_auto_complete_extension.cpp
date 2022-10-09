#include "sql_auto_complete_extension.hpp"

#include "duckdb/function/table_function.hpp"
#include "duckdb/common/exception.hpp"
#include "duckdb/main/client_context.hpp"
#include "duckdb/parser/parser.hpp"
#include "duckdb/parser/parsed_data/create_table_function_info.hpp"
#include "duckdb/catalog/catalog.hpp"
#include "duckdb/common/case_insensitive_map.hpp"
#include "duckdb/main/client_data.hpp"
#include "duckdb/catalog/catalog_entry/table_catalog_entry.hpp"
#include "duckdb/catalog/catalog_entry/view_catalog_entry.hpp"
#include "duckdb/parser/keyword_helper.hpp"
#include "duckdb/common/file_opener.hpp"

namespace duckdb {

struct SQLAutoCompleteFunctionData : public TableFunctionData {
	explicit SQLAutoCompleteFunctionData(vector<string> suggestions_p) : suggestions(move(suggestions_p)) {
	}

	vector<string> suggestions;
};

struct SQLAutoCompleteData : public GlobalTableFunctionState {
	SQLAutoCompleteData() : offset(0) {
	}

	idx_t offset;
};

struct AutoCompleteCandidate {
	AutoCompleteCandidate(string candidate_p, int32_t score_bonus = 0)
	    : candidate(move(candidate_p)), score_bonus(score_bonus) {
	}

	string candidate;
	//! The higher the score bonus, the more likely this candidate will be chosen
	int32_t score_bonus;
};

static vector<string> ComputeSuggestions(vector<AutoCompleteCandidate> available_suggestions, const string &prefix,
                                         const unordered_set<string> &extra_keywords, bool add_quotes = false) {
	for (auto &kw : extra_keywords) {
		available_suggestions.emplace_back(move(kw));
	}
	vector<pair<string, idx_t>> scores;
	scores.reserve(available_suggestions.size());
	for (auto &suggestion : available_suggestions) {
		const int32_t BASE_SCORE = 10;
		auto &str = suggestion.candidate;
		auto bonus = suggestion.score_bonus;

		D_ASSERT(BASE_SCORE - bonus >= 0);
		auto score = idx_t(BASE_SCORE - bonus);
		if (prefix.size() == 0) {
		} else if (prefix.size() < str.size()) {
			score += StringUtil::LevenshteinDistance(str.substr(0, prefix.size()), prefix);
		} else {
			score += StringUtil::LevenshteinDistance(str, prefix);
		}
		D_ASSERT(score >= 0);
		scores.emplace_back(str, score);
	}
	auto results = StringUtil::TopNStrings(scores, 20, 999);
	if (add_quotes) {
		for (auto &result : results) {
			if (extra_keywords.find(result) == extra_keywords.end()) {
				result = KeywordHelper::WriteOptionallyQuoted(result, '"', true);
			}
		}
	}
	return results;
}

static vector<string> InitialKeywords() {
	return vector<string> {"SELECT",     "INSERT",     "DELETE",   "UPDATE",  "CREATE",  "DROP",     "COPY",
	                       "ALTER",      "WITH",       "EXPORT",   "BEGIN",   "VACUUM",  "PREPARE",  "EXECUTE",
	                       "DEALLOCATE", "SET",        "CALL",     "ANALYZE", "EXPLAIN", "DESCRIBE", "SUMMARIZE",
	                       "LOAD",       "CHECKPOINT", "ROLLBACK", "COMMIT",  "CALL"};
}

static vector<AutoCompleteCandidate> SuggestKeyword(ClientContext &context) {
	auto keywords = InitialKeywords();
	vector<AutoCompleteCandidate> result;
	for (auto &kw : keywords) {
		result.emplace_back(move(kw));
	}
	return result;
}

static vector<CatalogEntry *> GetAllTables(ClientContext &context, bool for_table_names) {
	vector<CatalogEntry *> result;
	// scan all the schemas for tables and collect them and collect them
	// for column names we avoid adding internal entries, because it pollutes the auto-complete too much
	// for table names this is generally fine, however
	auto schemas = Catalog::GetCatalog(context).schemas->GetEntries<SchemaCatalogEntry>(context);
	for (auto &schema : schemas) {
		schema->Scan(context, CatalogType::TABLE_ENTRY, [&](CatalogEntry *entry) {
			if (!entry->internal || for_table_names) {
				result.push_back(entry);
			}
		});
	};
	if (for_table_names) {
		for (auto &schema : schemas) {
			schema->Scan(context, CatalogType::TABLE_FUNCTION_ENTRY,
			             [&](CatalogEntry *entry) { result.push_back(entry); });
		};
	} else {
		for (auto &schema : schemas) {
			schema->Scan(context, CatalogType::SCALAR_FUNCTION_ENTRY,
			             [&](CatalogEntry *entry) { result.push_back(entry); });
		};
	}

	// check the temp schema as well
	ClientData::Get(context).temporary_objects->Scan(context, CatalogType::TABLE_ENTRY, [&](CatalogEntry *entry) {
		if (!entry->internal || for_table_names) {
			result.push_back(entry);
		}
	});

	return result;
}

static vector<AutoCompleteCandidate> SuggestTableName(ClientContext &context) {
	vector<AutoCompleteCandidate> suggestions;
	auto all_entries = GetAllTables(context, true);
	for (auto &entry : all_entries) {
		// prioritize user-defined entries (views & tables)
		int32_t bonus = (entry->internal || entry->type == CatalogType::TABLE_FUNCTION_ENTRY) ? 0 : 1;
		suggestions.emplace_back(entry->name, bonus);
	}
	return suggestions;
}

static vector<AutoCompleteCandidate> SuggestColumnName(ClientContext &context) {
	vector<AutoCompleteCandidate> suggestions;
	auto all_entries = GetAllTables(context, false);
	for (auto &entry : all_entries) {
		if (entry->type == CatalogType::TABLE_ENTRY) {
			auto &table = (TableCatalogEntry &)*entry;
			for (auto &col : table.columns) {
				suggestions.emplace_back(col.GetName(), 1);
			}
		} else if (entry->type == CatalogType::VIEW_ENTRY) {
			auto &view = (ViewCatalogEntry &)*entry;
			for (auto &col : view.aliases) {
				suggestions.emplace_back(col, 1);
			}
		} else {
			if (StringUtil::CharacterIsOperator(entry->name[0])) {
				continue;
			}
			suggestions.emplace_back(entry->name);
		};
	}
	return suggestions;
}

static vector<AutoCompleteCandidate> SuggestFileName(ClientContext &context, string &prefix) {
	auto &fs = FileSystem::GetFileSystem(context);
	string search_dir;
	for (idx_t i = prefix.size(); i > 0; i--) {
		if (prefix[i - 1] == '/' || prefix[i - 1] == '\\') {
			search_dir = prefix.substr(0, i - 1);
			prefix = prefix.substr(i);
			break;
		}
	}
	if (search_dir.empty()) {
		search_dir = ".";
	} else {
		search_dir = fs.ExpandPath(search_dir, FileOpener::Get(context));
	}
	vector<AutoCompleteCandidate> result;
	fs.ListFiles(search_dir, [&](const string &fname, bool is_dir) {
		string suggestion;
		if (is_dir) {
			suggestion = fname + fs.PathSeparator();
		} else {
			suggestion = fname;
		}
		result.emplace_back(move(suggestion));
	});
	return result;
}

enum class SuggestionState : uint8_t { SUGGEST_KEYWORD, SUGGEST_TABLE_NAME, SUGGEST_COLUMN_NAME, SUGGEST_FILE_NAME };

static vector<string> GenerateSuggestions(ClientContext &context, const string &sql) {
	// for auto-completion, we consider 4 scenarios
	// * there is nothing in the buffer, or only one word -> suggest a keyword
	// * the previous keyword is SELECT, WHERE, BY, HAVING, ... -> suggest a column name
	// * the previous keyword is FROM, INSERT, UPDATE ,... -> select a table name
	// * we are in a string constant -> suggest a filename
	// figure out which state we are in by doing a run through the query
	idx_t pos = 0;
	idx_t last_pos = 0;
	bool seen_word = false;
	unordered_set<string> suggested_keywords;
	SuggestionState suggest_state = SuggestionState::SUGGEST_KEYWORD;
	case_insensitive_set_t column_name_keywords = {"SELECT", "WHERE", "BY",    "HAVING", "QUALIFY",
	                                               "LIMIT",  "SET",   "USING", "ON"};
	case_insensitive_set_t table_name_keywords = {"FROM",   "JOIN",  "INSERT", "UPDATE",
	                                              "DELETE", "ALTER", "DROP",   "CALL"};
	case_insensitive_map_t<unordered_set<string>> next_keyword_map;
	next_keyword_map["SELECT"] = {"FROM",    "WHERE",  "GROUP",  "HAVING", "WINDOW", "ORDER",     "LIMIT",
	                              "QUALIFY", "SAMPLE", "VALUES", "UNION",  "EXCEPT", "INTERSECT", "DISTINCT"};
	next_keyword_map["WITH"] = {"RECURSIVE", "SELECT", "AS"};
	next_keyword_map["INSERT"] = {"INTO", "VALUES", "SELECT", "DEFAULT"};
	next_keyword_map["DELETE"] = {"FROM", "WHERE", "USING"};
	next_keyword_map["UPDATE"] = {"SET", "WHERE"};
	next_keyword_map["CREATE"] = {"TABLE", "SCHEMA", "VIEW", "SEQUENCE", "MACRO", "FUNCTION"};
	next_keyword_map["DROP"] = next_keyword_map["CREATE"];
	next_keyword_map["ALTER"] = {"TABLE", "VIEW", "ADD", "DROP", "COLUMN", "SET", "TYPE", "DEFAULT", "DATA", "RENAME"};

regular_scan:
	for (; pos < sql.size(); pos++) {
		if (sql[pos] == '\'') {
			pos++;
			last_pos = pos;
			goto in_string_constant;
		}
		if (sql[pos] == '"') {
			pos++;
			last_pos = pos;
			goto in_quotes;
		}
		if (sql[pos] == '-' && pos + 1 < sql.size() && sql[pos + 1] == '-') {
			goto in_comment;
		}
		if (sql[pos] == ';') {
			// semicolon: restart suggestion flow
			suggest_state = SuggestionState::SUGGEST_KEYWORD;
			suggested_keywords.clear();
			continue;
		}
		if (StringUtil::CharacterIsSpace(sql[pos]) || StringUtil::CharacterIsOperator(sql[pos])) {
			if (seen_word) {
				goto process_word;
			}
		} else {
			seen_word = true;
		}
	}
	goto standard_suggestion;
in_comment:
	for (; pos < sql.size(); pos++) {
		if (sql[pos] == '\n' || sql[pos] == '\r') {
			pos++;
			goto regular_scan;
		}
	}
	// no suggestions inside comments
	return vector<string>();
in_quotes:
	for (; pos < sql.size(); pos++) {
		if (sql[pos] == '"') {
			pos++;
			last_pos = pos;
			seen_word = true;
			goto regular_scan;
		}
	}
	goto standard_suggestion;
in_string_constant:
	for (; pos < sql.size(); pos++) {
		if (sql[pos] == '\'') {
			pos++;
			last_pos = pos;
			seen_word = true;
			goto regular_scan;
		}
	}
	suggest_state = SuggestionState::SUGGEST_FILE_NAME;
	goto standard_suggestion;
process_word : {
	while (StringUtil::CharacterIsSpace(sql[last_pos]) || StringUtil::CharacterIsOperator(sql[last_pos])) {
		last_pos++;
	}
	auto next_word = sql.substr(last_pos, pos - last_pos);
	if (table_name_keywords.find(next_word) != table_name_keywords.end()) {
		suggest_state = SuggestionState::SUGGEST_TABLE_NAME;
	} else if (column_name_keywords.find(next_word) != column_name_keywords.end()) {
		suggest_state = SuggestionState::SUGGEST_COLUMN_NAME;
	}
	auto entry = next_keyword_map.find(next_word);
	if (entry != next_keyword_map.end()) {
		suggested_keywords = entry->second;
	} else {
		suggested_keywords.erase(next_word);
	}
	seen_word = false;
	last_pos = pos;
	goto regular_scan;
}
standard_suggestion:
	while (StringUtil::CharacterIsSpace(sql[last_pos]) || StringUtil::CharacterIsOperator(sql[last_pos])) {
		last_pos++;
	}
	auto last_word = sql.substr(last_pos, pos - last_pos);
	switch (suggest_state) {
	case SuggestionState::SUGGEST_KEYWORD:
		return ComputeSuggestions(SuggestKeyword(context), last_word, suggested_keywords);
	case SuggestionState::SUGGEST_TABLE_NAME:
		return ComputeSuggestions(SuggestTableName(context), last_word, suggested_keywords, true);
	case SuggestionState::SUGGEST_COLUMN_NAME:
		return ComputeSuggestions(SuggestColumnName(context), last_word, suggested_keywords, true);
	case SuggestionState::SUGGEST_FILE_NAME:
		return ComputeSuggestions(SuggestFileName(context, last_word), last_word, unordered_set<string>());
	default:
		throw InternalException("Unrecognized suggestion state");
	}
}

static unique_ptr<FunctionData> SQLAutoCompleteBind(ClientContext &context, TableFunctionBindInput &input,
                                                    vector<LogicalType> &return_types, vector<string> &names) {
	names.emplace_back("suggestion");
	return_types.emplace_back(LogicalType::VARCHAR);

	return make_unique<SQLAutoCompleteFunctionData>(GenerateSuggestions(context, StringValue::Get(input.inputs[0])));
}

unique_ptr<GlobalTableFunctionState> SQLAutoCompleteInit(ClientContext &context, TableFunctionInitInput &input) {
	return make_unique<SQLAutoCompleteData>();
}

void SQLAutoCompleteFunction(ClientContext &context, TableFunctionInput &data_p, DataChunk &output) {
	auto &bind_data = (SQLAutoCompleteFunctionData &)*data_p.bind_data;
	auto &data = (SQLAutoCompleteData &)*data_p.global_state;
	if (data.offset >= bind_data.suggestions.size()) {
		// finished returning values
		return;
	}
	// start returning values
	// either fill up the chunk or return all the remaining columns
	idx_t count = 0;
	while (data.offset < bind_data.suggestions.size() && count < STANDARD_VECTOR_SIZE) {
		auto &entry = bind_data.suggestions[data.offset++];

		// suggestion, VARCHAR
		output.SetValue(0, count, Value(entry));

		count++;
	}
	output.SetCardinality(count);
}

void SQLAutoCompleteExtension::Load(DuckDB &db) {
	Connection con(db);
	con.BeginTransaction();

	auto &context = *con.context;

	Catalog &catalog = Catalog::GetCatalog(context);
	TableFunction auto_complete_fun("sql_auto_complete", {LogicalType::VARCHAR}, SQLAutoCompleteFunction,
	                                SQLAutoCompleteBind, SQLAutoCompleteInit);
	CreateTableFunctionInfo auto_complete_info(auto_complete_fun);
	catalog.CreateTableFunction(context, &auto_complete_info);

	con.Commit();
}

std::string SQLAutoCompleteExtension::Name() {
	return "sql_auto_complete";
}

} // namespace duckdb
