// Database.h - Thin RAII wrapper over the SQLite licence store.
//
// Shared by every backend tool (CLI, validation server, Discord bot) so they
// all read and write exactly the same schema. Not thread safe on its own; the
// server serialises access behind a mutex.
#pragma once

#include <cstdint>
#include <functional>
#include <optional>
#include <string>
#include <vector>

struct sqlite3;
struct sqlite3_stmt;

namespace ngl {

/// One bound value for a prepared statement.
struct SqlValue
{
    enum class Kind { Null, Int, Text } kind = Kind::Null;
    std::int64_t integer = 0;
    std::string text;

    SqlValue() = default;
    SqlValue(std::int64_t v) : kind(Kind::Int), integer(v) {}
    SqlValue(int v) : kind(Kind::Int), integer(v) {}
    SqlValue(const std::string &v) : kind(Kind::Text), text(v) {}
    SqlValue(const char *v) : kind(Kind::Text), text(v ? v : "") {}
    static SqlValue null() { return SqlValue{}; }
};

/// One result row, addressable by column index.
class Row
{
public:
    explicit Row(sqlite3_stmt *stmt) : m_stmt(stmt) {}

    std::int64_t integer(int column) const;
    std::string text(int column) const;
    bool isNull(int column) const;

private:
    sqlite3_stmt *m_stmt;
};

class Database
{
public:
    Database() = default;
    ~Database();

    Database(const Database &) = delete;
    Database &operator=(const Database &) = delete;

    /// Opens (creating if needed) the database at \a path and applies the schema.
    bool open(const std::string &path, std::string *error = nullptr);
    void close();
    bool isOpen() const { return m_db != nullptr; }

    /// Runs a statement with no result rows (INSERT / UPDATE / DDL).
    bool execute(const std::string &sql, const std::vector<SqlValue> &params = {},
                 std::string *error = nullptr);

    /// Runs a query, invoking \a onRow for each result row.
    bool query(const std::string &sql, const std::vector<SqlValue> &params,
               const std::function<void(const Row &)> &onRow, std::string *error = nullptr);

    /// Rowid of the last successful INSERT.
    std::int64_t lastInsertId() const;
    /// Rows changed by the last execute().
    int changes() const;

    std::string lastError() const { return m_lastError; }

private:
    bool applySchema(std::string *error);

    sqlite3 *m_db = nullptr;
    std::string m_lastError;
};

} // namespace ngl
