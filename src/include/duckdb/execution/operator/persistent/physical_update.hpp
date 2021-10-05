//===----------------------------------------------------------------------===//
//                         DuckDB
//
// duckdb/execution/operator/persistent/physical_update.hpp
//
//
//===----------------------------------------------------------------------===//

#pragma once

#include "duckdb/execution/physical_operator.hpp"

namespace duckdb {
class DataTable;

//! Physically update data in a table
class PhysicalUpdate : public PhysicalOperator {
public:
	PhysicalUpdate(vector<LogicalType> types, TableCatalogEntry &tableref, DataTable &table, vector<column_t> columns,
	               vector<unique_ptr<Expression>> expressions, vector<unique_ptr<Expression>> bound_defaults,
	               idx_t estimated_cardinality)
	    : PhysicalOperator(PhysicalOperatorType::UPDATE, move(types), estimated_cardinality), tableref(tableref),
	      table(table), columns(std::move(columns)), expressions(move(expressions)),
	      bound_defaults(move(bound_defaults)) {
	}

	TableCatalogEntry &tableref;
	DataTable &table;
	vector<column_t> columns;
	vector<unique_ptr<Expression>> expressions;
	vector<unique_ptr<Expression>> bound_defaults;
	bool update_is_del_and_insert;

public:
	// Source interface
	unique_ptr<GlobalSourceState> GetGlobalSourceState(ClientContext &context) const override;
	void GetData(ExecutionContext &context, DataChunk &chunk, GlobalSourceState &gstate,
	             LocalSourceState &lstate) const override;

public:
	// Sink interface
	unique_ptr<GlobalSinkState> GetGlobalSinkState(ClientContext &context) const override;
	unique_ptr<LocalSinkState> GetLocalSinkState(ExecutionContext &context) const override;
	SinkResultType Sink(ExecutionContext &context, GlobalSinkState &state, LocalSinkState &lstate,
	                    DataChunk &input) const override;
	void Combine(ExecutionContext &context, GlobalSinkState &gstate, LocalSinkState &lstate) const override;

	bool IsSink() const override {
		return true;
	}
	bool ParallelSink() const override {
		return true;
	}
};

} // namespace duckdb
